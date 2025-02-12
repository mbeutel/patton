
#ifndef INCLUDED_PATTON_DETAIL_MEMORY_HPP_
#define INCLUDED_PATTON_DETAIL_MEMORY_HPP_


#include <limits>
#include <memory>       // for allocator_traits<>
#include <cstdint>      // for uint32_t
#include <cstddef>      // for size_t, ptrdiff_t, max_align_t
#include <utility>      // for forward<>()
#include <type_traits>  // for integral_constant<>, declval<>(), void_t<>, negation<>

#include <gsl-lite/gsl-lite.hpp>

#include <patton/detail/transaction.hpp>


namespace patton {

namespace gsl = ::gsl_lite;

namespace detail {


template <typename T> struct remove_extent_only;
template <typename T> struct remove_extent_only<T[]> { using type = T; };
template <typename T, std::ptrdiff_t N> struct remove_extent_only<T[N]> { using type = T; };
template <typename T> using remove_extent_only_t = typename remove_extent_only<T>::type;

template <typename T> struct extent_only;
template <typename T> struct extent_only<T[]> : std::integral_constant<std::ptrdiff_t, 0> { };
template <typename T, std::ptrdiff_t N> struct extent_only<T[N]> : std::integral_constant<std::ptrdiff_t, N> { };
template <typename T> constexpr std::ptrdiff_t extent_only_v = extent_only<T>::value;

template <typename A, typename = void> struct has_member_provides_static_alignment : std::false_type { };
template <typename A> struct has_member_provides_static_alignment<A, std::void_t<decltype(A::provides_static_alignment(std::declval<std::size_t>()))>>
    : std::is_convertible<decltype(A::provides_static_alignment(std::declval<std::size_t>())), bool> { };


std::size_t
constexpr floor_2p(std::size_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> (sizeof(std::size_t) > 4 ? 32 : 0);  // We write the expression this way to sidestep a warning that occurs when using `if constexpr`.
    x |= x >> (sizeof(std::size_t) > 8 ? 64 : 0);  // Sure, we can support 128-bit size_t, but, really?
    return (x + 1) >> 1; // assumes that x < powi(2, sizeof(std::size_t) * 8 - 1), which is given because the most significant bits have special meaning and have been masked out
}


    // = large_page_alignment | page_alignment | cache_line_alignment
constexpr std::size_t special_alignments = std::size_t(-1) & ~(std::size_t(-1) >> 3);


std::size_t
constexpr raw_alignment_in_bytes(std::size_t a) noexcept
{
    return std::max(std::size_t(1), floor_2p(a));
}

std::size_t
alignment_in_bytes(std::size_t a) noexcept;

template <typename T>
bool
constexpr is_alignment_power_of_2(T value) noexcept
{
    return value > 0
        && (value & (value - 1)) == 0;
}


bool
constexpr provides_static_alignment(std::size_t alignmentProvided, std::size_t alignmentRequested) noexcept
{
    std::size_t basicAlignmentProvided = alignmentProvided & ~special_alignments;
    std::size_t basicAlignmentRequested = alignmentRequested & ~special_alignments;
    if ((alignmentProvided & special_alignments) != 0)
    {
            // Page alignment must also guarantee cache line alignment.
        basicAlignmentProvided |= std::hardware_constructive_interference_size;  // assumed to be a lower bound for the cache line size
    }

    return detail::raw_alignment_in_bytes(basicAlignmentProvided) >= detail::raw_alignment_in_bytes(basicAlignmentRequested)
        && (alignmentProvided & special_alignments) >= (alignmentRequested & special_alignments);
}


std::size_t
lookup_special_alignments(std::size_t a) noexcept;

bool
inline provides_dynamic_alignment(std::size_t alignmentProvided, std::size_t alignmentRequested) noexcept
{
    return detail::alignment_in_bytes(alignmentProvided) >= detail::alignment_in_bytes(alignmentRequested);
}


template <typename T, typename A, typename... ArgsT>
T*
allocate(A& alloc, ArgsT&&... args)
{
    T* ptr = std::allocator_traits<A>::allocate(alloc, 1);
    auto transaction = detail::make_transaction(
        std::negation<std::is_nothrow_constructible<T, ArgsT...>>{ },
        [&alloc, ptr]
        {
            std::allocator_traits<A>::deallocate(alloc, ptr, 1);
        });
    std::allocator_traits<A>::construct(alloc, ptr, std::forward<ArgsT>(args)...);
    transaction.commit();
    return ptr;
}

template <typename T, typename A, typename SizeC>
T*
allocate_array_impl(std::true_type /*isNothrowDefaultConstructible*/, A& alloc, SizeC sizeC)
{
    T* ptr = std::allocator_traits<A>::allocate(alloc, std::size_t(sizeC));
    for (std::ptrdiff_t i = 0; i != std::ptrdiff_t(sizeC); ++i)
    {
        std::allocator_traits<A>::construct(alloc, &ptr[i]);
    }
    return ptr;
}
template <typename T, typename A, typename SizeC>
T*
allocate_array_impl(std::false_type /*isNothrowDefaultConstructible*/, A& alloc, SizeC sizeC)
{
    T* ptr = std::allocator_traits<A>::allocate(alloc, std::size_t(sizeC));
    std::ptrdiff_t i = 0;
    auto transaction = detail::make_transaction(
        [&alloc, ptr, &i, sizeC]
        {
            for (std::ptrdiff_t j = i - 1; j >= 0; --j)
            {
                std::allocator_traits<A>::destroy(alloc, &ptr[j]);
            }
            std::allocator_traits<A>::deallocate(alloc, ptr, std::size_t(sizeC));
        });
    for (; i != std::ptrdiff_t(sizeC); ++i)
    {
        std::allocator_traits<A>::construct(alloc, &ptr[i]);
    }
    transaction.commit();
    return ptr;
}
template <typename T, typename A, typename SizeC>
T*
allocate_array(A& alloc, SizeC sizeC)
{
    return detail::allocate_array_impl<T>(std::is_nothrow_default_constructible<T>{ }, alloc, sizeC);
}


void
set_out_of_bounds_write_trap(void* data, std::size_t size, std::size_t allocSize) noexcept;
[[nodiscard]] bool
check_out_of_bounds_write_trap(void const* data, std::size_t size, std::size_t allocSize) noexcept;

void*
aligned_alloc(std::size_t size, std::size_t alignment);
void
aligned_free(void* data, std::size_t size, std::size_t alignment) noexcept;

void*
large_page_alloc(std::size_t size);
void
large_page_free(void* data, std::size_t size) noexcept;

void*
page_alloc(std::size_t size);
void
page_free(void* data, std::size_t size) noexcept;


template <typename T, std::size_t Alignment, typename A, bool NeedAlignment>
class aligned_allocator_adaptor_base;
template <typename T, std::size_t Alignment, typename A>
class aligned_allocator_adaptor_base<T, Alignment, A, true> : public A
{
public:
    using A::A;

    [[nodiscard]] bool
    static constexpr provides_static_alignment(std::size_t a) noexcept
    {
        return detail::provides_static_alignment(Alignment, a);
    }

    [[nodiscard]] T*
    allocate(std::size_t n)
    {
        std::size_t a = detail::alignment_in_bytes(Alignment | alignof(T));
        if (n >= std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc{ };  // overflow
        std::size_t nbData = n * sizeof(T);
        std::size_t nbAlloc = nbData + a + sizeof(void*) - 1 + 2*sizeof(std::uint32_t);
        if (nbAlloc < nbData) throw std::bad_alloc{ };  // overflow

        using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;
        auto byteAllocator = ByteAllocator(*this); // may not throw
        void* mem = std::allocator_traits<ByteAllocator>::allocate(byteAllocator, nbAlloc);
        void* alignedMem = mem;
        void* alignResult = std::align(a, nbData + sizeof(void*), alignedMem, nbAlloc);
        gsl_Expects(alignResult != nullptr && nbAlloc >= nbData + sizeof(void*));  // should not happen

            // Store pointer to actual allocation at end of buffer. Use `memcpy()` so we don't have to worry about alignment.
        std::memcpy(static_cast<char*>(alignResult) + nbData + 2*sizeof(std::uint32_t), &mem, sizeof(void*));

        detail::set_out_of_bounds_write_trap(alignResult, nbData, nbData + 2*sizeof(std::uint32_t));

        return static_cast<T*>(alignResult);
    }
    void
    deallocate(T* ptr, std::size_t n) noexcept
    {
        std::size_t a = detail::alignment_in_bytes(Alignment | alignof(T));
        std::size_t nbData = n * sizeof(T);  // cannot overflow due to preceding check in `allocate()`
        std::size_t nbAlloc = nbData + a + sizeof(void*) - 1 +  + 2*sizeof(std::uint32_t);  // cannot overflow due to preceding check in `allocate()`

            // Retrieve pointer to actual allocation from end of buffer. Use `memcpy()` so we don't have to worry about alignment.
        void* mem;
        std::memcpy(&mem, reinterpret_cast<char*>(ptr) + nbData + 2*sizeof(std::uint32_t), sizeof(void*));

        if (!detail::check_out_of_bounds_write_trap(ptr, nbData, nbData + 2*sizeof(std::uint32_t)))
        {
            gsl_FailFast();  // an out-of-bounds write has damaged this allocation
        }

        using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;
        auto byteAllocator = ByteAllocator(*this); // may not throw
        std::allocator_traits<ByteAllocator>::deallocate(byteAllocator, static_cast<char*>(mem), nbAlloc);
    }
};
template <typename T, std::size_t Alignment, typename A>
class aligned_allocator_adaptor_base<T, Alignment, A, false> : public A
{
public:
    using A::A;
};


} // namespace detail

} // namespace patton


#endif // INCLUDED_PATTON_DETAIL_MEMORY_HPP_
