
# patton C++ library
# Author: Moritz Beutel
# patton library target


cmake_minimum_required(VERSION 3.20)

# dependencies
find_package(Threads REQUIRED)
find_package(gsl-lite 0.42 REQUIRED)

# library target
add_library(patton STATIC
    "cpuinfo.cpp"
    "errors.cpp"
    "memory.cpp"
    "new.cpp"
    "thread_squad.cpp"
)
add_library(patton::patton ALIAS patton)

# library properties
target_include_directories(patton
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)
target_compile_features(patton
    PUBLIC
        cxx_std_20
)

# compiler settings
include(TargetCompileSettings)
cmakeshift_target_compile_settings(patton
    SOURCE_FILE_ENCODING "UTF-8"
)
target_precompile_headers(patton
    PRIVATE
        # C standard library
        <cstddef>
        # C++ standard library
        <new>
        <memory>
        <string>
        <utility>
        <exception>
        <algorithm>
        <type_traits>
        <system_error>
        # other libraries
        <gsl-lite/gsl-lite.hpp>
)

if(MSVC)
    target_compile_definitions(patton
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
        target_precompile_headers(patton
            PRIVATE
                <Windows.h>
        )
    endif()
endif()

if (NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Sized deallocation must be explicitly enabled on Clang, cf. https://bcain-llvm.readthedocs.io/projects/clang/en/release_37/ReleaseNotes/#new-compiler-flags.
    target_compile_options(patton
        PRIVATE
            -fsized-deallocation
    )
endif()

# library dependencies
target_link_libraries(patton
    PUBLIC
        Threads::Threads
        gsl::gsl-lite-v1
)

# installation: public header files, target
install(
    DIRECTORY
        "${PROJECT_SOURCE_DIR}/include/" # the trailing '/' is significant
    TYPE INCLUDE
)
install(
    TARGETS patton
    EXPORT patton
    ARCHIVE INCLUDES
)
