# Agent

Core maintainer: @FW-Nagorko

## Development Setup

Project is using CMake as build system and vcpkg in Manifest Mode for dependency management.

### 1. Initial Requirements

- **Linux** (Kernel 5.x.+)
- **C++20 Compiler** (GCC 13+/Clang 14+)
- **CMake** (3.16+)
- **Git**

### 2. Vcpkg Installation

```bash
# Clone vcpkg repository
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg

# Run bootstraping script
~/vcpkg/bootstrap-vcpkg.sh

# (Optional) Set VCPKG_ROOT environment variable in .bashrc (or other rc)
# Otherway you will need to enter the path for every single build.
export VCPKG_ROOT=~/vcpkg
```

### 3. Build Project

```bash
# Configurate the project, compile dependencies.
cmake -B build -S . --preset default

# Compilation
cmake --build build
```

### 4. Run Tests

Tests build by default (`BUILD_TESTING=ON`). After a successful build:

```bash
# Run the discovered GoogleTest suite via CTest
ctest --test-dir build --output-on-failure

# Or run the test binary directly
./build/volta_tests
```

To configure without tests: `cmake -B build -S . --preset default -DBUILD_TESTING=OFF`.

### 5. Run Agent

```bash
# May require root/admin privileges to access RAPL/Affinity features.
./build/voltad --config agent.conf
```

## Agent Configuration

`voltad` looks for configuration in this order:

1. `--config PATH`
2. `$VOLTA_CONFIG`
3. `/etc/volta/agent.conf` (optional; missing → built-in defaults)
4. built-in defaults

For local builds, pass the config explicitly:

```bash
./build/voltad --config agent.conf
```

You can use `sources/agent/agent.example.conf` as a starting point.

### Supported keys

- `core_affinity` — either `"all"` or an array of CPU indices/ranges
  (for example `[2, "3-10"]`).
- `interval` — collection interval in milliseconds (`uint32`).
- `server_address` — IP address or resolvable hostname.
- `server_port` — TCP port in range `1..65535`.
- `metrics` — a string or an array of strings.
  Metric names may be written with or without the `METRIC_TYPE_` prefix.
- `time_window` — buffering window in seconds (`float`), and it must be
  greater than the collection interval.

### Example

```toml
core_affinity = [2, "3-10"]
interval = 500
server_address = "localhost"
server_port = 5000
time_window = 2.0

metrics = [
  "METRIC_TYPE_CPU_POWER_PACKAGE",
  "GPU_POWER",
]
```

Unknown top-level keys are ignored with a warning.

### FAQ 
**Q: Do I have to enter `vcpkg install`?**
**A:** No, project is working in Manifest Mode. CMake automatically reads the `vcpkg.json` file and installs whats needed in isolated environment inside build directory.

**Q: I changed `vcpkg.json`, but build see no difference.**
**A:** Clean up CMake cache, and build project once again:
```bash
rm -rf build
cmake -B build -S .
```
