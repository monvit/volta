import { useMemo } from "react";
import type uPlot from "uplot";
import { UplotReact } from "@/lib/uplot-react";
import { useMeasuredWidth } from "@/hooks/use-measured-width";

interface SparklineProps {
  values: number[];
  color: string;
  height?: number;
  className?: string;
}

/** Minimal axis-less chart */
export function Sparkline({ values, color, height = 36, className }: SparklineProps) {
  const [ref, width] = useMeasuredWidth();

  const data = useMemo<uPlot.AlignedData>(() => [values.map((_, i) => i), values], [values]);

  const baseOptions = useMemo(
    () =>
      ({
        legend: { show: false },
        cursor: { show: false },
        scales: { x: { time: false } },
        axes: [{ show: false }, { show: false }],
        series: [{}, { stroke: color, width: 1.25, fill: `${color}1f`, points: { show: false } }],
      }) satisfies Omit<uPlot.Options, "width" | "height">,
    [color],
  );

  const options = useMemo<uPlot.Options>(
    () => ({ ...baseOptions, width: Math.max(width, 1), height }),
    [baseOptions, width, height],
  );

  return (
    <div ref={ref} className={className} aria-hidden="true">
      {width > 0 && values.length > 1 && <UplotReact options={options} data={data} />}
    </div>
  );
}
