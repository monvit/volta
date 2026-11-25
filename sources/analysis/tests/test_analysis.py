from __future__ import annotations

import pytest

from volta_analysis.analysis import describe, group_by_series, run
from volta_analysis.metric_io import parse_csv
from volta_analysis.model import AnalysisOptions, AnalysisRequest, Point, series_key

X = series_key("METRIC_TYPE_CPU_UTILIZATION", "system")
Y = series_key("METRIC_TYPE_CPU_POWER_PACKAGE", "system")
T = series_key("METRIC_TYPE_CPU_TEMPERATURE", "system")


def request(sigma: float = 3.0, pairs: list[tuple[str, str]] | None = None) -> AnalysisRequest:
    points, _ = parse_csv("tests/fixtures/sample_dump.csv")
    return AnalysisRequest(
        source="csv",
        points=points,
        options=AnalysisOptions(
            bucket_seconds=10, sigma=sigma, min_points=2, pairs=pairs or [(X, Y)]
        ),
    )


def test_describe_matches_expected_values() -> None:
    points, _ = parse_csv("tests/fixtures/sample_dump.csv")
    grouped = group_by_series(points)

    stats = describe(grouped[X])

    assert stats is not None
    assert stats.count == 12
    assert stats.min == 10
    assert stats.max == 120
    assert stats.mean == pytest.approx(65)
    assert stats.median == pytest.approx(65)
    assert stats.stddev == pytest.approx(34.5195, rel=1e-4)


def test_run_computes_rollups_anomalies_and_regression() -> None:
    result = run(request(sigma=2.5))

    assert len(result.series) == 3
    utilisation = next(
        series for series in result.series if series.metric_type.endswith("UTILIZATION")
    )
    assert utilisation.count == 12

    rollup = next(item for item in result.rollups if item.metric_type.endswith("UTILIZATION"))
    assert len(rollup.points) == 2
    assert rollup.points[0].count == 9
    assert rollup.points[1].count == 3

    assert len(result.anomalies) == 1
    assert result.anomalies[0].metric_type.endswith("TEMPERATURE")
    assert result.anomalies[0].value == 2000

    assert len(result.correlations) == 1
    assert result.correlations[0].pearson_r == pytest.approx(1.0, abs=1e-9)
    assert result.regressions[0].slope == pytest.approx(2.0, rel=1e-6)
    assert result.regressions[0].intercept == pytest.approx(5.0, rel=1e-4)
    assert result.regressions[0].r_squared == pytest.approx(1.0)


def test_missing_pair_is_reported() -> None:
    result = run(request(pairs=[(X, "METRIC_TYPE_GPU_POWER@system")]))

    assert result.correlations == []
    assert any("pair not found" in warning for warning in result.warnings)


def test_shifted_timestamps_still_correlate() -> None:
    points: list[Point] = []
    for index in range(20):
        timestamp = index * 1_000_000_000
        points.append(
            Point(
                metric_type="CPU_UTILIZATION",
                device_id="system",
                timestamp_ns=timestamp,
                value=index + 1,
            )
        )
        points.append(
            Point(
                metric_type="RAM_USED",
                device_id="system",
                timestamp_ns=timestamp + 137_000_000,
                value=2 * (index + 1) + 5,
            )
        )

    result = run(
        AnalysisRequest(
            source="database",
            points=points,
            options=AnalysisOptions(
                bucket_seconds=1,
                min_points=8,
                pairs=[("CPU_UTILIZATION@system", "RAM_USED@system")],
            ),
        )
    )

    assert result.correlations[0].pearson_r == pytest.approx(1.0)
    assert result.correlations[0].n == 20
    assert result.regressions[0].slope == pytest.approx(2.0)
    assert result.regressions[0].intercept == pytest.approx(5.0)


def test_insufficient_overlap_is_reported() -> None:
    points = [
        Point(metric_type="A", device_id="system", timestamp_ns=1, value=1.0),
        Point(metric_type="B", device_id="system", timestamp_ns=1, value=2.0),
    ]

    result = run(
        AnalysisRequest(
            source="database",
            points=points,
            options=AnalysisOptions(min_points=8, pairs=[("A@system", "B@system")]),
        )
    )

    assert result.warnings
    assert "overlapping buckets" in result.warnings[0]


def test_empty_input_is_reported() -> None:
    result = run(AnalysisRequest(source="csv"))

    assert result.series == []
    assert result.warnings == ["no samples to analyse"]
