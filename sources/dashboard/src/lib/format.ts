/**
 * Value formatters for metric display.
 *
 * The {@link MetricBatch} carries only raw `double` values with no
 * unit information, so the unit/scaling is decided client-side per
 * metric type (see `metrics.ts`).
 */

const numberFormatters = new Map<number, Intl.NumberFormat>();

function nf(digits: number) {
  let f = numberFormatters.get(digits);
  if (!f) {
    f = new Intl.NumberFormat(undefined, {
      minimumFractionDigits: digits,
      maximumFractionDigits: digits,
    });
    numberFormatters.set(digits, f);
  }
  return f;
}

/** Pick a sensible fraction-digit count based on magnitude. */
function autoDigits(value: number) {
  const abs = Math.abs(value);
  if (abs === 0) return 0;
  if (abs >= 100) return 0;
  if (abs >= 10) return 1;
  return 2;
}

export function formatNumber(value: number, digits = autoDigits(value)) {
  if (!Number.isFinite(value)) return "—";
  return nf(digits).format(value);
}

function formatScaled(
  value: number,
  base: number,
  units: readonly string[],
) {
  if (!Number.isFinite(value)) return { value: "—", unit: units[0] };
  let v = value;
  let i = 0;
  while (Math.abs(v) >= base && i < units.length - 1) {
    v /= base;
    i++;
  }
  return { value: formatNumber(v, autoDigits(v)), unit: units[i] };
}

const BYTE_UNITS = ["B", "KB", "MB", "GB", "TB", "PB"] as const;
const BYTE_RATE_UNITS = ["B/s", "KB/s", "MB/s", "GB/s", "TB/s"] as const;

export function formatBytes(value: number) {
  return formatScaled(value, 1024, BYTE_UNITS);
}

export function formatBytesPerSec(value: number) {
  return formatScaled(value, 1024, BYTE_RATE_UNITS);
}

/** Assumes the raw value is already a percentage in the range [0, 100]. */
export function formatPercent(value: number) {
  return { value: formatNumber(value, 1), unit: "%" };
}

export function formatWatts(value: number) {
  return { value: formatNumber(value, 1), unit: "W" };
}

export function formatCelsius(value: number) {
  return { value: formatNumber(value, 1), unit: "°C" };
}

/** Assumes the raw clock value is expressed in MHz. */
export function formatClockMhz(value: number) {
  if (Math.abs(value) >= 1000) {
    return { value: formatNumber(value / 1000, 2), unit: "GHz" };
  }
  return { value: formatNumber(value, 0), unit: "MHz" };
}

export function formatCount(value: number) {
  return { value: formatNumber(value, 0), unit: "" };
}

export function withUnit(
  value: number,
  unit: string,
  digits?: number,
) {
  return { value: formatNumber(value, digits), unit };
}

/** Convert protobuf nanosecond timestamps to JS epoch milliseconds. */
export function nsToMs(ns: bigint) {
  return Number(ns / 1_000_000n);
}

const timeFormatter = new Intl.DateTimeFormat(undefined, {
  hour: "2-digit",
  minute: "2-digit",
  second: "2-digit",
  hour12: false,
});

export function formatClockTime(ms: number) {
  return timeFormatter.format(ms);
}
