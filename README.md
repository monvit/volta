# volta


## Development Setup

Project is using CMake as build system and vcpkg in Manifest Mode for dependency management.

### 1. Initial Requirements

- **Linux** (Kernel 5.x.+)
- **C++20 Compiler** (GCC 11+/Clang 14+)
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
# If VCPKG_ROOT was not set, point toolchain yourself using
# -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake -B build -S . 

# Compilation
cmake --build build
```

### 4. Run Agent

```bash
# May require root/admin privileges to access RAPL/Affinity features.
./build/source/agent/volta_agent
```

### FAQ 
**Q: Do I have to enter `vcpkg install`?**
**A:** No, project is working in Manifest Mode. CMake automatically reads the `vcpkg.json` file and installs whats needed in isolated environment inside build directory.

**Q: I changed `vcpkg.json`, but build see no difference.**
**A:** Clean up CMake cache, and build project once again:
```bash
rm -rf build
cmake -B build -S .
```
