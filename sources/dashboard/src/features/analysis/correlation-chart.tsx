import { useId, useMemo, useState } from "react";
import { useChartChromeColors } from "@/components/charts/detailed-chart";
import { useMeasuredWidth } from "@/hooks/use-measured-width";
import {
  pairRollups,
  samplePoints,
  type PairedPoint,
} from "@/features/analysis/correlation-chart-data";
import { formatTimestampNs, type Regression, type Rollup } from "@/lib/analysis";

const CHART_HEIGHT = 320;
const MAX_POINTS = 1000;
const POINT_COLOR = "#3b82f6";
const REGRESSION_COLOR = "#f97316";

interface CorrelationChartProps {
  xRollup: Rollup;
  yRollup: Rollup;
  regression?: Regression;
  xLabel: string;
  yLabel: string;
  formatX: (value: number) => string;
  formatY: (value: number) => string;
}

interface RegressionLine {
  x1: number;
  y1: number;
  x2: number;
  y2: number;
}

function extent(points: PairedPoint[], key: "x" | "y") {
  let min = Infinity;
  let max = -Infinity;

  for (const point of points) {
    min = Math.min(min, point[key]);
    max = Math.max(max, point[key]);
  }

  return { min, max };
}

function paddedDomain(min: number, max: number): [number, number] {
  const padding = min === max ? Math.max(Math.abs(min) * 0.05, 1) : (max - min) * 0.05;
  const lower = min - padding;
  const upper = max + padding;
  return Number.isFinite(lower) && Number.isFinite(upper) && lower < upper
    ? [lower, upper]
    : [min, max];
}

function scale(
  value: number,
  domainMin: number,
  domainMax: number,
  rangeMin: number,
  rangeMax: number,
) {
  if (domainMin === domainMax) return (rangeMin + rangeMax) / 2;
  return rangeMin + ((value - domainMin) / (domainMax - domainMin)) * (rangeMax - rangeMin);
}

function ticks(min: number, max: number, count: number) {
  return Array.from({ length: count }, (_, index) => {
    const ratio = index / (count - 1);
    return min * (1 - ratio) + max * ratio;
  });
}

function clamp(value: number, min: number, max: number) {
  return Math.min(Math.max(value, min), Math.max(min, max));
}

export function CorrelationChart({
  xRollup,
  yRollup,
  regression,
  xLabel,
  yLabel,
  formatX,
  formatY,
}: CorrelationChartProps) {
  const [ref, width] = useMeasuredWidth();
  const { axis: chartAxis, grid: chartGrid } = useChartChromeColors();
  const [activeBucketNs, setActiveBucketNs] = useState<number | null>(null);
  const titleId = useId();
  const descriptionId = useId();
  const tooltipId = useId();
  const points = useMemo(() => pairRollups(xRollup, yRollup), [xRollup, yRollup]);
  const visiblePoints = useMemo(() => samplePoints(points, MAX_POINTS), [points]);

  if (points.length === 0) {
    return (
      <div ref={ref} className="w-full min-w-0">
        <p className="py-6 text-center text-xs text-muted-foreground" role="status">
          No overlapping buckets with finite X and Y means to plot.
        </p>
      </div>
    );
  }

  const xExtent = extent(points, "x");
  const yExtent = extent(points, "y");
  let regressionLine: RegressionLine | null = null;

  if (
    regression &&
    xExtent.min < xExtent.max &&
    Number.isFinite(regression.slope) &&
    Number.isFinite(regression.intercept)
  ) {
    const y1 = regression.slope * xExtent.min + regression.intercept;
    const y2 = regression.slope * xExtent.max + regression.intercept;
    if (Number.isFinite(y1) && Number.isFinite(y2)) {
      regressionLine = { x1: xExtent.min, y1, x2: xExtent.max, y2 };
    }
  }

  if (regressionLine) {
    yExtent.min = Math.min(yExtent.min, regressionLine.y1, regressionLine.y2);
    yExtent.max = Math.max(yExtent.max, regressionLine.y1, regressionLine.y2);
  }

  const [xDomainMin, xDomainMax] = paddedDomain(xExtent.min, xExtent.max);
  const [yDomainMin, yDomainMax] = paddedDomain(yExtent.min, yExtent.max);
  const yTicks = ticks(yDomainMin, yDomainMax, 5);
  const xTickCount = width < 360 ? 3 : 5;
  const xTicks = ticks(xDomainMin, xDomainMax, xTickCount);
  const longestYLabel = Math.max(...yTicks.map((tick) => formatY(tick).length));
  const margin = {
    top: 16,
    right: 16,
    bottom: 58,
    left: Math.min(112, Math.max(68, Math.ceil(longestYLabel * 6.2) + 14)),
  };
  const plotLeft = margin.left;
  const plotRight = Math.max(plotLeft + 1, width - margin.right);
  const plotTop = margin.top;
  const plotBottom = CHART_HEIGHT - margin.bottom;
  const plotWidth = plotRight - plotLeft;
  const plotHeight = plotBottom - plotTop;
  const toX = (value: number) => scale(value, xDomainMin, xDomainMax, plotLeft, plotRight);
  const toY = (value: number) => scale(value, yDomainMin, yDomainMax, plotBottom, plotTop);
  const activePoint = visiblePoints.find((point) => point.bucketNs === activeBucketNs) ?? null;
  const activeX = activePoint ? toX(activePoint.x) : 0;
  const activeY = activePoint ? toY(activePoint.y) : 0;
  const tooltipWidth = Math.min(224, Math.max(0, width - 16));
  const tooltipHeight = 104;
  const tooltipLeft = clamp(activeX + 12, 8, width - tooltipWidth - 8);
  const preferredTooltipTop = activeY + 12;
  const tooltipTop = clamp(
    preferredTooltipTop + tooltipHeight > CHART_HEIGHT
      ? activeY - tooltipHeight - 12
      : preferredTooltipTop,
    8,
    CHART_HEIGHT - tooltipHeight - 8,
  );
  const reduced = visiblePoints.length < points.length;
  const description = `Scatter plot of ${points.length} paired bucket means, with ${xLabel} on the numerical X axis and ${yLabel} on the Y axis. ${regressionLine ? "A linear regression line spans the observed X data domain. " : ""}${reduced ? `Displaying ${visiblePoints.length} deterministically sampled points; axis domains and regression use all pairs.` : "All paired points are displayed."}`;

  return (
    <div ref={ref} className="w-full min-w-0">
      <figure className="m-0 flex flex-col gap-2">
        <figcaption className="flex flex-wrap items-center gap-x-4 gap-y-1 text-[11px] text-muted-foreground">
          <span className="inline-flex items-center gap-1.5">
            <span className="size-2 rounded-full" style={{ backgroundColor: POINT_COLOR }} />
            Paired bucket means
          </span>
          {regressionLine && (
            <span className="inline-flex items-center gap-1.5">
              <span className="h-0.5 w-4" style={{ backgroundColor: REGRESSION_COLOR }} />
              Regression fit
            </span>
          )}
          <span className="ml-auto tabular-nums">n = {points.length.toLocaleString()} buckets</span>
        </figcaption>
        <div className="relative w-full">
          {width > 0 && (
            <svg
              className="block max-w-full"
              width={width}
              height={CHART_HEIGHT}
              viewBox={`0 0 ${width} ${CHART_HEIGHT}`}
              role="img"
              aria-labelledby={`${titleId} ${descriptionId}`}
            >
              <title id={titleId}>{`${yLabel} versus ${xLabel}`}</title>
              <desc id={descriptionId}>{description}</desc>
              <g aria-hidden="true">
                {yTicks.map((tick, index) => {
                  const y = toY(tick);
                  return (
                    <g key={`y-${index}`}>
                      <line
                        x1={plotLeft}
                        x2={plotRight}
                        y1={y}
                        y2={y}
                        stroke={chartGrid}
                        strokeWidth={1}
                      />
                      <line x1={plotLeft - 4} x2={plotLeft} y1={y} y2={y} stroke={chartAxis} />
                      <text
                        x={plotLeft - 7}
                        y={y + 3}
                        textAnchor="end"
                        fill={chartAxis}
                        fontSize={10}
                      >
                        {formatY(tick)}
                      </text>
                    </g>
                  );
                })}
                {xTicks.map((tick, index) => {
                  const x = toX(tick);
                  const anchor =
                    index === 0 ? "start" : index === xTicks.length - 1 ? "end" : "middle";
                  return (
                    <g key={`x-${index}`}>
                      <line
                        x1={x}
                        x2={x}
                        y1={plotTop}
                        y2={plotBottom}
                        stroke={chartGrid}
                        strokeWidth={1}
                      />
                      <line x1={x} x2={x} y1={plotBottom} y2={plotBottom + 4} stroke={chartAxis} />
                      <text
                        x={x}
                        y={plotBottom + 17}
                        textAnchor={anchor}
                        fill={chartAxis}
                        fontSize={10}
                      >
                        {formatX(tick)}
                      </text>
                    </g>
                  );
                })}
                <line
                  x1={plotLeft}
                  x2={plotRight}
                  y1={plotBottom}
                  y2={plotBottom}
                  stroke={chartAxis}
                />
                <line x1={plotLeft} x2={plotLeft} y1={plotTop} y2={plotBottom} stroke={chartAxis} />
                <text
                  x={plotLeft + plotWidth / 2}
                  y={CHART_HEIGHT - 7}
                  textAnchor="middle"
                  fill={chartAxis}
                  fontSize={11}
                  fontWeight={500}
                >
                  {xLabel}
                </text>
                <text
                  x={13}
                  y={plotTop + plotHeight / 2}
                  textAnchor="middle"
                  fill={chartAxis}
                  fontSize={11}
                  fontWeight={500}
                  transform={`rotate(-90 13 ${plotTop + plotHeight / 2})`}
                >
                  {yLabel}
                </text>
              </g>
              {regressionLine && (
                <line
                  x1={toX(regressionLine.x1)}
                  y1={toY(regressionLine.y1)}
                  x2={toX(regressionLine.x2)}
                  y2={toY(regressionLine.y2)}
                  stroke={REGRESSION_COLOR}
                  strokeWidth={2}
                  strokeLinecap="round"
                  aria-hidden="true"
                />
              )}
              <g>
                {visiblePoints.map((point) => {
                  const x = toX(point.x);
                  const y = toY(point.y);
                  const active = point.bucketNs === activeBucketNs;
                  const pointDescription = `${xLabel}: ${formatX(point.x)}; ${yLabel}: ${formatY(point.y)}; bucket: ${formatTimestampNs(point.bucketNs)}`;
                  const deactivate = () => {
                    setActiveBucketNs((current) => (current === point.bucketNs ? null : current));
                  };

                  return (
                    <g key={point.bucketNs}>
                      {active && (
                        <circle
                          cx={x}
                          cy={y}
                          r={visiblePoints.length > 500 ? 6 : 7}
                          fill="none"
                          stroke={chartAxis}
                          strokeWidth={1.5}
                          pointerEvents="none"
                        />
                      )}
                      <circle
                        cx={x}
                        cy={y}
                        r={visiblePoints.length > 500 ? 3 : 4}
                        fill={POINT_COLOR}
                        fillOpacity={0.82}
                        stroke="var(--background)"
                        strokeWidth={1}
                        tabIndex={0}
                        aria-label={pointDescription}
                        aria-describedby={active ? tooltipId : undefined}
                        onPointerEnter={() => setActiveBucketNs(point.bucketNs)}
                        onPointerLeave={deactivate}
                        onFocus={() => setActiveBucketNs(point.bucketNs)}
                        onBlur={deactivate}
                      >
                        <title>{pointDescription}</title>
                      </circle>
                    </g>
                  );
                })}
              </g>
            </svg>
          )}
          {activePoint && width > 0 && (
            <div
              id={tooltipId}
              role="tooltip"
              className="pointer-events-none absolute z-10 border border-border bg-popover p-2 text-[11px] leading-relaxed text-popover-foreground shadow-sm"
              style={{ left: tooltipLeft, top: tooltipTop, width: tooltipWidth }}
            >
              <div className="mb-1 text-muted-foreground">
                Bucket {formatTimestampNs(activePoint.bucketNs)}
              </div>
              <div className="grid grid-cols-[minmax(0,1fr)_auto] gap-x-3 gap-y-0.5">
                <span className="min-w-0 break-words text-muted-foreground">{xLabel}</span>
                <span className="text-right font-medium break-words tabular-nums">
                  {formatX(activePoint.x)}
                </span>
                <span className="min-w-0 break-words text-muted-foreground">{yLabel}</span>
                <span className="text-right font-medium break-words tabular-nums">
                  {formatY(activePoint.y)}
                </span>
              </div>
            </div>
          )}
        </div>
        {reduced && (
          <p className="text-[11px] text-muted-foreground">
            Showing {visiblePoints.length.toLocaleString()} of {points.length.toLocaleString()}{" "}
            paired buckets using deterministic even sampling. Axis domains and regression use all
            pairs.
          </p>
        )}
      </figure>
    </div>
  );
}
