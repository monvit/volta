import { formatClockTime, formatNumber } from "@/lib/format";
import { CATEGORIES, getMetricMeta, type MetricCategory } from "@/lib/metrics";
import { MetricType } from "@/proto/gen/types/metric_pb";

export interface AnalysisRequestPayload {
  from: string;
  to: string;
  metric_types: number[];
  bucket_seconds: number;
  sigma: number;
  sma_period: number;
  ema_period: number;
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

export interface MovingAveragePoint {
  timestamp_ns: number;
  sma: number | null;
  ema: number;
}

export interface MovingAverageSeries {
  metric_type: string;
  device_id: string;
  points: MovingAveragePoint[];
}

export interface AnalysisResult {
  moving_averages: MovingAverageSeries[];
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

export function metricTypeFromName(metricTypeName: string): MetricType | undefined {
  const short = metricTypeName.replace(/^METRIC_TYPE_/, "");
  const value = MetricType[short as keyof typeof MetricType];
  return typeof value === "number" && value !== MetricType.UNSPECIFIED ? value : undefined;
}

export function parseAnalysisSeriesKey(key: string) {
  const idx = key.indexOf("@");
  if (idx === -1) return { metricType: key, deviceId: "system" };
  return { metricType: key.slice(0, idx), deviceId: key.slice(idx + 1) || "system" };
}

export function formatAnalysisDeviceId(deviceId: string): string | undefined {
  if (!deviceId || deviceId === "system") return undefined;

  const cpuSlash = /^cpu=s(\d+)\/(\d+)$/.exec(deviceId);
  if (cpuSlash) return `CPU ${cpuSlash[1]}:${cpuSlash[2]}`;

  const cpuDash = /^cpu:s(\d+)-c(\d+)$/.exec(deviceId);
  if (cpuDash) return `CPU ${cpuDash[1]}:${cpuDash[2]}`;

  const cpuSocket = /^cpu:s(\d+)$/.exec(deviceId);
  if (cpuSocket) return `CPU ${cpuSocket[1]}`;

  const gpuEq = /^gpu=(.+)$/.exec(deviceId);
  if (gpuEq) return `GPU ${gpuEq[1]}`;

  const gpuColon = /^gpu:(.+)$/.exec(deviceId);
  if (gpuColon) return `GPU ${gpuColon[1]}`;

  const net = /^net(?:_ifindex=|:ifindex)(\d+)$/.exec(deviceId);
  if (net) return `if${net[1]}`;

  const disk = /^disk[=:](\d+:\d+)$/.exec(deviceId);
  if (disk) return `Disk ${disk[1]}`;

  return deviceId;
}

export interface AnalysisSeriesDisplay {
  title: string;
  categoryLabel: string;
  category?: MetricCategory;
  deviceLabel?: string;
  metricType?: MetricType;
  formatValue: (value: number) => string;
}

export function describeAnalysisSeries(
  metricTypeName: string,
  deviceId: string,
): AnalysisSeriesDisplay {
  const metricType = metricTypeFromName(metricTypeName);
  const deviceLabel = formatAnalysisDeviceId(deviceId);

  if (metricType !== undefined) {
    const meta = getMetricMeta(metricType);
    const categoryMeta = CATEGORIES.find((c) => c.id === meta.category);
    return {
      title: meta.label,
      categoryLabel: categoryMeta?.label ?? meta.category.toUpperCase(),
      category: meta.category,
      deviceLabel,
      metricType,
      formatValue: (v: number) => {
        const { value, unit } = meta.format(v);
        return unit ? `${value} ${unit}` : value;
      },
    };
  }

  return {
    title: metricTypeName.replace(/^METRIC_TYPE_/, ""),
    categoryLabel: "Metric",
    deviceLabel,
    formatValue: (v: number) => formatNumber(v),
  };
}

export function describeAnalysisKey(key: string): AnalysisSeriesDisplay {
  const { metricType, deviceId } = parseAnalysisSeriesKey(key);
  return describeAnalysisSeries(metricType, deviceId);
}

export function formatAnalysisKeyLabel(key: string): string {
  const info = describeAnalysisKey(key);
  const base = `${info.categoryLabel} · ${info.title}`;
  return info.deviceLabel ? `${base} (${info.deviceLabel})` : base;
}

export function formatTimestampNs(ns: number): string {
  if (!Number.isFinite(ns)) return "—";
  return formatClockTime(Math.trunc(ns / 1_000_000));
}

export function formatGeneratedAt(iso: string): string {
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return iso;
  return new Intl.DateTimeFormat(undefined, {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    year: "numeric",
    month: "short",
    day: "2-digit",
    hour12: false,
  }).format(date);
}

export function describeCorrelation(r: number): {
  label: string;
  variant: "default" | "secondary" | "outline";
} {
  const abs = Math.abs(r);
  const direction = r >= 0 ? "positive" : "negative";
  if (abs >= 0.7) return { label: `Strong ${direction}`, variant: "default" };
  if (abs >= 0.4) return { label: `Moderate ${direction}`, variant: "secondary" };
  if (abs >= 0.2) return { label: `Weak ${direction}`, variant: "outline" };
  return { label: "Negligible", variant: "outline" };
}
