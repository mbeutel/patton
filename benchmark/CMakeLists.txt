
# patton C++ library
# Author: Moritz Beutel
# benchmarks


cmake_minimum_required(VERSION 3.20)

# dependencies
find_package(Threads REQUIRED)
find_package(gsl-lite 0.42 REQUIRED)
find_package(Catch2 3 REQUIRED)

include(TargetCompileSettings)

# benchmark runner target
add_library(benchmark-patton-runner OBJECT "main.cpp")
target_compile_features(benchmark-patton-runner PRIVATE cxx_std_20)
target_compile_definitions(benchmark-patton-runner PRIVATE CATCH_CONFIG_ENABLE_BENCHMARKING)
cmakeshift_target_compile_settings(benchmark-patton-runner
    SOURCE_FILE_ENCODING "UTF-8"
)
target_link_libraries(benchmark-patton-runner
    PRIVATE
        Catch2::Catch2
)

# benchmark target
add_executable(benchmark-patton
    "benchmark-thread_squad.cpp"
)

# compiler settings
target_include_directories(benchmark-patton
    PRIVATE
        "${PROJECT_SOURCE_DIR}/test/benchmark"
)
cmakeshift_target_compile_settings(benchmark-patton
    SOURCE_FILE_ENCODING "UTF-8"
)
target_compile_features(benchmark-patton
    PRIVATE
        cxx_std_20
)

# benchmark properties
#target_compile_definitions(benchmark-patton
#    PRIVATE
#        #CATCH_CONFIG_CONSOLE_WIDTH=120
#        #CATCH_CLARA_CONFIG_CONSOLE_WIDTH=120
#)

# benchmark dependencies
target_link_libraries(benchmark-patton
    PRIVATE
        gsl::gsl-lite-v1
        Catch2::Catch2
        patton
        benchmark-patton-runner
)
