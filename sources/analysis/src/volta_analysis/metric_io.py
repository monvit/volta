"""Input adapters: the agent's CSV dump and rows fetched from TimescaleDB."""

from __future__ import annotations

import csv
from pathlib import Path
from typing import Any

from volta_analysis.model import MAX_POINTS, Point

CSV_HEADER = {"metric_type", "device_id", "timestamp", "value"}


def parse_csv(path: Path | str, *, max_rows: int = MAX_POINTS) -> tuple[list[Point], list[str]]:
    """Read a `dump_*.csv` produced by the agent (semicolon separated, nanosecond timestamps)."""
    if max_rows < 1:
        raise ValueError("max_rows must be positive")

    warnings: list[str] = []
    points: list[Point] = []
    with open(path, newline="", encoding="utf-8-sig") as handle:
        header_line = handle.readline()
        delimiter = ";" if header_line.count(";") >= header_line.count(",") else ","
        handle.seek(0)

        reader = csv.DictReader(handle, delimiter=delimiter)
        fields = set(reader.fieldnames or ())
        if fields != CSV_HEADER:
            raise ValueError(
                f"unexpected CSV header: {sorted(fields)}; expected {sorted(CSV_HEADER)}"
            )

        for line_no, row in enumerate(reader, start=2):
            try:
                points.append(
                    Point(
                        metric_type=(row["metric_type"] or "").strip(),
                        device_id=(row["device_id"] or "").strip() or "system",
                        timestamp_ns=int(row["timestamp"]),
                        value=float(row["value"]),
                    )
                )
            except (TypeError, ValueError) as exc:
                warnings.append(f"line {line_no}: skipped malformed row ({exc})")
                continue

            if len(points) >= max_rows:
                warnings.append(f"stopped after max_rows={max_rows} samples")
                break

    if not points:
        warnings.append("no valid samples found")
    return points, warnings


def points_from_rows(rows: list[dict[str, Any]]) -> list[Point]:
    """Normalise rows sent by the Go server (metric_type already mapped to its enum name)."""
    return [
        Point(
            metric_type=str(row["metric_type"]),
            device_id=str(row.get("device_id") or "system"),
            timestamp_ns=int(row["timestamp_ns"]),
            value=float(row["value"]),
        )
        for row in rows
    ]
