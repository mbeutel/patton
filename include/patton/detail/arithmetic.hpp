
#ifndef INCLUDED_PATTON_DETAIL_ARITHMETIC_HPP_
#define INCLUDED_PATTON_DETAIL_ARITHMETIC_HPP_


#include <limits>
#include <type_traits>   // for common_type<>
#include <system_error>  // for errc


namespace patton::detail {


template <typename T>
struct arithmetic_result
{
    T value;
    std::errc ec;
};

template <typename A, typename B>
constexpr arithmetic_result<std::common_type_t<A, B>>
try_multiply_unsigned(A a, B b)
{
    // Borrowed from slowmath.

    using V = std::common_type_t<A, B>;

    if (b > 0 && a > std::numeric_limits<V>::max() / b) return { { }, std::errc::value_too_large };
    return { a * b, { } };
}

    // Computes ⌈x ÷ d⌉ ∙ d for x ∊ ℕ₀, d ∊ ℕ, d ≠ 0.
template <typename X, typename D>
constexpr arithmetic_result<std::common_type_t<X, D>>
try_ceili(X x, D d)
{
    // Borrowed from slowmath.

    using V = std::common_type_t<X, D>;

        // We have the following identities:
        //
        //     x = ⌊x ÷ d⌋ ∙ d + x mod d
        //     ⌈x ÷ d⌉ = ⌊(x + d - 1) ÷ d⌋ = ⌊(x - 1) ÷ d⌋ + 1
        //
        // Assuming x ≠ 0, we can derive the form
        //
        //     ⌈x ÷ d⌉ ∙ d = x + d - (x - 1) mod d - 1 .

    if (x == 0) return { 0, { } };
    V dx = d - (x - 1) % d - 1;
    if (x > std::numeric_limits<V>::max() - dx) return { { }, std::errc::value_too_large };
    return { x + dx, { } };
}


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_ARITHMETIC_HPP_
