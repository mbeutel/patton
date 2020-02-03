
#include <new>
#include <cstddef>   // for size_t, align_val_t
#include <algorithm> // for max()

#include <sysmakeshift/new.hpp>    // for hardware_large_page_size(), hardware_page_size(), hardware_cache_line_size()
#include <sysmakeshift/memory.hpp>

//#ifdef __linux__
//# include <sys/mman.h> // for madvise()
//#endif // __linux__

namespace sysmakeshift {

namespace detail {


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

//#ifdef __linux__
//void advise_large_pages(void* addr, std::size_t size)
//{
//    if (hardware_large_page_size() == 0) return; // large pages not supported
//    madvise(addr, size, MADV_HUGEPAGE);
//}
//#endif // __linux__

static std::size_t
floor_2p(std::size_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> (sizeof(std::size_t) > 4 ? 32 : 0);
    x |= x >> (sizeof(std::size_t) > 8 ? 64 : 0); // Sure, we can support 128-bit size_t, but, really?
    return (x + 1) >> 1; // assumes that x < powi(2, sizeof(std::size_t) * 8 - 1), which is given because the most significant bits have special meaning and have been masked out
}

std::size_t
lookup_special_alignments(std::size_t a) noexcept
{
    if ((a & large_page_alignment) != 0)
    {
            // This is without effect if `hardware_large_page_size()` returns 0, i.e. if large pages are not supported.
        a |= hardware_large_page_size();
    }
    if ((a & page_alignment) != 0)
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


} // namespace detail

} // namespace sysmakeshift
