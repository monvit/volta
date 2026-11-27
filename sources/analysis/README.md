# Analysis

Wspólny rdzeń analizy metryk dla Volty. Ten sam kod wykonuje:

- **CLI** — lokalna analiza pliku `dump_*.csv` zapisanego przez agenta,
- **sidecar HTTP** — analiza danych pobranych z TimescaleDB przez serwer Go na żądanie dashboardu.

Pełne opisanie obliczeń, parametrów, ostrzeżeń i granic matematycznych znajduje się w `docs/pl/ANALYSIS.md`.

## Struktura

| Plik | Rola |
|------|------|
| `src/volta_analysis/model.py` | Kontrakt wejścia/wyjścia (Pydantic), współdzielony przez CLI i API |
| `src/volta_analysis/metric_io.py` | Adaptery wejścia: CSV agenta i wiersze z bazy |
| `src/volta_analysis/analysis.py` | Czyste funkcje analityczne, bez I/O |
| `src/volta_analysis/cli.py` | `volta-analyze` (argparse) |
| `src/volta_analysis/api.py` | FastAPI: `POST /analyze`, `POST /report`, `GET /healthz` |
| `src/volta_analysis/report.py` | Generowanie raportu PDF (wykresy wektorowe) |

## Uruchomienie

```bash
uv sync --frozen
uv run volta-analyze ../agent/build/dump_2026-....csv --json
uv run volta-analyze dump.csv --pair \
  METRIC_TYPE_CPU_UTILIZATION@system,METRIC_TYPE_CPU_POWER_PACKAGE@system
uv run volta-analyze dump.csv --sma-period 20 --ema-period 20 --pdf --output raport.pdf
uv run uvicorn volta_analysis.api:app --host 0.0.0.0 --port 8000
```

CSV agenta ma nagłówek `metric_type;device_id;timestamp;value` (`;` mimo rozszerzenia `.csv`), znaczniki czasu w nanosekundach i nazwy enumów z `sources/proto/types/metric.proto`. Serwer Go mapuje numeryczny `metric_type` z bazy na te same nazwy przed wysłaniem do sidecaru.

## Wynik

`AnalysisResult` zawiera `series` (statystyki opisowe), `rollups` (agregaty w kubełkach `bucket_seconds`), `moving_averages` (SMA i EMA), `anomalies` (odchylenie > `sigma`), `correlations` (Pearson), `regressions` (OLS `y = a*x + b` oraz R²) i `warnings`.

SMA i EMA są liczone osobno dla każdej serii, w kolejności czasu, z pominięciem próbek niefinitywnych. Okresy `sma_period` i `ema_period` oznaczają liczbę próbek i domyślnie wynoszą 10. SMA jest `null` do zebrania pełnego okna; EMA zaczyna się od pierwszej próbki.

Korelacje i regresje liczone są na średnich z bucketów (`bucket_seconds`) dwóch serii, a nie na identycznych znacznikach czasu. `n` oznacza liczbę wspólnych bucketów i musi wynosić co najmniej `min_points`. Statystyki używają odchylenia standardowego populacji (ddof=0).

Raport PDF zawiera podsumowanie, maksymalnie 50 pozycji na sekcję oraz wektorowe wykresy trendu każdej séries (średnia z bucketów, SMA, EMA; do 12 serii) i wykresy punktowe korelacji z linią regresji (do 6 par).

## Testy i lint

```bash
uv run pytest
uv run ruff check .
uv run ruff format --check .
```
