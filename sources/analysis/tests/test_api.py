from __future__ import annotations

import json
from pathlib import Path

from fastapi.testclient import TestClient

from volta_analysis.analysis import run
from volta_analysis.api import app
from volta_analysis.cli import main
from volta_analysis.metric_io import parse_csv
from volta_analysis.model import AnalysisOptions, AnalysisRequest, series_key

FIXTURE = Path(__file__).parent / "fixtures" / "sample_dump.csv"
client = TestClient(app)


def test_healthz() -> None:
    assert client.get("/healthz").json() == {"status": "ok"}


def test_analyze_accepts_database_rows() -> None:
    points, _ = parse_csv(FIXTURE)
    rows = [
        {
            "metric_type": point.metric_type,
            "device_id": point.device_id,
            "timestamp_ns": point.timestamp_ns,
            "value": point.value,
        }
        for point in points
    ]
    pair = [
        series_key("METRIC_TYPE_CPU_UTILIZATION", "system"),
        series_key("METRIC_TYPE_CPU_POWER_PACKAGE", "system"),
    ]
    payload = {
        "source": "database",
        "points": rows,
        "options": {"bucket_seconds": 10, "sigma": 2.5, "min_points": 2, "pairs": [pair]},
    }

    response = client.post("/analyze", json=payload)

    assert response.status_code == 200
    body = response.json()
    assert body["source"] == "database"
    assert len(body["series"]) == 3
    assert body["correlations"][0]["pearson_r"] == 1.0


def test_cli_and_core_produce_same_result(tmp_path: Path) -> None:
    output = tmp_path / "result.json"
    pair = [
        series_key("METRIC_TYPE_CPU_UTILIZATION", "system"),
        series_key("METRIC_TYPE_CPU_POWER_PACKAGE", "system"),
    ]

    exit_code = main(
        [
            str(FIXTURE),
            "--bucket-seconds",
            "10",
            "--sigma",
            "2.5",
            "--min-points",
            "2",
            "--pair",
            ",".join(pair),
            "--json",
            "--output",
            str(output),
        ]
    )

    assert exit_code == 0
    cli_result = json.loads(output.read_text(encoding="utf-8"))

    points, warnings = parse_csv(FIXTURE)
    expected = run(
        AnalysisRequest(
            source="csv",
            points=points,
            options=AnalysisOptions(
                bucket_seconds=10, sigma=2.5, min_points=2, pairs=[tuple(pair)]
            ),
        )
    )
    expected.warnings.extend(warnings)

    assert cli_result["series"] == json.loads(expected.model_dump_json())["series"]
    assert cli_result["correlations"] == json.loads(expected.model_dump_json())["correlations"]
    assert cli_result["warnings"] == expected.warnings
