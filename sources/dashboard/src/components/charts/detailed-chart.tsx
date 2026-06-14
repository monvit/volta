import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type uPlot from "uplot";
import { UplotReact } from "@/lib/uplot-react";
import { useMeasuredWidth } from "@/hooks/use-measured-width";
import { formatClockTime } from "@/lib/format";

const TOOLTIP_SHIFT = 10;

function chartTooltipPlugin(formatValue: (value: number) => string): uPlot.Plugin {
  let tooltip: HTMLDivElement | null = null;
  let overLeft = 0;
  let overTop = 0;

  function hideTooltip() {
    if (tooltip) tooltip.style.display = "none";
  }

  function setTooltip(u: uPlot, idx: number) {
    if (!tooltip) return;

    const tsSec = u.data[0][idx] as number;
    const val = u.data[1][idx] as number | null | undefined;
    const top = val == null ? (u.cursor.top ?? 0) : u.valToPos(val, "y");
    const left = u.valToPos(tsSec, "x");

    tooltip.innerHTML = [
      `<div style="color:var(--muted-foreground)">${formatClockTime(tsSec * 1000)}</div>`,
      `<div style="font-weight:500">${val == null ? "—" : formatValue(val)}</div>`,
    ].join("");
    tooltip.style.display = "block";

    const x = overLeft + left;
    const y = overTop + top;
    const { offsetWidth: tw, offsetHeight: th } = tooltip;

    let tooltipLeft = x + TOOLTIP_SHIFT;
    if (tooltipLeft + tw > u.width) tooltipLeft = x - TOOLTIP_SHIFT - tw;

    let tooltipTop = y + TOOLTIP_SHIFT;
    if (tooltipTop + th > u.height) tooltipTop = y - TOOLTIP_SHIFT - th;

    tooltip.style.left = `${tooltipLeft}px`;
    tooltip.style.top = `${tooltipTop}px`;
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
            padding: "4px 8px",
            borderRadius: "6px",
            fontSize: "12px",
            lineHeight: "1.4",
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

function readChartChromeColors() {
  const el = document.createElement("span");
  el.style.display = "none";
  document.documentElement.appendChild(el);

  el.style.color = "var(--muted-foreground)";
  const axis = getComputedStyle(el).color;

  el.style.color = "var(--border)";
  const grid = getComputedStyle(el).color;

  el.remove();
  return { axis, grid };
}

function useChartChromeColors() {
  const [colors, setColors] = useState(readChartChromeColors);

  useEffect(() => {
    const update = () => setColors(readChartChromeColors());

    const observer = new MutationObserver(update);
    observer.observe(document.documentElement, {
      attributes: true,
      attributeFilter: ["class"],
    });

    const media = window.matchMedia("(prefers-color-scheme: dark)");
    media.addEventListener("change", update);

    return () => {
      observer.disconnect();
      media.removeEventListener("change", update);
    };
  }, []);

  return colors;
}

interface DetailedChartProps {
  /** Unix epoch milliseconds. */
  times: number[];
  values: number[];
  color: string;
  label: string;
  formatValue: (value: number) => string;
  height?: number;
  className?: string;
}

/** Full axis line chart for a time series. */
export function DetailedChart({
  times,
  values,
  color,
  label,
  formatValue,
  height = 260,
  className,
}: DetailedChartProps) {
  const [ref, width] = useMeasuredWidth();
  const { axis: chartAxis, grid: chartGrid } = useChartChromeColors();
  const zoomedRef = useRef(false);

  const resetScales = useCallback(() => !zoomedRef.current, []);

  const tooltipPlugin = useMemo(() => chartTooltipPlugin(formatValue), [formatValue]);

  const data = useMemo<uPlot.AlignedData>(
    () => [times.map((t) => t / 1000), values],
    [times, values],
  );

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
            size: 60,
            values: (_u, splits) => splits.map(formatValue),
          },
        ],
        series: [
          {},
          {
            label,
            stroke: color,
            width: 1.5,
            fill: `${color}1f`,
            points: { show: false },
            value: (_u, v) => (v == null ? "—" : formatValue(v)),
          },
        ],
      }) satisfies Omit<uPlot.Options, "width" | "height">,
    [chartAxis, chartGrid, color, label, formatValue, tooltipPlugin],
  );

  const options = useMemo<uPlot.Options>(
    () => ({ ...baseOptions, width: Math.max(width, 1), height }),
    [baseOptions, width, height],
  );

  const ready = times.length > 1 && values.length > 1;

  return (
    <div ref={ref} className={className}>
      {width > 0 && ready && <UplotReact options={options} data={data} resetScales={resetScales} />}
    </div>
  );
}
