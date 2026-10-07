#include <gtest/gtest.h>

#include "src/keys.h"

namespace {

TEST(KeysTest, ParsesTheAcceptedSpellings) {
  int key = -1;

  ASSERT_EQ(strange_parse_control_key("ctrl-q", &key), 0);
  EXPECT_EQ(key, 0x11);
  ASSERT_EQ(strange_parse_control_key("Ctrl+G", &key), 0);
  EXPECT_EQ(key, 0x07);
  ASSERT_EQ(strange_parse_control_key("^A", &key), 0);
  EXPECT_EQ(key, 0x01);
  ASSERT_EQ(strange_parse_control_key("none", &key), 0);
  EXPECT_EQ(key, STRANGE_NO_KEY);
}

TEST(KeysTest, RefusesKeysAShellNeedsAndAnythingElse) {
  int key = -1;

  EXPECT_EQ(strange_parse_control_key("ctrl-m", &key), -1);
  EXPECT_EQ(strange_parse_control_key("ctrl-i", &key), -1);
  EXPECT_EQ(strange_parse_control_key("ctrl-j", &key), -1);
  EXPECT_EQ(strange_parse_control_key("q", &key), -1);
  EXPECT_EQ(strange_parse_control_key("ctrl-qq", &key), -1);
  EXPECT_EQ(strange_parse_control_key("ctrl-1", &key), -1);
  EXPECT_EQ(strange_parse_control_key("", &key), -1);
}

TEST(KeysTest, LabelsKeysForDisplay) {
  char label[16];

  strange_control_key_label(0x11, label, sizeof(label));
  EXPECT_STREQ(label, "Ctrl-Q");
  strange_control_key_label(STRANGE_NO_KEY, label, sizeof(label));
  EXPECT_STREQ(label, "none");
}

}  // namespace
