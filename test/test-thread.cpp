
#include <thread>  // for thread::hardware_concurrency()

#include <patton/thread.hpp>

#include <iostream>

#include <gsl-lite/gsl-lite.hpp>

#include <catch2/catch_test_macros.hpp>


TEST_CASE("physical_concurrency() returns correct value")
{
    unsigned physicalConcurrency = patton::physical_concurrency();
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " cores\n";
    std::cout << "Physical concurrency: " << physicalConcurrency << " cores\n";

    CHECK(physicalConcurrency != 0);
    CHECK(physicalConcurrency <= std::thread::hardware_concurrency());
}

TEST_CASE("physical_core_ids() returns correct number of thread ids")
{
    unsigned physicalConcurrency = patton::physical_concurrency();
    auto physicalCoreIds = patton::physical_core_ids();
    std::cout << "Physical core ids: [";
    bool first = true;
    for (auto id : physicalCoreIds)
    {
        if (!first)
        {
            std::cout << ", ";
        }
        first = false;
        std::cout << id;
    }
    std::cout << "]\n";

    if (!physicalCoreIds.empty())
    {
        CHECK(gsl_lite::ssize(physicalCoreIds) == physicalConcurrency);
    }
}
