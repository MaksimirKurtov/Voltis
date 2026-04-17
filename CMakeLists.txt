cmake_minimum_required(VERSION 3.16)
project(voltisc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

add_library(voltis_runtime STATIC
    src/voltis_runtime.cpp
)

add_executable(voltisc
    src/main.cpp
    src/diagnostics.cpp
    src/lexer.cpp
    src/parser.cpp
    src/sema.cpp
    src/vir.cpp
    src/lowering.cpp
    src/codegen.cpp
    src/backend_llvm_ir.cpp
)

target_include_directories(voltisc PRIVATE src)
add_dependencies(voltisc voltis_runtime)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
