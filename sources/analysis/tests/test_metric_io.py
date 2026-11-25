from __future__ import annotations

from pathlib import Path

import pytest

from volta_analysis.metric_io import parse_csv, points_from_rows

FIXTURE = Path(__file__).parent / "fixtures" / "sample_dump.csv"


def test_parse_csv_reads_agent_format() -> None:
    points, warnings = parse_csv(FIXTURE)

    assert len(points) == 36
    assert points[0].metric_type == "METRIC_TYPE_CPU_UTILIZATION"
    assert points[0].device_id == "system"
    assert points[0].timestamp_ns == 1_000_000_000
    assert len(warnings) == 1
    assert "line 38" in warnings[0]


def test_parse_csv_rejects_unexpected_header(tmp_path: Path) -> None:
    bad = tmp_path / "bad.csv"
    bad.write_text("time,value\n1,2\n", encoding="utf-8")

    with pytest.raises(ValueError, match="unexpected CSV header"):
        parse_csv(bad)


def test_points_from_rows_normalises_device_id() -> None:
    rows = [
        {"metric_type": "METRIC_TYPE_GPU_POWER", "device_id": None, "timestamp_ns": 5, "value": 42}
    ]

    points = points_from_rows(rows)

    assert points[0].device_id == "system"
    assert points[0].value == 42
