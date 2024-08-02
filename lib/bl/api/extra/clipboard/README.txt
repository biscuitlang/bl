BL Wrapper for libclipboard 1.1

Compile module:

- Unzip libclipboard-1.1.zip
- cd libclipboard-1.1
- mkdir build
- cd build
- cmake .. -DCMAKE_BUILD_TYPE=Release
- cmake --build . --config=Release
- copy the resulting libraries into module-dir/your-target
- update module.yaml configuration file to link libraries for your target