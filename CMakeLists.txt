cmake_minimum_required(VERSION 3.25...4.4)
project(FunCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)  # no C++20 modules; skip the extra scan step

option(FUN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FUN_BUILD_TESTS "Build the unit tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# Dependencies come from MSYS2 UCRT64 (pacman: mingw-w64-ucrt-x86_64-raylib, -jsoncpp).
find_package(raylib CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

add_subdirectory(engine)
add_subdirectory(game)

if(FUN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
