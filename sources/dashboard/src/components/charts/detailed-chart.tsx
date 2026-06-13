import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type uPlot from "uplot";
import { UplotReact } from "@/lib/uplot-react";
import { useMeasuredWidth } from "@/hooks/use-measured-width";

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

  const data = useMemo<uPlot.AlignedData>(
    () => [times.map((t) => t / 1000), values],
    [times, values],
  );

  const baseOptions = useMemo(
    () =>
      ({
        scales: { x: { time: true } },
        legend: { show: false },
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
    [chartAxis, chartGrid, color, label, formatValue],
  );

  const options = useMemo<uPlot.Options>(
    () => ({ ...baseOptions, width: Math.max(width, 1), height }),
    [baseOptions, width, height],
  );

  const ready = times.length > 1 && values.length > 1;

  return (
    <div ref={ref} className={className}>
      {width > 0 && ready && (
        <UplotReact options={options} data={data} resetScales={resetScales} />
      )}
    </div>
  );
}
