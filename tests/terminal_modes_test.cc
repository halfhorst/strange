#include <cstring>

#include <gtest/gtest.h>

extern "C" {
#include "pty/terminal_modes.h"
}

namespace {

void Write(strange_terminal_modes *modes, const char *text) {
  strange_terminal_modes_write(modes, text, std::strlen(text));
}

TEST(TerminalModesTest, StartsOnTheMainScreenWithAVisibleCursor) {
  strange_terminal_modes modes;

  strange_terminal_modes_init(&modes);

  EXPECT_FALSE(modes.alternate_screen);
  EXPECT_TRUE(modes.cursor_visible);
}

TEST(TerminalModesTest, FollowsAlternateScreenAndCursorVisibility) {
  strange_terminal_modes modes;

  strange_terminal_modes_init(&modes);
  Write(&modes, "text\033[?1049h\033[?25l\033[1;31mmore");
  EXPECT_TRUE(modes.alternate_screen);
  EXPECT_FALSE(modes.cursor_visible);

  Write(&modes, "\033[?1049l\033[?25h");
  EXPECT_FALSE(modes.alternate_screen);
  EXPECT_TRUE(modes.cursor_visible);
}

TEST(TerminalModesTest, HandlesCombinedParametersAndSplitWrites) {
  strange_terminal_modes modes;

  strange_terminal_modes_init(&modes);
  Write(&modes, "\033[?1;47;25h");
  EXPECT_TRUE(modes.alternate_screen);
  EXPECT_TRUE(modes.cursor_visible);

  Write(&modes, "\033[?10");
  Write(&modes, "47l");
  EXPECT_FALSE(modes.alternate_screen);
}

TEST(TerminalModesTest, IgnoresLookalikeSequences) {
  strange_terminal_modes modes;

  strange_terminal_modes_init(&modes);
  Write(&modes, "\033[1049h\033[25l[?1049h\033[?1049m\033[?2004h");

  EXPECT_FALSE(modes.alternate_screen);
  EXPECT_TRUE(modes.cursor_visible);
}

TEST(TerminalModesTest, FullResetReturnsToDefaults) {
  strange_terminal_modes modes;

  strange_terminal_modes_init(&modes);
  Write(&modes, "\033[?1049h\033[?25l\033c");

  EXPECT_FALSE(modes.alternate_screen);
  EXPECT_TRUE(modes.cursor_visible);
}

}  // namespace
