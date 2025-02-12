
# patton C++ library
# Author: Moritz Beutel
# unit tests


cmake_minimum_required(VERSION 3.20)

# dependencies
find_package(Threads REQUIRED)
find_package(gsl-lite 0.42 REQUIRED)
find_package(Catch2 REQUIRED)

include(TargetCompileSettings)

# test target
add_executable(test-patton
    "test-buffer.cpp"
    "test-memory.cpp"
    "test-new.cpp"
    "test-thread.cpp"
    "test-thread_squad.cpp"
)

# compiler settings
cmakeshift_target_compile_settings(test-patton
    SOURCE_FILE_ENCODING "UTF-8"
)
target_compile_features(test-patton
    PRIVATE
        cxx_std_20
)
target_precompile_headers(test-patton
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
        <catch2/catch_test_macros.hpp>
        <catch2/generators/catch_generators.hpp>
)

# test properties
target_compile_definitions(test-patton
    PRIVATE
        #CATCH_CONFIG_CONSOLE_WIDTH=120
        #CATCH_CLARA_CONFIG_CONSOLE_WIDTH=120
        CATCH_CONFIG_CPP17_UNCAUGHT_EXCEPTIONS
        CATCH_CONFIG_CPP17_STRING_VIEW
        CATCH_CONFIG_CPP17_VARIANT
        CATCH_CONFIG_CPP17_OPTIONAL
        CATCH_CONFIG_CPP17_BYTE
        gsl_CONFIG_CONTRACT_CHECKING_AUDIT  # run audit-level checks
        gsl_CONFIG_CONTRACT_VIOLATION_THROWS  # this makes gsl-lite's precondition assertions throw `gsl::fail_fast` on failure
)

# test dependencies
target_link_libraries(test-patton
    PRIVATE
        Threads::Threads
        gsl::gsl-lite-v1
        Catch2::Catch2WithMain
        patton
)

# register tests
add_test(NAME test-patton COMMAND test-patton)
set_property(TEST test-patton PROPERTY FAIL_REGULAR_EXPRESSION "Sanitizer")
