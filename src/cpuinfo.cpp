
#include <new>
#include <span>
#include <atomic>
#include <memory>     // for unique_ptr<>
#include <string>
#include <vector>
#include <cstddef>    // for ptrdiff_t
#include <fstream>
#include <iostream>
#include <stdexcept>  // for runtime_error
#include <algorithm>  // for sort(), unique()

#if defined(_WIN32)
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <Windows.h>
# include <Memoryapi.h>
#elif defined(__linux__)
# include <unistd.h>
# include <stdio.h>
#elif defined(__APPLE__)
# include <unistd.h>
# include <sys/types.h>
# include <sys/sysctl.h>
#else
# error Unsupported operating system.
#endif

#include <gsl-lite/gsl-lite.hpp>  // for dim, gsl_ExpectsAudit(), narrow_failfast<>()

#include <patton/detail/errors.hpp>


namespace patton::detail {


struct cpu_info
{
#if defined(_WIN32)
    std::atomic<std::size_t> cache_line_size;
#endif // defined(_WIN32)
    std::atomic<unsigned> physical_concurrency;

#if defined(_WIN32) || defined(__linux__)
    std::atomic<int const*> core_thread_ids_ptr;
    std::vector<int> core_thread_ids;
#endif // defined(_WIN32) || defined(__linux__)
};


#if defined(__linux__)
struct physical_core_id
{
    int core_id;
    int physical_id;
    int processor;

    friend bool operator ==(physical_core_id const& lhs, physical_core_id const& rhs)
    {
        return lhs.core_id == rhs.core_id && lhs.physical_id == rhs.physical_id;
    }
    friend bool operator <(physical_core_id const& lhs, physical_core_id const& rhs)
    {
        if (lhs.core_id < rhs.core_id) return true;
        if (lhs.core_id > rhs.core_id) return false;
        if (lhs.physical_id < rhs.physical_id) return true;
        if (lhs.physical_id > rhs.physical_id) return false;
        return lhs.processor < rhs.processor;
    }
};
#endif // defined(__linux__)


static cpu_info cpu_info_value{ };

    // Returns the number of the lowest bit set. Expects that at least one bit is set.
template <typename T>
int
lowest_bit_set(T x)
{
    int result = 0;
    while (x != 0)
    {
        if ((x & 1) != 0)
        {
            return result;
        }
        ++result;
        x >>= 1;  // shift happens
    }
    gsl_FailFast();
}

void
init_cpu_info() noexcept
{
    auto initFunc = []
    {
        std::size_t newCacheLineSize = 0;
        unsigned newPhysicalConcurrency = 0;
        std::vector<int> coreThreadIds;
        auto lresult = cpu_info{ };

#if defined(_WIN32)
        std::unique_ptr<SYSTEM_LOGICAL_PROCESSOR_INFORMATION[]> dynSlpi;
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION* pSlpi = nullptr;
        DWORD nbSlpi = 0;
        BOOL success = GetLogicalProcessorInformation(pSlpi, &nbSlpi);
        if (!success && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            dynSlpi = std::make_unique<SYSTEM_LOGICAL_PROCESSOR_INFORMATION[]>((nbSlpi + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) - 1) / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            pSlpi = dynSlpi.get();
            success = GetLogicalProcessorInformation(pSlpi, &nbSlpi);
        }
        detail::win32_assert(success);
        for (std::ptrdiff_t i = 0, n = nbSlpi / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); i != n; ++i)
        {
            if (pSlpi[i].Relationship == RelationProcessorCore)
            {
                ++newPhysicalConcurrency;
                int id = detail::lowest_bit_set(pSlpi[i].ProcessorMask);
                coreThreadIds.push_back(id);
            }
            if (pSlpi[i].Relationship == RelationCache && pSlpi[i].Cache.Level == 1 && (pSlpi[i].Cache.Type == CacheData || pSlpi[i].Cache.Type == CacheUnified))
            {
                if (newCacheLineSize == 0)
                {
                    newCacheLineSize = pSlpi[i].Cache.LineSize;
                }
                else if (newCacheLineSize != pSlpi[i].Cache.LineSize)
                {
                    throw std::runtime_error("GetLogicalProcessorInformation() reports different L1 cache line sizes for different cores");  // ...and we cannot handle that
                }
            }
        }
        if (newCacheLineSize == 0)
        {
            throw std::runtime_error("GetLogicalProcessorInformation() did not report any L1 cache info");
        }
        if (newPhysicalConcurrency == 0)
        {
            throw std::runtime_error("GetLogicalProcessorInformation() did not report any processor cores");
        }

        coreThreadIds.shrink_to_fit();
        if (newCacheLineSize < std::hardware_constructive_interference_size)
        {
            gsl_FailFast();  // In this library we assume that `std::hardware_constructive_interference_size` at least acts as a lower bound for the cache line size.
        }
        cpu_info_value.cache_line_size.store(newCacheLineSize, std::memory_order_relaxed);

#elif defined(__linux__)
            // I can't believe that parsing /proc/cpuinfo is the accepted way to query the number of physical cores.
        auto f = std::ifstream("/proc/cpuinfo");
        if (!f) throw std::runtime_error("cannot open /proc/cpuinfo");  // something is really wrong if we cannot open that file
        auto ids = std::vector<physical_core_id>{ };
        auto line = std::string{ };
        int lastProcessor = -1;
        int lastCoreId = -1;
        int lastPhysicalId = -1;
        while (std::getline(f, line))
        {
            int nFields;
            int id;
            nFields = std::sscanf(line.c_str(), "processor : %d", &id);
            if (nFields == 1)
            {
                if (lastProcessor != -1) throw std::runtime_error("error parsing /proc/cpuinfo: missing \"processor\" value");
                lastProcessor = id;
            }
            else
            {
                nFields = std::sscanf(line.c_str(), "physical id : %d", &id);
                if (nFields == 1)
                {
                    if (lastPhysicalId != -1) throw std::runtime_error("error parsing /proc/cpuinfo: missing \"core id\" value");
                    lastPhysicalId = id;
                }
                else
                {
                    nFields = std::sscanf(line.c_str(), "core id : %d", &id);
                    if (nFields == 1)
                    {
                        if (lastCoreId != -1) throw std::runtime_error("error parsing /proc/cpuinfo: missing \"physical id\" value");
                        lastCoreId = id;
                    }
                }
            }
            if (lastProcessor != -1 && lastCoreId != -1 && lastPhysicalId != -1)
            {
                ids.push_back(detail::physical_core_id{ lastCoreId, lastPhysicalId, lastProcessor });
                lastProcessor = -1;
                lastCoreId = -1;
                lastPhysicalId = -1;
            }
        }
        f.close();

        std::sort(ids.begin(), ids.end());
        auto numUnique = std::unique(ids.begin(), ids.end()) - ids.begin();
        newPhysicalConcurrency = gsl::narrow_failfast<unsigned>(numUnique);

        coreThreadIds.resize(numUnique);
        std::transform(
            ids.begin(), ids.begin() + numUnique,
            coreThreadIds.begin(),
            [](physical_core_id const& id)
            {
                return id.processor;
            });
        std::sort(coreThreadIds.begin(), coreThreadIds.end());
#elif defined(__APPLE__)
        int result = 0;
        std::size_t nbResult = sizeof result;
        int ec = sysctlbyname("hw.physicalcpu", &result, &nbResult, 0, 0);
        if (ec != 0) throw std::runtime_error("cannot query hw.physicalcpu");
        newPhysicalConcurrency = gsl::narrow_failfast<unsigned>(result);
#else
# error Unsupported operating system.
#endif

        cpu_info_value.physical_concurrency.store(newPhysicalConcurrency, std::memory_order_relaxed);

#if defined(_WIN32) || defined(__linux__)
        int const* expectedPtr = nullptr;
        int const* desiredPtr = coreThreadIds.data();
        if (cpu_info_value.core_thread_ids_ptr.compare_exchange_strong(expectedPtr, desiredPtr))
        {
            cpu_info_value.core_thread_ids = std::move(coreThreadIds);
        }
#endif // defined(_WIN32) || defined(__linux__)

            // A release fence would be sufficient here, but we use sequential consistency by default to have other threads see
            // the results of our hard work as soon as possible.
        std::atomic_thread_fence(std::memory_order_seq_cst);
    };

    auto physicalConcurrency = cpu_info_value.physical_concurrency.load(std::memory_order_relaxed);
    if (physicalConcurrency == 0)
    {
        initFunc();
    }
}


} // namespace patton::detail

namespace patton {


#if defined(_WIN32)
std::size_t
hardware_cache_line_size() noexcept
{
    auto cacheLineSize = detail::cpu_info_value.cache_line_size.load(std::memory_order_relaxed);
    if (cacheLineSize == 0)
    {
        detail::init_cpu_info();
        cacheLineSize = detail::cpu_info_value.cache_line_size.load(std::memory_order_relaxed);
    }
    return cacheLineSize;
}
#endif // defined(_WIN32)

unsigned
physical_concurrency() noexcept
{
    auto physicalConcurrency = detail::cpu_info_value.physical_concurrency.load(std::memory_order_relaxed);
    if (physicalConcurrency == 0)
    {
        detail::init_cpu_info();
        physicalConcurrency = detail::cpu_info_value.physical_concurrency.load(std::memory_order_relaxed);
    }
    return physicalConcurrency;
}

std::span<int const>
physical_core_ids() noexcept
{
#if defined(_WIN32) || defined(__linux__)
    auto physicalConcurrency = detail::cpu_info_value.physical_concurrency.load(std::memory_order_relaxed);
    auto coreThreadIdsPtr = detail::cpu_info_value.core_thread_ids_ptr.load(std::memory_order_relaxed);
    if (physicalConcurrency == 0 || coreThreadIdsPtr == nullptr)
    {
        detail::init_cpu_info();
        physicalConcurrency = detail::cpu_info_value.physical_concurrency.load(std::memory_order_relaxed);
        coreThreadIdsPtr = detail::cpu_info_value.core_thread_ids_ptr.load(std::memory_order_relaxed);
    }
    return std::span<int const>(coreThreadIdsPtr, gsl::narrow_failfast<std::size_t>(physicalConcurrency));
#else // ^^^ defined(_WIN32) || defined(__linux__) ^^^ / vvv !defined(_WIN32) && !defined(__linux__) vvv
    return { };
#endif // defined(_WIN32) || defined(__linux__)
}


} // namespace patton
