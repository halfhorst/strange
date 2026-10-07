#include <cstdio>
#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#include "src/session.h"

namespace {

std::string Status(const char *terminal_name, int *result) {
  char output[512] = {};
  FILE *stream = tmpfile();

  *result = strange_session_print_status(stream, terminal_name);
  std::rewind(stream);
  const size_t length = std::fread(output, 1, sizeof(output) - 1, stream);
  std::fclose(stream);
  return std::string(output, length);
}

class SessionTest : public ::testing::Test {
 protected:
  void TearDown() override {
    unsetenv(STRANGE_SESSION_TTY_VARIABLE);
    unsetenv(STRANGE_SESSION_SCREENSAVER_VARIABLE);
    unsetenv(STRANGE_SESSION_TIMEOUT_VARIABLE);
    unsetenv(STRANGE_SESSION_COVER_FULLSCREEN_VARIABLE);
  }
};

TEST_F(SessionTest, NoSessionWithoutTheTerminalVariable) {
  int result = 0;

  unsetenv(STRANGE_SESSION_TTY_VARIABLE);

  EXPECT_FALSE(strange_session_is_current("/dev/ttys001"));
  EXPECT_EQ(Status("/dev/ttys001", &result), "Not running under strange\n");
  EXPECT_EQ(result, 1);
}

TEST_F(SessionTest, SessionInheritedFromAnotherTerminalDoesNotCount) {
  ASSERT_EQ(setenv(STRANGE_SESSION_TTY_VARIABLE, "/dev/ttys001", 1), 0);

  EXPECT_TRUE(strange_session_is_current("/dev/ttys001"));
  EXPECT_FALSE(strange_session_is_current("/dev/ttys002"));
  EXPECT_FALSE(strange_session_is_current(nullptr));
}

TEST_F(SessionTest, StatusReportsTheExportedConfiguration) {
  int result = 1;

  ASSERT_EQ(setenv(STRANGE_SESSION_TTY_VARIABLE, "/dev/ttys001", 1), 0);
  ASSERT_EQ(strange_session_export("wave", 45, 1), 0);

  EXPECT_EQ(Status("/dev/ttys001", &result),
            "Running under strange\n"
            "  screensaver: wave\n"
            "  timeout: 45 seconds\n"
            "  full-screen programs: covered\n");
  EXPECT_EQ(result, 0);
}

}  // namespace
