# Use Pre-built Package

* Download required compiler version from [Github](https://github.com/travisdoor/bl/releases).
* Unpack downloaded file.
* Optionally add `/path/to/blc/bin` to your system `PATH`.
* Run `blc --help`.

# Build from Source Code

Biscuit compiler is written in C and all major dependencies are packed in the compiler repository except [LLVM](https://llvm.org/). [CMake](https://cmake.org) is used as a build system.

## Supported targets

* `x86_64-pc-windows-msvc`
* `x86_64-pc-linux-gnu`
* `x86_64-unknown-linux-gnu`
* `x86_64-apple-darwin`
* `arm64-apple-darwin` (experimental)

## Windows

* Install Visual Studio 2022 or [MS Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools) with C/C++ support
* Download and compile

```bash
git clone -b release/0.11.0 https://github.com/travisdoor/bl.git
cd bl
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release"
cmake --build . --config Release
```

* You can add `bin` directory to the system `PATH`.

**In Powershell:**
```
[Environment]::SetEnvironmentVariable(
   "Path",
   [Environment]::GetEnvironmentVariable("Path", "User") + ";path\to\bl\bin",
   "User"
)
```

## Linux

* Install LLVM

This step might differ across linux distributions, following snippet might help. You might want to use  `-DLLVM_DIR` pointing to the custom location with LLVM.

```bash
# Ubuntu
apt-get install llvm-16-dev

# Fedora
dnf copr enable -y @fedora-llvm-team/llvm-snapshots
dnf install llvm16-devel

# Using LLVM installation script
mkdir llvm-16
cd llvm-16
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 16
```

* Download and compile


```bash
git clone -b release/0.11.0 https://github.com/travisdoor/bl.git
cd bl
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config=Release
```

* You can add `bin` directory to the system `PATH`.

```bash
export PATH=$PATH:/path/to/bl/bin
```

## macOS
* Install command line tools ``xcode-select --install``.
* Install LLVM using [brew](https://brew.sh) `brew install llvm@16` or you might want to use  `-DLLVM_DIR` pointing to the custom location with LLVM.
* Download and compile

```bash
git clone -b release/0.11.0 https://github.com/travisdoor/bl.git
cd bl
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release"
cmake --build . --config=Release
```

* You can add `bin` directory to the system `PATH`.

```bash
export PATH=$PATH:/path/to/bl/bin
```

!!! warning
    ARM support is experimental.


## Additional Setup

Following flags might be passed to CMake during configuration:

- `-DCMAKE_BUILD_TYPE=<Release|Debug>` - To toggle release/debug configuration.
- `-DCMAKE_INSTALL_PREFIX=<"path">` - To set installation directory.
- `-DTRACY_ENABLE=<ON|OFF>` - To toggle [Tracy profiler](https://github.com/wolfpld/tracy) integration.
- `-DLLVM_DIR=<"path">` - To set custom path to LLVM dev package. Must point to `llvm-directory/lib/cmake/llvm`.
- `-DBL_X64_TESTS_ENABLE=<ON|OFF>` - To toggle compilation of tests for experimental x64 backend.
- `-DBL_DEVELOPER=<ON|OFF>` - To toggle some incomplete experimental features (for example x64 backend).
- `-DBL_ASSERT_ENABLE=<ON|OFF>` - To toggle asserts (by default disabled in release mode).
- `-DBL_SIMD_ENABLE=<ON|OFF>` - To toggle SIMD. *Windows only*
- `-DBL_RPMALLOC_ENABLE=<ON|OFF>` - To toggle [rpmalloc](https://github.com/mjansson/rpmalloc).

## Configuration

The compiler requires configuration file to be generated before the first use.

Default configuration file `/path/to/bl/etc/bl.yaml` is created automatically on the first run. You can use `blc --where-is-config` to get full path to the default config file. To generate new one use `blc --configure` (the old one will be kept as a backup).

**Example Windows config file:**

```yaml
# Automatically generated configuration file used by 'blc' compiler.
# To generate new one use 'blc --configure' command.

# Compiler version, this should match the executable version 'blc --version'.
version: "0.11.0"

# Main API directory containing all modules and source files. This option is mandatory.
lib_dir: "C:/Develop/bl/lib/bl/api"

# Current default environment configuration.
x86_64-pc-windows-msvc:
    # Platform operating system preload file (relative to 'lib_dir').
    preload_file: "os/_windows.bl"
    # Optional path to the linker executable, 'lld' linker is used by default on some platforms.
    linker_executable: ""
    # Linker flags and options used to produce executable binaries.
    linker_opt_exec: "/NOLOGO /ENTRY:__os_start /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /MACHINE:x64"
    # Linker flags and options used to produce shared libraries.
    linker_opt_shared: "/NOLOGO /INCREMENTAL:NO /MACHINE:x64 /DLL"
    # File system location where linker should lookup for dependencies.
    linker_lib_path: "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22000.0/ucrt/x64;C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22000.0/um/x64;C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.32.31326//lib/x64"
```

## Unit Tests

To run compiler unit tests use:
```
cd path/to/bl/directory
blc -run doctor.bl
```