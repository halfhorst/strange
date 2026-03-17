#include <cstdio>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

extern "C" {
#include "pty/visible_screen.h"
}

namespace {

std::string Row(const ScreenBuffer &buffer, int row) {
  return std::string(buffer.buffer + (row * buffer.w), buffer.w);
}

std::string ReadFile(FILE *stream) {
  char output[256] = {};

  EXPECT_EQ(std::fflush(stream), 0);
  std::rewind(stream);
  const size_t read = std::fread(output, 1, sizeof(output), stream);
  return std::string(output, read);
}

TEST(VisibleScreenTest, TracksAnsiTextAndRestoresCapturedSnapshot) {
  strange_visible_screen screen = {};

  ASSERT_EQ(strange_visible_screen_init(&screen, 6, 2), 0);
  ASSERT_EQ(strange_visible_screen_write(&screen, "abc\rXY", 6), 0);
  ASSERT_EQ(strange_visible_screen_write(&screen, "\033[2;3HZ\033[?25l", 13),
            0);

  EXPECT_EQ(Row(screen.current, 0), "XYc   ");
  EXPECT_EQ(Row(screen.current, 1), "  Z   ");
  EXPECT_EQ(screen.cursor_x, 3);
  EXPECT_EQ(screen.cursor_y, 1);
  EXPECT_EQ(screen.cursor_visible, 0);

  ASSERT_EQ(strange_visible_screen_capture_snapshot(&screen), 0);
  ASSERT_EQ(strange_visible_screen_write(&screen, "\033[2JQQ", 6), 0);

  FILE *stream = tmpfile();
  ASSERT_NE(stream, nullptr);
  ASSERT_EQ(strange_visible_screen_restore(&screen, stream), 0);

  EXPECT_EQ(ReadFile(stream),
            "\033[2J\033[HXYc   \n  Z   \n\033[2;4H\033[?25l");

  std::fclose(stream);
  strange_visible_screen_destroy(&screen);
}

TEST(VisibleScreenTest, ResizeCanDiscardSnapshotAndResetCurrentContents) {
  strange_visible_screen screen = {};

  ASSERT_EQ(strange_visible_screen_init(&screen, 5, 2), 0);
  ASSERT_EQ(strange_visible_screen_write(&screen, "hello", 5), 0);
  ASSERT_EQ(strange_visible_screen_capture_snapshot(&screen), 0);

  ASSERT_EQ(strange_visible_screen_resize(&screen, 4, 3, 1, 1), 0);

  EXPECT_FALSE(screen.snapshot_valid);
  EXPECT_EQ(screen.current.w, 4);
  EXPECT_EQ(screen.current.h, 3);
  EXPECT_EQ(screen.cursor_x, 0);
  EXPECT_EQ(screen.cursor_y, 0);
  EXPECT_EQ(Row(screen.current, 0), "    ");
  EXPECT_EQ(Row(screen.current, 1), "    ");
  EXPECT_EQ(Row(screen.current, 2), "    ");

  strange_visible_screen_destroy(&screen);
}

TEST(VisibleScreenTest, SameSizeResizeKeepsTheCapturedSnapshot) {
  strange_visible_screen screen = {};

  ASSERT_EQ(strange_visible_screen_init(&screen, 5, 2), 0);
  ASSERT_EQ(strange_visible_screen_write(&screen, "hello", 5), 0);
  ASSERT_EQ(strange_visible_screen_capture_snapshot(&screen), 0);

  ASSERT_EQ(strange_visible_screen_resize(&screen, 5, 2, 1, 1), 0);

  EXPECT_TRUE(screen.snapshot_valid);
  EXPECT_EQ(Row(screen.snapshot, 0), "hello");

  strange_visible_screen_destroy(&screen);
}

TEST(VisibleScreenTest, RestoresARealShellTranscriptSnapshot) {
  strange_visible_screen screen = {};
  const char startup[] = "\033[?1034hsh$ ";
  const char command[] =
      "printf 'before-saver\\n'\r\nbefore-saver\r\nsh$ ";

  ASSERT_EQ(strange_visible_screen_init(&screen, 79, 23), 0);
  ASSERT_EQ(strange_visible_screen_write(&screen, startup, sizeof(startup) - 1),
            0);
  ASSERT_EQ(strange_visible_screen_write(&screen, command, sizeof(command) - 1),
            0);
  ASSERT_EQ(strange_visible_screen_capture_snapshot(&screen), 0);

  FILE *stream = tmpfile();
  ASSERT_NE(stream, nullptr);
  ASSERT_EQ(strange_visible_screen_restore(&screen, stream), 0);

  const std::string restored = ReadFile(stream);
  EXPECT_NE(restored.find("sh$ printf 'before-saver\\n'"), std::string::npos);
  EXPECT_NE(restored.find("before-saver"), std::string::npos);

  std::fclose(stream);
  strange_visible_screen_destroy(&screen);
}

TEST(VisibleScreenTest, OscSequencesDoNotPolluteTheVisibleBuffer) {
  strange_visible_screen screen = {};

  ASSERT_EQ(strange_visible_screen_init(&screen, 6, 1), 0);
  ASSERT_EQ(strange_visible_screen_write(&screen, "\033]0;title\aOK", 12), 0);

  EXPECT_EQ(Row(screen.current, 0), "OK    ");

  strange_visible_screen_destroy(&screen);
}

}  // namespace
