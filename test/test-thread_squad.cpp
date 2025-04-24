
#include <patton/thread.hpp>
#include <patton/thread_squad.hpp>

#include <thread>
#include <mutex>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>


#if defined(_WIN32) || defined(__linux__)
# define THREAD_PINNING_SUPPORTED
#endif // defined(_WIN32) || defined(__linux__)


template <typename T>
struct non_default_initializable
{
    T value;

    non_default_initializable(T _value)
        : value(std::move(_value))
    {
    }
};

TEST_CASE("thread_squad")
{
    int numHardwareThreads = static_cast<int>(std::thread::hardware_concurrency());
    int numCores = static_cast<int>(patton::physical_concurrency());

    GENERATE(range(0, 10)); // repetitions

    int numThreads = GENERATE_COPY(range(0, 5), (numCores + 1)/2, numCores, numHardwareThreads, 3*numHardwareThreads/2, 2*numHardwareThreads);
    CAPTURE(numThreads);

    unsigned numActualThreads = static_cast<unsigned>(numThreads);
    if (numActualThreads == 0)
    {
        numActualThreads = std::thread::hardware_concurrency();
    }

    std::mutex mutex;
    auto threadId_Count = std::unordered_map<std::thread::id, int>{ };
    auto threadIndex_Count = std::unordered_map<int, int>{ };
    int count = 0;

    auto params = patton::thread_squad::params{
        /*.num_threads = */ numThreads
    };
#ifdef THREAD_PINNING_SUPPORTED
    params.pin_to_hardware_threads = GENERATE(false, true);
#endif // !THREAD_PINNING_SUPPORTED

    auto action = [&]
    (patton::thread_squad::task_context ctx)
    {
        auto lock = std::unique_lock<std::mutex>(mutex);
        ++threadId_Count[std::this_thread::get_id()];
        ++threadIndex_Count[ctx.thread_index()];
        ++count;
    };

    SECTION("single task")
    {
        patton::thread_squad(params).run(action);
        CHECK(threadIndex_Count.size() == static_cast<std::size_t>(numActualThreads));
    }

    SECTION("fixed number of tasks")
    {
        int numTasks = GENERATE(0, 1, 2, 5, 10, 20);
        CAPTURE(numTasks);

        auto threadSquad = patton::thread_squad(params);
        for (int i = 0; i < numTasks; ++i)
        {
            threadSquad.run(action);
        }

        if (numTasks != 0)
        {
            if (params.pin_to_hardware_threads)
            {
                CHECK(threadId_Count.size() == static_cast<std::size_t>(numActualThreads));
            }
            CHECK(threadIndex_Count.size() == static_cast<std::size_t>(numActualThreads));
        }

        if (params.pin_to_hardware_threads)
        {
            for (auto const& id_count : threadId_Count)
            {
                CHECK(id_count.second == numTasks);
            }
        }
        for (auto const& index_count : threadIndex_Count)
        {
            CHECK(index_count.second == numTasks);
        }
    }

    SECTION("no deadlocks")
    {
        GENERATE(range(0, 3)); // more repetitions

        int numTasks = GENERATE(0, 1, 2, 5, 10, 20);
        CAPTURE(numTasks);

        auto threadSquad = patton::thread_squad(params);
        for (int i = 0; i < numTasks; ++i)
        {
            CAPTURE(i);
            threadSquad.run([](auto const&){ });
        }
    }

    SECTION("varying concurrency")
    {
        auto threadSquad = patton::thread_squad(params);
        for (int i = 1; i <= int(numActualThreads); ++i)
        {
            CAPTURE(i);
            threadSquad.run(action, i);
        }

        CHECK(count == int(numActualThreads * (numActualThreads + 1) / 2));
    }

    SECTION("reduction")
    {
        int numToSum = GENERATE(10'000);
        int sumOfNum = numToSum*(numToSum + 1)/2;
    
        auto threadSquad = patton::thread_squad(params);
        for (int i = 1; i <= int(numActualThreads); ++i)
        {
            CAPTURE(i);
            int sum = threadSquad.transform_reduce(
                [numToSum]
                (patton::thread_squad::task_context const& ctx)
                {
                    int partition = ((numToSum+1) + ctx.num_threads() - 1)/ctx.num_threads();
                    int first = ctx.thread_index()*partition;
                    int last = std::min<int>(first + partition, numToSum+1);
                    int partialSum = 0;
                    for (int i = first; i != last; ++i)
                    {
                        volatile int local = i;  // prevent the compiler (Clang, in particular) from optimizing this further
                        partialSum += local;
                    }
                    return non_default_initializable(partialSum);
                },
                non_default_initializable(0),
                [](non_default_initializable<int> lhs, non_default_initializable<int> rhs)
                {
                    return non_default_initializable(lhs.value + rhs.value);
                },
                i).value;
            CHECK(sum == sumOfNum);
        }
    }

    SECTION("synchronization")
    {
        int numToSum = GENERATE(10'000);
        int sumOfNum = numToSum*(numToSum + 1)/2;
    
        auto threadSquad = patton::thread_squad(params);
        for (int i = 1; i <= int(numActualThreads); ++i)
        {
            CAPTURE(i);
            bool reducedSumIsCorrectForEveryThread = threadSquad.transform_reduce_first(
                [numToSum, sumOfNum]
                (patton::thread_squad::task_context& ctx)
                {
                    int partition = ((numToSum+1) + ctx.num_threads() - 1)/ctx.num_threads();
                    int first = ctx.thread_index()*partition;
                    int last = std::min<int>(first + partition, numToSum+1);
                    int partialSum = 0;
                    for (int i = first; i != last; ++i)
                    {
                        volatile int local = i;  // prevent the compiler (Clang, in particular) from optimizing this further
                        partialSum += local;
                    }

                    int sum = ctx.reduce(
                        non_default_initializable(partialSum),
                        [](non_default_initializable<int> lhs, non_default_initializable<int> rhs)
                        {
                            return non_default_initializable(lhs.value + rhs.value);
                        }).value;
                    int sum2 = ctx.reduce_transform(
                        partialSum,
                        [](int lhs, int rhs)
                        {
                            return lhs + rhs;
                        },
                        std::identity{ });

                    return non_default_initializable(sum == sumOfNum && sum2 == sumOfNum);
                },
                [](non_default_initializable<bool> lhs, non_default_initializable<bool> rhs)
                {
                    return non_default_initializable(lhs.value && rhs.value);
                },
                i).value;
            CHECK(reducedSumIsCorrectForEveryThread);
        }
    }
}
