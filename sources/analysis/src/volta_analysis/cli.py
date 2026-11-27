"""CLI adapter: analyse an agent CSV dump locally."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from pydantic import ValidationError

from volta_analysis.analysis import run
from volta_analysis.metric_io import parse_csv
from volta_analysis.model import AnalysisOptions, AnalysisRequest, AnalysisResult, Point, series_key
from volta_analysis.report import render_pdf


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="volta-analyze",
        description="Analyse metric samples from an agent dump_*.csv file.",
    )
    parser.add_argument("csv", type=Path, help="path to the agent CSV dump")
    parser.add_argument(
        "--metric",
        action="append",
        default=[],
        metavar="NAME",
        help="only analyse this metric type (repeatable)",
    )
    parser.add_argument("--bucket-seconds", type=int, default=60)
    parser.add_argument("--sigma", type=float, default=3.0)
    parser.add_argument("--min-points", type=int, default=8)
    parser.add_argument("--sma-period", type=int, default=10, help="SMA window in samples")
    parser.add_argument("--ema-period", type=int, default=10, help="EMA span in samples")
    parser.add_argument(
        "--pair",
        action="append",
        default=[],
        metavar="SERIES,SERIES",
        help="correlate two series given as METRIC_TYPE@DEVICE (repeatable)",
    )
    output_format = parser.add_mutually_exclusive_group()
    output_format.add_argument("--json", action="store_true", help="emit the full result as JSON")
    output_format.add_argument("--pdf", action="store_true", help="emit a PDF report")
    parser.add_argument("--output", type=Path, help="write the output to a file instead of stdout")
    return parser


def parse_pair(value: str) -> tuple[str, str]:
    parts = value.split(",")
    if len(parts) != 2 or not all(part.strip() for part in parts):
        raise argparse.ArgumentTypeError("pair must be METRIC_TYPE@DEVICE,METRIC_TYPE@DEVICE")
    return parts[0].strip(), parts[1].strip()


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        pairs = [parse_pair(value) for value in args.pair]
        options = AnalysisOptions(
            bucket_seconds=args.bucket_seconds,
            sigma=args.sigma,
            min_points=args.min_points,
            pairs=pairs,
            sma_period=args.sma_period,
            ema_period=args.ema_period,
        )
    except (argparse.ArgumentTypeError, ValidationError) as exc:
        parser.error(str(exc))
        return 2

    try:
        points, warnings = parse_csv(args.csv)
    except (OSError, ValueError) as exc:
        print(f"volta-analyze: {exc}", file=sys.stderr)
        return 1

    selected: list[Point] = [
        point for point in points if not args.metric or point.metric_type in args.metric
    ]
    result = run(AnalysisRequest(source="csv", points=selected, options=options))
    if args.pdf:
        result.warnings.extend(
            warning.split(" (", 1)[0] if ": skipped malformed row (" in warning else warning
            for warning in warnings
        )
        pdf = render_pdf(result, options)
        if args.output:
            args.output.write_bytes(pdf)
        else:
            sys.stdout.buffer.write(pdf)
        return 0

    result.warnings.extend(warnings)
    text = result.model_dump_json(indent=2) if args.json else format_result(result)
    if args.output:
        args.output.write_text(text + "\n", encoding="utf-8")
    else:
        print(text)
    return 0


def format_result(result: AnalysisResult) -> str:
    lines = [f"source: {result.source}", "", "series:"]
    for stats in result.series:
        lines.append(
            f"  {series_key(stats.metric_type, stats.device_id)}: "
            f"n={stats.count} last={stats.last:.4g} min={stats.min:.4g} "
            f"max={stats.max:.4g} mean={stats.mean:.4g} stddev={stats.stddev:.4g}"
        )

    if result.moving_averages:
        lines.extend(["", "moving averages (latest):"])
        for item in result.moving_averages:
            if not item.points:
                continue
            latest = item.points[-1]
            sma = "n/a" if latest.sma is None else f"{latest.sma:.4g}"
            lines.append(
                f"  {series_key(item.metric_type, item.device_id)}: SMA={sma} EMA={latest.ema:.4g}"
            )

    if result.correlations:
        lines.extend(["", "correlations:"])
        for item in result.correlations:
            lines.append(f"  {item.x} ~ {item.y}: r={item.pearson_r:.4f} (n={item.n})")

    if result.regressions:
        lines.extend(["", "regressions:"])
        for item in result.regressions:
            lines.append(
                f"  {item.y} = {item.slope:.6g} * {item.x} + {item.intercept:.6g} "
                f"(R^2={item.r_squared:.4f})"
            )

    if result.anomalies:
        lines.extend(["", f"anomalies ({len(result.anomalies)}):"])
        for item in result.anomalies[:20]:
            lines.append(
                f"  {item.timestamp_ns} {series_key(item.metric_type, item.device_id)}: "
                f"{item.value:.4g} (z={item.z_score:.2f})"
            )

    if result.warnings:
        lines.extend(["", "warnings:"])
        lines.extend(f"  {warning}" for warning in result.warnings)
    return "\n".join(lines)
