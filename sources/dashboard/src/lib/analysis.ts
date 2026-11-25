export interface AnalysisRequestPayload {
  from: string;
  to: string;
  metric_types: number[];
  bucket_seconds: number;
  sigma: number;
  min_points: number;
  pairs: [string, string][];
}

export interface SeriesStats {
  metric_type: string;
  device_id: string;
  count: number;
  first: number;
  last: number;
  min: number;
  max: number;
  mean: number;
  median: number;
  stddev: number;
}

export interface RollupPoint {
  bucket_ns: number;
  count: number;
  mean: number;
  min: number;
  max: number;
}

export interface Rollup {
  metric_type: string;
  device_id: string;
  bucket_seconds: number;
  points: RollupPoint[];
}

export interface Anomaly {
  metric_type: string;
  device_id: string;
  timestamp_ns: number;
  value: number;
  z_score: number;
}

export interface Correlation {
  x: string;
  y: string;
  pearson_r: number;
  n: number;
}

export interface Regression {
  x: string;
  y: string;
  slope: number;
  intercept: number;
  r_squared: number;
  n: number;
}

export interface AnalysisResult {
  source: "csv" | "database";
  generated_at: string;
  series: SeriesStats[];
  rollups: Rollup[];
  anomalies: Anomaly[];
  correlations: Correlation[];
  regressions: Regression[];
  warnings: string[];
}

export function shortSeriesKey(key: string) {
  return key.replace("METRIC_TYPE_", "").replace("@system", "");
}
