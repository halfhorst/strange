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
  ScreenBuffer buffer = {};

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 2, 2, 3), 0);
  strange_screen_buffer_clear(&buffer);

  for (int cell = 0; cell < 4; ++cell) {
    const int index = cell * 3;
    EXPECT_EQ(buffer.buffer[index], SL_SPACE_CHAR);
    EXPECT_EQ(buffer.buffer[index + 1], SL_PAD_CHAR);
    EXPECT_EQ(buffer.buffer[index + 2], SL_PAD_CHAR);
  }

  const char glyph[] = {'X', 'Y', '\0'};
  write_to_buffer(&buffer, glyph, 2, 1, 0);
  EXPECT_EQ(buffer.buffer[3], 'X');
  EXPECT_EQ(buffer.buffer[4], 'Y');
  EXPECT_EQ(buffer.buffer[5], SL_PAD_CHAR);

  strange_screen_buffer_free(&buffer);
}

TEST(RendererCoreTest, ResizeReallocatesAndClearsTheNewFrame) {
  ScreenBuffer buffer = {};

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 1, 1, 1), 0);
  write_string_to_buffer(&buffer, "Z", 0, 0);

  ASSERT_EQ(strange_screen_buffer_resize(&buffer, 3, 2), 0);
  EXPECT_EQ(buffer.w, 3);
  EXPECT_EQ(buffer.h, 2);

  for (int index = 0; index < buffer.w * buffer.h; ++index) {
    EXPECT_EQ(buffer.buffer[index], SL_SPACE_CHAR);
  }

  strange_screen_buffer_free(&buffer);
}

TEST(RendererCoreTest, PresentWritesTheFrameAndAdvancesFrameCount) {
  strange_render_context context = {};
  char rendered[32] = {};

  ASSERT_EQ(strange_screen_buffer_init(&context.buffer, 2, 1, 1), 0);
  context.stream = tmpfile();
  ASSERT_NE(context.stream, nullptr);

  write_string_to_buffer(&context.buffer, "OK", 0, 0);
  ASSERT_EQ(strange_render_context_present(&context), 0);
  EXPECT_EQ(context.frame_count, 1U);

  ASSERT_EQ(std::fflush(context.stream), 0);
  std::rewind(context.stream);
  ASSERT_NE(std::fgets(rendered, sizeof(rendered), context.stream), nullptr);
  EXPECT_STREQ(rendered, "\033[HOK\n");

  std::fclose(context.stream);
  strange_screen_buffer_free(&context.buffer);
}

TEST(RendererCoreTest, WatermarkRendersInstructionsIntoTopRightCorner) {
  ScreenBuffer buffer = {};
  const char *wake_line = " any key wakes ";
  const char *disable_line = " Ctrl-Q disables ";

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 30, 4, 1), 0);
  strange_screen_buffer_clear(&buffer);
  render_screensaver_watermark(&buffer);

  std::string row0(buffer.buffer, buffer.w);
  std::string row1(buffer.buffer + buffer.w, buffer.w);

  EXPECT_EQ(row0.find(wake_line), 30U - std::strlen(wake_line));
  EXPECT_EQ(row1.find(disable_line), 30U - std::strlen(disable_line));

  strange_screen_buffer_free(&buffer);
}

}  // namespace
