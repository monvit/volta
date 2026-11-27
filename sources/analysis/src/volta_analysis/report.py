from __future__ import annotations

from collections.abc import Callable, Sequence
from dataclasses import dataclass
from io import BytesIO
from textwrap import wrap

from reportlab.graphics import renderPDF
from reportlab.graphics.shapes import Circle, Drawing, Line, PolyLine, Rect, String
from reportlab.lib import colors
from reportlab.lib.pagesizes import letter
from reportlab.pdfgen.canvas import Canvas

from volta_analysis.analysis import align
from volta_analysis.model import AnalysisOptions, AnalysisResult, MovingAverageSeries, series_key

_MAX_SECTION_ROWS = 50
_MAX_FIELD_LENGTH = 140
_MARGIN = 42
_LINE_HEIGHT = 14
_CHART_WIDTH = letter[0] - 2 * _MARGIN
_CHART_HEIGHT = 170
_CHART_LEFT = 56
_CHART_TOP = 36
_CHART_BOTTOM = 40
_LEGEND_WIDTH = 150
_MAX_CHART_POINTS = 400
_MAX_TREND_CHARTS = 12
_MAX_CORRELATION_CHARTS = 6
_MEAN_COLOR = colors.HexColor("#2563eb")
_SMA_COLOR = colors.HexColor("#16a34a")
_EMA_COLOR = colors.HexColor("#ea580c")
_SCATTER_COLOR = colors.HexColor("#6b7280")
_FIT_COLOR = colors.HexColor("#dc2626")
_NANOS_PER_MINUTE = 60 * 10**9


@dataclass(frozen=True)
class _Trace:
    name: str
    points: list[tuple[float, float]]
    color: colors.Color
    scatter: bool = False
    width: float = 1.0
    runs: list[list[tuple[float, float]]] | None = None
    markers: list[tuple[float, float]] | None = None


def _label(value: str) -> str:
    text = "".join(
        char if char.isprintable() else ascii(char)[1:-1] for char in value[:_MAX_FIELD_LENGTH]
    )
    text = text.encode("cp1252", errors="backslashreplace").decode("cp1252")
    if len(value) > _MAX_FIELD_LENGTH or len(text) > _MAX_FIELD_LENGTH:
        return text[: _MAX_FIELD_LENGTH - 3] + "..."
    return text


def _clip(text: str, limit: int) -> str:
    return text if len(text) <= limit else text[: limit - 3] + "..."


def _series_label(metric_type: str, device_id: str) -> str:
    return series_key(_label(metric_type), _label(device_id))


def _moving_summary(series: MovingAverageSeries) -> str:
    name = _series_label(series.metric_type, series.device_id)
    if not series.points:
        return f"{name}: no values"
    latest = series.points[-1]
    sma = "n/a" if latest.sma is None else f"{latest.sma:.6g}"
    return (
        f"{name}: {len(series.points)} samples; latest timestamp_ns="
        f"{_label(str(latest.timestamp_ns))}, SMA={sma}, EMA={latest.ema:.6g}"
    )


def _decimate(points: list[tuple[float, float]], limit: int) -> list[tuple[float, float]]:
    if len(points) <= limit:
        return points
    step = (len(points) - 1) / (limit - 1)
    return [points[round(index * step)] for index in range(limit)]


def _bounds(values: list[float]) -> tuple[float, float]:
    low, high = min(values), max(values)
    if low == high:
        padding = abs(low) * 0.1 or 1.0
        return low - padding, high + padding
    return low, high


def _tick(value: float) -> str:
    return f"{value:.4g}"


def _minutes(timestamp_ns: int, origin_ns: int) -> float:
    return (timestamp_ns - origin_ns) / _NANOS_PER_MINUTE


def _chart(title: str, traces: list[_Trace], x_label: str, y_label: str) -> Drawing | None:
    traces = [trace for trace in traces if trace.points]
    if not traces:
        return None

    xs = [x for trace in traces for x, _ in trace.points]
    ys = [y for trace in traces for _, y in trace.points]
    x_low, x_high = _bounds(xs)
    y_low, y_high = _bounds(ys)
    plot_width = _CHART_WIDTH - _CHART_LEFT - _LEGEND_WIDTH
    plot_height = _CHART_HEIGHT - _CHART_TOP - _CHART_BOTTOM

    def to_x(x: float) -> float:
        return _CHART_LEFT + (x - x_low) / (x_high - x_low) * plot_width

    def to_y(y: float) -> float:
        return _CHART_BOTTOM + (y - y_low) / (y_high - y_low) * plot_height

    drawing = Drawing(_CHART_WIDTH, _CHART_HEIGHT)
    drawing.add(
        String(0, _CHART_HEIGHT - 10, _clip(title, 100), fontName="Helvetica-Bold", fontSize=9)
    )
    for fraction in (0, 0.25, 0.5, 0.75, 1):
        y = _CHART_BOTTOM + fraction * plot_height
        x = _CHART_LEFT + fraction * plot_width
        drawing.add(
            Line(
                _CHART_LEFT,
                y,
                _CHART_LEFT + plot_width,
                y,
                strokeColor=colors.lightgrey,
                strokeWidth=0.25,
            )
        )
        drawing.add(
            String(
                _CHART_LEFT - 4,
                y - 3,
                _tick(y_low + fraction * (y_high - y_low)),
                textAnchor="end",
                fontSize=7,
            )
        )
        drawing.add(
            String(
                x,
                _CHART_BOTTOM - 11,
                _tick(x_low + fraction * (x_high - x_low)),
                textAnchor="middle",
                fontSize=7,
            )
        )
    drawing.add(
        Rect(
            _CHART_LEFT,
            _CHART_BOTTOM,
            plot_width,
            plot_height,
            strokeColor=colors.grey,
            strokeWidth=0.5,
            fillColor=None,
        )
    )
    drawing.add(
        String(
            _CHART_LEFT + plot_width / 2,
            6,
            _clip(x_label, 60),
            textAnchor="middle",
            fontSize=7,
        )
    )
    drawing.add(
        String(_CHART_LEFT, _CHART_BOTTOM + plot_height + 4, _clip(y_label, 60), fontSize=7)
    )

    for trace in traces:
        if trace.scatter:
            for x, y in _decimate(trace.points, _MAX_CHART_POINTS):
                drawing.add(Circle(to_x(x), to_y(y), 1.6, fillColor=trace.color, strokeWidth=0))
            continue
        for run in trace.runs or [trace.points]:
            if len(run) < 2:
                continue
            coords = [
                coord for x, y in _decimate(run, _MAX_CHART_POINTS) for coord in (to_x(x), to_y(y))
            ]
            drawing.add(
                PolyLine(coords, strokeColor=trace.color, strokeWidth=trace.width, fillColor=None)
            )
        for x, y in trace.markers or []:
            drawing.add(
                Circle(
                    to_x(x),
                    to_y(y),
                    2.2,
                    fillColor=trace.color,
                    strokeColor=colors.white,
                    strokeWidth=0.5,
                )
            )

    legend_x = _CHART_LEFT + plot_width + 12
    for index, trace in enumerate(traces):
        y = _CHART_BOTTOM + plot_height - 8 - index * 12
        if trace.scatter:
            drawing.add(Circle(legend_x + 4, y + 2.5, 2.5, fillColor=trace.color, strokeWidth=0))
        else:
            drawing.add(
                Line(
                    legend_x,
                    y + 2.5,
                    legend_x + 10,
                    y + 2.5,
                    strokeColor=trace.color,
                    strokeWidth=max(trace.width, 1.5),
                )
            )
        drawing.add(String(legend_x + 16, y, _clip(trace.name, 22), fontSize=7))
    return drawing


def _trend_charts(result: AnalysisResult) -> list[Drawing]:
    rollups = {series_key(item.metric_type, item.device_id): item for item in result.rollups}
    averages = {
        series_key(item.metric_type, item.device_id): item for item in result.moving_averages
    }
    drawings: list[Drawing] = []
    for stats in result.series[:_MAX_TREND_CHARTS]:
        key = series_key(stats.metric_type, stats.device_id)
        rollup = rollups.get(key)
        average = averages.get(key)
        bucket_ns = rollup.bucket_seconds * 10**9 if rollup else 0
        means = [(point.bucket_ns, point.mean) for point in rollup.points] if rollup else []
        smas = (
            [(point.timestamp_ns, point.sma) for point in average.points if point.sma is not None]
            if average
            else []
        )
        emas = [(point.timestamp_ns, point.ema) for point in average.points] if average else []
        if not means and not emas:
            continue
        origin = min(timestamp for timestamp, _ in means + emas)
        mean_runs = [
            [(_minutes(start, origin), value), (_minutes(start + bucket_ns, origin), value)]
            for start, value in means
        ]
        mean_markers = [(_minutes(start + bucket_ns / 2, origin), value) for start, value in means]
        chart = _chart(
            f"Trend: {_series_label(stats.metric_type, stats.device_id)}",
            [
                _Trace(
                    "SMA",
                    [(_minutes(t, origin), value) for t, value in smas],
                    _SMA_COLOR,
                    width=1.2,
                ),
                _Trace(
                    "EMA",
                    [(_minutes(t, origin), value) for t, value in emas],
                    _EMA_COLOR,
                    width=1.2,
                ),
                _Trace(
                    "mean (bucket)",
                    [point for run in mean_runs for point in run],
                    _MEAN_COLOR,
                    width=2.5,
                    runs=mean_runs,
                    markers=mean_markers,
                ),
            ],
            "minutes from first sample",
            "value",
        )
        if chart is not None:
            drawings.append(chart)
    return drawings


def _correlation_charts(result: AnalysisResult) -> list[Drawing]:
    rollups = {series_key(item.metric_type, item.device_id): item for item in result.rollups}
    fits = {(item.x, item.y): item for item in result.regressions}
    drawings: list[Drawing] = []
    for item in result.correlations[:_MAX_CORRELATION_CHARTS]:
        left = rollups.get(item.x)
        right = rollups.get(item.y)
        if left is None or right is None:
            continue
        xs, ys = align(left, right)
        if xs.size == 0:
            continue
        traces = [
            _Trace(
                "buckets",
                list(zip(xs.tolist(), ys.tolist(), strict=True)),
                _SCATTER_COLOR,
                scatter=True,
            )
        ]
        fit = fits.get((item.x, item.y))
        if fit is not None:
            x_low = float(xs.min())
            x_high = float(xs.max())
            traces.append(
                _Trace(
                    f"fit R^2={fit.r_squared:.3f}",
                    [
                        (x_low, fit.slope * x_low + fit.intercept),
                        (x_high, fit.slope * x_high + fit.intercept),
                    ],
                    _FIT_COLOR,
                )
            )
        chart = _chart(
            f"Correlation: r={item.pearson_r:.3f} (n={item.n})",
            traces,
            _label(item.x),
            _label(item.y),
        )
        if chart is not None:
            drawings.append(chart)
    return drawings


class _Report:
    def __init__(self, output: BytesIO) -> None:
        self.canvas = Canvas(output, pagesize=letter, pageCompression=1)
        self.canvas.setTitle("Volta analysis report")
        self.width, self.height = letter
        self.y = 0.0
        self._header()

    def _header(self) -> None:
        self.canvas.setFont("Helvetica-Bold", 18)
        self.canvas.drawString(_MARGIN, self.height - _MARGIN, "Volta analysis report")
        self.canvas.setFont("Helvetica", 9)
        self.canvas.drawRightString(self.width - _MARGIN, 24, f"Page {self.canvas.getPageNumber()}")
        self.y = self.height - _MARGIN - 28

    def _ensure_points(self, height: float) -> None:
        if self.y - height < _MARGIN:
            self.canvas.showPage()
            self._header()

    def row(self, text: str) -> None:
        lines = wrap(text, width=int((self.width - 2 * _MARGIN) / 6), break_on_hyphens=False)
        self._ensure_points((len(lines) - 1) * _LINE_HEIGHT)
        self.canvas.setFont("Courier", 10)
        for line in lines:
            self.canvas.drawString(_MARGIN, self.y, line)
            self.y -= _LINE_HEIGHT

    def heading(self, title: str) -> None:
        self.y -= _LINE_HEIGHT
        self._ensure_points(7 * _LINE_HEIGHT)
        self.canvas.setFont("Helvetica-Bold", 12)
        self.canvas.drawString(_MARGIN, self.y, title)
        self.y -= _LINE_HEIGHT

    def section[T](self, title: str, items: Sequence[T], render: Callable[[T], str]) -> None:
        self.heading(f"{title} ({len(items)})")
        for item in items[:_MAX_SECTION_ROWS]:
            self.row(render(item))
        if not items:
            self.row("None")
        elif len(items) > _MAX_SECTION_ROWS:
            self.row(f"{len(items) - _MAX_SECTION_ROWS} more omitted.")

    def chart(self, drawing: Drawing) -> None:
        self._ensure_points(drawing.height)
        renderPDF.draw(drawing, self.canvas, _MARGIN, self.y - drawing.height)
        self.y -= drawing.height + _LINE_HEIGHT


def render_pdf(result: AnalysisResult, options: AnalysisOptions) -> bytes:
    output = BytesIO()
    report = _Report(output)
    report.row(f"Source: {result.source} | Generated: {result.generated_at.isoformat()}")
    report.row(
        f"SMA period: {_label(str(options.sma_period))} samples | "
        f"EMA period: {_label(str(options.ema_period))} samples"
    )
    report.section(
        "Series statistics",
        result.series,
        lambda item: (
            f"{_series_label(item.metric_type, item.device_id)}: n={item.count} "
            f"first={item.first:.6g} last={item.last:.6g} min={item.min:.6g} "
            f"max={item.max:.6g} mean={item.mean:.6g} median={item.median:.6g} "
            f"stddev={item.stddev:.6g}"
        ),
    )
    report.section(
        "Anomalies",
        result.anomalies,
        lambda item: (
            f"{_series_label(item.metric_type, item.device_id)}: "
            f"timestamp_ns={_label(str(item.timestamp_ns))} "
            f"value={item.value:.6g} z={item.z_score:.4g}"
        ),
    )
    report.section(
        "Correlations",
        result.correlations,
        lambda item: f"{_label(item.x)} vs {_label(item.y)}: r={item.pearson_r:.4f} n={item.n}",
    )
    report.section(
        "Regressions",
        result.regressions,
        lambda item: (
            f"y={_label(item.y)} vs x={_label(item.x)}: "
            f"slope={item.slope:.6g} intercept={item.intercept:.6g} "
            f"R^2={item.r_squared:.4f} n={item.n}"
        ),
    )
    report.section("Moving averages", result.moving_averages, _moving_summary)
    report.heading("Charts")
    charts = _trend_charts(result) + _correlation_charts(result)
    if not charts:
        report.row("None")
    for chart in charts:
        report.chart(chart)
    report.section("Warnings", result.warnings, _label)
    report.canvas.save()
    return output.getvalue()
