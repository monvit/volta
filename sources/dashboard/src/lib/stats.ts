export interface SeriesStats {
  count: number;
  last: number;
  min: number;
  max: number;
  mean: number;
  stddev: number;
}

/** Summary statistics over the currently buffered live window. */
export function computeStats(values: number[]) {
  const finite = values.filter((v) => Number.isFinite(v));
  const count = finite.length;
  if (count === 0) {
    return {
      count: 0,
      last: Number.NaN,
      min: Number.NaN,
      max: Number.NaN,
      mean: Number.NaN,
      stddev: Number.NaN,
    };
  }
  let min = Infinity;
  let max = -Infinity;
  let sum = 0;
  for (const v of finite) {
    if (v < min) min = v;
    if (v > max) max = v;
    sum += v;
  }
  const mean = sum / count;
  let sqSum = 0;
  for (const v of finite) sqSum += (v - mean) ** 2;
  const stddev = Math.sqrt(sqSum / count);
  return { count, last: finite[finite.length - 1], min, max, mean, stddev };
}
