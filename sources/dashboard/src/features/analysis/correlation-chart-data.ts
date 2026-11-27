import type { Rollup } from "@/lib/analysis";

export interface PairedPoint {
  bucketNs: number;
  x: number;
  y: number;
}

export function pairRollups(xRollup: Rollup, yRollup: Rollup): PairedPoint[] {
  const xValues = new Map<number, number>();
  const yValues = new Map<number, number>();

  for (const point of xRollup.points) xValues.set(point.bucket_ns, point.mean);
  for (const point of yRollup.points) yValues.set(point.bucket_ns, point.mean);

  const buckets = [...xValues.keys()]
    .filter((bucketNs) => Number.isFinite(bucketNs) && yValues.has(bucketNs))
    .sort((a, b) => a - b);
  const paired: PairedPoint[] = [];

  for (const bucketNs of buckets) {
    const x = xValues.get(bucketNs)!;
    const y = yValues.get(bucketNs)!;
    if (Number.isFinite(x) && Number.isFinite(y)) paired.push({ bucketNs, x, y });
  }

  return paired;
}

export function samplePoints(points: PairedPoint[], limit: number): PairedPoint[] {
  if (limit < 2 || points.length <= limit) return points;

  return Array.from({ length: limit }, (_, index) => {
    const sourceIndex = Math.round((index * (points.length - 1)) / (limit - 1));
    return points[sourceIndex];
  });
}
