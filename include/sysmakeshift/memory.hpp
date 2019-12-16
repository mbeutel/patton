﻿
#ifndef INCLUDED_SYSMAKESHIFT_MEMORY_HPP_
#define INCLUDED_SYSMAKESHIFT_MEMORY_HPP_


#include <new>         // for align_val_t, bad_alloc
#include <limits>
#include <cstddef>     // for size_t, ptrdiff_t
#include <cstdlib>     // for calloc(), free()
#include <cstring>     // for memcpy()
#include <memory>      // for unique_ptr<>, allocator<>, allocator_traits<>, align()
#include <utility>     // for forward<>()
#include <type_traits> // for is_nothrow_default_constructible<>, enable_if<>, is_same<>, remove_cv<>

#include <gsl/gsl-lite.hpp> // for Expects(), gsl_NODISCARD

#include <sysmakeshift/detail/memory.hpp>
#include <sysmakeshift/detail/type_traits.hpp> // for can_instantiate<>
#include <sysmakeshift/detail/enum.hpp>        // for SYSMAKESHIFT_DEFINE_ENUM_BITMASK_OPERATORS()


namespace sysmakeshift
{


    //ᅟ
    // Allocator adaptor that interposes construct() calls to convert value initialization into default initialization.
    // cf. https://stackoverflow.com/a/21028912
    //
template <typename T, typename A = std::allocator<T>>
class default_init_allocator : public A
{
public:
    using A::A;

    template <typename U>
    struct rebind
    {
        using other = default_init_allocator<U, typename std::allocator_traits<A>::template rebind_alloc<U>>;
    };

    template <typename U>
    void construct(U* ptr)
    noexcept(std::is_nothrow_default_constructible<U>::value)
    {
        ::new(static_cast<void*>(ptr)) U;
    }
    template <typename U, typename...ArgsT>
    void construct(U* ptr, ArgsT&&... args)
    {
        std::allocator_traits<A>::construct(static_cast<A&>(*this), ptr, std::forward<ArgsT>(args)...);
    }
};


    //ᅟ
    // Allocator that always returns zero-initialized memory.
    //
template <typename T>
class zero_init_allocator
{
public:
    using value_type = T;

    constexpr zero_init_allocator(void) noexcept
    {
    }
    template <typename U>
    constexpr zero_init_allocator(zero_init_allocator<U> const&) noexcept
    {
    }

    gsl_NODISCARD value_type* allocate(std::size_t n)
    {
        auto mem = std::calloc(n, sizeof(value_type));
        if (mem == nullptr) throw std::bad_alloc{ };
        return static_cast<value_type*>(mem);
    }

    void deallocate(value_type* p, std::size_t) noexcept
    {
        std::free(p);
    }
};

template <typename T, typename U>
gsl_NODISCARD bool operator ==(zero_init_allocator<T> const&, zero_init_allocator<U> const&) noexcept
{
    return true;
}
template <typename T, typename U>
gsl_NODISCARD bool operator !=(zero_init_allocator<T> const& x, zero_init_allocator<U> const& y) noexcept
{
    return !(x == y);
}


    //ᅟ
    // Represents an alignment to use for aligned allocations.
    // In addition to the special values, any positive integer that is a power of 2 may be cast to `alignment`.
    // Multiple alignment requirements can be combined using bitmask operations, e.g. `alignment::cache_line | alignment(alignof(T))`.
    //
enum class alignment : std::size_t
{
    large_page = std::size_t(1) << (sizeof(std::size_t) * 8u - 1u),
    page       = std::size_t(1) << (sizeof(std::size_t) * 8u - 2u),
    cache_line = std::size_t(1) << (sizeof(std::size_t) * 8u - 3u),
    none = 1
};
SYSMAKESHIFT_DEFINE_ENUM_BITMASK_OPERATORS(alignment)


    //ᅟ
    // Allocator that aligns memory allocations by the given size using the default allocator, i.e. global `operator new()` with `std::align_val_t`.
    //
template <typename T, alignment Alignment>
class aligned_default_allocator
{
public:
    using value_type = T;

    template <typename U>
    struct rebind
    {
        using other = aligned_default_allocator<U, Alignment>;
    };

    constexpr aligned_default_allocator(void) noexcept
    {
    }
    template <typename U>
    constexpr aligned_default_allocator(aligned_default_allocator<U, Alignment> const&) noexcept
    {
    }

    gsl_NODISCARD T* allocate(std::size_t n)
    {
        std::size_t a = detail::alignment_in_bytes(Alignment | alignment(alignof(T)));
        if (n >= std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc{ }; // overflow
        std::size_t nbData = n * sizeof(T);
        return static_cast<T*>(detail::aligned_alloc(nbData, a));
    }
    void deallocate(T* ptr, std::size_t n) noexcept
    {
        std::size_t a = detail::alignment_in_bytes(Alignment | alignment(alignof(T)));
        std::size_t nbData = n * sizeof(T); // cannot overflow due to preceding check in allocate()
        detail::aligned_free(ptr, nbData, a);
    }
};

template <typename T, typename U, alignment Alignment>
gsl_NODISCARD bool operator ==(aligned_default_allocator<T, Alignment>, aligned_default_allocator<U, Alignment>) noexcept
{
    return true;
}
template <typename T, typename U, alignment Alignment>
gsl_NODISCARD bool operator !=(aligned_default_allocator<T, Alignment> x, aligned_default_allocator<U, Alignment> y) noexcept
{
    return !(x == y);
}


    //ᅟ
    // Allocator adaptor that aligns memory allocations by the given size.
    //
template <typename T, alignment Alignment, typename A>
class aligned_allocator : public A
{
public:
    using A::A; // TODO: does this allow constructing an aligned allocator from the underlying allocator?

    template <typename U>
    struct rebind
    {
        using other = aligned_allocator<U, Alignment, typename std::allocator_traits<A>::template rebind_alloc<U>>;
    };

    gsl_NODISCARD T* allocate(std::size_t n)
    {
        std::size_t a = detail::alignment_in_bytes(Alignment | alignment(alignof(T)));
        if (n >= std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc{ }; // overflow
        std::size_t nbData = n * sizeof(T);
        std::size_t nbAlloc = nbData + a + sizeof(void*) - 1;
        if (nbAlloc < nbData) throw std::bad_alloc{ }; // overflow

        using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;
        auto byteAllocator = ByteAllocator(*this); // may not throw
        void* mem = std::allocator_traits<ByteAllocator>::allocate(byteAllocator, nbAlloc);
        void* alignedMem = mem;
        void* alignResult = std::align(a, nbData + sizeof(void*), alignedMem, nbAlloc);
        Expects(alignResult != nullptr && nbAlloc >= nbData + sizeof(void*)); // should not happen

            // Store pointer to actual allocation at end of buffer. Use `memcpy()` so we don't have to worry about alignment.
        std::memcpy(static_cast<char*>(alignResult) + nbData, &mem, sizeof(void*));

        return static_cast<T*>(alignResult);
    }
    void deallocate(T* ptr, std::size_t n) noexcept
    {
        std::size_t a = detail::alignment_in_bytes(Alignment | alignment(alignof(T)));
        std::size_t nbData = n * sizeof(T); // cannot overflow due to preceding check in allocate()
        std::size_t nbAlloc = nbData + a + sizeof(void*) - 1; // cannot overflow due to preceding check in allocate()

            // Retrieve pointer to actual allocation from end of buffer. Use `memcpy()` so we don't have to worry about alignment.
        void* mem;
        std::memcpy(&mem, reinterpret_cast<char*>(ptr) + nbData, sizeof(void*));
        
        using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;
        auto byteAllocator = ByteAllocator(*this); // may not throw
        std::allocator_traits<ByteAllocator>::deallocate(byteAllocator, static_cast<char*>(mem), nbAlloc);
    }
};
template <typename T, alignment Alignment>
class aligned_allocator<T, Alignment, aligned_default_allocator<T, Alignment>> : public aligned_default_allocator<T, Alignment>
{
public:
    using aligned_default_allocator<T, Alignment>::aligned_default_allocator; // TODO: does this allow constructing an aligned allocator from the underlying allocator?

    template <typename U>
    struct rebind
    {
        using other = aligned_allocator<U, Alignment, typename std::allocator_traits<aligned_default_allocator<T, Alignment>>::template rebind_alloc<U>>;
    };
};


    //ᅟ
    // Deleter for `std::unique_ptr<>` which supports custom allocators.
    //ᅟ
    //ᅟ    auto p1 = allocate_unique<float>(MyAllocator<float>{ }, 3.14159f); // returns `std::unique_ptr<float, allocator_deleter<float, MyAllocator<float>>>`
    //ᅟ    auto p2 = allocate_unique<float[]>(MyAllocator<float>{ }, 42); // returns `std::unique_ptr<float[], allocator_deleter<float[], MyAllocator<float>>>`
    //ᅟ    auto p3 = allocate_unique<float[42]>(MyAllocator<float>{ }); // returns `std::unique_ptr<float[42], allocator_deleter<float[42], MyAllocator<float>>>`
    //
template <typename T, typename A>
class allocator_deleter : private A // for EBO
{
public:
    allocator_deleter(const A& _alloc)
        : A(_alloc)
    {
    }
    void operator ()(T* ptr)
    {
        std::allocator_traits<A>::destroy(*this, ptr);
        std::allocator_traits<A>::deallocate(*this, ptr, 1);
    }
};
template <typename T, typename A>
class allocator_deleter<T[], A> : private A // for EBO
{
private:
    std::size_t size_;

public:
    allocator_deleter(const A& _alloc, std::size_t _size)
        : A(_alloc), size_(_size)
    {
    }
    void operator ()(T ptr[])
    {
        for (std::ptrdiff_t i = 0, n = std::ptrdiff_t(size_); i != n; ++i)
        {
            std::allocator_traits<A>::destroy(*this, &ptr[i]);
        }
        std::allocator_traits<A>::deallocate(*this, ptr, size_);
    }
};
template <typename T, std::ptrdiff_t N, typename A>
class allocator_deleter<T[N], A> : private A // for EBO
{
public:
    allocator_deleter(const A& _alloc)
        : A(_alloc)
    {
    }
    void operator ()(T ptr[])
    {
        for (std::ptrdiff_t i = 0; i != N; ++i)
        {
            std::allocator_traits<A>::destroy(*this, &ptr[i]);
        }
        std::allocator_traits<A>::deallocate(*this, ptr, N);
    }
};


    //ᅟ
    // Allocates an object of type `T` with the given allocator, constructs it with the given arguments and returns a `std::unique_ptr<>` to the object.
    //ᅟ
    //ᅟ    auto p = allocate_unique<float>(MyAllocator<float>{ }, 3.14159f);
    //ᅟ    // returns `std::unique_ptr<float, allocator_deleter<float, MyAllocator<float>>>`
    //
template <typename T, typename A, typename... ArgsT>
std::enable_if_t<!detail::can_instantiate_v<detail::remove_extent_only_t, T>, std::unique_ptr<T, allocator_deleter<T, A>>>
allocate_unique(A alloc, ArgsT&&... args)
{
    using NCVT = std::remove_cv_t<T>;
    static_assert(std::is_same<typename std::allocator_traits<A>::value_type, NCVT>::value, "allocator has mismatching value_type");

    NCVT* ptr = detail::allocate<NCVT>(std::is_nothrow_constructible<NCVT, ArgsT...>{ }, alloc, std::forward<ArgsT>(args)...);
    return { ptr, { std::move(alloc) } };
}

    //ᅟ
    // Allocates a fixed-size array of type `ArrayT` with the given allocator, default-constructs the elements and returns a `std::unique_ptr<>` to the array.
    //ᅟ
    //ᅟ    auto p = allocate_unique<float[42]>(MyAllocator<float>{ });
    //ᅟ    // returns `std::unique_ptr<float[42], allocator_deleter<float[42], MyAllocator<float>>>`
    //
template <typename ArrayT, typename A>
std::enable_if_t<detail::extent_only<ArrayT>::value != 0, std::unique_ptr<ArrayT, allocator_deleter<ArrayT, A>>>
allocate_unique(A alloc)
{
    using T = std::remove_cv_t<detail::remove_extent_only_t<ArrayT>>;
    static_assert(std::is_same<typename std::allocator_traits<A>::value_type, T>::value, "allocator has mismatching value_type");

    T* ptr = detail::allocate_array<T>(std::is_nothrow_default_constructible<T>{ }, alloc, detail::extent_only<ArrayT>{ });
    return { ptr, { std::move(alloc) } };
}

    //ᅟ
    // Allocates an array of type `ArrayT` with the given allocator, default-constructs the elements and returns a `std::unique_ptr<>` to the array.
    //ᅟ
    //ᅟ    auto p = allocate_unique<float[]>(MyAllocator<float>{ }, 42);
    //ᅟ    // returns `std::unique_ptr<float[], allocator_deleter<float[], MyAllocator<float>>>`
    //
template <typename ArrayT, typename A>
std::enable_if_t<detail::extent_only<ArrayT>::value == 0, std::unique_ptr<ArrayT, allocator_deleter<ArrayT, A>>>
allocate_unique(A alloc, std::size_t size)
{
    using T = std::remove_cv_t<detail::remove_extent_only_t<ArrayT>>;
    static_assert(std::is_same<typename std::allocator_traits<A>::value_type, T>::value, "allocator has mismatching value_type");

    T* ptr = detail::allocate_array<T>(std::is_nothrow_default_constructible<T>{ }, alloc, size);
    return { ptr, { std::move(alloc), size } };
}


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_MEMORY_HPP_
