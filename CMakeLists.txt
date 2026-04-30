cmake_minimum_required(VERSION 3.16)
project(voltisc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(VOLTIS_PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_definitions(VOLTIS_PLATFORM_MACOS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(VOLTIS_PLATFORM_WINDOWS)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    add_compile_definitions(VOLTIS_ARCH_AARCH64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    add_compile_definitions(VOLTIS_ARCH_X86_64)
endif()

add_executable(voltisc
    src/main.cpp
    src/target.cpp
    src/abi.cpp
    src/binary_emit_utils.cpp
    src/executable_format_emit_utils.cpp
    src/file_utils.cpp
    src/target_capability_registry.cpp
    src/diagnostics.cpp
    src/lexer.cpp
    src/parser.cpp
    src/sema.cpp
    src/vir.cpp
    src/vir_passes.cpp
    src/lowering.cpp
    src/codegen.cpp
    src/backend_pe_x64.cpp
    src/backend_llvm_ir.cpp
    src/emit/elf_writer.cpp
    src/emit/macho_writer.cpp
    src/codegen/x64_codegen.cpp
    src/codegen/aarch64_codegen.cpp
)

target_include_directories(voltisc PRIVATE src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
