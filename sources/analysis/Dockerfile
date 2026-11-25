FROM python:3.13-slim

COPY --from=ghcr.io/astral-sh/uv:0.13.9 /uv /uvx /bin/

WORKDIR /app
ENV UV_COMPILE_BYTECODE=1 \
    UV_LINK_MODE=copy \
    PYTHONUNBUFFERED=1 \
    PATH="/app/.venv/bin:$PATH"

COPY pyproject.toml uv.lock README.md ./
COPY src ./src

RUN uv sync --frozen --no-dev --no-editable \
    && useradd --create-home --uid 10001 volta \
    && chown -R volta:volta /app

USER volta

EXPOSE 8000

HEALTHCHECK --interval=10s --timeout=3s --retries=5 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/healthz', timeout=2)"

CMD ["uvicorn", "volta_analysis.api:app", "--host", "0.0.0.0", "--port", "8000"]
