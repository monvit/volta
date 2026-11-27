"""HTTP adapter: the sidecar called by the Go server."""

from __future__ import annotations

from fastapi import FastAPI, Response

from volta_analysis.analysis import run
from volta_analysis.model import AnalysisRequest, AnalysisResult
from volta_analysis.report import render_pdf

app = FastAPI(title="Volta Analysis", version="0.1.0")


@app.get("/healthz")
def healthz() -> dict[str, str]:
    return {"status": "ok"}


@app.post("/analyze", response_model=AnalysisResult)
def analyze(request: AnalysisRequest) -> AnalysisResult:
    return run(request)


@app.post(
    "/report",
    response_class=Response,
    responses={
        200: {"content": {"application/pdf": {"schema": {"type": "string", "format": "binary"}}}}
    },
)
def report(request: AnalysisRequest) -> Response:
    return Response(
        content=render_pdf(run(request), request.options),
        media_type="application/pdf",
        headers={
            "Cache-Control": "no-store",
            "Content-Disposition": 'attachment; filename="volta-report.pdf"',
        },
    )
