#!/bin/bash

mkdir -p build/release
mkdir -p build/debug

cd build/release
cmake -G"Ninja" ../../ -DCMAKE_BUILD_TYPE=Release -DBL_RPMALLOC_ENABLE=ON
ninja

#cd build/debug
#cmake -G"Ninja" ../../ -DCMAKE_BUILD_TYPE=Debug -DBL_RPMALLOC_ENABLE=ON
#ninja

cp compile_commands.json ../../compile_commands.json
