"""Pure, transport-independent analysis of metric series."""

from __future__ import annotations

import math
from collections import defaultdict
from collections.abc import Iterable

import numpy as np

from volta_analysis.model import (
    AnalysisRequest,
    AnalysisResult,
    Anomaly,
    Correlation,
    Point,
    Regression,
    Rollup,
    RollupPoint,
    SeriesStats,
    series_key,
)


def run(request: AnalysisRequest) -> AnalysisResult:
    result = AnalysisResult(source=request.source)
    options = request.options
    grouped = group_by_series(request.points)

    if not grouped:
        result.warnings.append("no samples to analyse")
        return result

    rollups_by_key: dict[str, Rollup] = {}
    for key, points in grouped.items():
        stats = describe(points)
        if stats is None:
            result.warnings.append(f"{key}: no finite values")
            continue
        result.series.append(stats)
        current = rollup(points, options.bucket_seconds)
        rollups_by_key[key] = current
        result.rollups.append(current)
        result.anomalies.extend(detect_anomalies(points, options.sigma))

    for x, y in options.pairs:
        if x not in grouped or y not in grouped:
            result.warnings.append(f"pair not found: {x} / {y}")
            continue

        xs, ys = align(rollups_by_key[x], rollups_by_key[y])
        if xs.size < options.min_points:
            result.warnings.append(
                f"{x} / {y}: only {xs.size} overlapping buckets (min {options.min_points})"
            )
            continue
        if np.std(xs) == 0 or np.std(ys) == 0:
            result.warnings.append(f"{x} / {y}: constant series, correlation undefined")
            continue

        pearson_r = float(np.corrcoef(xs, ys)[0, 1])
        result.correlations.append(Correlation(x=x, y=y, pearson_r=pearson_r, n=int(xs.size)))

        slope, intercept = np.polyfit(xs, ys, 1)
        result.regressions.append(
            Regression(
                x=x,
                y=y,
                slope=float(slope),
                intercept=float(intercept),
                r_squared=pearson_r**2,
                n=int(xs.size),
            )
        )

    return result


def group_by_series(points: Iterable[Point]) -> dict[str, list[Point]]:
    grouped: dict[str, list[Point]] = defaultdict(list)
    for point in points:
        grouped[series_key(point.metric_type, point.device_id)].append(point)
    for series in grouped.values():
        series.sort(key=lambda point: point.timestamp_ns)
    return dict(grouped)


def describe(points: list[Point]) -> SeriesStats | None:
    values = np.asarray([point.value for point in points], dtype=float)
    finite = values[np.isfinite(values)]
    if finite.size == 0:
        return None

    return SeriesStats(
        metric_type=points[0].metric_type,
        device_id=points[0].device_id,
        count=int(finite.size),
        first=float(finite[0]),
        last=float(finite[-1]),
        min=float(np.min(finite)),
        max=float(np.max(finite)),
        mean=float(np.mean(finite)),
        median=float(np.median(finite)),
        stddev=float(np.std(finite)),
    )


def rollup(points: list[Point], bucket_seconds: int) -> Rollup:
    bucket_ns = bucket_seconds * 1_000_000_000
    buckets: dict[int, list[float]] = defaultdict(list)
    for point in points:
        if math.isfinite(point.value):
            buckets[point.timestamp_ns // bucket_ns].append(point.value)

    return Rollup(
        metric_type=points[0].metric_type,
        device_id=points[0].device_id,
        bucket_seconds=bucket_seconds,
        points=[
            RollupPoint(
                bucket_ns=index * bucket_ns,
                count=len(values),
                mean=float(np.mean(values)),
                min=float(np.min(values)),
                max=float(np.max(values)),
            )
            for index, values in sorted(buckets.items())
        ],
    )


def detect_anomalies(points: list[Point], sigma: float) -> list[Anomaly]:
    values = np.asarray([point.value for point in points], dtype=float)
    finite = values[np.isfinite(values)]
    if finite.size < 2:
        return []

    mean = float(np.mean(finite))
    stddev = float(np.std(finite))
    if stddev == 0:
        return []

    return [
        Anomaly(
            metric_type=point.metric_type,
            device_id=point.device_id,
            timestamp_ns=point.timestamp_ns,
            value=point.value,
            z_score=abs(point.value - mean) / stddev,
        )
        for point in points
        if math.isfinite(point.value) and abs(point.value - mean) / stddev > sigma
    ]


def align(left: Rollup, right: Rollup) -> tuple[np.ndarray, np.ndarray]:
    """Join two series on bucket boundaries, using each bucket's mean.

    Collectors timestamp samples independently, so exact-timestamp joins almost
    never overlap on real data. Bucket means also give one point per interval.
    """
    left_values = {point.bucket_ns: point.mean for point in left.points}
    right_values = {point.bucket_ns: point.mean for point in right.points}
    common = sorted(left_values.keys() & right_values.keys())
    return (
        np.asarray([left_values[ts] for ts in common], dtype=float),
        np.asarray([right_values[ts] for ts in common], dtype=float),
    )
