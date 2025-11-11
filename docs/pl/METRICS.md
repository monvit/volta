# Obsługiwane Metryki

## Metryki CPU

|Metryka|Jednostka|Komentarz|
|-|-|-|
| Pobór Mocy (Pakiet) | Waty (W) | Całkowita moc procesora. |
| Pobór Mocy (Rdzenie) | Waty (W) | Moc zużywana tylko przez rdzenie. |
| Prędkość Zegara | Megahertz (MHz) | Średnia lub na rdzeń. |
| Średnie Zużycie | Procent (%) | Całkowite oraz na rdzeń. |
| Temperatura | Stopnie C (°C) | Całkowita oraz na rdzeń. |
| Czas `iowait` | Procent (%) | Krytyczne dla dysków/sieci. Procent czasu, gdy CPU czekał na I/O. |
| Cache Hit/Miss | Liczba/Procent | Krytyczne dla HPC. Stosunek trafień do chybień w L1/L2/L3. |
| Aktywne Procesy | Liczba | Liczba procesów w stanie running lub runnable |

### Intel

- Moc: Intel RAPL (odczyt z `/sys/class/powercap`).
- Temperatura/Zegar: `coretemp` / `cpufreq` (odczyt z `/sys/class/hwmon` i `/sys/devices/system/cpu/...`).
- Zużycie/iowait: Obliczeniowe (na podstawie `/proc/stat`).
- Cache Hit/Miss: PMU (Performance Monitoring Units) - odczyt przez `perf_event_open` (Linux).

### AMD

- Moc: `zenpower` / `k10temp` (odczyt z `/sys/class/hwmon`).
- Temperatura/Zegar: `k10temp` / `cpufreq`.
- Zużycie/iowait: Obliczeniowe (na podstawie `/proc/stat`).
- Cache Hit/Miss: PMU (Performance Monitoring Units) - odczyt przez `perf_event_open` (Linux).

## Metryki GPU

### NVIDIA

| Metryka | Jednostka | Źródło (API) | Komentarz |
| - | - | - | - |
| Pobór Mocy | Waty (W) | NVML | - |
| Prędkość Zegara | Megahertz (MHz) | NVML | - |
| Średnie Zużycie | Procent (%) | NVML | - |
| Temperatura | Stopnie C (°C) | NVML | - |
| Zajętość vRAM | Bajty (B) | NVML | - |
| Użycie Magistrali PCIe | Bajty na sekundę (B/s) | NVML | - |
| Użycie Streaming Multiprocessors | Procent (%) | DCGM | - |
| Użycie Shared Memory | Procent (%) | DCGM | - |
| Użycie Rejestrów | Procent (%) | DCGM | - |

### AMD

| Metryka | Jednostka | Źródło (API) | Komentarz |
| - | - | - | - |
| Pobór Mocy | Waty (W) | ROCm-SMI | - |
| Prędkość Zegara | Megahertz (MHz) | ROCm-SMI | - |
| Średnie Zużycie | Procent (%) | ROCm-SMI | - |
| Temperatura | Stopnie C (°C) | ROCm-SMI | - |
| Zajętość vRAM | Bajty (B) | ROCm-SMI | - |
| Użycie Magistrali PCIe | Bajty na sekundę (B/s) | ROCm-SMI | - |
| Użycie Compute Unit | Procent (%) | ROCm-SMI | - |
| Użycie Shared Memory | Procent (%) | ROCm-SMI | - |
| Użycie Rejestrów | Procent (%) | ROCm-SMI | - |

### Intel

| Metryka | Jednostka | Źródło (API) | Komentarz |
| - | - | - | - |
| Pobór Mocy | Waty (W) | oneAPI Level Zero | - |
| Prędkość Zegara | Megahertz (MHz) | oneAPI Level Zero | - |
| Średnie Zużycie | Procent (%) | oneAPI Level Zero | - |
| Temperatura | Stopnie C (°C) | oneAPI Level Zero | - |
| Zajętość vRAM | Bajty (B) | oneAPI Level Zero | - |
| Użycie Magistrali PCIe | Bajty na sekundę (B/s) | oneAPI Level Zero | - |
| Użycie Execution Unit | Procent (%) | oneAPI Level Zero | - |
| Użycie Shared Memory | Procent (%) | oneAPI Level Zero | - |
| Użycie Rejestrów | Procent (%) | oneAPI Level Zero | - |

## Metryki RAM

| Metryka | Jednostka | Źródło (API) | Komentarz |
| - | - | - | - |
| Pobór Mocy | Waty (W) | Intel RAPL | Dostępne na serwerach na architekturze Intela. |
| Pamięć Całkowita | Bajty (B) | `/proc/meminfo` | - |
| Pamięć Dostępna | Bajty (B) | `/proc/meminfo` | - |
| Pamięć Używana | Bajty (B) | Obliczeniowa | - |
| Pamięć Cache | Bajty (B) | `/proc/meminfo` | - |
| Użycie Swap | Bajty (B) / % | `/proc/meminfo` | - |
| Aktywność Swap | Liczba/s | `/proc/vmstat` | - |

## Metryki Dysków

| Metryka | Jednostka | Źródło (API) | Komentarz |
| - | - | - | - |
| Odczyty | Bajty na sekundę (B/s) | `/proc/diskstats` | Przepustowość odczytu. |
| Zapisy | Bajty na sekundę (B/s) | `/proc/diskstats` | Przepustowość zapisu. |
| Operacje Odczytu | IOPS (Operacje/s) | `/proc/diskstats` | Liczba operacji odczytu na sekundę. |
| Operacje Zapisu | IOPS (Operacje/s) | `/proc/diskstats` | Liczba operacji zapisu na sekundę. |
| Czas Aktywności | Procent (%) | `/proc/diskstats` | Procentowy czas, gdy dysk był zajęty. |
| Pojemność (Użycie) | Bajty (B) / % | `statvfs()` | Użycie partycji / wolumenu. |

## Metryki Sieciowe

| Metryka | Jednostka | Źródło (API) | Komentarz |
| - | - | - | - |
| Dane Odebrane | Bity na sekundę (b/s) | `/proc/net/dev` | Przepustowość wejściowa interfejsu. |
| Dane Wysłane | Bity na sekundę (b/s) | `/proc/net/dev` | Przepustowość wyjściowa interfejsu. |
| Pakiety Odebrane | Pakiety na sekundę (pps) | `/proc/net/dev` | Liczba pakietów (wejście) na sekundę. |
| Pakiety Wysłane | Pakiety na sekundę (pps) | `/proc/net/dev` | Liczba pakietów (wyjście) na sekundę. |
