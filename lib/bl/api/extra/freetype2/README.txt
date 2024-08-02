BL Wrapper for FreeType 2.9.1

Note: 

The module is currently linked statically on Windows (binary is included). On other platforms we
rely on freetype installed on the system. This should be unified in future, because we might run
into ABI incompatibility issues across different versions of the library.

Compile module:

- Unzip freetype_2.9.zip
- cd freetype_2.9
- mkdir build
- cd build
- cmake .. -DCMAKE_BUILD_TYPE=Release
- cmake --build . --config=Release
- copy the resulting libraries into module-dir/your-target
- update module.yaml configuration file to link libraries for your target