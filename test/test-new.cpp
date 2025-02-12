
#include <patton/new.hpp>

#include <iostream>

#include <catch2/catch_test_macros.hpp>


constexpr bool is_power_of_2(std::size_t value) noexcept
{
    return value > 0
        && (value & (value - 1)) == 0;
}


TEST_CASE("hardware_cache_line_size() returns correct value")
{
    std::size_t cacheLineSize = patton::hardware_cache_line_size();
    std::cout << "Cache line size: " << cacheLineSize << " B\n";

    CHECK(is_power_of_2(cacheLineSize));
#if defined(_M_IX86) || defined(_M_AMD64) || defined(__i386__) || defined(__x86_64__)
    CHECK(cacheLineSize == 64);
#endif // defined(_M_IX86) || defined(_M_AMD64) || defined(__i386__) || defined(__x86_64__)
}

TEST_CASE("hardware_page_size() returns correct value")
{
    std::size_t pageSize = patton::hardware_page_size();
    std::cout << "Page size: " << pageSize << " B\n";

    CHECK(is_power_of_2(pageSize));
#if defined(_M_IX86) || defined(_M_AMD64) || defined(__i386__) || defined(__x86_64__)
    CHECK(pageSize == 4096);
#endif // defined(_M_IX86) || defined(_M_AMD64) || defined(__i386__) || defined(__x86_64__)
}

TEST_CASE("hardware_large_page_size() returns sane value")
{
    std::size_t largePageSize = patton::hardware_large_page_size();
    std::cout << "Large page size: " << largePageSize << " B\n";

    if (largePageSize != 0) CHECK(is_power_of_2(largePageSize));
}
