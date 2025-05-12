
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

# Obtain source dependencies.
# We use CPM to fetch dependencies in a stand-alone build but configure it to prefer packages provided through other means.
option(CPM_USE_LOCAL_PACKAGES "Always try to use `find_package` to get dependencies" ON)
option(CPM_LOCAL_PACKAGES_ONLY "Only use `find_package` to get dependencies" OFF)
set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/CPM-cache")
include(cmake/CPM.cmake)
#CPMAddPackage(NAME fmt           GIT_TAG 11.1.3 GITHUB_REPOSITORY fmtlib/fmt          SYSTEM OFF)
CPMAddPackage(NAME gsl-lite      VERSION 0.43.0 GITHUB_REPOSITORY gsl-lite/gsl-lite   SYSTEM OFF)
#CPMAddPackage(NAME xsimd         GIT_TAG 13.1.0 GITHUB_REPOSITORY xtensor-stack/xsimd SYSTEM OFF)
if(PATTON_BUILD_TESTING)
    set(CMAKE_CXX_STANDARD 17)  # workaround, cf. https://github.com/catchorg/Catch2/issues/2046
    CPMAddPackage(NAME Catch2    VERSION  3.8.1 GITHUB_REPOSITORY catchorg/Catch2     SYSTEM OFF)
endif()
#if(PATTON_BUILD_BENCHMARKS)
#    CPMAddPackage(NAME nanobench VERSION 4.3.11 GITHUB_REPOSITORY martinus/nanobench  SYSTEM OFF)
#endif()
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
include(cmake/InstallBasicPackageFiles.cmake)
install_basic_package_files(patton
    COMPATIBILITY SameMajorVersion
    DEPENDENCIES
        "Threads"
        "gsl-lite 0.43"
)
