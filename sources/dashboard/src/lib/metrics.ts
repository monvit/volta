import {
  CpuIcon,
  GraphicsCardIcon,
  HardDrivesIcon,
  MemoryIcon,
  NetworkIcon,
  type Icon,
} from "@phosphor-icons/react";
import { MetricType, type DeviceID } from "@/proto/gen/types/metric_pb";
import {
  formatBytes,
  formatBytesPerSec,
  formatCelsius,
  formatClockMhz,
  formatCount,
  formatPercent,
  formatWatts,
  withUnit,
} from "@/lib/format";

export type MetricCategory = "cpu" | "gpu" | "memory" | "disk" | "network";

export type FormattedValue = { value: string; unit: string };

export interface CategoryMeta {
  id: MetricCategory;
  label: string;
  icon: Icon;
}

/** Categories in dashboard display order. */
export const CATEGORIES: readonly CategoryMeta[] = [
  { id: "cpu", label: "CPU", icon: CpuIcon },
  { id: "gpu", label: "GPU", icon: GraphicsCardIcon },
  { id: "memory", label: "Memory", icon: MemoryIcon },
  { id: "disk", label: "Disk", icon: HardDrivesIcon },
  { id: "network", label: "Network", icon: NetworkIcon },
];

interface MetricMeta {
  label: string;
  category: MetricCategory;
  format: (value: number) => FormattedValue;
}

const iops = (v: number) => withUnit(v, "IOPS", 0);
const packetsPerSec = (v: number) => withUnit(v, "pkt/s", 0);

// The wire format carries only a numeric MetricType + raw doubles, so labels,
// units, and categories are supplied here, keyed by the generated enum.
const METRIC_META: Record<number, MetricMeta> = {
  [MetricType.CPU_POWER_PACKAGE]: { label: "Package Power", category: "cpu", format: formatWatts },
  [MetricType.CPU_POWER_CORES]: { label: "Cores Power", category: "cpu", format: formatWatts },
  [MetricType.CPU_CLOCK_SPEED]: { label: "Clock Speed", category: "cpu", format: formatClockMhz },
  [MetricType.CPU_UTILIZATION]: { label: "Utilization", category: "cpu", format: formatPercent },
  [MetricType.CPU_TEMPERATURE]: { label: "Temperature", category: "cpu", format: formatCelsius },
  [MetricType.CPU_IOWAIT]: { label: "IO Wait", category: "cpu", format: formatPercent },
  [MetricType.CPU_CACHE_HIT_RATIO]: {
    label: "Cache Hit Ratio",
    category: "cpu",
    format: formatPercent,
  },
  [MetricType.CPU_ACTIVE_PROCESSES]: {
    label: "Active Processes",
    category: "cpu",
    format: formatCount,
  },

  [MetricType.GPU_POWER]: { label: "Power", category: "gpu", format: formatWatts },
  [MetricType.GPU_CLOCK_SPEED]: { label: "Clock Speed", category: "gpu", format: formatClockMhz },
  [MetricType.GPU_UTILIZATION]: { label: "Utilization", category: "gpu", format: formatPercent },
  [MetricType.GPU_TEMPERATURE]: { label: "Temperature", category: "gpu", format: formatCelsius },
  [MetricType.GPU_VRAM_USED]: { label: "VRAM Used", category: "gpu", format: formatBytes },
  [MetricType.GPU_PCIE_BANDWIDTH]: {
    label: "PCIe Bandwidth",
    category: "gpu",
    format: formatBytesPerSec,
  },
  [MetricType.GPU_COMPUTE_UNIT_UTILIZATION]: {
    label: "Compute Unit Util.",
    category: "gpu",
    format: formatPercent,
  },
  [MetricType.GPU_SHARED_MEMORY_UTILIZATION]: {
    label: "Shared Memory Util.",
    category: "gpu",
    format: formatPercent,
  },
  [MetricType.GPU_REGISTER_UTILIZATION]: {
    label: "Register Util.",
    category: "gpu",
    format: formatPercent,
  },

  [MetricType.RAM_POWER]: { label: "Power", category: "memory", format: formatWatts },
  [MetricType.RAM_TOTAL]: { label: "Total", category: "memory", format: formatBytes },
  [MetricType.RAM_AVAILABLE]: { label: "Available", category: "memory", format: formatBytes },
  [MetricType.RAM_USED]: { label: "Used", category: "memory", format: formatBytes },
  [MetricType.RAM_CACHED]: { label: "Cached", category: "memory", format: formatBytes },
  [MetricType.SWAP_USED]: { label: "Swap Used", category: "memory", format: formatBytes },
  [MetricType.SWAP_ACTIVITY]: {
    label: "Swap Activity",
    category: "memory",
    format: formatBytesPerSec,
  },

  [MetricType.DISK_READ_THROUGHPUT]: {
    label: "Read Throughput",
    category: "disk",
    format: formatBytesPerSec,
  },
  [MetricType.DISK_WRITE_THROUGHPUT]: {
    label: "Write Throughput",
    category: "disk",
    format: formatBytesPerSec,
  },
  [MetricType.DISK_READ_IOPS]: { label: "Read IOPS", category: "disk", format: iops },
  [MetricType.DISK_WRITE_IOPS]: { label: "Write IOPS", category: "disk", format: iops },
  [MetricType.DISK_BUSY_TIME]: { label: "Busy Time", category: "disk", format: formatPercent },
  [MetricType.DISK_CAPACITY_USED]: {
    label: "Capacity Used",
    category: "disk",
    format: formatPercent,
  },

  [MetricType.NET_BYTES_RECEIVED]: {
    label: "Bytes Received",
    category: "network",
    format: formatBytesPerSec,
  },
  [MetricType.NET_BYTES_SENT]: {
    label: "Bytes Sent",
    category: "network",
    format: formatBytesPerSec,
  },
  [MetricType.NET_PACKETS_RECEIVED]: {
    label: "Packets Received",
    category: "network",
    format: packetsPerSec,
  },
  [MetricType.NET_PACKETS_SENT]: {
    label: "Packets Sent",
    category: "network",
    format: packetsPerSec,
  },
};

export function getMetricMeta(type: MetricType) {
  return (
    METRIC_META[type] ?? {
      label: `Metric ${type}`,
      category: "cpu",
      format: (v) => withUnit(v, ""),
    }
  );
}

export function formatMetricValue(type: MetricType, value: number) {
  return getMetricMeta(type).format(value);
}

/** Human label for a decoded {@link DeviceID} oneof, e.g. "CPU 0:3" or "GPU 0000:01:00.0". */
export function formatDeviceId(device?: DeviceID) {
  const d = device?.device;
  switch (d?.case) {
    case "cpu":
      return `CPU ${d.value.socketIndex}:${d.value.coreIndex}`;
    case "gpu": {
      const hex = (n: number, w: number) => n.toString(16).padStart(w, "0");
      const g = d.value;
      return `GPU ${hex(g.pciDomain, 4)}:${hex(g.pciBus, 2)}:${hex(g.pciDevice, 2)}.${g.pciFunction}`;
    }
    case "network":
      return `if${d.value.ifindex}`;
    case "disk":
      return `disk ${d.value.major}:${d.value.minor}`;
    default:
      return undefined;
  }
}

/** Stable identity for a live series: one metric type, at most one device per batch. */
export function seriesKey(type: MetricType, device?: DeviceID) {
  return `${type}::${formatDeviceId(device) ?? ""}`;
}
