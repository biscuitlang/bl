# Installation

* Use Pre-built Package
* Download compiler from [Github](https://github.com/biscuitlang/bl/releases/tag/0.13.0).
* Unpack downloaded file.
* Optionally add /path/to/blc/bin to your system PATH.
* Run blc --help.

# Nightly Build

* You might use a nightly release build (created daily from the current master branch) [Github](https://github.com/biscuitlang/bl/actions/workflows/nightly.yml).
* Note that the nightly versions might be unstable.
* Make sure you're using [master](https://biscuitlang.org/versions/master/) documentation.

# Build from Source Code

Biscuit compiler is using [nob](https://github.com/tsoding/nob.h) "build system". Since `nob` build system is written in C, it needs to be compiled first in order to compile the compiler itself. There is helper script to do so called `build.bat` or `build.sh`.

## Supported targets

* `x86_64-pc-windows-msvc`
* `x86_64-pc-linux-gnu`
* `x86_64-unknown-linux-gnu`
* `x86_64-apple-darwin` (deprecated)
* `arm64-apple-darwin`

## Windows

* Install Visual Studio 2022 or [MS Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools) with C/C++ support
* Run `vcvars64.bat` in your shell or use Visual Studio Developer Command Prompt.
* Download and compile

```bash
git clone https://github.com/biscuitlang/bl.git
cd bl
build.bat
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

This step might differ across linux distributions, following snippet might help.

```bash
# Ubuntu
apt-get install llvm-18-dev

# Fedora
dnf copr enable -y @fedora-llvm-team/llvm-snapshots
dnf install llvm18-devel

# Using LLVM installation script
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 18
```

* Download and compile


```bash
git clone https://github.com/biscuitlang/bl.git
cd bl
./build.sh
```

* You can add `bin` directory to the system `PATH`.

```bash
export PATH=$PATH:/path/to/bl/bin
```

## macOS
* Install command line tools ``xcode-select --install``.
* Install dependencies using [brew](https://brew.sh) `brew install llvm@18 zlib ncurses`.
* Download and compile

```bash
git clone https://github.com/biscuitlang/bl.git
cd bl
./build.sh
```

* You can add `bin` directory to the system `PATH`.

```bash
export PATH=$PATH:/path/to/bl/bin
```


## Additional Setup

To show additional options pass `help` to the `build` script.

```
build help
Usage:
        build [options]

Options:
        all        Build everything and run unit tests.
        assert     Build bl compiler in release mode with asserts enabled.
        build-all  Build everything.
        clean      Remove build directory and exit.
        debug      Build bl compiler in debug mode.
        docs       Build bl documentation.
        help       Print this help and exit.
        release    Build bl compiler in release mode (default).
        runtime    Build compiler runtime.
        test       Run tests.
```

Other internal options might be adjusted directly in `nob.c` file.

## Configuration

The compiler requires configuration file to be generated before the first use. Default configuration file `/path/to/bl/etc/bl.yaml` is created automatically on the first run. 

You can use `blc --where-is-config` to get full path to the default config file. 

To generate new one use `blc --configure` (the old one will be kept as a backup).

!!! warning
	New configuration file should be generated after changes done to the working environment (things like system update, VS update etc.). It's recommended to regenerate the config after compiler updates.

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
# Invoke tests directly.
blc -run doctor.bl
# Use build script
build test
```
