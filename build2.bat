@echo off

set BL_ROOT_DIR=%CD%

echo Run in "%BL_ROOT_DIR%".

for /f "delims=" %%i in ('where cl 2^>nul') do set BL_CL=%%i
if not defined BL_CL (
    echo CL.exe not found in PATH. Make sure the environment is set up correctly.
)

set BL_IS_DEBUG=0

if "%1"=="debug" (
	set BL_IS_DEBUG=1
) else if not "%1"=="" (
    echo Invalid argument: %1!
	exit
)

set BL_BIN_DIR=%BL_ROOT_DIR%\bin
set BL_BUILD_DIR=%BL_ROOT_DIR%\build2
set BL_DEPS_DIR=%BL_ROOT_DIR%\deps
set BL_SRC_DIR=%BL_ROOT_DIR%\src

set BL_LLVM_VER=18.1.8
set BL_TRACY_VER=0.9.1

set BL_VERSION_MAJOR=0
set BL_VERSION_MINOR=13
set BL_VERSION_PATCH=0
set BL_VERSION=%BL_VERSION_MAJOR%.%BL_VERSION_MINOR%.%BL_VERSION_PATCH%
set BL_LINKER=bl-lld.exe

if %BL_IS_DEBUG%==1 (
	echo Building in DEBUG mode.
	set BL_BUILD_DIR=%BL_BUILD_DIR%\debug
) else (
	set BL_BUILD_DIR=%BL_BUILD_DIR%\release
)

if not exist "%BL_BUILD_DIR%" (
    mkdir "%BL_BUILD_DIR%"
)

set BL_INCLUDE=^
 /I"%BL_DEPS_DIR%\dyncall-1.2\dyncall"^
 /I"%BL_DEPS_DIR%\dyncall-1.2\dynload"^
 /I"%BL_DEPS_DIR%\dyncall-1.2\dyncallback"^
 /I"%BL_DEPS_DIR%\libyaml-0.2.5\include"^
 /I"%BL_DEPS_DIR%\tracy-%BL_TRACY_VER%\public\tracy"^
 /I"%BL_DEPS_DIR%\rpmalloc-1.4.4\rpmalloc"^
 /I"%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64-2\include"

set BL_BASE_FLAGS=^
 /EHsc^
 /D_WIN32^
 /D_WINDOWS^
 /DNOMINMAX^
 /GF^
 /MD

if %BL_IS_DEBUG%==1 (
	set BL_BASE_FLAGS=%BL_BASE_FLAGS% /Od /Zi
) else (
	set BL_BASE_FLAGS=%BL_BASE_FLAGS% /O2 /Oi /DNDEBUG
)


set BL_FLAGS=^
 %BL_BASE_FLAGS%^
 /DBL_VERSION_MAJOR=%BL_VERSION_MAJOR%^
 /DBL_VERSION_MINOR=%BL_VERSION_MINOR%^
 /DBL_VERSION_PATCH=%BL_VERSION_PATCH%^
 /DBL_VERSION="\"%BL_VERSION%"\"^
 /DBL_LINKER="\"%BL_LINKER%"\"^
 /DYAML_DECLARE_STATIC^
 /D_CRT_SECURE_NO_WARNINGS^
 /D_CRT_NONSTDC_NO_DEPRECATE^
 /W3

if %BL_IS_DEBUG%==1 (
	set BL_FLAGS=%BL_FLAGS% /DBL_DEBUG /DBL_ASSERT_ENABLE=1
) else (
	set BL_FLAGS=%BL_FLAGS%
)

set BL_LINKER_FLAGS= -incremental:no -opt:ref -subsystem:console /NODEFAULTLIB:MSVCRTD.lib

set BL_LIBS=^
 llvm_api.obj^
 "%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64-2\lib\LLVM.lib"^
 "%BL_BUILD_DIR%\yaml.lib"^
 "%BL_BUILD_DIR%\dyncall.lib"^
 kernel32.lib^
 Shlwapi.lib^
 Ws2_32.lib^
 dbghelp.lib

if not exist "%BL_BUILD_DIR%\yaml.lib" (
	call "%BL_DEPS_DIR%\libyaml-0.2.5\build.bat
)

if not exist "%BL_BUILD_DIR%\dyncall.lib" (
	call "%BL_DEPS_DIR%\dyncall-1.2\build.bat
)

cd %BL_BUILD_DIR%
"%BL_CL%" /nologo /c %BL_SRC_DIR%\llvm_api.cpp -std:c++17 /I"%INCLUDE%" %BL_INCLUDE% %BL_FLAGS% /Follvm_api.obj
"%BL_CL%" /nologo    %BL_SRC_DIR%\main.c       -std:c11   /I"%INCLUDE%" %BL_INCLUDE% %BL_FLAGS% /link %BL_LIBS% %BL_LINKER_FLAGS% /OUT:blc.exe
cd %BL_ROOT_DIR%


if not exist "%BL_BIN_DIR%" (
    mkdir "%BL_BIN_DIR%"
)

echo Copying results into "%BL_BIN_DIR%".
xcopy /Y "%BL_BUILD_DIR%\blc.exe" "%BL_BIN_DIR%\" >nul 2>&1
xcopy /Y "%BL_DEPS_DIR%\vswhere-2.8.4\vswhere.exe" "%BL_BIN_DIR%\" >nul 2>&1
xcopy /Y "%BL_DEPS_DIR%\lld.exe" "%BL_BIN_DIR%\" >nul 2>&1
rename "%BL_BIN_DIR%\lld.exe" "bl-lld.exe" >nul 2>&1
if %BL_IS_DEBUG%==1 (
	xcopy /Y "%BL_BUILD_DIR%\blc.pdb" "%BL_BIN_DIR%\" >nul 2>&1
)

echo DONE!