# Analysis

Wspólny rdzeń analizy metryk dla Volty. Ten sam kod wykonuje:

- **CLI** — lokalna analiza pliku `dump_*.csv` zapisanego przez agenta,
- **sidecar HTTP** — analiza danych pobranych z TimescaleDB przez serwer Go na żądanie dashboardu.

## Struktura

| Plik | Rola |
|---|---|
| `src/volta_analysis/model.py` | Kontrakt wejścia/wyjścia (Pydantic), współdzielony przez CLI i API |
| `src/volta_analysis/metric_io.py` | Adaptery wejścia: CSV agenta i wiersze z bazy |
| `src/volta_analysis/analysis.py` | Czyste funkcje analityczne, bez I/O |
| `src/volta_analysis/cli.py` | `volta-analyze` (argparse) |
| `src/volta_analysis/api.py` | FastAPI: `POST /analyze`, `GET /healthz` |

## Uruchomienie

```bash
uv sync --frozen
uv run volta-analyze ../agent/build/dump_2026-....csv --json
uv run volta-analyze dump.csv --pair \
  METRIC_TYPE_CPU_UTILIZATION@system,METRIC_TYPE_CPU_POWER_PACKAGE@system
uv run uvicorn volta_analysis.api:app --host 0.0.0.0 --port 8000
```

CSV agenta ma nagłówek `metric_type;device_id;timestamp;value` (`;` mimo rozszerzenia
`.csv`), znaczniki czasu w nanosekundach i nazwy enumów z `sources/proto/types/metric.proto`.
Serwer Go mapuje numeryczny `metric_type` z bazy na te same nazwy przed wysłaniem do sidecaru.

## Wynik

`AnalysisResult` zawiera `series` (count/first/last/min/max/mean/median/stddev), `rollups`
(agregaty w oknach `bucket_seconds`), `anomalies` (odchylenie > `sigma` odchyleń standardowych),
`correlations` (Pearson), `regressions` (OLS `y = a*x + b` oraz R²) i `warnings`.
Korelacje i regresje liczone są na średnich z bucketów (`bucket_seconds`) dwóch serii,
a nie na identycznych znacznikach czasu — collector agency znaczniki nadają niezależnie.
`n` oznacza liczbę wspólnych bucketów i musi wynosić co najmniej `min_points`, więc okno
analizy powinno obejmować co najmniej `min_points` bucketów. Statystyki używają odchylenia
standardowego populacji (ddof=0), zgodnie z `dashboard/src/lib/stats.ts`.

Kontrakt jest zwykłym JSON-em walidowanym przez Pydantic. Serwer Go przekazuje odpowiedź
sidecaru do dashboardu bez przepisywania schematu (`json.RawMessage`).

## Testy i lint

```bash
uv run pytest
uv run ruff check .
uv run ruff format --check .
```
