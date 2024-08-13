cmake_minimum_required(VERSION 3.5)
project(bl VERSION 0.11.2)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_HEADERS_IN_COMPILE_COMMANDS ON)
set(HAVE_64_BIT 1)

if(NOT CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/bl" CACHE STRING "Destination directory used to install BL." FORCE)
endif()

message(STATUS "Installation prefix set to: '${CMAKE_INSTALL_PREFIX}'.")

set(BL_DEPS_REMOTE biscuitlang.org/releases)
set(BL_DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

#Set a default build type if none was specified
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE)
	message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
	set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE})
endif()

set(CMAKE_C_FLAGS_DEBUG "-DBL_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-DBL_DEBUG")

option(BL_ASSERT_ENABLE "Turn on assert checks." OFF)
if(BL_ASSERT_ENABLE)
	add_compile_definitions(BL_ASSERT_ENABLE=1)
else()
	add_compile_definitions(
		$<$<CONFIG:Debug>:BL_ASSERT_ENABLE=1>
		$<$<CONFIG:Release>:BL_ASSERT_ENABLE=0>)
endif()

option(BL_DEVELOPER "Turn on incomplete stuff in development." OFF)
if(BL_DEVELOPER)
	add_compile_definitions(BL_DEVELOPER=1)
else()
	add_compile_definitions(BL_DEVELOPER=0)
endif()

option(TRACY_ENABLE "Turn on Tracy profiler." OFF)
set(TRACY_VER "0.9.1" CACHE STRING "Set Tracy profiler version.")

option(BL_X64_TESTS_ENABLE "Build testing program for x64 backend." OFF)

#setup output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(PDB_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin")
set(BL_CONFIG_FILE etc/bl.yaml)
set(BL_API_DIR "../lib/bl/api") # relative to the executable

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(BL_LLVM_VER "18.1.8")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(BL_LLVM_VER "18.1")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(BL_LLVM_VER "18.1")
endif()

if(MSVC AND NOT LLVM_DIR)
#DOWNLOAD LLVM
	set(BL_LLVM_DIR ${BL_DEPS_DIR}/llvm-${BL_LLVM_VER}-win64)
	set(BL_LLVM_DIR_ZIP ${BL_DEPS_DIR}/llvm-${BL_LLVM_VER}-win64.zip)
	set(BL_LLVM_REMOTE_ZIP ${BL_DEPS_REMOTE}/llvm-${BL_LLVM_VER}-win64.zip)

	if(NOT EXISTS ${BL_LLVM_DIR})
		if(NOT EXISTS ${BL_LLVM_DIR_ZIP})
			message(STATUS "Downloading LLVM binaries from ${BL_LLVM_REMOTE_ZIP}.")
			file(
				DOWNLOAD ${BL_LLVM_REMOTE_ZIP}
				${BL_LLVM_DIR_ZIP}
				SHOW_PROGRESS
				EXPECTED_HASH MD5=0DDE5D41A4F9212FB49D2A280FEBB77E)
		endif()
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar -xf ${BL_LLVM_DIR_ZIP}
			WORKING_DIRECTORY ${BL_DEPS_DIR}
		)
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E rm -f ${BL_LLVM_DIR_ZIP}
			WORKING_DIRECTORY ${BL_DEPS_DIR}
		)
	endif()
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(LLVM_DIR "${BL_LLVM_DIR}/debug/lib/cmake/llvm")
	else()
		set(LLVM_DIR "${BL_LLVM_DIR}/release/lib/cmake/llvm")
	endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(LLD_BIN ${BL_DEPS_DIR}/lld.exe)
	set(LLD_ZIP lld-18.1.8-win64-release.zip)
	set(LLD_ZIP_MD5 358C490AF5AF5652DDF441E2893DC104)
	set(BL_VSWHERE_EXE "vswhere.exe")
	set(BL_VSWHERE "${BL_DEPS_DIR}/vswhere-2.8.4/${BL_VSWHERE_EXE}")
	set(BL_LLD_EXE "bl-lld.exe")
	set(BL_LINKER ${BL_LLD_EXE})

	# Installation step for VSWHERE
	install(
		FILES ${BL_VSWHERE}
		PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
		DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
		RENAME ${BL_VSWHERE_EXE}
	)

	#DOWNLOAD LLD binary
	if(NOT EXISTS ${LLD_BIN})
		set(LLD_LOCAL_ZIP ${BL_DEPS_DIR}/${LLD_ZIP})
		set(LLD_REMOTE_ZIP ${BL_DEPS_REMOTE}/${LLD_ZIP})
		if(NOT EXISTS ${LLD_LOCAL_ZIP})
			message(STATUS "Downloading LLD binaries.")
			file(
			DOWNLOAD ${LLD_REMOTE_ZIP}
			${LLD_LOCAL_ZIP}
			SHOW_PROGRESS
			EXPECTED_HASH MD5=${LLD_ZIP_MD5})
		endif()
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar -xf ${LLD_LOCAL_ZIP}
			WORKING_DIRECTORY ${BL_DEPS_DIR}
		)
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E rm -f ${LLD_LOCAL_ZIP}
			WORKING_DIRECTORY ${BL_DEPS_DIR}
		)
	endif()

endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	#set(LLD_BIN ${BL_DEPS_DIR}/lld)
	#set(LLD_ZIP lld-13.0.0-linux64-release.zip)
	#set(LLD_ZIP_MD5 f19860ad1cb21a385d99d584f0a96638)
	set(BL_LLD_EXE "lld")
	set(BL_LINKER ${BL_LLD_EXE})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	#set(LLD_BIN ${BL_DEPS_DIR}/lld)
	#set(LLD_ZIP lld-11.0.0-macos-release.zip)
	#set(LLD_ZIP_MD5 b37f7524cf2d12efa4f27268713b45f3)
	#set(BL_LLD_EXE "bl-lld")
	#set(BL_LINKER ${BL_LLD_EXE})
	set(BL_LLD_EXE "lld")
	set(BL_LINKER ${BL_LLD_EXE})
endif()

# SIMD
option(BL_SIMD_ENABLE "Enable or disable SIMD." ON)
if(BL_SIMD_ENABLE)
	if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
		message(STATUS "SIMD enabled.")
		add_definitions(-DBL_USE_SIMD)
	endif()
endif()

find_package(LLVM ${BL_LLVM_VER} REQUIRED)

set(HEADER_FILES
	src/arena.h
	src/assembly.h
	src/ast.h
	src/basic_types.h
	src/bldebug.h
	src/blmemory.h
	src/builder.h
	src/common.h
	src/conf.h
	src/config.h
	src/error.h
	src/llvm_api.h
	src/mir.h
	src/mir_printer.h
	src/scope.h
	src/stb_ds.h
	src/table.h
	src/threading.h
	src/tokens.h
	src/tokens_inline_utils.h
	src/unit.h
	src/vm.h
	src/vmdbg.h
	src/wbs.h
	src/winpthreads.h
	src/x86_64_instructions.h
)

set(SOURCE_FILES
	src/arena.c
	src/asm_writer.c
	src/assembly.c
	src/ast.c
	src/ast_printer.c
	src/bc_writer.c
	src/bldebug.c
	src/blmemory.c
	src/build_api.c
	src/builder.c
	src/common.c
	src/conf.c
	src/docs.c
	src/file_loader.c
	src/intrinsic.c
	src/ir.c
	src/ir_opt.c
	src/lexer.c
	src/linker.c
	src/lld_ld.c
	src/lld_link.c
	src/llvm_api.cpp
	src/main.c
	src/mir.c
	src/mir_printer.c
	src/mir_writer.c
	src/native_bin.c
	src/obj_writer.c
	src/parser.c
	src/scope.c
	src/setup.c
	src/table.c
	src/threading.c
	src/token_printer.c
	src/tokens.c
	src/unit.c
	src/vm.c
	src/vm_runner.c
	src/vmdbg.c
	src/x86_64.c
)

# rpmalloc
option(BL_RPMALLOC_ENABLE "Enable or disable rpmalloc https://github.com/mjansson/rpmalloc." ON)
if(BL_RPMALLOC_ENABLE)
	add_compile_definitions(BL_RPMALLOC_ENABLE=1)
	set(SOURCE_FILES ${SOURCE_FILES} ${BL_DEPS_DIR}/rpmalloc-1.4.4/rpmalloc/rpmalloc.c)
	message(STATUS "RPMalloc enabled.")
else()
	add_compile_definitions(BL_RPMALLOC_ENABLE=0)
	message(STATUS "RPMalloc disabled.")
endif()

# LLVM
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(LLVM_LIBS core support X86 AArch64 passes)

add_executable(blc ${SOURCE_FILES} ${HEADER_FILES})

add_subdirectory(${BL_DEPS_DIR}/dyncall-1.2)
add_subdirectory(${BL_DEPS_DIR}/libyaml-0.2.5)

if(TRACY_ENABLE)
	add_definitions(-DTRACY_ENABLE)
	add_subdirectory(${BL_DEPS_DIR}/tracy-${TRACY_VER})
	target_link_libraries(blc PUBLIC Tracy::TracyClient)
	message(STATUS "Tracy profiler enabled.")
endif()

if(MSVC)
	add_definitions(-DNOMINMAX)
	target_link_libraries(blc PUBLIC shlwapi DbgHelp)
	set(MSVC_FLAGS /Z7 /Gr /GF /EHsc)
	set(MSVC_RELEASE_FLAGS ${MSVC_FLAGS} /O2 /Oi /Ot /GL /arch:AVX)
	set(MSVC_DEBUG_FLAGS  ${MSVC_FLAGS} /Od /MDd)
	target_compile_options(blc PRIVATE
		$<$<CONFIG:Release>: ${MSVC_RELEASE_FLAGS}>
		$<$<CONFIG:Debug>:   ${MSVC_DEBUG_FLAGS}>)
	target_compile_options(dynload_s PRIVATE
		$<$<CONFIG:Release>: ${MSVC_RELEASE_FLAGS}>
		$<$<CONFIG:Debug>:   ${MSVC_DEBUG_FLAGS}>)
	target_compile_options(yaml PRIVATE
		$<$<CONFIG:Release>: ${MSVC_RELEASE_FLAGS} /wd4996 /wd4267 /wd4244>
		$<$<CONFIG:Debug>:   ${MSVC_DEBUG_FLAGS} /wd4996 /wd4267 /wd4244>)

	set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}")
else()
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(blc PRIVATE Threads::Threads)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wl,--export-dynamic -Wno-unused-command-line-argument")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wl,--export-dynamic -Wno-unused-command-line-argument")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wl,--export-dynamic -Wno-unused-command-line-argument")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wl,--export-dynamic -Wno-unused-command-line-argument")
endif()

target_link_libraries(
	blc
	PRIVATE dynload_s
			dyncall_s
			dyncallback_s
			yaml
			${CMAKE_DL_LIBS}
			${LLVM_LIBS}
)

target_include_directories(
	blc
	PRIVATE src
			${BL_DEPS_DIR}/dyncall-1.2/dyncall
			${BL_DEPS_DIR}/dyncall-1.2/dynload
			${BL_DEPS_DIR}/dyncall-1.2/dyncallback
			${BL_DEPS_DIR}/libyaml-0.2.5/include
			${BL_DEPS_DIR}/tracy-${TRACY_VER}/public/tracy
			${BL_DEPS_DIR}/rpmalloc-1.4.4/rpmalloc
			${LLVM_INCLUDE_DIRS}
)

set_target_properties(dyncall_s dyncallback_s dynload_s PROPERTIES FOLDER dyncall)
set_target_properties(acc_gen intrinsics_gen omp_gen PROPERTIES FOLDER LLVM)
set_target_properties(yaml PROPERTIES FOLDER yaml)

# dlib runtime
add_library(dlib STATIC src/dlib/dlib_runtime.c ${HEADER_FILES})

target_include_directories(
	dlib
	PRIVATE src
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(BL_DLIB_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/bl/api/std/dlib/win32)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(BL_DLIB_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/bl/api/std/dlib/darwin)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(BL_DLIB_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/bl/api/std/dlib/linux)
endif()

set_target_properties(dlib
	PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${BL_DLIB_OUTPUT_DIRECTORY}
	ARCHIVE_OUTPUT_DIRECTORY ${BL_DLIB_OUTPUT_DIRECTORY} # Works on windows for .lib
)

# MISC
if (BL_X64_TESTS_ENABLE)
	add_executable(blc-x64-test src/x86_64_instructions.test.c)
	target_include_directories(
		blc-x64-test PRIVATE
		${BL_DEPS_DIR}/tracy-${TRACY_VER}/public/tracy
		src)
endif()

# Installation

install(TARGETS blc RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(DIRECTORY lib/bl DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	# Copy LLD executable into output directory
	if(NOT EXISTS ${LLD_BIN})
		message(FATAL_ERROR "LLD not found expected location is ${LLD_BIN}" )
	endif()
	add_custom_command(TARGET blc POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${LLD_BIN} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BL_LLD_EXE})
	add_custom_command(TARGET blc POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${BL_VSWHERE} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BL_VSWHERE_EXE})

	# Installation step for LLD
	install(
		FILES ${LLD_BIN}
		PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
		DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
		RENAME ${BL_LLD_EXE}
	)
endif()

configure_file(${CMAKE_SOURCE_DIR}/src/config.h.in ${CMAKE_SOURCE_DIR}/src/config.h)

