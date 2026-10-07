#include <cstdio>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

extern "C" {
#include "pty/watermark.h"
#include "src/renderer.h"
}

namespace {

TEST(RendererCoreTest, ClearAndWriteHonorCharacterWidthPadding) {
  strange_screen_buffer buffer = {};

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 2, 2, 3), 0);
  strange_screen_buffer_clear(&buffer);

  for (int cell = 0; cell < 4; ++cell) {
    const int index = cell * 3;
    EXPECT_EQ(buffer.buffer[index], STRANGE_SPACE_CHAR);
    EXPECT_EQ(buffer.buffer[index + 1], STRANGE_PAD_CHAR);
    EXPECT_EQ(buffer.buffer[index + 2], STRANGE_PAD_CHAR);
  }

  const char glyph[] = {'X', 'Y', '\0'};
  strange_screen_buffer_write(&buffer, glyph, 2, 1, 0);
  EXPECT_EQ(buffer.buffer[3], 'X');
  EXPECT_EQ(buffer.buffer[4], 'Y');
  EXPECT_EQ(buffer.buffer[5], STRANGE_PAD_CHAR);

  strange_screen_buffer_free(&buffer);
}

TEST(RendererCoreTest, ResizeReallocatesAndClearsTheNewFrame) {
  strange_screen_buffer buffer = {};

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 1, 1, 1), 0);
  strange_screen_buffer_write_string(&buffer, "Z", 0, 0);

  ASSERT_EQ(strange_screen_buffer_resize(&buffer, 3, 2), 0);
  EXPECT_EQ(buffer.w, 3);
  EXPECT_EQ(buffer.h, 2);

  for (int index = 0; index < buffer.w * buffer.h; ++index) {
    EXPECT_EQ(buffer.buffer[index], STRANGE_SPACE_CHAR);
  }

  strange_screen_buffer_free(&buffer);
}

TEST(RendererCoreTest, PresentWritesTheFrameAndAdvancesFrameCount) {
  strange_render_context context = {};
  char rendered[32] = {};

  ASSERT_EQ(strange_screen_buffer_init(&context.buffer, 2, 1, 1), 0);
  context.stream = tmpfile();
  ASSERT_NE(context.stream, nullptr);

  strange_screen_buffer_write_string(&context.buffer, "OK", 0, 0);
  ASSERT_EQ(strange_render_context_present(&context), 0);
  EXPECT_EQ(context.frame_count, 1U);

  ASSERT_EQ(std::fflush(context.stream), 0);
  std::rewind(context.stream);
  ASSERT_NE(std::fgets(rendered, sizeof(rendered), context.stream), nullptr);
  EXPECT_STREQ(rendered, "\033[1;1HOK");

  std::fclose(context.stream);
  strange_render_context_destroy(&context);
}

TEST(RendererCoreTest, PresentAfterTheFirstFrameWritesOnlyChangedCells) {
  strange_render_context context = {};
  char rendered[64] = {};

  ASSERT_EQ(strange_screen_buffer_init(&context.buffer, 12, 2, 1), 0);
  context.stream = tmpfile();
  ASSERT_NE(context.stream, nullptr);

  strange_screen_buffer_write_string(&context.buffer, "hello world", 0, 1);
  ASSERT_EQ(strange_render_context_present(&context), 0);
  ASSERT_EQ(strange_render_context_present(&context), 0);
  const long after_identical_frame = std::ftell(context.stream);

  strange_screen_buffer_write_string(&context.buffer, "J", 0, 1);
  strange_screen_buffer_write_string(&context.buffer, "p", 2, 1);
  strange_screen_buffer_write_string(&context.buffer, "R", 8, 1);
  ASSERT_EQ(strange_render_context_present(&context), 0);

  const long first_frame_bytes =
      static_cast<long>(std::strlen("\033[1;1H\033[2;1H")) + 24;
  EXPECT_EQ(after_identical_frame, first_frame_bytes);

  ASSERT_EQ(std::fseek(context.stream, after_identical_frame, SEEK_SET), 0);
  ASSERT_NE(std::fgets(rendered, sizeof(rendered), context.stream), nullptr);
  EXPECT_STREQ(rendered, "\033[2;1HJep\033[2;9HR");

  std::fclose(context.stream);
  strange_render_context_destroy(&context);
}

TEST(RendererCoreTest, WatermarkRendersInstructionsIntoTopRightCorner) {
  strange_screen_buffer buffer = {};
  const char *wake_line = " any key wakes ";
  const char *disable_line = " Ctrl-Q disables ";

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 30, 4, 1), 0);
  strange_screen_buffer_clear(&buffer);
  strange_watermark_render(&buffer, 0, 0x11);

  std::string row0(buffer.buffer, buffer.w);
  std::string row1(buffer.buffer + buffer.w, buffer.w);

  EXPECT_EQ(row0.find(wake_line), 30U - std::strlen(wake_line));
  EXPECT_EQ(row1.find(disable_line), 30U - std::strlen(disable_line));

  strange_screen_buffer_clear(&buffer);
  strange_watermark_render(&buffer, 0, 0);
  EXPECT_EQ(std::string(buffer.buffer + buffer.w, buffer.w),
            std::string(30, ' '));

  strange_screen_buffer_free(&buffer);
}

}  // namespace
