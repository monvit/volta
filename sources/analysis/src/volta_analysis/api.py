"""HTTP adapter: the sidecar called by the Go server."""

from __future__ import annotations

from fastapi import FastAPI

from volta_analysis.analysis import run
from volta_analysis.model import AnalysisRequest, AnalysisResult

app = FastAPI(title="Volta Analysis", version="0.1.0")


@app.get("/healthz")
def healthz() -> dict[str, str]:
    return {"status": "ok"}


@app.post("/analyze", response_model=AnalysisResult)
def analyze(request: AnalysisRequest) -> AnalysisResult:
    return run(request)
