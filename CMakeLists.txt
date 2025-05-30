﻿
# patton C++ library
# Author: Moritz Beutel


cmake_minimum_required(VERSION 3.30)

# Define the project.
project(patton
    VERSION 1.0.0
    LANGUAGES CXX)

# Define build options.
option(PATTON_BUILD_TESTING "Build tests" OFF)
option(PATTON_BUILD_BENCHMARKS "Build benchmarks" OFF)
set(PATTON_COMPILE_OPTIONS "" CACHE STRING "Extra compile options which should not be passed on when building dependencies (e.g. warning flags)")

# Obtain source dependencies.
set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/CPM-cache")
include(cmake/CPM.cmake)
CPMFindPackage(NAME gsl-lite      VERSION  1.0.1 GITHUB_REPOSITORY gsl-lite/gsl-lite   SYSTEM OFF)
if(PATTON_BUILD_TESTING OR PATTON_BUILD_BENCHMARKS)
    set(CMAKE_CXX_STANDARD 17)  # workaround, cf. https://github.com/catchorg/Catch2/issues/2046
    CPMFindPackage(NAME Catch2    VERSION  3.8.1 GITHUB_REPOSITORY catchorg/Catch2     SYSTEM OFF)
endif()

# Include target definitions.
add_subdirectory(src)
if(PATTON_BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
if(PATTON_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

# Install.
include(cmake/InstallBasicPackageFiles.cmake)
install_basic_package_files(patton
    COMPATIBILITY SameMajorVersion
    DEPENDENCIES
        "Threads"
        "gsl-lite 1.0"
)
