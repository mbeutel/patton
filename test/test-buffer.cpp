
#include <patton/buffer.hpp>

#include <gsl-lite/gsl-lite.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>


namespace {


namespace gsl = ::gsl_lite;


TEST_CASE("aligned_buffer<> properly aligns elements")
{
    constexpr std::size_t alignment = 4 * sizeof(int);
    std::size_t numElements = GENERATE(range(0, 9));

    auto bufNI = patton::aligned_buffer<int, alignment>(numElements);
    auto buf42A = patton::aligned_buffer<int, alignment>(numElements, 42);
    auto buf42B = patton::aligned_buffer<int, alignment>(numElements, std::in_place, 42);

    // TODO: add checks
}


TEST_CASE("aligned_row_buffer<> properly aligns elements")
{
    constexpr std::size_t alignment = 4 * sizeof(int);
    std::size_t numRows = GENERATE(range(0, 2));
    std::size_t numCols = GENERATE(range(0, 9));

    auto bufNI = patton::aligned_row_buffer<int, alignment>(numRows, numCols);
    auto buf42A = patton::aligned_row_buffer<int, alignment>(numRows, numCols, 42);
    auto buf42B = patton::aligned_row_buffer<int, alignment>(numRows, numCols, std::in_place, 42);

    // TODO: add checks
}


} // anonymous namespace
