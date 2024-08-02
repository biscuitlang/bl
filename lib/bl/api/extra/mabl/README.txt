BL Wrapper for miniaudio 0.11.9

Compile module:

- mkdir build
- cd build
- cmake .. -DCMAKE_BUILD_TYPE=Release
- cmake --build . --config=Release
- copy the resulting libraries into module-dir/your-target
- update module.yaml configuration file to link libraries for your target