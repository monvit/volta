# Analiza metryk — dokumentacja

Moduł `volta_analysis` (`sources/analysis/`) to wspólny rdzeń analityczny Volty. Ten sam kod wykonuje analizę:

- **lokalnie** — CLI `volta-analyze` czyta plik zrzutu agenta (`dump_*.csv`) bezpośrednio z dysku, nie wymaga serwera ani bazy danych;
- **zdalnie** — sidecar HTTP (`POST /analyze`, `POST /report`) przyjmuje dane pobrane przez serwer Go z TimescaleDB i zwraca wynik JSON lub raport PDF dashboardowi.

Kontrakt wejścia/wyjścia jest zwykłym JSONem walidowanym przez Pydantic (`model.py`), identyczny w obu trybach.

## 1. Przepływ danych

```
agent (CSV dump / TimescaleDB)  →  analysis.run(request)  →  AnalysisResult
                                     ↑                      ↓
                              CLI lokalnie        sidecar HTTP / serwer Go → dashboard
```

Serwer Go (`sources/server/internal/httpserver/httpserver.go`):

1. pyta bazę o próbki agenta w zadanym przedziale czasu (`from`/`to`);
2. mapuje numeryczny `metric_type` z protobufa na nazwę enumu (np. `METRIC_TYPE_CPU_UTILIZATION`) i ustawia domyślne `device_id = "system"`;
3. wysyła żądanie `AnalysisRequest` z `source: "database"` do sidecaru Pythona (`http://localhost:8000/analyze` lub `/report`);
4. przekazuje odpowiedź (`json.RawMessage` lub PDF) dalej do dashboardu bez przepisywania schematu.

Dashboard (`sources/dashboard/src/features/analysis/analysis-panel.tsx`) wybiera dwie serie (`Series X`, `Series Y`), zakres czasu i parametry, a po kliknięciu **Run analysis** wywołuje `POST /api/agents/{id}/analyze`. Przycisk **Download PDF** wywołuje `POST /api/agents/{id}/report`.

## 2. Wejście — `AnalysisRequest`

| Pole | Typ | Domyślne | Znaczenie |
|------|-----|----------|-----------|
| `source` | `"csv"` \| `"database"` | wymagane | źródło danych |
| `points` | lista `Point` | `[]` | próbki: `metric_type`, `device_id`, `timestamp_ns`, `value` |
| `options` | `AnalysisOptions` | `AnalysisOptions()` | parametry analizy |

`AnalysisOptions`:

| Pole | Typ | Domyślne | Ograniczenie | Znaczenie |
|------|-----|----------|--------------|-----------|
| `bucket_seconds` | `int` | `60` | ≥ 1 | szerokość kubełka czasu w sekundach |
| `sigma` | `float` | `3.0` | > 0 | próg z-score dla wykrywania anomalii |
| `min_points` | `int` | `8` | ≥ 2 | minimalna liczba wspólnych kubełków do korelacji/regresji |
| `pairs` | lista `[x, y]` | `[]` | — | pary serii do korelacji i regresji, format `METRIC@DEVICE` |
| `sma_period` | `int` | `10` | ≥ 1 | liczba próbek w oknie SMA |
| `ema_period` | `int` | `10` | ≥ 1 | rozpiętość próbek dla EMA |

## 3. Wyjście — `AnalysisResult`

| Pole | Znaczenie |
|------|-----------|
| `series` | statystyki opisowe dla każdej serii |
| `rollups` | średnie, min/max i liczba próbek w kubełkach `bucket_seconds` |
| `moving_averages` | SMA i EMA dla każdej serii |
| `anomalies` | próbki odchylające się o więcej niż `sigma` odchyleń standardowych |
| `correlations` | korelacja Pearsona dla wybranych par |
| `regressions` | prosta regresja liniowa OLS dla wybranych par |
| `warnings` | ostrzeżenia (brak danych, za mało kubełków, seria stała itp.) |

## 4. Szczegóły poszczególnych analiz

### Statystyki serii (`series`)

Liczone dla każdej pary `metric_type@device_id` na wszystkich skończonych (nie-NaN, nie-±inf) próbkach:

| Miara | Znaczenie |
|-------|-----------|
| `count` | liczba skończonych próbek `n` |
| `first` / `last` | pierwsza i ostatnia wartość w kolejności czasu |
| `min` / `max` | minimum i maksimum |
| `mean` | średnia arytmetyczna |
| `median` | mediana |
| `stddev` | odchylenie standardowe **populacji** (ddof=0, nie próbkowe) |

### Kubełki czasu (`rollups`)

`bucket_seconds` dzieli oś czasu na przedziały o tej szerokości. Każda skończona próbka trafia do kubełka:

```
index = timestamp_ns // (bucket_seconds * 1_000_000_000)
```

Dla każdego niepustego kubełka liczona jest średnia (`mean`), liczba próbek (`count`), `min` i `max`.

Kubełki są podstawą korelacji i regresji, ponieważ collectory różnych urządzeń znaczniki czasu nadają niezależnie — porównywanie pojedynczych próbek 1:1 prawie nigdy nie znajduje wspólnych znaczników, a średnie z tych samych kubełków tak.

### Średnie kroczące: SMA i EMA (`moving_averages`)

Liczone **po próbkach**, nie po kubełkach. Okresy podawane w liczbie próbek (domyślnie 10).

- **SMA (Simple Moving Average)** — średnia arytmetyczna z ostatnich `sma_period` skończonych próbek. Pierwsze `sma_period − 1` wartości to `null` (okno nie jest jeszcze pełne).
- **EMA (Exponential Moving Average)** — `alpha = 2 / (ema_period + 1)`, startuje od pierwszej skończonej próbki, a następnie:

```
ema_t = alpha * value_t + (1 − alpha) * ema_{t−1}
```

Próbki niefinitywne (NaN, ±inf) są pomijane, ale ich znaczniki czasu nie zajmują miejsca w oknie.

### Anomalie (`anomalies`)

Dla każdej serii liczone są średnia i odchylenie standardowe populacji z całej serii. Próbka jest oznaczona jako anomalii, gdy:

```
z_score = |wartość − średnia| / stddev > sigma
```

Domyślny próg `sigma = 3.0`. W wyniku zwracany jest też `z_score` każdej anomalie oraz jej znacznik czasu i wartość.

**Granica matematyczna:** dla `n` próbek największe możliwe z-score wynosi √(n−1). Przy `n ≤ 10` i `sigma = 3.0` żadna anomia nie może zostać wykryta (z-score nigdy nie przekroczy 3).

### Korelacja Pearsona (`correlations`)

Dla wybranej pary serii `(x, y)`:

1. Pobierane są ich kubełki (`rollups`).
2. Łączone są dokładnie po tym samym `bucket_ns` (przecięcie kubełków obu serii).
3. Jeśli liczba wspólnych kubełków `< min_points` — brak korelacji i ostrzeżenie.
4. Jeśli któraś serie jest stała (stddev = 0) — korelacja niezdefiniowana, ostrzeżenie.
5. W przeciwnym razie liczony jest współczynnik korelacji Pearsona `r` z wartości bucket-mean obu serii.

Znaczenie `r` (od −1 do 1):

- `1` — doskonały liniowy związek dodatni,
- `0` — brak liniowego związku,
- `−1` — doskonały liniowy związek ujemny.

Pole `n` oznacza liczbę **wspólnych kubełków**, a nie liczbę surowych próbek.

### Regresja liniowa OLS (`regressions`)

Dla tej samej pary co korelacja, na tych samych wspólnych kubełkach, dopasowywana jest prosta metodą najmniejszych kwadratów:

```
y = slope * x + intercept
```

- `slope` — o ile zmienia się `y` na jednostkę zmiany `x`.
- `intercept` — wartość `y` dla `x = 0`.
- `r_squared` — współczynnik determinacji; w prostej regresji liniowej wynosi `r²`, czyli udział wariancji `y` wyjaśniony przez `x`.

## 5. Ostrzeżenia (`warnings`)

| Ostrzeżenie | Przyczyna |
|-------------|-----------|
| `no samples to analyse` | brak próbek w zapytaniu |
| `X: no finite values` | seria X zawiera same NaN lub ±inf |
| `pair not found: X / Y` | jedna z serii w parze nie występuje w danych |
| `only N overlapping buckets (min M)` | za mało wspólnych kubełków do korelacji/regresji |
| `X / Y: constant series, correlation undefined` | jedna z serii ma odchylenie standardowe 0 |

## 6. Przykład obliczeń

Zakładamy dane `sample_dump.csv` (12 próbek na serię, znaczniki co 1 s) i opcje:

```
bucket_seconds = 10, sigma = 3.0, min_points = 2
pairs = [(METRIC_TYPE_CPU_UTILIZATION@system, METRIC_TYPE_CPU_POWER_PACKAGE@system)]
```

- 12 próbek trafia do 2 kubełków 10-sekundowych (0–9 s i 10–11 s).
- `rollups` dla CPU Utilization: kubełek 0 → średnia 50, kubełek 10 s → średnia 110.
- `moving_averages` z `sma_period = 3`: pierwsze 2 wartości SMA to `null`, potem średnie z okien 3-probkowych.
- Korelacja obliczana na 2 wspólnych kubełkach: `r = 1.0`, `n = 2`.
- Regresja: `y = 2.0 × x + 5.0`, `R² = 1.0`.
- Temperatura ma wartość 2000 przy znaczniku 12 s → z-score ≈ 3.317 > 3 → **1 anomia**.

## 7. Uruchomienie

### Lokalnie (CLI)

```bash
cd sources/analysis
uv sync --frozen

# pełny wynik JSON
uv run volta-analyze dump.csv --json

# korelacja dwóch serii
uv run volta-analyze dump.csv --pair \
  METRIC_TYPE_CPU_UTILIZATION@system,METRIC_TYPE_CPU_POWER_PACKAGE@system

# niestandardowe okresy średnich i raport PDF
uv run volta-analyze dump.csv --sma-period 20 --ema-period 20 --pdf --output raport.pdf

# sidecar HTTP
uv run uvicorn volta_analysis.api:app --host 0.0.0.0 --port 8000
```

Plik CSV agenta ma nagłówek `metric_type;device_id;timestamp;value` (średnik mimo rozszerzenia `.csv`), znaczniki czasu w nanosekundach i nazwy enumów z `sources/proto/types/metric.proto`.

### Zdalnie (dashboard + serwer)

Wymagane: baza danych włączona i analiza aktywna (`analysis.enabled = true`, `analysis.url` wskazujący na sidecar).

Dashboard wysyła `POST /api/agents/{id}/analyze` z treścią:

```json
{
  "from": "2026-01-01T10:00:00Z",
  "to": "2026-01-01T11:00:00Z",
  "metric_types": [103, 104],
  "bucket_seconds": 60,
  "sigma": 3.0,
  "min_points": 8,
  "sma_period": 10,
  "ema_period": 10,
  "pairs": [
    ["METRIC_TYPE_CPU_UTILIZATION@system", "METRIC_TYPE_CPU_POWER_PACKAGE@system"]
  ]
}
```

Serwer Go pobiera próbki z bazy, wywołuje sidecar i zwraca wynik JSON. Raport PDF: `POST /api/agents/{id}/report` zwraca `application/pdf`.

## 8. Raport PDF (`POST /report`, `volta-analyze --pdf`)

Generowany przez `src/volta_analysis/report.py` w formacie wektorowym (ReportLab). Zawiera:

- źródło i czas wygenerowania,
- okresy SMA / EMA,
- sekcję statystyk serii (maksymalnie 50 pozycji),
- anomalie, korelacje, regresje,
- średnie kroczące,
- ostrzeżenia,
- wektorowe wykresy trendu dla każdej serii (średnia z bucketów, SMA, EMA; do 12 serii),
- wykresy punktowe korelacji z linią regresji (do 6 par).

W dashboardzie pobieralny przyciskiem **Download PDF**; w CLI przez `--pdf --output raport.pdf`.

## 9. Lokalna vs zdalna analiza — różnice

| Aspekt | Lokalnie (CLI) | Zdalnie (dashboard / serwer) |
|--------|----------------|------------------------------|
| Źródło danych | plik `dump_*.csv` agenta | TimescaleDB przez serwer Go |
| Wejście | `source: "csv"` | `source: "database"` |
| Wymagania | tylko Python + zależności | serwer Go, DB, sidecar analysis |
| Wynik | JSON / tekst / PDF w CLI | JSON / PDF przez REST |
| `device_id` | jak w CSV agenta | mapowane z bazy (może różnić się formatem, patrz niżej) |

## 10. Ważne ograniczenia i uwagi

1. **`min_points` odnosi się do kubełków, a nie surowych próbek.** Przy `bucket_seconds = 60` i oknie 5 minut masz tylko 5 kubełków, więc korelacja nie powstanie (wymaga ≥ 8). Okno musi obejmować co najmniej `min_points × bucket_seconds` sekund.
2. **Przy `n ≤ 10` i `sigma = 3.0` nie wykryje się żadnej anomalii** (maksymalne z-score to √(n−1) ≤ 3).
3. **Format `device_id` musi być spójny.** Dashboard generuje klucze w formacie bazy danych (`cpu:sX-cY`, `gpu:%04x:%02x:%02x.%x`, `net:ifindexN`, `disk:M:m`, `system`). Starszy format z CSV agenta (`cpu=sX/Y`, `gpu=...`, `net_ifindex=N`) nie będzie dopasowany do danych z bazy i da ostrzeżenie `pair not found`.
4. **Korelacja liczy się na średnich z bucketów**, a nie na identycznych znacznikach czasu. Dwa collectory z niezależnymi zegarami mogą nie mieć wspólnych timestampów, ale trafią do tych samych kubełków.
5. **Limit CSV:** `MAX_POINTS = 100 000` próbek na plik.
6. **Limit zapytania serwera:** `max_rows` (domyślnie 100 000) obcina pobrane próbki z bazy.

## 9. Testy i jakość kodu

```bash
cd sources/analysis
uv run pytest          # testy analizy, API, CLI, raport PDF
uv run ruff check .    # lint
uv run ruff format --check .  # format
```

Dashboard (TypeScript) ma własne kontakty typów i build: `pnpm` (lub lokalne `node_modules/.bin/vp`) w `sources/dashboard`.
