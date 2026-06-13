import { useCallback, useEffect, useRef } from "react";
import uPlot from "uplot";
import "uplot/dist/uPlot.min.css";

import { dataMatch, optionsUpdateState } from "./wrappers-common";

/** Vendored from `uplot-react` (MIT, github.com/skalinichev/uplot-wrappers) */

interface UplotReactProps {
  options: uPlot.Options;
  data: uPlot.AlignedData;
  /** Render into an external element instead of the internal div. */
  target?: HTMLElement;
  onCreate?: (chart: uPlot) => void;
  onDelete?: (chart: uPlot) => void;
  resetScales?: boolean | (() => boolean);
  className?: string;
}

export function UplotReact({
  options,
  data,
  target,
  onCreate,
  onDelete,
  resetScales = true,
  className,
}: UplotReactProps) {
  const chartRef = useRef<uPlot | null>(null);
  const targetRef = useRef<HTMLDivElement>(null);
  const propOptionsRef = useRef(options);
  const propTargetRef = useRef(target);
  const propDataRef = useRef(data);
  const onCreateRef = useRef(onCreate);
  const onDeleteRef = useRef(onDelete);

  useEffect(() => {
    onCreateRef.current = onCreate;
    onDeleteRef.current = onDelete;
  });

  const destroy = useCallback((chart: uPlot | null) => {
    if (chart) {
      onDeleteRef.current?.(chart);
      chart.destroy();
      chartRef.current = null;
    }
  }, []);

  const create = useCallback(() => {
    const chart = new uPlot(
      propOptionsRef.current,
      propDataRef.current,
      propTargetRef.current ?? (targetRef.current as HTMLDivElement),
    );
    chartRef.current = chart;
    onCreateRef.current?.(chart);
  }, []);

  useEffect(() => {
    create();
    return () => destroy(chartRef.current);
  }, [create, destroy]);

  useEffect(() => {
    if (propOptionsRef.current !== options) {
      const state = optionsUpdateState(propOptionsRef.current, options);
      propOptionsRef.current = options;
      if (!chartRef.current || state === "create") {
        destroy(chartRef.current);
        create();
      } else if (state === "update") {
        chartRef.current.setSize({ width: options.width, height: options.height });
      }
    }
  }, [options, create, destroy]);

  useEffect(() => {
    if (propDataRef.current !== data) {
      if (!chartRef.current) {
        propDataRef.current = data;
        create();
      } else if (!dataMatch(propDataRef.current, data)) {
        const shouldResetScales =
          typeof resetScales === "function" ? resetScales() : resetScales;
        chartRef.current.setData(data, shouldResetScales);
        if (!shouldResetScales) chartRef.current.redraw();
      }
      propDataRef.current = data;
    }
  }, [data, resetScales, create]);

  useEffect(() => {
    if (propTargetRef.current !== target) {
      propTargetRef.current = target;
      create();
    }
    return () => destroy(chartRef.current);
  }, [target, create, destroy]);

  return target ? null : <div ref={targetRef} className={className} />;
}

export { dataMatch, optionsUpdateState } from "./wrappers-common";
