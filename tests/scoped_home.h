#ifndef STRANGE_TESTS_SCOPED_HOME_H_
#define STRANGE_TESTS_SCOPED_HOME_H_

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <gtest/gtest.h>

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

  std::filesystem::path screensaver_dir() const { return home_path_ / ".strange"; }

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

#endif
