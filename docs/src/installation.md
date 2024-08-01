# Installation

Currently only installation from the source code is possible.

**Important:** The compiler and language are not production ready yet, you might miss some fundamental features. Also it's recommented to use `CMAKE_INSTALL_PREFIX` to set installation directory (e.g. `-DCMAKE_INSTALL_PREFIX=C:\Develop\bl`).

**Supported targets:**

* `x86_64-pc-windows-msvc`
* `x86_64-pc-linux-gnu`
* `x86_64-unknown-linux-gnu`
* `x86_64-apple-darwin`
* `arm64-apple-darwin` (experimental)

## Windows

* Install [git](https://git-scm.com)
* Install [CMake](https://cmake.org)
* Install Visual Studio 2022 or [MS Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools) with C/C++ support
* Download and compile

```bash
git clone https://github.com/travisdoor/bl.git
cd bl
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="install/directory"
cmake --build . --config Release --target install
```

* Compiler will be installed into `CMAKE_INSTALL_PREFIX` directory.
* Add `bin` directory to the system `PATH`.

**In Powershell:**
```
[Environment]::SetEnvironmentVariable(
   "Path",
   [Environment]::GetEnvironmentVariable("Path", "User") + ";install\directory\bin",
   "User"
)
```

## Ubuntu
* Install needed tools
```bash
apt-get install git cmake build-essential llvm-16-dev
```

* In case your distribution does not provide LLVM-16 you can alternatively use following script
```bash
mkdir llvm-16
cd llvm-16
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 16
```

* Download and compile

Note that in case the `CMAKE_INSTALL_PREFIX` is not specified, `/urs/local` will be used as default.

```bash
git clone https://github.com/travisdoor/bl.git
cd bl
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX="install/directory"
make install
```

* Add `bin` directory to the system `PATH`. Not needed in case install directory is `/usr/local`.

```bash
export PATH=$PATH:/install/directory/bin
```

## Fedora
* Install needed tools
```bash
dnf copr enable -y @fedora-llvm-team/llvm-snapshots
dnf install git cmake gcc g++ zlib-devel llvm16-devel
```

* Download and compile

Note that in case the `CMAKE_INSTALL_PREFIX` is not specified, `/urs/local` will be used as default.

```bash
git clone https://github.com/travisdoor/bl.git
cd bl
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX="install/directory"
make install
```

* Add `bin` directory to the system `PATH`. Not needed in case install directory is `/usr/local`.

```bash
export PATH=$PATH:/install/directory/bin
```

## macOS
* Install command line tools ``xcode-select --install``.
* Install other needed tools using [brew](https://brew.sh) `brew install git cmake llvm@16`.
* Download and compile

```bash
git clone https://github.com/travisdoor/bl.git
cd bl
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX="install/directory"
make install
```

* Add `bin` directory to the system `PATH`.

```bash
export PATH=$PATH:/install/directory/bin
```

!!! warning
    ARM support is experimental.


## Configuration

Default configuration file `install/directory/etc/bl.yaml` is created automatically. You can use `blc --where-is-config` to get full path to the default config file. To generate new one use `blc --configure` (the old one will be kept as a backup).

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
cd path/to/git/root/directory
blc -run doctor.bl
```