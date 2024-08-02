BL Wrapper for glfw 3.3.2

Note: 

The module is currently linked statically on Windows (binary is included). On other platforms we
rely on glfw3 installed on the system. This should be unified in future, because we might run
into ABI incompatibility issues across different versions of the library.

Compile module:

- Unzip glfw-3.3.2.zip
- cd glfw-3.3.2
- mkdir build
- cd build
- cmake .. -DCMAKE_BUILD_TYPE=Release
- cmake --build . --config=Release
- copy the resulting libraries into module-dir/your-target
- update module.yaml configuration file to link libraries for your target