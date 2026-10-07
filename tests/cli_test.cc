#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <gtest/gtest.h>

#include "tests/scoped_home.h"

extern "C" {
#include "src/cli.h"
}

namespace {

std::string CaptureListOutput() {
  char error[256] = {0};
  FILE *stream = tmpfile();
  std::string output;

  EXPECT_NE(stream, nullptr);
  if (stream == nullptr) {
    return output;
  }

  EXPECT_EQ(strange_cli_print_list(stream, error, sizeof(error)), 0) << error;
  std::fflush(stream);
  std::rewind(stream);

  char buffer[256];
  while (std::fgets(buffer, sizeof(buffer), stream) != nullptr) {
    output += buffer;
  }

  std::fclose(stream);
  return output;
}

std::string SectionBody(const std::string &output, const std::string &header,
                        const std::string &next_header = "") {
  const size_t header_pos = output.find(header);
  size_t body_start = 0;
  size_t body_end = std::string::npos;

  if (header_pos == std::string::npos) {
    return "";
  }

  body_start = header_pos + header.size();
  if (!next_header.empty()) {
    body_end = output.find(next_header, body_start);
  }

  return output.substr(body_start, body_end - body_start);
}

TEST(CliTest, ParsesNamedScreensaverWithDefaultTimeout) {
  char arg0[] = "strange";
  char arg1[] = "denabase";
  char *argv[] = {arg0, arg1};
  strange_cli_options options = {};
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(2, argv, &options, error, sizeof(error)), 0);
  EXPECT_EQ(options.command, STRANGE_CLI_COMMAND_RUN_NAMED);
  EXPECT_EQ(options.timeout_seconds, STRANGE_DEFAULT_TIMEOUT_SECONDS);
  EXPECT_STREQ(options.screensaver_name, "denabase");
}

TEST(CliTest, ParsesNamedScreensaverWithExplicitTimeout) {
  char arg0[] = "strange";
  char arg1[] = "--timeout";
  char arg2[] = "7";
  char arg3[] = "digital-rain";
  char *argv[] = {arg0, arg1, arg2, arg3};
  strange_cli_options options = {};
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(4, argv, &options, error, sizeof(error)), 0);
  EXPECT_EQ(options.command, STRANGE_CLI_COMMAND_RUN_NAMED);
  EXPECT_EQ(options.timeout_seconds, 7);
  EXPECT_STREQ(options.screensaver_name, "digital-rain");
}

TEST(CliTest, ParsesRandomModeWithMultipleNames) {
  char arg0[] = "strange";
  char arg1[] = "--random";
  char arg2[] = "denabase";
  char arg3[] = "digital-rain";
  char *argv[] = {arg0, arg1, arg2, arg3};
  strange_cli_options options = {};
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(4, argv, &options, error, sizeof(error)), 0);
  EXPECT_EQ(options.command, STRANGE_CLI_COMMAND_RUN_RANDOM);
  EXPECT_EQ(options.random_name_count, 2U);
  EXPECT_STREQ(options.random_names[0], "denabase");
  EXPECT_STREQ(options.random_names[1], "digital-rain");
}

TEST(CliTest, ParsesListMode) {
  char arg0[] = "strange";
  char arg1[] = "--list";
  char *argv[] = {arg0, arg1};
  strange_cli_options options = {};
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(2, argv, &options, error, sizeof(error)), 0);
  EXPECT_EQ(options.command, STRANGE_CLI_COMMAND_LIST);
}

TEST(CliTest, RejectsZeroTimeout) {
  char arg0[] = "strange";
  char arg1[] = "--timeout";
  char arg2[] = "0";
  char arg3[] = "denabase";
  char *argv[] = {arg0, arg1, arg2, arg3};
  strange_cli_options options = {};
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(4, argv, &options, error, sizeof(error)), -1);
  EXPECT_NE(std::string(error).find("positive integer"),
            std::string::npos);
}

TEST(CliTest, RejectsTimeoutWithRandomMode) {
  char arg0[] = "strange";
  char arg1[] = "--timeout";
  char arg2[] = "5";
  char arg3[] = "--random";
  char arg4[] = "denabase";
  char *argv[] = {arg0, arg1, arg2, arg3, arg4};
  strange_cli_options options = {};
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(5, argv, &options, error, sizeof(error)), -1);
  EXPECT_NE(std::string(error).find("`--random`"), std::string::npos);
}

TEST(CliTest, RejectsExtraPositionalArguments) {
  char arg0[] = "strange";
  char arg1[] = "denabase";
  char arg2[] = "digital-rain";
  char *argv[] = {arg0, arg1, arg2};
  strange_cli_options options = {};
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(3, argv, &options, error, sizeof(error)), -1);
  EXPECT_NE(std::string(error).find("exactly one"), std::string::npos);
}

TEST(CliTest, ResolvesNamedBuiltInScreensaver) {
  char arg0[] = "strange";
  char arg1[] = "denabase";
  char *argv[] = {arg0, arg1};
  strange_cli_options options = {};
  const strange_screensaver_descriptor *descriptor = nullptr;
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(2, argv, &options, error, sizeof(error)), 0);
  ASSERT_EQ(strange_cli_resolve_screensaver(&options, &descriptor, error,
                                            sizeof(error)),
            0);
  ASSERT_NE(descriptor, nullptr);
  EXPECT_STREQ(descriptor->name, "denabase");
}

TEST(CliTest, StatusStandsAlone) {
  char arg0[] = "strange";
  char status[] = "--status";
  char name[] = "denabase";
  strange_cli_options options = {};
  char error[256] = {0};

  char *alone[] = {arg0, status};
  ASSERT_EQ(strange_cli_parse(2, alone, &options, error, sizeof(error)), 0);
  EXPECT_EQ(options.command, STRANGE_CLI_COMMAND_STATUS);

  char *with_name[] = {arg0, status, name};
  EXPECT_EQ(strange_cli_parse(3, with_name, &options, error, sizeof(error)),
            -1);
}

TEST(CliTest, NowSelectsPreviewAndRejectsSessionOptions) {
  char arg0[] = "strange";
  char cover[] = "--cover-fullscreen";
  char now[] = "--now";
  char random[] = "--random";
  char timeout[] = "--timeout";
  char seconds[] = "9";
  char name[] = "denabase";
  strange_cli_options options = {};
  char error[256] = {0};

  char *plain[] = {arg0, name};
  ASSERT_EQ(strange_cli_parse(2, plain, &options, error, sizeof(error)), 0);
  EXPECT_FALSE(options.preview);

  char *named[] = {arg0, now, name};
  ASSERT_EQ(strange_cli_parse(3, named, &options, error, sizeof(error)), 0);
  EXPECT_TRUE(options.preview);
  EXPECT_STREQ(options.screensaver_name, "denabase");

  char *with_random[] = {arg0, now, random, name};
  ASSERT_EQ(strange_cli_parse(4, with_random, &options, error, sizeof(error)),
            0);
  EXPECT_TRUE(options.preview);
  EXPECT_EQ(options.command, STRANGE_CLI_COMMAND_RUN_RANDOM);

  char *with_timeout[] = {arg0, now, timeout, seconds, name};
  EXPECT_EQ(strange_cli_parse(5, with_timeout, &options, error, sizeof(error)),
            -1);

  char *with_cover[] = {arg0, cover, now, name};
  EXPECT_EQ(strange_cli_parse(4, with_cover, &options, error, sizeof(error)),
            -1);

  char *misplaced[] = {arg0, name, now};
  EXPECT_EQ(strange_cli_parse(3, misplaced, &options, error, sizeof(error)),
            -1);
}

TEST(CliTest, CoverFullscreenIsOffUnlessGivenFirst) {
  char arg0[] = "strange";
  char flag[] = "--cover-fullscreen";
  char random[] = "--random";
  char name[] = "denabase";
  strange_cli_options options = {};
  char error[256] = {0};

  char *plain[] = {arg0, name};
  ASSERT_EQ(strange_cli_parse(2, plain, &options, error, sizeof(error)), 0);
  EXPECT_FALSE(options.cover_fullscreen);

  char *named[] = {arg0, flag, name};
  ASSERT_EQ(strange_cli_parse(3, named, &options, error, sizeof(error)), 0);
  EXPECT_TRUE(options.cover_fullscreen);
  EXPECT_STREQ(options.screensaver_name, "denabase");

  char *with_random[] = {arg0, flag, random, name};
  ASSERT_EQ(strange_cli_parse(4, with_random, &options, error, sizeof(error)),
            0);
  EXPECT_TRUE(options.cover_fullscreen);
  EXPECT_EQ(options.command, STRANGE_CLI_COMMAND_RUN_RANDOM);

  char *misplaced[] = {arg0, name, flag};
  EXPECT_EQ(strange_cli_parse(3, misplaced, &options, error, sizeof(error)),
            -1);
}

TEST(CliTest, UserScreensaverOverridesBuiltinOfTheSameName) {
  ScopedHomeOverride home;
  char arg0[] = "strange";
  char arg1[] = "denabase";
  char *argv[] = {arg0, arg1};
  strange_cli_options options = {};
  const strange_screensaver_descriptor *descriptor = nullptr;
  char error[256] = {0};

  home.WriteFile(home.screensaver_dir() / "denabase.lua",
                 "return { character_width = 2 }");

  ASSERT_EQ(strange_cli_parse(2, argv, &options, error, sizeof(error)), 0);
  ASSERT_EQ(strange_cli_resolve_screensaver(&options, &descriptor, error,
                                            sizeof(error)),
            0)
      << error;
  ASSERT_NE(descriptor, nullptr);
  EXPECT_STREQ(descriptor->name, "denabase");
  EXPECT_EQ(descriptor->character_width, 2);
}

TEST(CliTest, ReportsUserScreensaverThatFailsToLoad) {
  ScopedHomeOverride home;
  char arg0[] = "strange";
  char arg1[] = "broken";
  char *argv[] = {arg0, arg1};
  strange_cli_options options = {};
  const strange_screensaver_descriptor *descriptor = nullptr;
  char error[256] = {0};

  home.WriteFile(home.screensaver_dir() / "broken.lua", "return 7");

  ASSERT_EQ(strange_cli_parse(2, argv, &options, error, sizeof(error)), 0);
  ASSERT_EQ(strange_cli_resolve_screensaver(&options, &descriptor, error,
                                            sizeof(error)),
            -1);
  EXPECT_NE(std::string(error).find("must return a table"), std::string::npos);
  EXPECT_EQ(descriptor, nullptr);
}

TEST(CliTest, RandomResolutionRejectsUnknownScreensaverBeforeSelection) {
  char arg0[] = "strange";
  char arg1[] = "--random";
  char arg2[] = "denabase";
  char arg3[] = "missing";
  char *argv[] = {arg0, arg1, arg2, arg3};
  strange_cli_options options = {};
  const strange_screensaver_descriptor *descriptor = nullptr;
  char error[256] = {0};

  ASSERT_EQ(strange_cli_parse(4, argv, &options, error, sizeof(error)), 0);
  ASSERT_EQ(strange_cli_resolve_screensaver(&options, &descriptor, error,
                                            sizeof(error)),
            -1);
  EXPECT_NE(std::string(error).find("missing"), std::string::npos);
  EXPECT_EQ(descriptor, nullptr);
}

TEST(CliTest, RandomResolutionChoosesProvidedBuiltIn) {
  char arg0[] = "strange";
  char arg1[] = "--random";
  char arg2[] = "denabase";
  char arg3[] = "digital-rain";
  char *argv[] = {arg0, arg1, arg2, arg3};
  strange_cli_options options = {};
  const strange_screensaver_descriptor *descriptor = nullptr;
  char error[256] = {0};

  std::srand(1);
  ASSERT_EQ(strange_cli_parse(4, argv, &options, error, sizeof(error)), 0);
  ASSERT_EQ(strange_cli_resolve_screensaver(&options, &descriptor, error,
                                            sizeof(error)),
            0);
  ASSERT_NE(descriptor, nullptr);
  EXPECT_TRUE(std::strcmp(descriptor->name, "denabase") == 0 ||
              std::strcmp(descriptor->name, "digital-rain") == 0);
}

TEST(CliTest, PrintListSeparatesBuiltInAndUserScreensavers) {
  ScopedHomeOverride home;
  std::string output;
  std::string builtin_section;
  std::string user_section;

  home.WriteFile(home.screensaver_dir() / "custom.lua", "return {}");
  home.WriteFile(home.screensaver_dir() / "denabase.lua", "return {}");

  output = CaptureListOutput();
  builtin_section = SectionBody(output, "Built-in screensavers:\n",
                                "User screensavers:\n");
  user_section = SectionBody(output, "User screensavers:\n");

  EXPECT_NE(output.find("Built-in screensavers:\n"), std::string::npos);
  EXPECT_NE(output.find("User screensavers:\n"), std::string::npos);
  EXPECT_NE(builtin_section.find("  denabase\n"), std::string::npos);
  EXPECT_NE(builtin_section.find("  digital-rain\n"), std::string::npos);
  EXPECT_EQ(builtin_section.find("  custom\n"), std::string::npos);
  EXPECT_NE(user_section.find("  custom\n"), std::string::npos);
  EXPECT_NE(user_section.find("  denabase\n"), std::string::npos);
  EXPECT_EQ(user_section.find("  digital-rain\n"), std::string::npos);
}

TEST(CliTest, PrintListDoesNotValidateUserArtifacts) {
  ScopedHomeOverride home;
  std::string output;

  home.WriteFile(home.screensaver_dir() / "broken.lua",
                 "this is not a runnable screensaver");

  output = CaptureListOutput();

  EXPECT_NE(output.find("User screensavers:\n  broken\n"), std::string::npos);
}

}  // namespace
