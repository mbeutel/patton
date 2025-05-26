
#include <patton/thread.hpp>
#include <patton/thread_squad.hpp>

#include <gsl-lite/gsl-lite.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "params.hpp"


#if defined(_WIN32) || defined(__linux__)
# define THREAD_PINNING_SUPPORTED
#endif // defined(_WIN32) || defined(__linux__)


static patton::thread_squad::params
getThreadSquadParams()
{
    auto params = patton::thread_squad::params{
        .num_threads = global_benchmark_params.num_threads,
        .spin_wait = global_benchmark_params.spin_wait
    };
#ifdef THREAD_PINNING_SUPPORTED
    params.pin_to_hardware_threads = true;
#endif // !THREAD_PINNING_SUPPORTED
    if (params.num_threads == 0 && global_benchmark_params.no_smt)
    {
        params.num_threads = gsl_lite::narrow_failfast<int>(patton::physical_concurrency());
        params.hardware_thread_mappings = patton::physical_core_ids();
    }
    return params;
}

TEST_CASE("thread_squad: create-run-destroy")
{
    auto params = getThreadSquadParams();

    auto action = []
    (patton::thread_squad::task_context /*ctx*/)
    {
    };

    BENCHMARK("create-run-destroy")
    {
        patton::thread_squad(params).run(action);
    };
}

TEST_CASE("thread_squad: run")
{
    auto params = getThreadSquadParams();

    auto action = []
    (patton::thread_squad::task_context /*ctx*/)
    {
    };

    auto threadSquad = patton::thread_squad(params);

    BENCHMARK("run")
    {
        threadSquad.run(action);
    };
}
