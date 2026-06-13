import type uPlot from "uplot";

/** Vendored from `uplot-wrappers-common` (MIT, github.com/skalinichev/uplot-wrappers) */

type OptionsUpdateState = "keep" | "update" | "create";

export function optionsUpdateState(lhsOptions: uPlot.Options, rhsOptions: uPlot.Options) {
  const { width: lhsWidth, height: lhsHeight, ...lhs } = lhsOptions;
  const { width: rhsWidth, height: rhsHeight, ...rhs } = rhsOptions;

  let state: OptionsUpdateState = "keep";
  if (lhsHeight !== rhsHeight || lhsWidth !== rhsWidth) {
    state = "update";
  }

  const lhsRest = lhs as Record<string, unknown>;
  const rhsRest = rhs as Record<string, unknown>;
  if (Object.keys(lhsRest).length !== Object.keys(rhsRest).length) {
    return "create";
  }
  for (const key of Object.keys(lhsRest)) {
    if (!Object.is(lhsRest[key], rhsRest[key])) {
      return "create";
    }
  }
  return state;
}

export function dataMatch(lhs: uPlot.AlignedData, rhs: uPlot.AlignedData) {
  if (lhs.length !== rhs.length) {
    return false;
  }
  for (let s = 0; s < lhs.length; s++) {
    const lhsSeries = lhs[s];
    const rhsSeries = rhs[s];
    if (lhsSeries.length !== rhsSeries.length) {
      return false;
    }
    for (let i = 0; i < lhsSeries.length; i++) {
      if (lhsSeries[i] !== rhsSeries[i]) {
        return false;
      }
    }
  }
  return true;
}
