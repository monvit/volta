from __future__ import annotations

from typing import Protocol, Iterable

from volta_analytics.core.models import AnalysisQuery, Metric


class MetricSource(Protocol):
    def read(self, query: AnalysisQuery) -> Iterable[Metric]:
        """Yield samples matching the query."""
