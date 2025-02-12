
#include <patton/memory.hpp>

#include <vector>
#include <memory>  // for allocator<>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>


namespace {


TEST_CASE("aligned_allocator<> properly aligns allocations")
{
    constexpr std::size_t alignment = 4 * sizeof(int);

    using Allocator = patton::aligned_allocator<int, alignment>;

    std::size_t numElements = GENERATE(range(0, 9));
    auto v = std::vector<int, Allocator>(numElements);

    // TODO: add checks
}

TEST_CASE("aligned_allocator_adaptor<> properly aligns allocations")
{
    constexpr std::size_t alignment = 4 * sizeof(int);

    using Allocator = patton::aligned_allocator_adaptor<int, alignment, std::allocator<int>>;

    std::size_t numElements = GENERATE(range(0, 9));
    auto v = std::vector<int, Allocator>(numElements);

    // TODO: add checks
}

// TODO: add tests for allocate_unique<>()


} // anonymous namespace
