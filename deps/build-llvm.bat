@echo off
mkdir build
cd build
cmake ..\llvm -G"Ninja" -DCMAKE_INSTALL_PREFIX="llvm-18-release" -DLLVM_PARALLEL_LINK_JOBS=4 -DLLVM_TARGETS_TO_BUILD="X86;AArch64" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ZLIB=OFF -DCMAKE_C_COMPILER=CL -DCMAKE_CXX_COMPILER=CL
ninja install
cd llvm-18-release

set LIBS=^
 "lib\LLVMCore.lib"^
 "lib\LLVMSupport.lib"^
 "lib\LLVMX86CodeGen.lib"^
 "lib\LLVMX86AsmParser.lib"^
 "lib\LLVMX86Desc.lib"^
 "lib\LLVMX86Disassembler.lib"^
 "lib\LLVMX86Info.lib"^
 "lib\LLVMAArch64CodeGen.lib"^
 "lib\LLVMAArch64AsmParser.lib"^
 "lib\LLVMAArch64Desc.lib"^
 "lib\LLVMAArch64Disassembler.lib"^
 "lib\LLVMAArch64Info.lib"^
 "lib\LLVMAArch64Utils.lib"^
 "lib\LLVMPasses.lib"^
 "lib\LLVMAsmPrinter.lib"^
 "lib\LLVMGlobalISel.lib"^
 "lib\LLVMSelectionDAG.lib"^
 "lib\LLVMMCDisassembler.lib"^
 "lib\LLVMCFGuard.lib"^
 "lib\LLVMCodeGen.lib"^
 "lib\LLVMCodeGenTypes.lib"^
 "lib\LLVMIRPrinter.lib"^
 "lib\LLVMTarget.lib"^
 "lib\LLVMCoroutines.lib"^
 "lib\LLVMHipStdPar.lib"^
 "lib\LLVMipo.lib"^
 "lib\LLVMInstrumentation.lib"^
 "lib\LLVMBitWriter.lib"^
 "lib\LLVMFrontendOpenMP.lib"^
 "lib\LLVMScalarOpts.lib"^
 "lib\LLVMAggressiveInstCombine.lib"^
 "lib\LLVMFrontendOffloading.lib"^
 "lib\LLVMLinker.lib"^
 "lib\LLVMInstCombine.lib"^
 "lib\LLVMObjCARCOpts.lib"^
 "lib\LLVMVectorize.lib"^
 "lib\LLVMTransformUtils.lib"^
 "lib\LLVMAnalysis.lib"^
 "lib\LLVMProfileData.lib"^
 "lib\LLVMSymbolize.lib"^
 "lib\LLVMDebugInfoDWARF.lib"^
 "lib\LLVMDebugInfoPDB.lib"^
 "lib\LLVMDebugInfoMSF.lib"^
 "lib\LLVMDebugInfoBTF.lib"^
 "lib\LLVMObject.lib"^
 "lib\LLVMMCParser.lib"^
 "lib\LLVMMC.lib"^
 "lib\LLVMDebugInfoCodeView.lib"^
 "lib\LLVMIRReader.lib"^
 "lib\LLVMBitReader.lib"^
 "lib\LLVMAsmParser.lib"^
 "lib\LLVMRemarks.lib"^
 "lib\LLVMBitstreamReader.lib"^
 "lib\LLVMTextAPI.lib"^
 "lib\LLVMBinaryFormat.lib"^
 "lib\LLVMTargetParser.lib"^
 "lib\LLVMDemangle.lib"

echo Packing into single static lib: %LIBS%
lib /nologo /OUT:lib\LLVM.lib %LIBS%
