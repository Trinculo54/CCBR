cmake_minimum_required(VERSION 3.20)
# Bump VERSION before tagging a release (the tag build checks they match).
project(ccbr VERSION 1.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Targets Windows. On Linux use -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake")
endif()

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    message(FATAL_ERROR "Build with llvm-mingw (clang): the mod relies on its MSVC-style SEH and GNU extensions together.")
else()
    add_compile_options(-Wall -Wno-missing-field-initializers -fms-extensions)
    add_compile_options("$<$<CONFIG:Release>:-O2;-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    add_link_options(-static)
endif()

# The FH2 Mod Platform (git submodule): d3dcompiler_47.dll and DWrite.dll, shipped
# next to azimuth.exe, and fh2mod.h.
add_subdirectory(platform)
configure_file(cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/fh2_version.h)

# The mod: fh2-mods\CCBR.dll
add_library(ccbr SHARED src/ccbr.cpp src/version.rc)
target_include_directories(ccbr PRIVATE src ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(ccbr PRIVATE CCBR_VERSION="${PROJECT_VERSION}")
target_link_libraries(ccbr PRIVATE fh2mod_api)
set_target_properties(ccbr PROPERTIES OUTPUT_NAME CCBR PREFIX "")

# The tests (tests/README.md): the ghost and see-through logic with the game faked, and a harness that
# maps the real game exe and starts the mod against it.
option(CCBR_TESTS "Build the tests" OFF)
if(CCBR_TESTS)
    add_executable(unit_test tests/unit_test.cpp)
    target_include_directories(unit_test PRIVATE src)
    target_compile_definitions(unit_test PRIVATE CCBR_VERSION="${PROJECT_VERSION}")
    target_link_libraries(unit_test PRIVATE fh2mod_api)
    add_executable(harness tests/harness.cpp)
    target_compile_definitions(harness PRIVATE UNICODE _UNICODE)
    target_link_options(harness PRIVATE -municode -Wl,--image-base=0x10000000)
endif()
