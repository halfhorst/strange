#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include "src/screensaver_registry.h"
}

namespace {

int g_init_calls = 0;
int g_update_calls = 0;
int g_cleanup_calls = 0;
int g_init_width = 0;
unsigned long g_last_frame_count = 0;

class ScopedHomeOverride {
 public:
  ScopedHomeOverride() {
    const char *existing_home = std::getenv("HOME");
    if (existing_home != nullptr) {
      had_home_ = true;
      original_home_ = existing_home;
    }

    std::string home_template =
        (std::filesystem::current_path() / ".tmp-home-XXXXXX").string();
    std::vector<char> buffer(home_template.begin(), home_template.end());
    buffer.push_back('\0');

    char *created = mkdtemp(buffer.data());
    EXPECT_NE(created, nullptr);
    if (created != nullptr) {
      home_path_ = created;
      EXPECT_EQ(setenv("HOME", created, 1), 0);
    }
  }

  ~ScopedHomeOverride() {
    if (!home_path_.empty()) {
      std::filesystem::remove_all(home_path_);
    }

    if (had_home_) {
      setenv("HOME", original_home_.c_str(), 1);
    } else {
      unsetenv("HOME");
    }
  }

  std::filesystem::path home_path() const { return home_path_; }

  std::filesystem::path screensaver_dir() const { return home_path_ / ".strange"; }

  void CreateScreensaverDir() const {
    std::filesystem::create_directories(screensaver_dir());
  }

  void WriteFile(const std::filesystem::path &path,
                 const std::string &contents = "") const {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    stream << contents;
  }

 private:
  bool had_home_ = false;
  std::string original_home_;
  std::filesystem::path home_path_;
};

void ResetLifecycleCounters() {
  g_init_calls = 0;
  g_update_calls = 0;
  g_cleanup_calls = 0;
  g_init_width = 0;
  g_last_frame_count = 0;
}

int TestInit(void **state, ScreenBuffer *buffer) {
  ++g_init_calls;
  g_init_width = buffer != nullptr ? buffer->w : 0;
  *state = std::malloc(1);
  return *state == nullptr ? -1 : 0;
}

int TestUpdate(void *state, ScreenBuffer *buffer,
               const strange_screensaver_frame *frame) {
  if (state == nullptr || buffer == nullptr) {
    return -1;
  }

  ++g_update_calls;
  g_last_frame_count = frame != nullptr ? frame->frame_count : 0;
  write_string_to_buffer(buffer, "X", 0, 0);
  return 0;
}

void TestCleanup(void *state) {
  ++g_cleanup_calls;
  std::free(state);
}

TEST(ScreensaverRegistryTest, DescriptorValidationRequiresOnlyName) {
  strange_screensaver_descriptor descriptor = {};

  errno = 0;
  EXPECT_EQ(strange_screensaver_descriptor_validate(&descriptor), -1);
  EXPECT_EQ(errno, EINVAL);

  descriptor.name = "demo";
  errno = 0;
  EXPECT_EQ(strange_screensaver_descriptor_validate(&descriptor), 0);
  EXPECT_EQ(errno, 0);
}

TEST(ScreensaverRegistryTest, CharacterWidthDefaultsToOne) {
  strange_screensaver_descriptor descriptor = {
      .name = "demo",
  };

  EXPECT_EQ(strange_screensaver_character_width(&descriptor), 1);
  descriptor.character_width = 3;
  EXPECT_EQ(strange_screensaver_character_width(&descriptor), 3);
}

TEST(ScreensaverRegistryTest, InstanceLifecycleUsesDescriptorCallbacks) {
  strange_screensaver_descriptor descriptor = {
      .name = "demo",
      .init = TestInit,
      .update = TestUpdate,
      .cleanup = TestCleanup,
  };
  strange_screensaver_instance instance = {};
  strange_screensaver_frame frame = {};
  ScreenBuffer buffer = {};

  ResetLifecycleCounters();
  ASSERT_EQ(strange_screen_buffer_init(&buffer, 8, 4, 1), 0);
  ASSERT_EQ(
      strange_screensaver_instance_init(&instance, &descriptor, &buffer), 0);
  EXPECT_EQ(g_init_calls, 1);
  EXPECT_EQ(g_init_width, 8);

  frame.frame_count = 7;
  ASSERT_EQ(
      strange_screensaver_instance_update(&instance, &buffer, &frame), 0);
  EXPECT_EQ(g_update_calls, 1);
  EXPECT_EQ(g_last_frame_count, 7U);
  EXPECT_EQ(buffer.buffer[0], 'X');

  strange_screensaver_instance_cleanup(&instance);
  EXPECT_EQ(g_cleanup_calls, 1);
  EXPECT_EQ(instance.descriptor, nullptr);
  EXPECT_EQ(instance.state, nullptr);
  strange_screen_buffer_free(&buffer);
}

TEST(ScreensaverRegistryTest, BuiltinRegistryContainsCurrentBuiltInDemos) {
  size_t count = 0;
  const strange_screensaver_descriptor *const *descriptors =
      strange_builtin_screensavers(&count);
  bool saw_denabase = false;
  bool saw_digital_rain = false;

  ASSERT_NE(descriptors, nullptr);
  EXPECT_EQ(count, 2U);

  for (size_t index = 0; index < count; ++index) {
    ASSERT_NE(descriptors[index], nullptr);
    EXPECT_EQ(strange_screensaver_descriptor_validate(descriptors[index]), 0);
    if (std::strcmp(descriptors[index]->name, "denabase") == 0) {
      saw_denabase = true;
    }
    if (std::strcmp(descriptors[index]->name, "digital-rain") == 0) {
      saw_digital_rain = true;
    }
  }

  EXPECT_TRUE(saw_denabase);
  EXPECT_TRUE(saw_digital_rain);
}

TEST(ScreensaverRegistryTest, BuiltinRegistryFindsDenabaseDescriptor) {
  const strange_screensaver_descriptor *descriptor =
      strange_builtin_screensaver_find("denabase");

  ASSERT_NE(descriptor, nullptr);
  EXPECT_EQ(strange_screensaver_descriptor_validate(descriptor), 0);
  EXPECT_STREQ(descriptor->name, "denabase");
  EXPECT_EQ(strange_screensaver_character_width(descriptor), 1);
}

TEST(ScreensaverRegistryTest, BuiltinRegistryFindsDigitalRainDescriptor) {
  const strange_screensaver_descriptor *descriptor =
      strange_builtin_screensaver_find("digital-rain");

  ASSERT_NE(descriptor, nullptr);
  EXPECT_EQ(strange_screensaver_descriptor_validate(descriptor), 0);
  EXPECT_STREQ(descriptor->name, "digital-rain");
  EXPECT_EQ(strange_screensaver_character_width(descriptor), 3);
}

TEST(ScreensaverRegistryTest, CatalogDiscoversOnlyDirectUserArtifacts) {
  ScopedHomeOverride home;
  strange_screensaver_catalog catalog = {};
  char error[256] = {0};
  size_t count = 0;
  const strange_screensaver_record *records = nullptr;
  const strange_screensaver_record *record = nullptr;

  home.CreateScreensaverDir();
  home.WriteFile(home.screensaver_dir() / "denabase.lua", "return {}");
  home.WriteFile(home.screensaver_dir() / "notes.txt", "ignore");
  home.WriteFile(home.screensaver_dir() / "nested" / "digital-rain.lua",
                 "return {}");

  ASSERT_EQ(
      strange_screensaver_catalog_init(&catalog, error, sizeof(error)), 0);
  records = strange_screensaver_catalog_records(&catalog, &count);
  ASSERT_NE(records, nullptr);
  EXPECT_EQ(count, 3U);

  record = strange_screensaver_catalog_find(&catalog, "denabase");
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->source, STRANGE_SCREENSAVER_RECORD_SOURCE_LUA);
  EXPECT_NE(std::string(record->path).find("denabase.lua"), std::string::npos);

  record = strange_screensaver_catalog_find(&catalog, "digital-rain");
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->source, STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN);

  strange_screensaver_catalog_free(&catalog);
}

TEST(ScreensaverRegistryTest, CatalogRejectsLuaAndNativeUserNameCollision) {
  ScopedHomeOverride home;
  strange_screensaver_catalog catalog = {};
  char error[256] = {0};

  home.CreateScreensaverDir();
  home.WriteFile(home.screensaver_dir() / "custom.lua", "return {}");
#ifdef __APPLE__
  home.WriteFile(home.screensaver_dir() / "custom.dylib");
#else
  home.WriteFile(home.screensaver_dir() / "custom.so");
#endif

  ASSERT_EQ(
      strange_screensaver_catalog_init(&catalog, error, sizeof(error)), -1);
  EXPECT_NE(std::string(error).find("custom"), std::string::npos);
  strange_screensaver_catalog_free(&catalog);
}

TEST(ScreensaverRegistryTest, DenabaseBuiltinRendersThroughSharedContract) {
  const strange_screensaver_descriptor *descriptor =
      strange_builtin_screensaver_find("denabase");
  strange_screensaver_instance instance = {};
  strange_screensaver_frame frame = {};
  ScreenBuffer buffer = {};
  int helix_pixels = 0;

  std::srand(1);
  ASSERT_NE(descriptor, nullptr);
  ASSERT_EQ(strange_screen_buffer_init(
                &buffer, 60, 20, strange_screensaver_character_width(descriptor)),
            0);
  ASSERT_EQ(
      strange_screensaver_instance_init(&instance, descriptor, &buffer), 0);

  frame.frame_count = 4;
  ASSERT_EQ(
      strange_screensaver_instance_update(&instance, &buffer, &frame), 0);

  std::string rendered(buffer.buffer,
                       static_cast<size_t>(buffer.w * buffer.h));
  EXPECT_NE(rendered.find("IDENT #09817 (H. sapiens)"), std::string::npos);
  EXPECT_NE(rendered.find("<"), std::string::npos);
  EXPECT_NE(rendered.find(">"), std::string::npos);

  for (int index = 0; index < buffer.w * buffer.h; ++index) {
    if (buffer.buffer[index] == '0') {
      ++helix_pixels;
    }
  }
  EXPECT_GT(helix_pixels, 0);

  strange_screensaver_instance_cleanup(&instance);
  strange_screen_buffer_free(&buffer);
}

TEST(ScreensaverRegistryTest, DenabaseHelixPairsTheBaseMarkedInTheTable) {
  const strange_screensaver_descriptor *descriptor =
      strange_builtin_screensaver_find("denabase");
  strange_screensaver_instance instance = {};
  ScreenBuffer buffer = {};
  const int focus_row = 10;
  const int table_right = 30;
  int pairs_checked = 0;

  std::srand(1);
  ASSERT_NE(descriptor, nullptr);
  ASSERT_EQ(strange_screen_buffer_init(&buffer, 60, 20, 1), 0);
  ASSERT_EQ(
      strange_screensaver_instance_init(&instance, descriptor, &buffer), 0);

  // Every fourth frame scrolls one row and every second row holds a base
  // pair, so this walks the pair on the focus row through three table rows.
  for (unsigned long step = 1; step <= 80; ++step) {
    strange_screensaver_frame frame = {};
    frame.frame_count = step * 8;
    strange_screen_buffer_clear(&buffer);
    ASSERT_EQ(
        strange_screensaver_instance_update(&instance, &buffer, &frame), 0);
    frame.frame_count = step * 8 + 4;
    strange_screen_buffer_clear(&buffer);
    ASSERT_EQ(
        strange_screensaver_instance_update(&instance, &buffer, &frame), 0);

    const std::string marker_row(buffer.buffer + (focus_row - 1) * buffer.w,
                                 table_right);
    const std::string row(buffer.buffer + focus_row * buffer.w, buffer.w);
    const size_t helix_pointer = row.find('>', table_right + 1);
    ASSERT_NE(helix_pointer, std::string::npos);
    EXPECT_GT(row.rfind('<'), helix_pointer);
    const size_t marked_column = marker_row.find('v');
    ASSERT_NE(marked_column, std::string::npos);

    const char base = row[marked_column];
    const size_t left = row.find_first_of("ACGT", table_right + 1);
    const size_t right = row.find_last_of("ACGT");
    if (left == std::string::npos) {
      // The strands cross here and leave no room for the pair.
      continue;
    }
    ASSERT_NE(left, right);
    ++pairs_checked;

    const std::string pair = {row[left], row[right]};
    const std::string expected = base == 'A'   ? "AT"
                                 : base == 'T' ? "TA"
                                 : base == 'G' ? "GC"
                                               : "CG";
    const std::string reversed(expected.rbegin(), expected.rend());
    EXPECT_TRUE(pair == expected || pair == reversed)
        << "step " << step << ": table marks " << base << " but the helix shows "
        << pair;
  }
  EXPECT_GT(pairs_checked, 50);

  strange_screensaver_instance_cleanup(&instance);
  strange_screen_buffer_free(&buffer);
}

TEST(ScreensaverRegistryTest, DigitalRainBuiltinRendersThroughSharedContract) {
  const strange_screensaver_descriptor *descriptor =
      strange_builtin_screensaver_find("digital-rain");
  strange_screensaver_instance instance = {};
  strange_screensaver_frame frame = {};
  ScreenBuffer buffer = {};
  bool rendered_anything = false;

  std::srand(1);
  ASSERT_NE(descriptor, nullptr);
  ASSERT_EQ(strange_screen_buffer_init(
                &buffer, 32, 12, strange_screensaver_character_width(descriptor)),
            0);
  ASSERT_EQ(
      strange_screensaver_instance_init(&instance, descriptor, &buffer), 0);

  for (unsigned long frame_count = 0; frame_count < 2000; ++frame_count) {
    strange_screen_buffer_clear(&buffer);
    frame.frame_count = frame_count;
    ASSERT_EQ(
        strange_screensaver_instance_update(&instance, &buffer, &frame), 0);

    for (int index = 0; index < buffer.w * buffer.h * buffer.character_width;
         index += buffer.character_width) {
      if (buffer.buffer[index] != ' ') {
        rendered_anything = true;
        break;
      }
    }

    if (rendered_anything) {
      break;
    }
  }

  EXPECT_TRUE(rendered_anything);
  strange_screensaver_instance_cleanup(&instance);
  strange_screen_buffer_free(&buffer);
}

}  // namespace
