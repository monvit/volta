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

A local daemon uses your `$XDG_RUNTIME_DIR` for its control socket. Since
Linux 5.10, only root can read the RAPL `energy_uj` counters, so a daemon run
as your user collects no CPU package power and logs no error. The systemd
service below grants the capability it needs.

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

The install uses FHS paths: binaries in `bin`, the
example configuration in `share/volta`, `volta-agent.service` in
`lib/systemd/system`, and the `volta` user definition in `lib/sysusers.d`.
The service runs as the unprivileged `volta` user. Its control socket is
`/run/volta/control.sock`, and its state (`agent.uuid` and export dumps) is
in `/var/lib/volta`.

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

`volta config init [PATH]` writes the built-in defaults as TOML and refuses
to replace an existing file. To point the service at another file, set
`VOLTA_CONFIG` with `systemctl edit volta-agent`. The file must be outside
`/home`, because the unit hides it. To compile in a different default path,
configure with `-DVOLTA_CONFIG_PATH=/path/to/agent.conf`.

The service does not run as root. It holds one capability,
`CAP_DAC_READ_SEARCH`, because since Linux 5.10 (CVE-2020-8694) only root can
read `energy_uj` under `/sys/class/powercap`. The capability lets the agent
read any file, but grants no write access. The agent can write only to
`/run/volta` and `/var/lib/volta`, and of the device nodes NVML can open
only `/dev/nvidia*`.

### Service smoke test

```bash
systemctl is-active --quiet volta-agent
systemctl status volta-agent --no-pager
journalctl -u volta-agent -n 30 --no-pager

# Members of the volta group can use the control socket.
sudo usermod -aG volta "$USER"   # log out and back in for the group to apply
volta export start
sleep 2
volta export stop
ls /var/lib/volta/dumps
```

The CLI uses `/run/volta/control.sock` whenever `/run/volta` exists. Set
`VOLTA_CONTROL_SOCKET=/absolute/socket/path` to use a different socket. The
socket is mode `0660` in a `0750` directory, both owned by `volta:volta`, so
root and members of `volta` can control the service. The service writes
dumps to `/var/lib/volta/dumps` and can't write an export anywhere else.

### FAQ

**Q: Do I have to enter `vcpkg install`?**
**A:** No, project is working in Manifest Mode. CMake automatically reads the `vcpkg.json` file and installs whats needed in isolated environment inside build directory.

**Q: I changed `vcpkg.json`, but build see no difference.**
**A:** Clean up CMake cache, and build project once again:

```bash
rm -rf build
cmake -B build -S .
```
