#include "control/control_command.h"

#include <filesystem>
#include <string>
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

TEST(ControlReplyTest, OkAndErrRoundTrip) {
  EXPECT_EQ(Encode(ControlReply{true, "Dump started: /var/lib/volta/a.csv"}),
            "ok\nDump started: /var/lib/volta/a.csv");
  auto ok = DecodeReply("ok\nDump started: /var/lib/volta/a.csv");
  ASSERT_TRUE(ok.has_value());
  EXPECT_TRUE(ok->ok);
  EXPECT_EQ(ok->message, "Dump started: /var/lib/volta/a.csv");

  EXPECT_EQ(Encode(ControlReply{false, "Dump already running: /tmp/a.csv"}),
            "err\nDump already running: /tmp/a.csv");
  auto err = DecodeReply("err\nDump already running: /tmp/a.csv");
  ASSERT_TRUE(err.has_value());
  EXPECT_FALSE(err->ok);
  EXPECT_EQ(err->message, "Dump already running: /tmp/a.csv");
}

TEST(ControlReplyTest, EmptyMessage) {
  EXPECT_EQ(Encode(ControlReply{true, ""}), "ok\n");
  auto ok = DecodeReply("ok\n");
  ASSERT_TRUE(ok.has_value());
  EXPECT_TRUE(ok->ok);
  EXPECT_TRUE(ok->message.empty());

  EXPECT_EQ(Encode(ControlReply{false, ""}), "err\n");
  auto err = DecodeReply("err\n");
  ASSERT_TRUE(err.has_value());
  EXPECT_FALSE(err->ok);
  EXPECT_TRUE(err->message.empty());
}

TEST(ControlReplyTest, EmbeddedNewlines) {
  const std::string message = "first\nsecond\nthird";
  EXPECT_EQ(Encode(ControlReply{true, message}), "ok\n" + message);
  auto ok = DecodeReply("ok\nfirst\nsecond\nthird");
  ASSERT_TRUE(ok.has_value());
  EXPECT_TRUE(ok->ok);
  EXPECT_EQ(ok->message, message);

  EXPECT_EQ(Encode(ControlReply{false, message}), "err\n" + message);
  auto err = DecodeReply("err\nfirst\nsecond\nthird");
  ASSERT_TRUE(err.has_value());
  EXPECT_FALSE(err->ok);
  EXPECT_EQ(err->message, message);
}

TEST(ControlReplyTest, Malformed) {
  EXPECT_FALSE(DecodeReply("").has_value());
  EXPECT_FALSE(DecodeReply("ok").has_value());
  EXPECT_FALSE(DecodeReply("err").has_value());
  EXPECT_FALSE(DecodeReply("okmessage").has_value());
  EXPECT_FALSE(DecodeReply("\nmessage").has_value());
  EXPECT_FALSE(DecodeReply("okay\nmessage").has_value());
  EXPECT_FALSE(DecodeReply("error\nmessage").has_value());
  EXPECT_FALSE(DecodeReply("OK\nmessage").has_value());
  EXPECT_FALSE(DecodeReply("ERR\nmessage").has_value());
  EXPECT_FALSE(DecodeReply("nope\nmessage").has_value());
}

}  // namespace
}  // namespace control
}  // namespace agent
}  // namespace volta
