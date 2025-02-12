
#ifndef INCLUDED_PATTON_NEW_HPP_
#define INCLUDED_PATTON_NEW_HPP_


#include <cstddef> // for size_t


namespace patton {


    // It is controversial whether C++17 `std::hardware_{constructive|destructive}_interference_size` should really be a constexpr
    // value, cf. the discussion at https://lists.llvm.org/pipermail/cfe-dev/2018-May/058073.html and the proposal paper P0154R1
    // at http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0154r1.html.
    //
    // My current take is that the constexpr constants should be reasonable but not necessarily accurate values to minimize the
    // impact of false sharing. To determine an accurate value at runtime, call `hardware_cache_line_size()`.


    //
    // Reports the operating system's large page size in bytes, or 0 if large pages are not available or not supported.
    //
[[nodiscard]] std::size_t
hardware_large_page_size() noexcept;

    //
    // Reports the operating system's page size in bytes.
    //
[[nodiscard]] std::size_t
hardware_page_size() noexcept;

    //
    // Reports the CPU architecture's cache line size in bytes.
    //
[[nodiscard]] std::size_t
hardware_cache_line_size() noexcept;


} // namespace patton


#endif // INCLUDED_PATTON_NEW_HPP_
