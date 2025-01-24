@echo off

set BL_DC_SRC_DIR=%BL_DEPS_DIR%\dyncall-1.2
set BL_DC_BUILD_DIR=%BL_BUILD_DIR%\dyncall

if not exist "%BL_DC_BUILD_DIR%" (
    mkdir "%BL_DC_BUILD_DIR%"
)

rem set BL_DC_INCLUDE_DIR=%BL_DEPS_DIR%\libyaml-0.2.5\include
set BL_DC_SRC_FILES=^
 "%BL_DC_SRC_DIR%\dyncall\dyncall_vector.c"^
 "%BL_DC_SRC_DIR%\dyncall\dyncall_struct.c"^
 "%BL_DC_SRC_DIR%\dyncall\dyncall_api.c"^
 "%BL_DC_SRC_DIR%\dyncall\dyncall_callvm.c"^
 "%BL_DC_SRC_DIR%\dyncall\dyncall_callvm_base.c"^
 "%BL_DC_SRC_DIR%\dyncall\dyncall_callf.c"^
 "%BL_DC_SRC_DIR%\dyncallback\dyncall_thunk.c"^
 "%BL_DC_SRC_DIR%\dyncallback\dyncall_alloc_wx.c"^
 "%BL_DC_SRC_DIR%\dyncallback\dyncall_args.c"^
 "%BL_DC_SRC_DIR%\dyncallback\dyncall_callback.c"^
 "%BL_DC_SRC_DIR%\dynload\dynload.c"^
 "%BL_DC_SRC_DIR%\dynload\dynload_syms.c"

set BL_DC_INCLUDE=/I"%BL_DC_SRC_DIR%\dyncall"

set BL_DC_FLAGS=^
 %BL_BASE_FLAGS%

cd %BL_DC_BUILD_DIR%
ml64 /nologo /c "%BL_DC_SRC_DIR%\dyncall\dyncall_call_x64_generic_masm.asm
ml64 /nologo /c "%BL_DC_SRC_DIR%\dyncallback\dyncall_callback_x64_masm.asm
"%BL_CL%" /nologo /c %BL_DC_SRC_FILES% /std:c11 /I"%INCLUDE%" %BL_DC_INCLUDE% %BL_DC_FLAGS%
cd %BL_BUILD_DIR%
lib /nologo /OUT:dyncall.lib "%BL_DC_BUILD_DIR%\*.obj"
cd %BL_ROOT_DIR%

