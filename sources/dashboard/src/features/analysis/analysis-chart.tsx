import { useCallback, useMemo, useRef } from "react";
import uPlot from "uplot";
import { useChartChromeColors } from "@/components/charts/detailed-chart";
import { useMeasuredWidth } from "@/hooks/use-measured-width";
import type { MovingAverageSeries, Rollup } from "@/lib/analysis";
import { formatClockTime } from "@/lib/format";
import { UplotReact } from "@/lib/uplot-react";

const MEAN_COLOR = "#3b82f6";
const SMA_COLOR = "#22c55e";
const EMA_COLOR = "#f97316";
const TOOLTIP_SHIFT = 10;

interface AnalysisChartProps {
  rollup?: Rollup;
  movingAverage?: MovingAverageSeries;
  formatValue: (value: number) => string;
  height?: number;
}

function buildAlignedData(rollup?: Rollup, movingAverage?: MovingAverageSeries): uPlot.AlignedData {
  const rows = new Map<number, { mean: number | null; sma: number | null; ema: number | null }>();

  const ensure = (tsSec: number) => {
    let entry = rows.get(tsSec);
    if (!entry) {
      entry = { mean: null, sma: null, ema: null };
      rows.set(tsSec, entry);
    }
    return entry;
  };

  for (const point of rollup?.points ?? []) {
    ensure(point.bucket_ns / 1_000_000_000).mean = point.mean;
  }

  for (const point of movingAverage?.points ?? []) {
    const entry = ensure(point.timestamp_ns / 1_000_000_000);
    entry.sma = point.sma;
    entry.ema = point.ema;
  }

  const timestamps = [...rows.keys()].sort((a, b) => a - b);
  const means: (number | null)[] = [];
  const smas: (number | null)[] = [];
  const emas: (number | null)[] = [];

  for (const ts of timestamps) {
    const entry = rows.get(ts)!;
    means.push(entry.mean);
    smas.push(entry.sma);
    emas.push(entry.ema);
  }

  return [timestamps, means, smas, emas];
}

function multiSeriesTooltipPlugin(formatValue: (value: number) => string): uPlot.Plugin {
  let tooltip: HTMLDivElement | null = null;
  let overLeft = 0;
  let overTop = 0;

  const seriesMeta = [
    { index: 1, label: "Bucket Mean", color: MEAN_COLOR },
    { index: 2, label: "SMA", color: SMA_COLOR },
    { index: 3, label: "EMA", color: EMA_COLOR },
  ];

  function hideTooltip() {
    if (tooltip) tooltip.style.display = "none";
  }

  function setTooltip(u: uPlot, idx: number) {
    if (!tooltip) return;

    const tsSec = u.data[0][idx] as number;
    const rows = seriesMeta
      .map((item) => {
        const val = u.data[item.index]?.[idx] as number | null | undefined;
        if (val == null) return null;
        return `<div style="display:flex;align-items:center;gap:6px"><span style="display:inline-block;width:7px;height:7px;background:${item.color}"></span><span style="color:var(--muted-foreground)">${item.label}</span><span style="margin-left:auto;font-weight:500">${formatValue(val)}</span></div>`;
      })
      .filter((row): row is string => row !== null);

    tooltip.innerHTML = [
      `<div style="color:var(--muted-foreground);margin-bottom:2px">${formatClockTime(tsSec * 1000)}</div>`,
      ...(rows.length ? rows : [`<div>—</div>`]),
    ].join("");
    tooltip.style.display = "block";

    const x = overLeft + u.valToPos(tsSec, "x");
    const y = overTop + (u.cursor.top ?? 0);
    const { offsetWidth: tw, offsetHeight: th } = tooltip;

    let tooltipLeft = x + TOOLTIP_SHIFT;
    if (tooltipLeft + tw > u.width) tooltipLeft = x - TOOLTIP_SHIFT - tw;

    let tooltipTop = y + TOOLTIP_SHIFT;
    if (tooltipTop + th > u.height) tooltipTop = y - TOOLTIP_SHIFT - th;

    tooltip.style.left = `${tooltipLeft}px`;
    tooltip.style.top = `${Math.max(0, tooltipTop)}px`;
  }

  return {
    hooks: {
      ready: [
        (u) => {
          tooltip = document.createElement("div");
          Object.assign(tooltip.style, {
            position: "absolute",
            display: "none",
            pointerEvents: "none",
            zIndex: "10",
            padding: "6px 8px",
            fontSize: "11px",
            lineHeight: "1.45",
            minWidth: "150px",
            border: "1px solid var(--border)",
            background: "var(--popover)",
            color: "var(--popover-foreground)",
            boxShadow: "0 1px 2px rgb(0 0 0 / 0.05)",
            fontVariantNumeric: "tabular-nums",
          });
          overLeft = Number.parseFloat(u.over.style.left);
          overTop = Number.parseFloat(u.over.style.top);
          u.root.querySelector(".u-wrap")!.appendChild(tooltip);
        },
      ],
      setCursor: [
        (u) => {
          const idx = u.cursor.idx;
          if (idx == null) hideTooltip();
          else setTooltip(u, idx);
        },
      ],
      destroy: [() => hideTooltip()],
    },
  };
}

export function AnalysisChart({
  rollup,
  movingAverage,
  formatValue,
  height = 190,
}: AnalysisChartProps) {
  const [ref, width] = useMeasuredWidth();
  const { axis: chartAxis, grid: chartGrid } = useChartChromeColors();
  const zoomedRef = useRef(false);
  const resetScales = useCallback(() => !zoomedRef.current, []);

  const tooltipPlugin = useMemo(() => multiSeriesTooltipPlugin(formatValue), [formatValue]);
  const data = useMemo(() => buildAlignedData(rollup, movingAverage), [rollup, movingAverage]);

  const baseOptions = useMemo(
    () =>
      ({
        scales: { x: { time: true } },
        legend: { show: false },
        plugins: [tooltipPlugin],
        hooks: {
          setSelect: [
            (u) => {
              if (u.select.width > 0) zoomedRef.current = true;
            },
          ],
        },
        cursor: {
          y: false,
          points: { size: 5 },
          bind: {
            dblclick: (_u, _targ, handler) => (e) => {
              zoomedRef.current = false;
              return handler(e);
            },
          },
        },
        axes: [
          {
            stroke: chartAxis,
            grid: { stroke: chartGrid, width: 1 },
            ticks: { stroke: chartGrid, width: 1 },
            font: "11px monospace",
          },
          {
            stroke: chartAxis,
            grid: { stroke: chartGrid, width: 1 },
            ticks: { stroke: chartGrid, width: 1 },
            font: "11px monospace",
            size: 68,
            values: (_u, splits) => splits.map(formatValue),
          },
        ],
        series: [
          {},
          {
            label: "Bucket Mean",
            stroke: MEAN_COLOR,
            width: 2,
            spanGaps: true,
            points: { show: true, size: 5, fill: MEAN_COLOR },
          },
          {
            label: "SMA",
            stroke: SMA_COLOR,
            width: 1.5,
            spanGaps: true,
            points: { show: false },
          },
          {
            label: "EMA",
            stroke: EMA_COLOR,
            width: 1.5,
            spanGaps: true,
            points: { show: false },
          },
        ],
      }) satisfies Omit<uPlot.Options, "width" | "height">,
    [chartAxis, chartGrid, formatValue, tooltipPlugin],
  );

  const options = useMemo<uPlot.Options>(
    () => ({ ...baseOptions, width: Math.max(width, 1), height }),
    [baseOptions, width, height],
  );

  const ready = data[0].length > 1;

  return (
    <div className="flex flex-col gap-2">
      <div className="flex flex-wrap items-center gap-4 text-[11px] text-muted-foreground">
        <span className="inline-flex items-center gap-1.5">
          <span className="size-2 bg-[#3b82f6]" />
          Bucket Mean
        </span>
        <span className="inline-flex items-center gap-1.5">
          <span className="size-2 bg-[#22c55e]" />
          SMA
        </span>
        <span className="inline-flex items-center gap-1.5">
          <span className="size-2 bg-[#f97316]" />
          EMA
        </span>
        <span className="ml-auto text-[10px]">Drag to zoom · double-click to reset</span>
      </div>
      <div ref={ref} className="w-full">
        {ready ? (
          width > 0 && <UplotReact options={options} data={data} resetScales={resetScales} />
        ) : (
          <p className="py-6 text-center text-xs text-muted-foreground">
            Not enough samples to plot trend chart.
          </p>
        )}
      </div>
    </div>
  );
}
