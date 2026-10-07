#include <string>

#include <gtest/gtest.h>

#include "tests/scoped_home.h"

#include "src/lua_screensaver.h"
#include "src/screensaver_loader.h"

namespace {

std::string Row(const strange_screen_buffer &buffer, int row) {
  return std::string(buffer.buffer + (row * buffer.w * buffer.character_width),
                     buffer.w * buffer.character_width);
}

class LoadedScreensaver {
 public:
  LoadedScreensaver(strange_screensaver_record_source source,
                    const std::string &path) {
    strange_screensaver_record record = {};
    record.name = "fixture";
    record.source = source;
    record.path = path.c_str();
    loaded_ = strange_screensaver_load(&record, &descriptor, error,
                                       sizeof(error)) == 0;
  }

  ~LoadedScreensaver() { strange_lua_screensaver_unload(); }

  bool loaded() const { return loaded_; }

  const strange_screensaver_descriptor *descriptor = nullptr;
  char error[256] = {0};

 private:
  bool loaded_ = false;
};

class ScreensaverLoaderTest : public ::testing::Test {
 protected:
  std::string WriteScript(const std::string &contents) {
    const std::filesystem::path path = home_.screensaver_dir() / "fixture.lua";
    home_.WriteFile(path, contents);
    return path.string();
  }

  ScopedHomeOverride home_;
};

TEST_F(ScreensaverLoaderTest, LuaScriptDrawsThroughTheBufferApi) {
  LoadedScreensaver saver(
      STRANGE_SCREENSAVER_RECORD_SOURCE_LUA,
      WriteScript("return {\n"
                  "  name = 'drawn',\n"
                  "  character_width = 3,\n"
                  "  init = function(buffer) return { w = buffer.w } end,\n"
                  "  update = function(state, buffer, frame)\n"
                  "    buffer:write('\\239\\189\\177', 1, 0)\n"
                  "    buffer:write_string(state.w .. 'x' .. buffer.h, 0, 1)\n"
                  "    buffer:write(tostring(frame.frame_count), 3, 1)\n"
                  "    buffer:write('!', 99, 99)\n"
                  "  end,\n"
                  "}\n"));
  ASSERT_TRUE(saver.loaded()) << saver.error;
  EXPECT_STREQ(saver.descriptor->name, "drawn");
  EXPECT_EQ(saver.descriptor->character_width, 3);

  strange_screen_buffer buffer = {};
  strange_screensaver_instance instance = {};
  timespec now = {5, 0};
  strange_screensaver_frame frame = {&now, 7};

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 4, 2, 3), 0);
  ASSERT_EQ(strange_screensaver_instance_init(&instance, saver.descriptor,
                                              &buffer),
            0);
  ASSERT_EQ(strange_screensaver_instance_update(&instance, &buffer, &frame), 0)
      << strange_screensaver_error();

  EXPECT_EQ(Row(buffer, 0), std::string(" \0\0\xEF\xBD\xB1 \0\0 \0\0", 12));
  EXPECT_EQ(Row(buffer, 1), std::string("4\0\0x\0\0" "2\0\0" "7\0\0", 12));

  strange_screensaver_instance_cleanup(&instance);
  strange_screen_buffer_free(&buffer);
}

TEST_F(ScreensaverLoaderTest, LuaScriptDefaultsItsNameAndCallbacks) {
  LoadedScreensaver saver(STRANGE_SCREENSAVER_RECORD_SOURCE_LUA,
                          WriteScript("return {}"));
  ASSERT_TRUE(saver.loaded()) << saver.error;
  EXPECT_STREQ(saver.descriptor->name, "fixture");
  EXPECT_EQ(saver.descriptor->character_width, 1);

  strange_screen_buffer buffer = {};
  strange_screensaver_instance instance = {};

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 2, 1, 1), 0);
  ASSERT_EQ(strange_screensaver_instance_init(&instance, saver.descriptor,
                                              &buffer),
            0);
  EXPECT_EQ(strange_screensaver_instance_update(&instance, &buffer, nullptr), 0);

  strange_screensaver_instance_cleanup(&instance);
  strange_screen_buffer_free(&buffer);
}

TEST_F(ScreensaverLoaderTest, LuaRuntimeErrorFailsTheUpdateWithItsMessage) {
  LoadedScreensaver saver(
      STRANGE_SCREENSAVER_RECORD_SOURCE_LUA,
      WriteScript("local kept\n"
                  "return {\n"
                  "  init = function(buffer) kept = buffer end,\n"
                  "  update = function() kept:clear() end,\n"
                  "}\n"));
  ASSERT_TRUE(saver.loaded()) << saver.error;

  strange_screen_buffer buffer = {};
  strange_screensaver_instance instance = {};

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 2, 1, 1), 0);
  ASSERT_EQ(strange_screensaver_instance_init(&instance, saver.descriptor,
                                              &buffer),
            0);
  EXPECT_EQ(strange_screensaver_instance_update(&instance, &buffer, nullptr),
            -1);
  EXPECT_NE(std::string(strange_screensaver_error())
                .find("buffer used outside the call"),
            std::string::npos);

  strange_screensaver_instance_cleanup(&instance);
  strange_screen_buffer_free(&buffer);
}

TEST_F(ScreensaverLoaderTest, LuaLoadErrorsNameTheProblem) {
  LoadedScreensaver syntax(STRANGE_SCREENSAVER_RECORD_SOURCE_LUA,
                           WriteScript("return {"));
  EXPECT_FALSE(syntax.loaded());
  EXPECT_NE(std::string(syntax.error).find("fixture.lua"), std::string::npos);

  LoadedScreensaver width(STRANGE_SCREENSAVER_RECORD_SOURCE_LUA,
                          WriteScript("return { character_width = 0 }"));
  EXPECT_FALSE(width.loaded());
  EXPECT_NE(std::string(width.error).find("character_width"),
            std::string::npos);
}

TEST_F(ScreensaverLoaderTest, SharedLibraryExportsARunnableDescriptor) {
  LoadedScreensaver saver(STRANGE_SCREENSAVER_RECORD_SOURCE_DYNAMIC,
                          "examples/bounce" STRANGE_PLUGIN_EXTENSION);
  ASSERT_TRUE(saver.loaded()) << saver.error;
  EXPECT_STREQ(saver.descriptor->name, "bounce");

  strange_screen_buffer buffer = {};
  strange_screensaver_instance instance = {};
  timespec now = {0, 0};
  strange_screensaver_frame frame = {&now, 1};

  ASSERT_EQ(strange_screen_buffer_init(&buffer, 8, 4, 1), 0);
  ASSERT_EQ(strange_screensaver_instance_init(&instance, saver.descriptor,
                                              &buffer),
            0);
  ASSERT_EQ(strange_screensaver_instance_update(&instance, &buffer, &frame), 0);
  EXPECT_EQ(Row(buffer, 2), "    O   ");

  strange_screensaver_instance_cleanup(&instance);
  strange_screen_buffer_free(&buffer);
}

TEST_F(ScreensaverLoaderTest, SharedLibraryBuiltForAnotherApiVersionIsRefused) {
  LoadedScreensaver stale(STRANGE_SCREENSAVER_RECORD_SOURCE_DYNAMIC,
                          "tests/fixtures/stale_api" STRANGE_PLUGIN_EXTENSION);
  EXPECT_FALSE(stale.loaded());
  EXPECT_NE(std::string(stale.error).find("API version"), std::string::npos);
  EXPECT_EQ(stale.descriptor, nullptr);
}

TEST_F(ScreensaverLoaderTest, SharedLibraryLoadErrorsNameTheFile) {
  LoadedScreensaver missing(STRANGE_SCREENSAVER_RECORD_SOURCE_DYNAMIC,
                            "examples/absent" STRANGE_PLUGIN_EXTENSION);
  EXPECT_FALSE(missing.loaded());
  EXPECT_NE(std::string(missing.error).find("examples/absent"),
            std::string::npos);
}

}  // namespace
