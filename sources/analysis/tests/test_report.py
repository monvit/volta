from __future__ import annotations

from io import BytesIO
from pathlib import Path

import pytest
from fastapi.testclient import TestClient
from pypdf import PdfReader

from volta_analysis.analysis import run
from volta_analysis.api import app
from volta_analysis.cli import main
from volta_analysis.model import AnalysisOptions, AnalysisRequest, AnalysisResult, Point
from volta_analysis.report import render_pdf


def read_pdf(data: bytes) -> list[str]:
    assert data.startswith(b"%PDF-")
    assert data.rstrip().endswith(b"%%EOF")
    return [page.extract_text() for page in PdfReader(BytesIO(data), strict=True).pages]


@pytest.fixture
def dump(tmp_path: Path) -> Path:
    path = tmp_path / "dump.csv"
    path.write_text(
        "metric_type;device_id;timestamp;value\n"
        "temperature;cpu;0;2\n"
        "temperature;cpu;1000000000;4\n"
        "temperature;cpu;2000000000;8\n"
        "temperature;cpu;3000000000;private-invalid-value\n",
        encoding="utf-8",
    )
    return path


def test_cli_pdf_file(dump: Path, tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    output = tmp_path / "report.pdf"
    assert (
        main(
            [str(dump), "--pdf", "--sma-period", "2", "--ema-period", "1", "--output", str(output)]
        )
        == 0
    )
    assert capsys.readouterr() == ("", "")
    text = "\n".join(read_pdf(output.read_bytes()))
    assert "Source: csv" in text
    assert "SMA period: 2 samples | EMA period: 1 samples" in text
    assert "SMA=6, EMA=8" in text
    assert "line 5: skipped malformed row" in text
    assert "private-invalid-value" not in text
    assert str(dump) not in text


def test_cli_text_includes_moving_averages(dump: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert main([str(dump), "--sma-period", "2", "--ema-period", "1"]) == 0
    assert "temperature@cpu: SMA=6 EMA=8" in capsys.readouterr().out


def test_cli_pdf_stdout(dump: Path, capsysbinary: pytest.CaptureFixture[bytes]) -> None:
    assert main([str(dump), "--pdf"]) == 0
    captured = capsysbinary.readouterr()
    assert captured.err == b""
    text = "\n".join(read_pdf(captured.out))
    assert "SMA period: 10 samples | EMA period: 10 samples" in text
    assert "SMA=n/a" in text


@pytest.mark.parametrize(
    "args", [["--pdf", "--json"], ["--sma-period", "0"], ["--ema-period", "0"]]
)
def test_cli_rejects_invalid_pdf_flags_or_periods(dump: Path, args: list[str]) -> None:
    with pytest.raises(SystemExit) as exc:
        main([str(dump), *args])
    assert exc.value.code == 2


def test_report_endpoint_uses_analysis_request() -> None:
    points = [
        {"metric_type": metric, "device_id": "cpu", "timestamp_ns": i * 10**9, "value": value}
        for metric, values in [("temperature", [2, 4, 8]), ("power", [4, 8, 16])]
        for i, value in enumerate(values)
    ]
    response = TestClient(app).post(
        "/report",
        json={
            "source": "database",
            "points": points,
            "options": {
                "sma_period": 2,
                "ema_period": 1,
                "bucket_seconds": 1,
                "sigma": 1.2,
                "min_points": 2,
                "pairs": [["temperature@cpu", "power@cpu"], ["missing", "power@cpu"]],
            },
        },
    )

    assert response.status_code == 200
    assert response.headers["content-type"] == "application/pdf"
    assert response.headers["cache-control"] == "no-store"
    text = "\n".join(read_pdf(response.content))
    assert "Source: database" in text
    assert "Series statistics (2)" in text
    assert "temperature@cpu: n=3 first=2 last=8" in text
    assert "Anomalies (2)" in text
    assert "timestamp_ns=2000000000 value=8" in text
    assert "Correlations (1)" in text
    assert "r=1.0000 n=3" in text
    assert "Regressions (1)" in text
    assert "slope=2" in text
    assert "Moving averages (2)" in text
    assert "SMA=6, EMA=8" in text
    assert "Warnings (1)" in text
    assert "pair not found: missing / power@cpu" in text


def test_report_endpoint_validates_request() -> None:
    response = TestClient(app).post(
        "/report", json={"source": "database", "options": {"ema_period": 0}}
    )
    assert response.status_code == 422
    assert response.json()["detail"][0]["loc"] == ["body", "options", "ema_period"]


def test_pdf_paginates_and_bounds_report_content() -> None:
    result = AnalysisResult(
        source="csv",
        warnings=[f"warning {i}: Café 中\n " + "x" * 1000 + " DO NOT RENDER" for i in range(200)],
    )
    pages = read_pdf(render_pdf(result, AnalysisOptions()))
    assert 1 < len(pages) < 10
    for number, page in enumerate(pages, 1):
        assert f"Page {number}" in page
    text = "\n".join(pages)
    assert r"Café \u4e2d\n" in text
    assert "..." in text
    assert "150 more omitted." in text
    assert "DO NOT RENDER" not in text
    assert "warning 199" not in text


def test_pdf_renders_trend_and_correlation_charts() -> None:
    points = [
        Point(metric_type=metric, device_id="cpu", timestamp_ns=i * 10**9, value=value)
        for metric, values in [
            ("temperature", [2, 4, 8, 6, 9, 3]),
            ("power", [4, 8, 16, 12, 18, 6]),
        ]
        for i, value in enumerate(values)
    ]
    options = AnalysisOptions(
        sma_period=2,
        ema_period=2,
        bucket_seconds=1,
        min_points=2,
        pairs=[("temperature@cpu", "power@cpu")],
    )
    pages = read_pdf(
        render_pdf(run(AnalysisRequest(source="csv", points=points, options=options)), options)
    )
    text = "\n".join(pages)
    assert "Charts" in text
    assert "Trend: temperature@cpu" in text
    assert "Trend: power@cpu" in text
    assert "Correlation: r=1.000 (n=6)" in text
    assert "fit R^2=1.000" in text


def test_pdf_paginates_many_charts() -> None:
    points = [
        Point(
            metric_type=f"metric{index}", device_id="cpu", timestamp_ns=i * 10**9, value=i + index
        )
        for index in range(30)
        for i in range(5)
    ]
    options = AnalysisOptions(bucket_seconds=1)
    pages = read_pdf(
        render_pdf(run(AnalysisRequest(source="csv", points=points, options=options)), options)
    )
    text = "\n".join(pages)
    assert len(pages) > 3
    assert text.count("Trend: metric") == 12
    assert "Warnings (0)" in text


def test_trend_chart_draws_bucket_mean_with_markers(dump: Path) -> None:
    from volta_analysis.metric_io import parse_csv
    from volta_analysis.report import _trend_charts

    points, _ = parse_csv(dump)
    options = AnalysisOptions(bucket_seconds=5)
    result = run(AnalysisRequest(source="csv", points=points, options=options))
    charts = _trend_charts(result)
    assert charts
    labels = [node.text for node in charts[0].contents if hasattr(node, "text")]
    assert "mean (bucket)" in labels
    lines = [node for node in charts[0].contents if type(node).__name__ == "PolyLine"]
    assert any(len(node.points) == 4 for node in lines)
