"""Contracts shared by the CLI, the HTTP sidecar and the Go server."""

from __future__ import annotations

from datetime import UTC, datetime
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field

Source = Literal["csv", "database"]

MAX_POINTS = 100_000


def series_key(metric_type: str, device_id: str) -> str:
    """Identify a series independently of its transport (CSV names or DB names)."""
    return f"{metric_type}@{device_id}"


class Point(BaseModel):
    model_config = ConfigDict(frozen=True)

    metric_type: str
    device_id: str
    timestamp_ns: int
    value: float


class AnalysisOptions(BaseModel):
    bucket_seconds: int = Field(default=60, ge=1)
    sigma: float = Field(default=3.0, gt=0)
    min_points: int = Field(default=8, ge=2)
    pairs: list[tuple[str, str]] = Field(default_factory=list)
    sma_period: int = Field(default=10, ge=1, strict=True)
    ema_period: int = Field(default=10, ge=1, strict=True)


class AnalysisRequest(BaseModel):
    source: Source
    points: list[Point] = Field(default_factory=list, max_length=MAX_POINTS)
    options: AnalysisOptions = Field(default_factory=AnalysisOptions)


class SeriesStats(BaseModel):
    metric_type: str
    device_id: str
    count: int
    first: float
    last: float
    min: float
    max: float
    mean: float
    median: float
    stddev: float


class RollupPoint(BaseModel):
    bucket_ns: int
    count: int
    mean: float
    min: float
    max: float


class Rollup(BaseModel):
    metric_type: str
    device_id: str
    bucket_seconds: int
    points: list[RollupPoint]


class MovingAveragePoint(BaseModel):
    timestamp_ns: int
    sma: float | None
    ema: float


class MovingAverageSeries(BaseModel):
    metric_type: str
    device_id: str
    points: list[MovingAveragePoint]


class Anomaly(BaseModel):
    metric_type: str
    device_id: str
    timestamp_ns: int
    value: float
    z_score: float


class Correlation(BaseModel):
    x: str
    y: str
    pearson_r: float
    n: int


class Regression(BaseModel):
    x: str
    y: str
    slope: float
    intercept: float
    r_squared: float
    n: int


class AnalysisResult(BaseModel):
    source: Source
    generated_at: datetime = Field(default_factory=lambda: datetime.now(UTC))
    series: list[SeriesStats] = Field(default_factory=list)
    rollups: list[Rollup] = Field(default_factory=list)
    moving_averages: list[MovingAverageSeries] = Field(default_factory=list)
    anomalies: list[Anomaly] = Field(default_factory=list)
    correlations: list[Correlation] = Field(default_factory=list)
    regressions: list[Regression] = Field(default_factory=list)
    warnings: list[str] = Field(default_factory=list)
