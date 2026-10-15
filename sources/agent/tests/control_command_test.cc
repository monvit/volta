#include "control/control_command.h"

#include <filesystem>
#include <variant>

#include "gtest/gtest.h"

namespace volta {
namespace agent {
namespace control {
namespace {

TEST(ControlCommandTest, RoundTrip) {
  auto start =
      Decode(Encode(DumpStart{std::filesystem::path{"/var/lib/volta/dumps"}}));
  ASSERT_TRUE(start.has_value());
  auto* dump = std::get_if<DumpStart>(&*start);
  ASSERT_NE(dump, nullptr);
  EXPECT_EQ(dump->path->string(), "/var/lib/volta/dumps");

  auto bare = Decode(Encode(DumpStart{}));
  ASSERT_TRUE(bare.has_value());
  EXPECT_FALSE(std::get<DumpStart>(*bare).path.has_value());

  auto end = Decode(Encode(DumpEnd{}));
  ASSERT_TRUE(end.has_value());
  EXPECT_TRUE(std::holds_alternative<DumpEnd>(*end));
}

TEST(ControlCommandTest, WireQuirks) {
  auto empty = Decode("dump_start;");
  ASSERT_TRUE(empty.has_value());
  EXPECT_FALSE(std::get<DumpStart>(*empty).path.has_value());

  auto semi = Decode(Encode(DumpStart{std::filesystem::path{"/tmp/a;b"}}));
  ASSERT_TRUE(semi.has_value());
  EXPECT_EQ(std::get<DumpStart>(*semi).path->string(), "/tmp/a;b");

  EXPECT_FALSE(Decode("dump_startfoo").has_value());
  EXPECT_FALSE(Decode("dump_end;extra").has_value());
}

}  // namespace
}  // namespace control
}  // namespace agent
}  // namespace volta
