#include <cstddef>

#include "buffer.h"
#include "gtest/gtest.h"

namespace volta {
namespace agent {
namespace {

Sample MakeSample(int64_t timestamp_ns, double value) {
  return Sample{.timestamp_ns = timestamp_ns, .value = value};
}

TEST(SeriesBufferTest, FreshBufferIsEmptyAndHasNoLatest) {
  SeriesBuffer buffer(4);

  EXPECT_TRUE(buffer.Empty());
  EXPECT_EQ(buffer.Size(), 0u);
  EXPECT_EQ(buffer.Capacity(), 4u);
  EXPECT_FALSE(buffer.Latest().has_value());
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 0u);
}

TEST(SeriesBufferTest, PushToZeroCapacityIsNoOp) {
  SeriesBuffer buffer(0);

  buffer.Push(MakeSample(1, 1.0));

  EXPECT_TRUE(buffer.Empty());
  EXPECT_EQ(buffer.Size(), 0u);
  EXPECT_FALSE(buffer.Latest().has_value());
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 0u);
}

TEST(SeriesBufferTest, PushUpToCapacityRetainsSamplesInOrder) {
  SeriesBuffer buffer(3);
  buffer.Push(MakeSample(10, 1.0));
  buffer.Push(MakeSample(20, 2.0));
  buffer.Push(MakeSample(30, 3.0));

  EXPECT_FALSE(buffer.Empty());
  EXPECT_EQ(buffer.Size(), 3u);
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 3u);

  const auto snapshot = buffer.GetSnapshot();
  ASSERT_EQ(snapshot.samples.size(), 3u);
  EXPECT_EQ(snapshot.samples[0].timestamp_ns, 10);
  EXPECT_EQ(snapshot.samples[0].value, 1.0);
  EXPECT_EQ(snapshot.samples[1].timestamp_ns, 20);
  EXPECT_EQ(snapshot.samples[1].value, 2.0);
  EXPECT_EQ(snapshot.samples[2].timestamp_ns, 30);
  EXPECT_EQ(snapshot.samples[2].value, 3.0);
  EXPECT_EQ(snapshot.end, buffer.GetHead());
}

TEST(SeriesBufferTest, PushBeyondCapacityEvictsOldestAndAdvancesTail) {
  SeriesBuffer buffer(2);
  buffer.Push(MakeSample(10, 1.0));
  buffer.Push(MakeSample(20, 2.0));
  const size_t tail_before_overflow = buffer.GetTail();

  buffer.Push(MakeSample(30, 3.0));

  EXPECT_EQ(buffer.Size(), 2u);
  EXPECT_EQ(buffer.GetTail(), tail_before_overflow + 1);
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 2u);

  const auto snapshot = buffer.GetSnapshot();
  ASSERT_EQ(snapshot.samples.size(), 2u);
  EXPECT_EQ(snapshot.samples[0].timestamp_ns, 20);
  EXPECT_EQ(snapshot.samples[0].value, 2.0);
  EXPECT_EQ(snapshot.samples[1].timestamp_ns, 30);
  EXPECT_EQ(snapshot.samples[1].value, 3.0);
}

TEST(SeriesBufferTest, LatestReturnsMostRecentSampleIncludingAfterWrap) {
  SeriesBuffer buffer(2);
  buffer.Push(MakeSample(10, 1.0));
  ASSERT_TRUE(buffer.Latest().has_value());
  EXPECT_EQ(buffer.Latest()->timestamp_ns, 10);
  EXPECT_EQ(buffer.Latest()->value, 1.0);

  buffer.Push(MakeSample(20, 2.0));
  buffer.Push(MakeSample(30, 3.0));

  ASSERT_TRUE(buffer.Latest().has_value());
  EXPECT_EQ(buffer.Latest()->timestamp_ns, 30);
  EXPECT_EQ(buffer.Latest()->value, 3.0);
}

TEST(SeriesBufferTest, GetNextSnapshotSizeEqualsUnsentCount) {
  SeriesBuffer buffer(4);
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 0u);

  buffer.Push(MakeSample(1, 1.0));
  buffer.Push(MakeSample(2, 2.0));
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 2u);

  const auto snapshot = buffer.GetSnapshot();
  buffer.AckSnapshot(snapshot);
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 0u);

  buffer.Push(MakeSample(3, 3.0));
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 1u);
}

TEST(SeriesBufferTest, GetSnapshotReturnsUnsentSamplesAndEndMarker) {
  SeriesBuffer buffer(4);
  buffer.Push(MakeSample(11, 1.5));
  buffer.Push(MakeSample(22, 2.5));

  const auto snapshot = buffer.GetSnapshot();

  ASSERT_EQ(snapshot.samples.size(), 2u);
  EXPECT_EQ(snapshot.samples[0].timestamp_ns, 11);
  EXPECT_EQ(snapshot.samples[0].value, 1.5);
  EXPECT_EQ(snapshot.samples[1].timestamp_ns, 22);
  EXPECT_EQ(snapshot.samples[1].value, 2.5);
  EXPECT_EQ(snapshot.end, buffer.GetHead());
}

TEST(SeriesBufferTest, AckSnapshotAdvancesTailPastAckedSamples) {
  SeriesBuffer buffer(4);
  buffer.Push(MakeSample(1, 1.0));
  buffer.Push(MakeSample(2, 2.0));
  buffer.Push(MakeSample(3, 3.0));

  const auto snapshot = buffer.GetSnapshot();
  const size_t head_at_ack = snapshot.end;
  buffer.AckSnapshot(snapshot);

  EXPECT_EQ(buffer.GetTail(), head_at_ack);
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 0u);
  EXPECT_TRUE(buffer.Empty());
}

TEST(SeriesBufferTest, AckSnapshotIsMonotonicAndIdempotent) {
  SeriesBuffer buffer(4);
  buffer.Push(MakeSample(1, 1.0));
  buffer.Push(MakeSample(2, 2.0));
  buffer.Push(MakeSample(3, 3.0));

  const auto first = buffer.GetSnapshot();
  buffer.AckSnapshot(first);
  const size_t tail_after_first = buffer.GetTail();

  buffer.AckSnapshot(first);
  EXPECT_EQ(buffer.GetTail(), tail_after_first);

  SeriesBuffer::Snapshot stale{.samples = {}, .end = 0};
  buffer.AckSnapshot(stale);
  EXPECT_EQ(buffer.GetTail(), tail_after_first);

  buffer.Push(MakeSample(4, 4.0));
  const auto second = buffer.GetSnapshot();
  ASSERT_GT(second.end, first.end);
  buffer.AckSnapshot(second);
  EXPECT_EQ(buffer.GetTail(), second.end);
  EXPECT_GE(buffer.GetTail(), tail_after_first);
}

TEST(SeriesBufferTest, SetCapacityResetsToEmptyConsistentState) {
  SeriesBuffer buffer(3);
  buffer.Push(MakeSample(1, 1.0));
  buffer.Push(MakeSample(2, 2.0));
  buffer.Push(MakeSample(3, 3.0));
  buffer.Push(MakeSample(4, 4.0));  // wrap; advances tail
  ASSERT_GT(buffer.GetTail(), 0u);
  ASSERT_GT(buffer.GetHead(), 0u);

  buffer.SetCapacity(5);

  EXPECT_EQ(buffer.Capacity(), 5u);
  EXPECT_EQ(buffer.GetHead(), 0u);
  EXPECT_EQ(buffer.GetTail(), 0u);
  EXPECT_TRUE(buffer.Empty());
  EXPECT_EQ(buffer.Size(), 0u);
  EXPECT_EQ(buffer.GetNextSnapshotSize(), 0u);
  EXPECT_FALSE(buffer.Latest().has_value());

  // Snapshot sizing must not underflow after reset on a previously live buffer.
  const auto snapshot = buffer.GetSnapshot();
  EXPECT_TRUE(snapshot.samples.empty());
  EXPECT_EQ(snapshot.end, 0u);
}

}  // namespace
}  // namespace agent
}  // namespace volta
