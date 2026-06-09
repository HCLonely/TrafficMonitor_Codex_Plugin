cmake_minimum_required(VERSION 3.21)

set(CODEX_USAGE_VERSION "0.1.0" CACHE STRING "CodexUsage plugin version")

project(CodexUsage VERSION "${CODEX_USAGE_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CodexUsageVersion.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/CodexUsageVersion.h"
    @ONLY
)

add_library(CodexUsageCore STATIC
    src/CodexUsageCore.cpp
    src/JsonLite.cpp
)

target_include_directories(CodexUsageCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_definitions(CodexUsageCore PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(CodexUsageCore PRIVATE /utf-8)
endif()

add_library(CodexUsage SHARED
    src/CodexUsagePlugin.cpp
    src/CodexUsageFetcher.cpp
)

target_include_directories(CodexUsage PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../TrafficMonitor/include"
)

target_compile_definitions(CodexUsage PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(CodexUsage PRIVATE /utf-8)
endif()

target_link_libraries(CodexUsage PRIVATE
    CodexUsageCore
    shlwapi
    winhttp
)

set_target_properties(CodexUsage PROPERTIES
    PREFIX ""
    OUTPUT_NAME "CodexUsage"
)

add_executable(CodexUsageTests
    tests/CodexUsageCoreTests.cpp
)

target_link_libraries(CodexUsageTests PRIVATE CodexUsageCore)

enable_testing()
add_test(NAME CodexUsageTests COMMAND CodexUsageTests)
