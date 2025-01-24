@echo off
for /f "delims=" %%i in ('where cl 2^>nul') do set CL_PATH=%%i
if not defined CL_PATH (
    echo CL.exe not found in PATH. Make sure the environment is set up correctly.
)

set BL_DEPS_DIR=deps
set BL_LLVM_VER=18.1.8
set BL_TRACY_VER=0.9.1

set BL_INCLUDE= ^
	/I%BL_DEPS_DIR%\dyncall-1.2\dyncall ^
	/I%BL_DEPS_DIR%\dyncall-1.2\dynload ^
	/I%BL_DEPS_DIR%\dyncall-1.2\dyncallback ^
	/I%BL_DEPS_DIR%\libyaml-0.2.5\include ^
	/I%BL_DEPS_DIR%\tracy-%BL_TRACY_VER%\public\tracy ^
	/I%BL_DEPS_DIR%\rpmalloc-1.4.4\rpmalloc ^
	/I%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\include

set BL_FLAGS= ^
	/EHsc ^
	/DWIN32 ^
	/D_WINDOWS ^
	/DNOMINMAX ^
	/DYAML_DECLARE_STATIC ^
	/GF ^
	/EHsc ^
	/MD

set BL_LINKER_FLAGS= -incremental:no -opt:ref -subsystem:console /LTCG

set BL_LIBS=^
	llvm_api.obj ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMCore.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMSupport.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMX86CodeGen.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMX86AsmParser.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMX86Desc.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMX86Disassembler.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMX86Info.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAArch64CodeGen.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAArch64AsmParser.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAArch64Desc.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAArch64Disassembler.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAArch64Info.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAArch64Utils.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMPasses.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAsmPrinter.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMGlobalISel.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMSelectionDAG.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMMCDisassembler.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMCFGuard.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMCodeGen.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMCodeGenTypes.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMIRPrinter.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMTarget.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMCoroutines.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMHipStdPar.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMipo.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMInstrumentation.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMBitWriter.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMFrontendOpenMP.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMScalarOpts.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAggressiveInstCombine.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMFrontendOffloading.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMLinker.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMInstCombine.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMObjCARCOpts.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMVectorize.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMTransformUtils.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAnalysis.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMProfileData.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMSymbolize.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMDebugInfoDWARF.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMDebugInfoPDB.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMDebugInfoMSF.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMDebugInfoBTF.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMObject.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMMCParser.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMMC.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMDebugInfoCodeView.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMIRReader.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMBitReader.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMAsmParser.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMRemarks.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMBitstreamReader.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMTextAPI.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMBinaryFormat.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMTargetParser.lib ^
	%BL_DEPS_DIR%\llvm-%BL_LLVM_VER%-win64\release\lib\LLVMDemangle.lib ^
	build\release\%BL_DEPS_DIR%\dyncall-1.2\dyncall\dyncall_s.lib ^
	build\release\%BL_DEPS_DIR%\dyncall-1.2\dynload\dynload_s.lib ^
	build\release\%BL_DEPS_DIR%\dyncall-1.2\dyncallback\dyncallback_s.lib ^
	build\release\%BL_DEPS_DIR%\libyaml-0.2.5\yaml.lib ^
	kernel32.lib ^
	Shlwapi.lib ^
	Ws2_32.lib ^
	dbghelp.lib


"%CL_PATH%" /nologo /c src\llvm_api.cpp -std:c++17 /I"%INCLUDE%" %BL_INCLUDE% %BL_FLAGS% /Follvm_api.obj
"%CL_PATH%" /nologo src\main.c          -std:c11   /I"%INCLUDE%" %BL_INCLUDE% %BL_FLAGS% /link %BL_LIBS% %BL_LINKER_FLAGS% /OUT:blc.exe