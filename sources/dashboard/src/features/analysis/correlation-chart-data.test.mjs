import assert from "node:assert/strict";
import test from "node:test";
import { pairRollups, samplePoints } from "./correlation-chart-data.ts";

const rollup = (entries) => ({
  metric_type: "metric",
  device_id: "system",
  bucket_seconds: 10,
  points: entries.map(([bucket_ns, mean]) => ({ bucket_ns, mean, count: 1, min: mean, max: mean })),
});

test("pairs only overlapping buckets in time order, using their means", () => {
  const x = rollup([
    [30, 3],
    [10, 1],
    [20, 2],
    [40, 4],
  ]);
  const y = rollup([
    [20, 20],
    [30, 30],
    [50, 50],
  ]);
  assert.deepEqual(pairRollups(x, y), [
    { bucketNs: 20, x: 2, y: 20 },
    { bucketNs: 30, x: 3, y: 30 },
  ]);
});

test("uses the last occurrence of duplicate buckets and omits non-finite points", () => {
  const x = rollup([
    [1, 99],
    [1, 0],
    [2, Infinity],
    [3, 3],
    [NaN, 5],
  ]);
  const y = rollup([
    [1, 4],
    [1, -4],
    [2, 2],
    [3, NaN],
    [NaN, 1],
  ]);
  assert.deepEqual(pairRollups(x, y), [{ bucketNs: 1, x: 0, y: -4 }]);
  assert.deepEqual(pairRollups(rollup([[1, 1]]), rollup([[2, 2]])), []);
});

test("limits plotted points while preserving the first and last buckets", () => {
  const points = Array.from({ length: 1001 }, (_, bucketNs) => ({
    bucketNs,
    x: bucketNs,
    y: 2 * bucketNs,
  }));
  const sampled = samplePoints(points, 1000);
  assert.equal(sampled.length, 1000);
  assert.deepEqual(sampled[0], points[0]);
  assert.deepEqual(sampled.at(-1), points.at(-1));
  assert.deepEqual(samplePoints(points.slice(0, 2), 1000), points.slice(0, 2));
});
