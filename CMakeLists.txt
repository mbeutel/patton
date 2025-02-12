
# patton C++ library
# Author: Moritz Beutel


cmake_minimum_required(VERSION 3.20)

# Define the project.
project(patton
    VERSION 1.0.0
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Define build options.
option(PATTON_BUILD_TESTING "Build tests" OFF)
option(PATTON_BUILD_BENCHMARKS "Build benchmarks" OFF)

# Obtain source dependencies.
# We use CPM mainly to fetch test and benchmark dependencies in a source build. When used with Vcpkg,
# CPM will find the gsl-lite package provided by Vcpkg and not install any packages itself.
set(CPM_USE_LOCAL_PACKAGES ON)
set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/CPM-cache")
include(cmake/CPM.cmake)
#CPMAddPackage(NAME fmt           GIT_TAG 11.1.3 GITHUB_REPOSITORY fmtlib/fmt          SYSTEM OFF)
CPMAddPackage(NAME gsl-lite      GIT_TAG 0.42.0 GITHUB_REPOSITORY gsl-lite/gsl-lite   SYSTEM OFF)
#CPMAddPackage(NAME xsimd         GIT_TAG 13.1.0 GITHUB_REPOSITORY xtensor-stack/xsimd SYSTEM OFF)
if(PATTON_BUILD_TESTING)
    CPMAddPackage(NAME catch2    VERSION  3.8.0 GITHUB_REPOSITORY catchorg/Catch2     SYSTEM OFF)
endif()
if(PATTON_BUILD_BENCHMARKS)
    CPMAddPackage(NAME nanobench VERSION 4.3.11 GITHUB_REPOSITORY martinus/nanobench  SYSTEM OFF)
endif()
#CPMAddPackage(NAME lyra          GIT_TAG  1.6.1 GITHUB_REPOSITORY bfgroup/Lyra        SYSTEM OFF)

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
include(InstallBasicPackageFiles)
cmakeshift_install_basic_package_files(patton
    COMPATIBILITY SameMajorVersion
    DEPENDENCIES
        "Threads"
        "gsl-lite 0.40"
)
