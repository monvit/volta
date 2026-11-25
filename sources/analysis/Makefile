UV ?= uv
CSV ?=

.PHONY: sync analyze serve test lint format

sync:
	$(UV) sync --frozen

analyze:
	$(UV) run volta-analyze $(CSV)

serve:
	$(UV) run uvicorn volta_analysis.api:app --host 0.0.0.0 --port 8000

test:
	$(UV) run pytest

lint:
	$(UV) run ruff check .
	$(UV) run ruff format --check .

format:
	$(UV) run ruff format .
