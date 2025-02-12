
#include <new>          // for operator new, bad_alloc
#include <cerrno>
#include <cstddef>      // for size_t, align_val_t
#include <algorithm>    // for min(), max()
#include <system_error>

#ifdef _WIN32
# include <Windows.h>
#else
// assume POSIX
# include <sys/mman.h> // for mmap(), munmap(), madvise()
#endif

#include <gsl-lite/gsl-lite.hpp>  // for gsl_Assert(), gsl_FailFast()

#include <patton/new.hpp>    // for hardware_large_page_size(), hardware_page_size(), hardware_cache_line_size()
#include <patton/memory.hpp>

#include <patton/detail/arithmetic.hpp> // for try_ceili()
#include <patton/detail/errors.hpp>


namespace patton::detail {


constexpr std::size_t maxTrapCount = 4;
constexpr std::uint32_t trapVal = 0xDEADBEEFu;
void
set_out_of_bounds_write_trap(void* data, std::size_t size, std::size_t allocSize) noexcept
{
    std::uint32_t ltrapVal = trapVal;

        // Store known data pattern to just-out-of-bounds area.
    std::size_t trapCount = std::min(maxTrapCount, (allocSize - size)/sizeof(std::uint32_t));
    for (std::size_t i = 0; i < trapCount; ++i)
    {
        std::memcpy(static_cast<char*>(data) + size + i*sizeof(std::uint32_t), &ltrapVal, sizeof(std::uint32_t));
    }
}
[[nodiscard]] bool
check_out_of_bounds_write_trap(void const* data, std::size_t size, std::size_t allocSize) noexcept
{
        // Check known data pattern in just-out-of-bounds area to detect inadvertent tampering (poor man's ASan).
    std::size_t trapCount = std::min(maxTrapCount, (allocSize - size)/sizeof(std::uint32_t));
    for (std::size_t i = 0; i < trapCount; ++i)
    {
        std::uint32_t observedVal;
        std::memcpy(&observedVal, static_cast<char const*>(data) + size + i*sizeof(std::uint32_t), sizeof(std::uint32_t));
        if (observedVal != trapVal)
        {
            return false;  // an out-of-bounds write has damaged this allocation
        }
    }
    return true;
}

void*
aligned_alloc(std::size_t size, std::size_t alignment)
{
    return ::operator new(size, std::align_val_t(alignment));
}
void
aligned_free(void* data, std::size_t size, std::size_t alignment) noexcept
{
    return ::operator delete(data, size, std::align_val_t(alignment));
}

void*
large_page_alloc([[maybe_unused]] std::size_t size)
{
#if defined(__linux__) || defined(_WIN32)
    std::size_t largePageSize = hardware_large_page_size();
    if (largePageSize != 0)
    {
        auto fullSizeR = detail::try_ceili(size, largePageSize);
        if (fullSizeR.ec != std::errc{ })
        {
            throw std::bad_alloc{ };
        }
# if defined(__linux__)
        void* data = ::mmap(NULL, fullSizeR.value, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        detail::posix_assert(data != nullptr);
        int ec = ::madvise(data, fullSizeR.value, MADV_HUGEPAGE);
        if (ec != 0)
        {
            ec = errno;
            ::munmap(data, fullSizeR.value);
            detail::posix_raise(ec);
        }
        detail::set_out_of_bounds_write_trap(data, size, fullSizeR.value);
        return data;
# elif defined(_WIN32)
        // TODO: should we do anything about the privileges here? (cf. https://docs.microsoft.com/en-us/windows/win32/memory/large-page-support, https://stackoverflow.com/a/42380052)
        void* data = ::VirtualAlloc(NULL, fullSizeR.value, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
        detail::win32_assert(data != nullptr);
        detail::set_out_of_bounds_write_trap(data, size, fullSizeR.value);
        return data;
# endif
    }
#endif // defined(__linux__) || defined(_WIN32)
    throw std::system_error(std::make_error_code(std::errc::not_supported));
}
void
large_page_free([[maybe_unused]] void* data, [[maybe_unused]] std::size_t size) noexcept
{
#if defined(__linux__) || defined(_WIN32)
    auto allocSizeR = detail::try_ceili(size, hardware_large_page_size());
    gsl_Assert(allocSizeR.ec == std::errc{ });
    if (!detail::check_out_of_bounds_write_trap(data, size, allocSizeR.value))
    {
        gsl_FailFast();  // an out-of-bounds write has damaged this allocation
    }
# if defined(__linux__)
    detail::posix_assert(::munmap(data, size) == 0);
# elif defined(_WIN32)
    detail::win32_assert(::VirtualFree(data, 0, MEM_RELEASE));
# endif
#else // !(defined(__linux__) || defined(_WIN32))
    std::terminate(); // should never happen because `large_page_alloc()` would already have thrown
#endif
}

void*
page_alloc(std::size_t size)
{
    std::size_t pageSize = hardware_page_size();
    gsl_Assert(pageSize != 0);
    auto fullSizeR = detail::try_ceili(size, pageSize);
    if (fullSizeR.ec != std::errc{ })
    {
        throw std::bad_alloc{ };
    }
    void* data;
#if defined(_WIN32)
    data = ::VirtualAlloc(NULL, fullSizeR.value, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    detail::win32_assert(data != nullptr);
#else  // assume POSIX
    data = ::mmap(NULL, fullSizeR.value, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    detail::posix_assert(data != nullptr);
# if defined(__linux__)
    int ec = ::madvise(data, fullSizeR.value, MADV_NOHUGEPAGE);
    if (ec != 0)
    {
        ec = errno;
        ::munmap(data, fullSizeR.value);
        detail::posix_raise(ec);
    }
# endif // defined(__linux__)
#endif
    detail::set_out_of_bounds_write_trap(data, size, fullSizeR.value);
    return data;
}
void
page_free(void* data, std::size_t size) noexcept
{
    auto allocSizeR = detail::try_ceili(size, hardware_page_size());
    gsl_Assert(allocSizeR.ec == std::errc{ });
    if (!detail::check_out_of_bounds_write_trap(data, size, allocSizeR.value))
    {
        gsl_FailFast();  // an out-of-bounds write has damaged this allocation
    }
#if defined(_WIN32)
    detail::win32_assert(::VirtualFree(data, 0, MEM_RELEASE));
#else // assume POSIX
    detail::posix_assert(::munmap(data, size) == 0);
#endif
}


std::size_t
lookup_special_alignments(std::size_t a) noexcept
{
    if ((a & large_page_alignment) != 0)
    {
            // This is without effect if `hardware_large_page_size()` returns 0, i.e. if large pages are not supported.
        a |= hardware_large_page_size();
    }
    if ((a & (large_page_alignment | page_alignment)) != 0)
    {
        a |= hardware_page_size();
    }
    if ((a & cache_line_alignment) != 0)
    {
        a |= hardware_cache_line_size();
    }

        // Mask out flags with special meaning.
    a &= ~(large_page_alignment | page_alignment | cache_line_alignment);

    return a;
}
std::size_t
alignment_in_bytes(std::size_t a) noexcept
{
    return std::max(std::size_t(1), floor_2p(lookup_special_alignments(a)));
}


} // namespace patton::detail
