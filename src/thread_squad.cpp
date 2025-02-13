
//#define DEBUG_WAIT_CHAIN
#ifdef DEBUG_WAIT_CHAIN
# include <cstdio>
# define THREAD_SQUAD_DBG(...) std::printf(__VA_ARGS__); std::fflush(stdout)
#else // DEBUG_WAIT_CHAIN
# define THREAD_SQUAD_DBG(...)
#endif // DEBUG_WAIT_CHAIN

#include <new>
#include <string>
#include <memory>              // for unique_ptr<>
#include <cstddef>             // for size_t, ptrdiff_t
#include <cstring>             // for wcslen(), swprintf()
#include <utility>             // for move()
#include <algorithm>           // for min(), max()
#include <exception>           // for terminate()
#include <stdexcept>           // for range_error
#include <type_traits>         // for remove_pointer<>
#include <system_error>
#include <thread>
#include <atomic>

#if defined(_WIN32)
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <Windows.h>  // GetLastError(), SetThreadAffinityMask()
# include <process.h>  // _beginthreadex()
#elif defined(__linux__) || defined(__APPLE__)
# define USE_PTHREAD
# ifdef __linux__
#  define USE_PTHREAD_SETAFFINITY
# endif
# include <cerrno>
# include <pthread.h>  // pthread_self(), pthread_setaffinity_np()
#else
# error Unsupported operating system.
#endif

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
# include <emmintrin.h>
#endif // defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)

#if defined(_WIN32) || defined(USE_PTHREAD_SETAFFINITY)
# define THREAD_PINNING_SUPPORTED
#endif // defined(_WIN32) || defined(USE_PTHREAD_SETAFFINITY)

#include <gsl-lite/gsl-lite.hpp>  // for index, narrow_failfast<>(), narrow_cast<>()

#include <patton/buffer.hpp>        // for aligned_buffer<>
#include <patton/thread_squad.hpp>

#include <patton/detail/errors.hpp>


#ifdef _MSC_VER
# pragma warning(disable: 4324)  // structure was padded due to alignment specifier
#endif // _MSC_VER


namespace patton {

namespace detail {


#if defined(_WIN32)
//
// Usage: SetThreadName ((DWORD)-1, "MainThread");
//
constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;      // Must be 0x1000.
    LPCSTR szName;     // Pointer to name (in user address space).
    DWORD dwThreadId;  // Thread ID (-1 implies caller thread).
    DWORD dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

    // Sets the name of the thread with the given thread id for debugging purposes.
    // A thread id of -1 can be used to refer to the current thread.
void
setThreadNameViaException(DWORD dwThreadId, const char* threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadId = dwThreadId;
    info.dwFlags = 0;
#pragma warning(push)
# pragma warning(disable: 6320)  // C6320: exception-filter expression is the constant EXCEPTION_EXECUTE_HANDLER. This may mask exceptions that were not intended to be handled
# pragma warning(disable: 6322)  // C6322: empty __except block
    __try
    {
        ::RaiseException(MS_VC_EXCEPTION, 0, sizeof info / sizeof(ULONG_PTR), (ULONG_PTR*) &info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#pragma warning(pop)
}

typedef HRESULT SetThreadDescriptionType(HANDLE hThread, PCWSTR lpThreadDescription);

void
setCurrentThreadDescription(const wchar_t* threadDescription)
{
    static SetThreadDescriptionType* const setThreadDescription = []
    {
        HMODULE hKernel32 = ::GetModuleHandleW(L"kernel32.dll");
        gsl_Expects(hKernel32 != NULL);
        return (SetThreadDescriptionType*) ::GetProcAddress(hKernel32, "SetThreadDescriptionType");  // available since Windows 10 1607 or Windows Server 2016
    }();

    if (setThreadDescription != nullptr)
    {
        setThreadDescription(::GetCurrentThread(), threadDescription);
    }
    else
    {
        std::string narrowDesc;
        narrowDesc.resize(std::wcslen(threadDescription));
        for (std::ptrdiff_t i = 0, c = std::ptrdiff_t(narrowDesc.size()); i < c; ++i)
        {
            narrowDesc[i] = gsl::narrow_cast<char>(threadDescription[i]);
        }
        detail::setThreadNameViaException(DWORD(-1), narrowDesc.c_str());
    }
}

#elif defined(USE_PTHREAD_SETAFFINITY)
struct cpu_set
{
private:
    std::size_t cpuCount_;
    cpu_set_t* data_;

public:
    cpu_set()
        : cpuCount_(std::thread::hardware_concurrency())
    {
        data_ = CPU_ALLOC(cpuCount_);
        if (data_ == nullptr)
        {
            throw std::bad_alloc();
        }
        std::size_t lsize = size();
        CPU_ZERO_S(lsize, data_);
    }
    std::size_t
    size() const noexcept
    {
        return CPU_ALLOC_SIZE(cpuCount_);
    }
    const cpu_set_t*
    data() const noexcept
    {
        return data_;
    }
    void
    set_cpu_flag(std::size_t coreIdx)
    {
        gsl_Expects(coreIdx < cpuCount_);
        std::size_t lsize = size();
        CPU_SET_S(coreIdx, lsize, data_);
    }
    ~cpu_set()
    {
        CPU_FREE(data_);
    }
};

#endif // _WIN32

#ifdef THREAD_PINNING_SUPPORTED
[[maybe_unused]] static void
setThreadAffinity(std::thread::native_handle_type handle, std::size_t coreIdx)
{
# if defined(_WIN32)
    if (coreIdx >= sizeof(DWORD_PTR) * 8)  // bits per byte
    {
        throw std::range_error("cannot currently handle more than 8*sizeof(void*) CPUs on Windows");
    }
    detail::win32_assert(SetThreadAffinityMask((HANDLE) handle, DWORD_PTR(1) << coreIdx) != 0);
# elif defined(USE_PTHREAD_SETAFFINITY)
    cpu_set cpuSet;
    cpuSet.set_cpu_flag(coreIdx);
    detail::posix_check(::pthread_setaffinity_np((pthread_t) handle, cpuSet.size(), cpuSet.data()));
# else
#  error Unsupported operating system.
# endif
}

# ifdef USE_PTHREAD_SETAFFINITY
static void
setThreadAttrAffinity(pthread_attr_t& attr, std::size_t coreIdx)
{
    cpu_set cpuSet;
    cpuSet.set_cpu_flag(coreIdx);
    detail::posix_check(::pthread_attr_setaffinity_np(&attr, cpuSet.size(), cpuSet.data()));
}
# endif // USE_PTHREAD_SETAFFINITY
#endif // THREAD_PINNING_SUPPORTED

#if defined(_WIN32)
struct win32_handle_deleter
{
    void
    operator ()(HANDLE handle) const noexcept
    {
        ::CloseHandle(handle);
    }
};
using win32_handle = std::unique_ptr<std::remove_pointer_t<HANDLE>, win32_handle_deleter>;
#elif defined(USE_PTHREAD)
class pthread_handle
{
private:
    pthread_t handle_;

public:
    pthread_t
    get() const noexcept
    {
        return handle_;
    }
    pthread_t
    release() noexcept
    {
        return std::exchange(handle_, { });
    }

    explicit
    operator bool() const noexcept
    {
        return handle_ != pthread_t{ };
    }

    pthread_handle()
        : handle_{ }
    {
    }
    explicit pthread_handle(pthread_t _handle)
        : handle_(_handle)
    {
    }
    pthread_handle(pthread_handle&& rhs) noexcept
        : handle_(rhs.release())
    {
    }
    pthread_handle&
    operator =(pthread_handle&& rhs) noexcept
    {
        if (&rhs != this)
        {
            if (handle_ != pthread_t{ })
            {
                pthread_detach(handle_);
            }
            handle_ = rhs.release();
        }
        return *this;
    }
    ~pthread_handle()
    {
        if (handle_ != pthread_t{ })
        {
            ::pthread_detach(handle_);
        }
    }
};

struct PThreadAttr
{
    pthread_attr_t attr;

    PThreadAttr()
    {
        detail::posix_check(::pthread_attr_init(&attr));
    }
    ~PThreadAttr()
    {
        detail::posix_check(::pthread_attr_destroy(&attr));
    }
};
#else
# error Unsupported operating system.
#endif // _WIN32


#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
static inline void
pause()
{
    _mm_pause();
}
#else
static inline void
pause()
{
    [[maybe_unused]] volatile int v = 0;
}
#endif // defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)

constexpr int spinCount = 6;  // 4 or 6
constexpr int spinRep = 1;  // 2 or 1
constexpr int pauseCountExp = 9;
constexpr int yieldCountExp = 0;  // 6

template <typename T>
bool
wait_equal_exponential_backoff(std::atomic<T> const& a, T oldValue)
{
    int lspinCount = spinCount;
    if (a.load(std::memory_order_relaxed) != oldValue) return true;
    for (int i = 0; i < (1 << pauseCountExp); ++i)
    {
        int n = 1;
        for (int j = 0; j < lspinCount; ++j)
        {
            for (int r = 0; r < spinRep; ++r)
            {
                for (int k = 0; k < n; ++k)
                {
                    [[maybe_unused]] volatile int v = j + k;
                }
                if (a.load(std::memory_order_relaxed) != oldValue) return true;
            }
            n *= 2;
        }
        if (a.load(std::memory_order_relaxed) != oldValue) return true;
        detail::pause();
    }
    int lyieldCount = 1 << yieldCountExp;
    for (int i = 0; i < lyieldCount; ++i)
    {
        if (a.load(std::memory_order_relaxed) != oldValue) return true;
        std::this_thread::yield();
    }
    return false;
}

enum class wait_mode
{
    wait,
    spin_wait
};

template <typename T>
T
wait_and_load(
    std::atomic<T>& a, T oldValue,
    wait_mode waitMode = wait_mode::spin_wait)
{
    if (waitMode != wait_mode::spin_wait || !detail::wait_equal_exponential_backoff(a, oldValue))
    {
        a.wait(oldValue, std::memory_order_relaxed);
    }
    return a.load(std::memory_order_acquire);
}

template <typename T>
T
toggle_and_notify(
    std::atomic<T>& a)
{
    std::atomic_thread_fence(std::memory_order_release);

    T oldValue = a.load(std::memory_order_relaxed);
    T newValue = 1 ^ oldValue;
    a.store(newValue, std::memory_order_release);
    a.notify_one();
    return oldValue;
}


class os_thread
{
public:
#if defined(_WIN32)
    using thread_proc = unsigned (__stdcall *)(void* data);
#elif defined(USE_PTHREAD)
    using thread_proc = void* (*)(void* data);
#else
# error Unsupported operating system.
#endif // _WIN32

private:
#if defined(_WIN32)
    detail::win32_handle handle_;
#elif defined(USE_PTHREAD)
    detail::pthread_handle handle_;
#else
# error Unsupported operating system
#endif
    std::size_t coreAffinity_;

public:
    os_thread()
        : coreAffinity_{ std::size_t(-1) }
    {
    }

    bool
    is_running() const noexcept
    {
#if defined(_WIN32)
        return handle_ != nullptr;
#elif defined(USE_PTHREAD)
        return handle_.get() != pthread_t{ };
#else
# error Unsupported operating system
#endif
    }

    void
    set_core_affinity(std::size_t _coreAffinity)
    {
        gsl_Expects(!is_running());

        coreAffinity_ = _coreAffinity;
    }

    void
    fork(thread_proc proc, void* ctx)
    {
        gsl_Expects(!is_running());

#if defined(_WIN32)
        DWORD threadCreationFlags = coreAffinity_ != std::size_t(-1) ? CREATE_SUSPENDED : 0;
        handle_ = detail::win32_handle((HANDLE) ::_beginthreadex(NULL, 0, proc, ctx, threadCreationFlags, nullptr));
        detail::win32_assert(handle_ != nullptr);
        if (coreAffinity_ != std::size_t(-1))
        {
            detail::setThreadAffinity(handle_.get(), coreAffinity_);
            DWORD result = ::ResumeThread(handle_.get());
            detail::win32_assert(result != DWORD(-1));
        }
#elif defined(USE_PTHREAD)
        PThreadAttr attr;
# ifdef USE_PTHREAD_SETAFFINITY
        if (coreAffinity_ != std::size_t(-1))
        {
            detail::setThreadAttrAffinity(attr.attr, coreAffinity_);
        }
# endif // USE_PTHREAD_SETAFFINITY
        auto lhandle = pthread_t{ };
        detail::posix_check(::pthread_create(&lhandle, &attr.attr, proc, ctx));
        handle_ = pthread_handle(lhandle);
#else
# error Unsupported operating system
#endif
    }

    void
    join()
    {
        gsl_Expects(is_running());

#if defined(_WIN32)
        DWORD result = ::WaitForSingleObject(handle_.get(), INFINITE);
        detail::win32_assert(result != WAIT_FAILED);
        handle_.reset();
#elif defined(USE_PTHREAD)
        detail::posix_check(::pthread_join(handle_.get(), NULL));
        handle_.release();
#else
# error Unsupported operating system.
#endif
    }
};


#ifdef THREAD_PINNING_SUPPORTED
static std::size_t
get_hardware_thread_id(int threadIdx, int maxNumHardwareThreads, std::span<int const> hardwareThreadMappings)
{
    gsl_Expects(threadIdx >= 0);
    gsl_Expects(maxNumHardwareThreads > 0);
    gsl_Expects(hardwareThreadMappings.empty() || maxNumHardwareThreads <= std::ssize(hardwareThreadMappings));

    auto subidx = threadIdx % maxNumHardwareThreads;
    return gsl::narrow_failfast<std::size_t>(
        !hardwareThreadMappings.empty() ? hardwareThreadMappings[subidx]
        : subidx);
}
#endif // THREAD_PINNING_SUPPORTED


#if defined(_WIN32)
static unsigned __stdcall
thread_squad_thread_func(void* data);
#elif defined(USE_PTHREAD)
static void*
thread_squad_thread_func(void* data);
#else
# error Unsupported operating system.
#endif // _WIN32


class thread_squad_impl : public thread_squad_impl_base
{
public:
    class alignas(std::hardware_destructive_interference_size) thread_data
    {
        friend thread_squad_impl;

    private:
            // structure
        thread_squad_impl& threadSquad_;
        int threadIdx_;
        int numSubthreads_;

            // resources
        os_thread osThread_;

            // synchronization data
        std::atomic<int> incoming_;
        std::atomic<int> outgoing_;

    public:
        thread_data(thread_squad_impl& _impl) noexcept
            : threadSquad_(_impl),
              incoming_(0),
              outgoing_(0)
        {
        }

        void
        notify_subthreads() noexcept
        {
            int numThreadsToWake = threadSquad_.num_threads_for_task();
            threadSquad_.notify_subthreads(threadIdx_, numThreadsToWake);
        }

        void
        wait_for_subthreads() noexcept
        {
            int numThreadsToWaitFor = threadSquad_.num_threads_for_task();
            threadSquad_.wait_for_subthreads(threadIdx_, numThreadsToWaitFor);
        }

        thread_squad_task&
        task_wait() noexcept
        {
            auto currentSense = outgoing_.load(std::memory_order_relaxed);
            THREAD_SQUAD_DBG("patton thread squad, thread %d: waiting for incoming sense %d\n", threadIdx_, (1 ^ currentSense));
            detail::wait_and_load(incoming_, currentSense, threadSquad_.waitMode_);
            THREAD_SQUAD_DBG("patton thread squad, thread %d: processing task\n", threadIdx_);
            gsl_Assert(threadSquad_.task_ != nullptr);
            return *threadSquad_.task_;
        }

        void
        task_run(thread_squad_task& task) const noexcept
        {
            if (threadIdx_ < task.params.concurrency)
            {
                    // Like the parallel overloads of the standard algorithms, we terminate (implicitly) if an exception is thrown
                    // by a task because the semantics of exceptions in multiplexed actions are unclear.
                task.execute(threadSquad_, threadIdx_);
            }
        }

        void
        task_signal_completion() noexcept
        {
            THREAD_SQUAD_DBG("patton thread squad, thread %d: signaling outgoing sense %d\n", threadIdx_, (1 ^ outgoing_.load(std::memory_order_relaxed)));
            detail::toggle_and_notify(outgoing_);
        }

        int
        thread_idx() const noexcept
        {
            return threadIdx_;
        }

        static thread_data&
        from_thread_context(void* ctx)
        {
            gsl_Expects(ctx != nullptr);

            return *static_cast<thread_data*>(ctx);
        }
    };


private:
    static constexpr int treeBreadth = 8;

        // synchronization data
    aligned_buffer<thread_data, cache_line_alignment> threadData_;
    wait_mode waitMode_;

        // task-specific data
    detail::thread_squad_task* task_;


    int
    num_threads_for_task() const noexcept
    {
        return task_ == nullptr || task_->params.join_requested
            ? numThreads
            : task_->params.concurrency;
    }

    static int
    next_substride(int stride)
    {
        return (stride + (treeBreadth - 1)) / treeBreadth;
    }

    void
    init(int first, int last, int stride)
    {
        if (stride != 1)
        {
            int substride = next_substride(stride);
            for (int i = first; i < last; i += substride)
            {
                init(i, std::min(i + substride, last), substride);
            }
        }
        threadData_[first].numSubthreads_ = stride;
    }

    void
    notify_subthreads_impl(int first, int last, int stride)
    {
        while (stride != 1)
        {
            int substride = next_substride(stride);
            for (int i = first + substride; i < last; i += substride)
            {
                notify_thread(first, i);
            }
            last = std::min(first + substride, last);
            stride = substride;
        }
    }

    void
    wait_for_subthreads_impl(int first, int last, int stride)
    {
        int substride = next_substride(stride);
        if (stride != 1)
        {
            wait_for_subthreads_impl(first, std::min(first + substride, last), substride);
        }
        for (int i = first + substride; i < last; i += substride)
        {
            wait_for_thread(first, i, waitMode_);
        }
    }


public:
    thread_squad_impl(thread_squad::params const& params)
        : thread_squad_impl_base{ params.num_threads },
          threadData_(gsl::narrow_failfast<std::size_t>(params.num_threads), std::in_place, *this),
          waitMode_(params.spin_wait ? wait_mode::spin_wait : wait_mode::wait)
    {
        for (int i = 0; i < numThreads; ++i)
        {
            threadData_[i].threadIdx_ = i;
        }
#ifdef THREAD_PINNING_SUPPORTED
        if (params.pin_to_hardware_threads)
        {
            for (int i = 0; i < numThreads; ++i)
            {
                std::size_t coreAffinity = detail::get_hardware_thread_id(
                    i, params.max_num_hardware_threads, params.hardware_thread_mappings);
                THREAD_SQUAD_DBG("patton thread squad, thread -1: pin %d to CPU %d\n", i, int(coreAffinity));
                threadData_[i].osThread_.set_core_affinity(coreAffinity);
            }
        }
#endif // THREAD_PINNING_SUPPORTED

        init(0, numThreads, numThreads);
    }

    bool
    is_running() const noexcept
    {
        return threadData_[0].osThread_.is_running();
    }

    void
    fork_all_threads()
    {
        int numThreadsToWake = num_threads_for_task();
        for (int i = 0; i < numThreadsToWake; ++i)
        {
            THREAD_SQUAD_DBG("patton thread squad, thread -1: notifying %d with incoming sense %d\n", i, (1 ^ threadData_[i].incoming_.load(std::memory_order_relaxed)));
            detail::toggle_and_notify(threadData_[i].incoming_);
        }
        for (int i = 0; i < numThreads; ++i)
        {
            THREAD_SQUAD_DBG("patton thread squad, thread -1: forking %d\n", i);
            threadData_[i].osThread_.fork(thread_squad_thread_func, thread_context_for(i));
        }
    }

    void
    join_all_threads()
    {
        for (int i = 0; i < numThreads; ++i)
        {
            THREAD_SQUAD_DBG("patton thread squad, thread -1: joining %d\n", i);
            threadData_[i].osThread_.join();
        }
    }

    void
    notify_thread([[maybe_unused]] int callingThreadIdx, int targetThreadIdx)
    {
        THREAD_SQUAD_DBG("patton thread squad, thread %d: notifying %d with incoming sense %d\n", callingThreadIdx, targetThreadIdx, (1 ^ threadData_[targetThreadIdx].incoming_.load(std::memory_order_relaxed)));
        detail::toggle_and_notify(threadData_[targetThreadIdx].incoming_);
    }

    void
    wait_for_thread([[maybe_unused]] int callingThreadIdx, int targetThreadIdx, wait_mode waitMode = wait_mode::spin_wait)
    {
        int currentSense = threadData_[targetThreadIdx].incoming_.load(std::memory_order_relaxed);
        int prevSense = 1 ^ currentSense;
        THREAD_SQUAD_DBG("patton thread squad, thread %d: awaiting %d for outgoing sense %d\n", callingThreadIdx, targetThreadIdx, currentSense);
        detail::wait_and_load(threadData_[targetThreadIdx].outgoing_, prevSense, waitMode);
        THREAD_SQUAD_DBG("patton thread squad, thread %d: awaited %d\n", callingThreadIdx, targetThreadIdx);
    }

    void
    notify_subthreads(int callingThreadIdx, int _concurrency)
    {
        int stride = threadData_[callingThreadIdx].numSubthreads_;
        notify_subthreads_impl(callingThreadIdx, std::min(callingThreadIdx + stride, _concurrency), stride);
    }

    void
    wait_for_subthreads(int callingThreadIdx, int _concurrency)
    {
        int stride = threadData_[callingThreadIdx].numSubthreads_;
        wait_for_subthreads_impl(callingThreadIdx, std::min(callingThreadIdx + stride, _concurrency), stride);
    }

    void
    store_task(detail::thread_squad_task& task)
    noexcept  // We cannot really handle exceptions here.
    {
        task_ = &task;
    }

    void
    release_task()
    {
        task_ = nullptr;
    }

    void* thread_context_for(int threadIdx)
    {
        return &threadData_[threadIdx];
    }
};

static void
run_thread(thread_squad_impl::thread_data& threadData)
{
    int pass = 0;
    for (;;)
    {
        bool joinRequested;
        {
            auto& task = threadData.task_wait();  // must not be referenced after signaling completion!
            joinRequested = task.params.join_requested;
            THREAD_SQUAD_DBG("patton thread squad, thread %d: beginning pass %d\n", threadData.thread_idx(), pass);
            if (pass > 0)
            {
                threadData.notify_subthreads();
            }
            threadData.task_run(task);
            threadData.wait_for_subthreads();
        }
        threadData.task_signal_completion();

            // We only require `pass > 0` on all passes after the first one, so clamp the value to avoid UB and wraparound.
        if (pass < std::numeric_limits<int>::max())
        {
            ++pass;
        }

        if (joinRequested) break;
    }
    THREAD_SQUAD_DBG("patton thread squad, thread %d: exiting after %d passes\n", threadData.thread_idx(), pass);
}


//
//      Threads:            X X X X X X X X X     X X X X X X X X     X X X X X X X     X X X X X X
//                          X X X X X X X X X     X X X X X X X X     X X X X X X X     X X X X X X
//                          X X X X X X X X X     X X X X X X X X     X X X X X X X     X X X X X X
//
//      Wait sequence:      w-^   w-^   w-^       w-^ w-^ w-^ w-^     w-^ w-^ w-^       w-^   w-^
//                          w---^ w---^ w---^     w---^   w---^       w---^   w---^     w---^ w---^
//                          w-----^               w-------^           w-------^         w-----^
//                          w-----------^
//
//      Subthread counts:   9 1 1 3 1 1 3 1 1     8 1 2 1 4 1 2 1     8 1 2 1 4 1 2     6 1 1 3 1 1
//


static void
run(thread_squad_impl& self, detail::thread_squad_task& op)
noexcept  // We cannot really handle exceptions here.
{
    bool haveWork = (op.params.concurrency != 0) || (op.params.join_requested && self.is_running());

    if (haveWork)
    {
        if (!self.is_running())
        {
            THREAD_SQUAD_DBG("patton thread squad: setting up\n");
        }
        if (op.params.join_requested)
        {
            THREAD_SQUAD_DBG("patton thread squad: tearing down\n");
        }

        self.store_task(op);
        if (self.is_running())
        {
            self.notify_thread(-1, 0);
        }
        else
        {
            self.fork_all_threads();
        }
        if (!op.params.join_requested)
        {
            self.wait_for_thread(-1, 0, wait_mode::wait); // no spin wait in main thread
        }
        else
        {
            self.join_all_threads();
        }
        self.release_task();
    }
}

#if defined(_WIN32)
static unsigned __stdcall
thread_squad_thread_func(void* ctx)
{
    auto& threadData = thread_squad_impl::thread_data::from_thread_context(ctx);
    {
        wchar_t buf[64];
        std::swprintf(buf, sizeof buf / sizeof(wchar_t), L"patton thread squad, thread %d",
            threadData.thread_idx());
        detail::setCurrentThreadDescription(buf);
    }

    detail::run_thread(threadData);

    return 0;
}
#elif defined(USE_PTHREAD)
static void*
thread_squad_thread_func(void* ctx)
{
    auto& threadData = thread_squad_impl::thread_data::from_thread_context(ctx);
    {
#if defined(__linux__)
        char buf[64] { };
        std::sprintf(buf, "squad thrd %d",
            threadData.thread_idx());
        if (buf[15] != '\0')
        {
                // pthread on Linux restricts the thread name to 16 characters. (It will also reuse the
                // remainder of the previous name if we don't overwrite all 16 characters.)
            buf[12] = '.'; buf[13] = '.'; buf[14] = '.'; buf[15] = '\0';
        }
        detail::posix_check(::pthread_setname_np(::pthread_self(), buf));
#elif defined(__APPLE__)
        char buf[64];
        std::sprintf(buf, "patton thread squad, thread %d",
            threadData.thread_idx());
        detail::posix_check(::pthread_setname_np(buf));
#endif
    }

    detail::run_thread(threadData);

    return nullptr;
}
#else
# error Unsupported operating system.
#endif


struct alignas(std::hardware_destructive_interference_size) thread_squad_nop : thread_squad_task
{
public:
    ~thread_squad_nop() = default;

    void
    execute([[maybe_unused]] thread_squad_impl_base& impl, [[maybe_unused]] int i) override
    {
    }
};


void
thread_squad_impl_deleter::operator ()(thread_squad_impl_base* base)
{
    auto impl = static_cast<thread_squad_impl*>(base);
    auto memGuard = std::unique_ptr<thread_squad_impl>(impl);

    auto noOpTask = thread_squad_nop{ };
    noOpTask.params.join_requested = true;
    detail::run(*impl, noOpTask);
}


} // namespace detail


detail::thread_squad_handle
thread_squad::create(thread_squad::params p)
{
        // Replace placeholder arguments with appropriate default values.
    int hardwareConcurrency = gsl::narrow_failfast<int>(std::thread::hardware_concurrency());
    if (p.num_threads == 0)
    {
        p.num_threads = hardwareConcurrency;
    }
    if (p.max_num_hardware_threads == 0)
    {
        p.max_num_hardware_threads =
            !p.hardware_thread_mappings.empty() ? gsl::narrow_failfast<int>(p.hardware_thread_mappings.size())
          : hardwareConcurrency;
    }
    p.max_num_hardware_threads = std::max(p.max_num_hardware_threads, hardwareConcurrency);

        // Check system support for thread pinning.
#ifndef THREAD_PINNING_SUPPORTED
    if (p.pin_to_hardware_threads)
    {
            // Thread pinning not currently supported on this OS.
        throw std::system_error(std::make_error_code(std::errc::not_supported),
            "pinning to hardware threads is not implemented on this operating system");
    }
#endif // !THREAD_PINNING_SUPPORTED

    return detail::thread_squad_handle(new detail::thread_squad_impl(p));
}

void
thread_squad::do_run(detail::thread_squad_task& op)
{
    auto impl = static_cast<detail::thread_squad_impl*>(handle_.get());
    if (!op.params.join_requested)
    {
        detail::run(*impl, op);
    }
    else
    {
        auto memGuard = std::unique_ptr<detail::thread_squad_impl>(impl);
        detail::thread_squad_handle(std::move(handle_)).release();
        detail::run(*impl, op);
    }
}


} // namespace patton
