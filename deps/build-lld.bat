@echo off
mkdir build-lld
cd build-lld

cmake -S ..\llvm -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DLLVM_ENABLE_PROJECTS=lld ^
  -DLLVM_TARGETS_TO_BUILD="X86;AArch64" ^
  -DLLVM_ENABLE_ASSERTIONS=OFF ^
  -DLLVM_ENABLE_DIA_SDK=OFF ^
  -DLLVM_INCLUDE_TESTS=OFF ^
  -DLLVM_INCLUDE_BENCHMARKS=OFF ^
  -DCMAKE_INSTALL_PREFIX=llvm-23-release-lld

ninja install
