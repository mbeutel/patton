
#ifndef INCLUDED_PATTON_BUFFER_HPP_
#define INCLUDED_PATTON_BUFFER_HPP_


#include <new>           // for bad_alloc
#include <span>
#include <memory>        // for unique_ptr<>, allocator_traits<>
#include <cstddef>       // for size_t, ptrdiff_t
#include <utility>       // for move(), forward<>(), exchange(), in_place
#include <type_traits>   // for is_const<>, is_volatile<>, is_reference<>, is_nothrow_constructible<>, enable_if<>, negation<>
#include <system_error>  // for errc

#include <gsl-lite/gsl-lite.hpp>  // for gsl_Expects(), owner<>

#include <patton/memory.hpp>  // for aligned_allocator<>, aligned_allocator_adaptor<>

#include <patton/detail/buffer.hpp>
#include <patton/detail/arithmetic.hpp>   // for try_multiply_unsigned(), try_ceili()
#include <patton/detail/transaction.hpp>


namespace patton {


namespace gsl = ::gsl_lite;


    //
    // Buffer with aligned elements.
    //ᅟ
    //ᅟ    auto threadData = aligned_buffer<ThreadData, cache_line_alignment>(numThreads);
    //ᅟ    // every `threadData[i]` has cache-line alignment => no false sharing
    //ᅟ
    // Supports special alignment values such as `cache_line_alignment`.
    // Multiple alignment requirements can be combined using bitmask operations, e.g. `cache_line_alignment | alignof(T)`.
    //
template <typename T, std::size_t Alignment, typename A = aligned_allocator<T, Alignment>>
class aligned_buffer : private aligned_allocator_adaptor<T, Alignment | alignof(T), A>
{
    static_assert(!std::is_const<T>::value && !std::is_volatile<T>::value, "buffer element type must not have cv qualifiers");
    static_assert(!std::is_reference<T>::value, "buffer element type must not be a reference");

    struct internal_constructor { };

public:
    using allocator_type = aligned_allocator_adaptor<T, Alignment | alignof(T), A>;

private:
    using byte_allocator_ = typename std::allocator_traits<allocator_type>::template rebind_alloc<char>;
    static constexpr bool allocator_is_default_constructible_ = std::is_default_constructible<allocator_type>::value;

    gsl::owner<char*> data_;
    std::size_t size_; // # elements
    std::size_t bytesPerElement_;

    std::size_t
    static computeBytesPerElement()
    {
        auto bytesPerElementR = detail::try_ceili(sizeof(T), detail::alignment_in_bytes(Alignment | alignof(T)));
        if (bytesPerElementR.ec != std::errc{ }) throw std::bad_alloc{ };
        return bytesPerElementR.value;
    }

    template <typename... Ts>
    aligned_buffer(internal_constructor, std::size_t _size, allocator_type _allocator, Ts&&... args)
        : allocator_type(std::move(_allocator)), size_(_size), bytesPerElement_(computeBytesPerElement())
    {
        if (_size == 0)
        {
            data_ = nullptr;
        }
        else
        {
            auto numBytesR = detail::try_multiply_unsigned(_size, bytesPerElement_);
            if (numBytesR.ec != std::errc{ }) throw std::bad_alloc{ };
            std::size_t numBytes = numBytesR.value;

            auto alloc = byte_allocator_(get_allocator());
            data_ = std::allocator_traits<byte_allocator_>::allocate(alloc, numBytes);

            std::size_t numElementsConstructed;
            auto transaction = detail::make_transaction(
                std::negation<std::is_nothrow_constructible<T, Ts...>>{ },
                [this, &numElementsConstructed]
                {
                    detail::destroy_aligned_buffer<T>(data_, get_allocator(), numElementsConstructed, bytesPerElement_);
                    auto alloc = byte_allocator_(get_allocator());
                    std::allocator_traits<byte_allocator_>::deallocate(alloc, data_, size_ * bytesPerElement_);
                });
            detail::construct_aligned_buffer<T>(data_, get_allocator(), numElementsConstructed, _size, bytesPerElement_,
                std::is_nothrow_constructible<T, Ts...>{ }, std::forward<Ts>(args)...);
            transaction.commit();
        }
    }
    void
    destroy_and_free() noexcept
    {
        detail::destroy_aligned_buffer<T>(data_, get_allocator(), size_, bytesPerElement_);
        auto alloc = byte_allocator_(get_allocator());
        std::allocator_traits<byte_allocator_>::deallocate(alloc, data_, size_ * bytesPerElement_);
    }

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = T const*;
    using reference = T&;
    using const_reference = T const&;

    using iterator = detail::aligned_buffer_iterator<T>;
    using const_iterator = detail::aligned_buffer_iterator<T const>;

    template <typename U = int, std::enable_if_t<allocator_is_default_constructible_, U> = 0>
    constexpr aligned_buffer() noexcept
        : allocator_type{ }, data_(nullptr), size_(0), bytesPerElement_(0)
    {
    }
    aligned_buffer(allocator_type _alloc) noexcept
        : allocator_type(std::move(_alloc)), data_(nullptr), size_(0), bytesPerElement_(0)
    {
    }
    template <typename U = int, std::enable_if_t<allocator_is_default_constructible_, U> = 0>
    explicit aligned_buffer(std::size_t _size)
        : aligned_buffer(internal_constructor{ }, _size, { })
    {
    }
    template <typename U = int, std::enable_if_t<allocator_is_default_constructible_, U> = 0>
    explicit aligned_buffer(std::size_t _size, T const& _value)
        : aligned_buffer(internal_constructor{ }, _size, { }, _value)
    {
    }
    explicit aligned_buffer(std::size_t _size, A _alloc)
        : aligned_buffer(internal_constructor{ }, _size, std::move(_alloc))
    {
    }
    explicit aligned_buffer(std::size_t _size, T const& _value, A _alloc)
        : aligned_buffer(internal_constructor{ }, _size, std::move(_alloc), _value)
    {
    }
    template <typename... Ts,
              typename U = int, std::enable_if_t<allocator_is_default_constructible_, U> = 0>
    explicit aligned_buffer(std::size_t _size, std::in_place_t, Ts&&... _args)
        : aligned_buffer(internal_constructor{ }, _size, { }, std::forward<Ts>(_args)...)
    {
    }
    template <typename... Ts>
    explicit aligned_buffer(std::size_t _size, A _alloc, std::in_place_t, Ts&&... _args)
        : aligned_buffer(internal_constructor{ }, _size, std::move(_alloc), std::forward<Ts>(_args)...)
    {
    }

    constexpr aligned_buffer(aligned_buffer&& rhs) noexcept
        : data_(std::exchange(rhs.data_, { })),
          size_(std::exchange(rhs.size_, { })),
          bytesPerElement_(rhs.bytesPerElement_)
    {
    }
    constexpr aligned_buffer&
    operator =(aligned_buffer&& rhs) noexcept
    {
        if (this != &rhs)
        {
            if (data_ != nullptr)
            {
                destroy_and_free();
            }
            data_ = std::exchange(rhs.data_, { });
            size_ = std::exchange(rhs.size_, { });
            bytesPerElement_ = std::exchange(rhs.bytesPerElement_, 0);
        }
        return *this;
    }

    ~aligned_buffer()
    {
        if (data_ != nullptr)
        {
            destroy_and_free();
        }
    }

    [[nodiscard]] allocator_type
    get_allocator() const noexcept
    {
        return *this;
    }

    [[nodiscard]] std::size_t
    size() const noexcept
    {
        return size_;
    }
    [[nodiscard]] reference
    operator [](std::size_t i)
    {
        gsl_Expects(i < size_);

        return *reinterpret_cast<pointer>(&data_[i * bytesPerElement_]);
    }
    [[nodiscard]] const_reference
    operator [](std::size_t i) const
    {
        gsl_Expects(i < size_);

        return *reinterpret_cast<pointer>(&data_[i * bytesPerElement_]);
    }

    [[nodiscard]] iterator
    begin() noexcept
    {
        return { data_, 0, bytesPerElement_ };
    }
    [[nodiscard]] const_iterator
    begin() const noexcept
    {
        return { data_, 0, bytesPerElement_ };
    }
    [[nodiscard]] iterator
    end() noexcept
    {
        return { data_, size_, bytesPerElement_ };
    }
    [[nodiscard]] const_iterator
    end() const noexcept
    {
        return { data_, size_, bytesPerElement_ };
    }

    [[nodiscard]] constexpr bool
    empty() const noexcept
    {
        return size_ == 0;
    }

    [[nodiscard]] reference
    front()
    {
        gsl_Expects(!empty());
        return (*this)[0];
    }
    [[nodiscard]] const_reference
    front() const
    {
        gsl_Expects(!empty());
        return (*this)[0];
    }
    [[nodiscard]] reference
    back()
    {
        gsl_Expects(!empty());
        return (*this)[size() - 1];
    }
    [[nodiscard]] const_reference
    back() const
    {
        gsl_Expects(!empty());
        return (*this)[size() - 1];
    }
};


    //
    // Two-dimensional buffer with aligned rows.
    //ᅟ
    //ᅟ    auto threadData = aligned_row_buffer<float, cache_line_alignment>(rows, cols);
    //ᅟ    // every `threadData[i][0]` has cache-line alignment => no false sharing
    //ᅟ
    // Supports special alignment values such as `cache_line_alignment`.
    // Multiple alignment requirements can be combined using bitmask operations, e.g. `cache_line_alignment | alignof(T)`.
    //
template <typename T, std::size_t Alignment, typename A = aligned_allocator<T, Alignment>>
class aligned_row_buffer : private aligned_allocator_adaptor<T, Alignment | alignof(T), A>
{
    static_assert(!std::is_const<T>::value && !std::is_volatile<T>::value, "buffer element type must not have cv qualifiers");
    static_assert(!std::is_reference<T>::value, "buffer element type must not be a reference");

    struct internal_constructor { };

public:
    using allocator_type = aligned_allocator_adaptor<T, Alignment | alignof(T), A>;

private:
    using byte_allocator_ = typename std::allocator_traits<allocator_type>::template rebind_alloc<char>;
    static constexpr bool allocator_is_default_constructible_ = std::is_default_constructible<allocator_type>::value;

    gsl::owner<char*> data_;
    std::size_t rows_;
    std::size_t cols_;
    std::size_t bytesPerRow_;

    template <typename... Ts>
    aligned_row_buffer(internal_constructor, std::size_t _rows, std::size_t _cols, allocator_type _allocator, Ts&&... args)
        : allocator_type(std::move(_allocator)), rows_(_rows), cols_(_cols)
    {
        auto rawBytesPerRowR = detail::try_multiply_unsigned(sizeof(T), _cols);
        auto bytesPerRowR = detail::try_ceili(rawBytesPerRowR.value, detail::alignment_in_bytes(Alignment | alignof(T)));
        auto numBytesR = detail::try_multiply_unsigned(_rows, bytesPerRowR.value);
        if (rawBytesPerRowR.ec != std::errc{ } || bytesPerRowR.ec != std::errc{ } || numBytesR.ec != std::errc{ }) throw std::bad_alloc{ };
        bytesPerRow_ = bytesPerRowR.value;

        if (_rows == 0 || _cols == 0)
        {
            data_ = nullptr;
        }
        else
        {
            auto alloc = byte_allocator_(get_allocator());
            data_ = std::allocator_traits<byte_allocator_>::allocate(alloc, numBytesR.value);

            std::size_t numElementsConstructed;
            auto transaction = detail::make_transaction(
                std::negation<std::is_nothrow_constructible<T, Ts...>>{ },
                [this, &numElementsConstructed]
                {
                    detail::destroy_aligned_row_buffer<T>(data_, get_allocator(), rows_, cols_, bytesPerRow_, numElementsConstructed);
                    auto alloc = byte_allocator_(get_allocator());
                    std::allocator_traits<byte_allocator_>::deallocate(alloc, data_, rows_ * bytesPerRow_);
                });
            detail::construct_aligned_row_buffer<T>(data_, get_allocator(), numElementsConstructed, _rows, _cols, bytesPerRow_,
                std::is_nothrow_constructible<T, Ts...>{ }, std::forward<Ts>(args)...);
            transaction.commit();
        }
    }
    void destroy_and_free() noexcept
    {
        detail::destroy_aligned_row_buffer<T>(data_, get_allocator(), rows_, cols_, bytesPerRow_);
        auto alloc = byte_allocator_(get_allocator());
        std::allocator_traits<byte_allocator_>::deallocate(alloc, data_, rows_ * bytesPerRow_);
    }

public:
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = std::span<T>;
    using const_reference = std::span<T const>;

    using iterator = detail::aligned_row_buffer_iterator<T>;
    using const_iterator = detail::aligned_row_buffer_iterator<T const>;

    template <typename U = int, std::enable_if_t<allocator_is_default_constructible_, U> = 0>
    aligned_row_buffer() noexcept
        : allocator_type{ }, data_(nullptr), rows_(0), cols_(0), bytesPerRow_(0)
    {
    }
    aligned_row_buffer(allocator_type _alloc) noexcept
        : allocator_type(std::move(_alloc)), data_(nullptr), rows_(0), cols_(0), bytesPerRow_(0)
    {
    }
    template <typename U = int, std::enable_if_t<allocator_is_default_constructible_, U> = 0>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols)
        : aligned_row_buffer(internal_constructor{ }, _rows, _cols, { })
    {
    }
    template <typename U = int, std::enable_if_t<allocator_is_default_constructible_, U> = 0>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, T const& _value)
        : aligned_row_buffer(internal_constructor{ }, _rows, _cols, { }, _value)
    {
    }
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, A _alloc)
        : aligned_row_buffer(internal_constructor{ }, _rows, _cols, std::move(_alloc))
    {
    }
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, T const& _value, A _alloc)
        : aligned_row_buffer(internal_constructor{ }, _rows, _cols, std::move(_alloc), _value)
    {
    }
    template <typename... Ts,
              typename U = int, std::enable_if_t<allocator_is_default_constructible_, U> = 0>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, std::in_place_t, Ts&&... _args)
        : aligned_row_buffer(internal_constructor{ }, _rows, _cols, { }, std::forward<Ts>(_args)...)
    {
    }
    template <typename... Ts>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, A _alloc, std::in_place_t, Ts&&... _args)
        : aligned_row_buffer(internal_constructor{ }, _rows, _cols, std::move(_alloc), std::forward<Ts>(_args)...)
    {
    }

    constexpr aligned_row_buffer(aligned_row_buffer&& rhs) noexcept
        : data_(std::exchange(rhs.data_, { })),
          rows_(std::exchange(rhs.rows_, { })),
          cols_(std::exchange(rhs.cols_, { })),
          bytesPerRow_(std::exchange(rhs.bytesPerRow_, { }))
    {
    }
    constexpr aligned_row_buffer&
    operator =(aligned_row_buffer&& rhs) noexcept
    {
        if (this != &rhs)
        {
            if (data_ != nullptr)
            {
                destroy_and_free();
            }
            data_ = std::exchange(rhs.data_, { });
            rows_ = std::exchange(rhs.rows_, { });
            cols_ = std::exchange(rhs.cols_, { });
            bytesPerRow_ = std::exchange(rhs.bytesPerRow_, { });
        }
    }

    ~aligned_row_buffer()
    {
        if (data_ != nullptr)
        {
            destroy_and_free();
        }
    }

    [[nodiscard]] allocator_type
    get_allocator() const noexcept
    {
        return *this;
    }

    [[nodiscard]] std::size_t
    rows() const noexcept
    {
        return rows_;
    }
    [[nodiscard]] std::size_t
    columns() const noexcept
    {
        return cols_;
    }

    [[nodiscard]] std::size_t
    size() const noexcept
    {
        return rows();
    }
    [[nodiscard]] std::span<T>
    operator [](std::size_t i)
    {
        gsl_Expects(i < rows_);

        return { reinterpret_cast<T*>(&data_[i * bytesPerRow_]), cols_ };
    }
    [[nodiscard]] std::span<T const>
    operator [](std::size_t i) const
    {
        gsl_Expects(i < rows_);

        return { reinterpret_cast<T*>(&data_[i * bytesPerRow_]), cols_ };
    }

    [[nodiscard]] iterator
    begin() noexcept
    {
        return { data_, 0, cols_, bytesPerRow_ };
    }
    [[nodiscard]] const_iterator
    begin() const noexcept
    {
        return { data_, 0, cols_, bytesPerRow_ };
    }
    [[nodiscard]] iterator
    end() noexcept
    {
        return { data_, rows_, cols_, bytesPerRow_ };
    }
    [[nodiscard]] const_iterator
    end() const noexcept
    {
        return { data_, rows_, cols_, bytesPerRow_ };
    }

    [[nodiscard]] constexpr bool
    empty() const noexcept
    {
        return rows_ == 0;
    }

    [[nodiscard]] reference
    front()
    {
        gsl_Expects(!empty());
        return (*this)[0];
    }
    [[nodiscard]] const_reference
    front() const
    {
        gsl_Expects(!empty());
        return (*this)[0];
    }
    [[nodiscard]] reference
    back()
    {
        gsl_Expects(!empty());
        return (*this)[size() - 1];
    }
    [[nodiscard]] const_reference
    back() const
    {
        gsl_Expects(!empty());
        return (*this)[size() - 1];
    }
};


} // namespace patton


#endif // INCLUDED_PATTON_BUFFER_HPP_
