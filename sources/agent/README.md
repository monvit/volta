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

### 5. Run Agent Locally

```bash
./build/voltad --config agent.conf
```

A local daemon uses your `$XDG_RUNTIME_DIR` for its control socket. RAPL
collection reads Linux powercap sysfs counters and normally needs no elevated
privileges; see the system-service notes below if your kernel restricts those
read-only files.

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
- `log_level` — minimum log level: `trace` (or `noisy`), `debug`, `info`,
  `warning` (or `warn`), or `error`. Defaults to `info`.

### Example

```toml
core_affinity = [2, "3-10"]
interval = 500
server_address = "localhost"
server_port = 5000
time_window = 2.0
log_level = "info"

metrics = [
  "METRIC_TYPE_CPU_POWER_PACKAGE",
  "GPU_POWER",
]
```

Unknown top-level keys are ignored with a warning.

## Install and Run as a systemd Service

The install uses FHS paths: binaries in `bin`, the example configuration in
`share/volta`, and `volta-agent.service` in `lib/systemd/system`. The service
runs with a private, dynamically allocated, non-root systemd UID. Its runtime
socket is `/run/volta/control.sock`; writable state is under `/var/lib/volta`.

Configure the install prefix before building so the generated unit points to
the installed binary (for example `/usr/bin/voltad`):

```bash
cmake -S . -B build --preset default -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
sudo systemd-sysusers                 # creates the volta user/group from sysusers.d
sudo systemctl daemon-reload
sudo /usr/bin/volta config init       # creates /etc/volta/agent.conf, no overwrite
sudo editor /etc/volta/agent.conf    # set your collector server as needed
sudo systemctl enable --now volta-agent
```

The agent reads config in this order: `--config PATH`, `$VOLTA_CONFIG`,
`/etc/volta/agent.conf`, then built-in defaults. `VOLTA_CONFIG` also overrides
the system config for the service when set in its environment. `volta config
init [PATH]` serializes the built-in defaults to TOML and refuses to replace
an existing file. `cmake` exposes `VOLTA_CONFIG_PATH` to select a different
compiled-in default path.

The service deliberately has no capabilities and does not run as root. It
only reads the RAPL `energy_uj` counters under `/sys/class/powercap`, which are
world-readable on standard Linux powercap setups. If your distribution changes
those attributes to root-only, grant read access to the powercap counters via
your platform's udev policy rather than running the whole agent as root or
adding broad DAC-bypass capabilities.

### Service smoke test

```bash
systemctl is-active --quiet volta-agent
systemctl status volta-agent --no-pager
journalctl -u volta-agent -n 30 --no-pager

# Exercise the service's Unix datagram control socket (root can access it).
sudo /usr/bin/volta export start
sleep 2
sudo /usr/bin/volta export stop
```

Control commands use `/run/volta/control.sock` for the system service. Set
`VOLTA_CONTROL_SOCKET=/absolute/socket/path` to override it. The service
socket is mode `0660`; root can control it by default.

### FAQ 
**Q: Do I have to enter `vcpkg install`?**
**A:** No, project is working in Manifest Mode. CMake automatically reads the `vcpkg.json` file and installs whats needed in isolated environment inside build directory.

**Q: I changed `vcpkg.json`, but build see no difference.**
**A:** Clean up CMake cache, and build project once again:
```bash
rm -rf build
cmake -B build -S .
```
