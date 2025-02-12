
#ifndef INCLUDED_PATTON_DETAIL_ERRORS_HPP_
#define INCLUDED_PATTON_DETAIL_ERRORS_HPP_


#include <cstdint>       // for uint32_t
#include <system_error>


namespace patton::detail {


[[noreturn]] void
posix_raise(int errorCode);

[[noreturn]] void
posix_raise_last_error();

void
inline posix_check(int errorCode)
{
    if (errorCode != 0)
    {
        detail::posix_raise(errorCode);
    }
}

void
inline posix_assert(bool success)
{
    if (!success)
    {
        detail::posix_raise_last_error();
    }
}

#ifdef _WIN32
[[noreturn]] void
win32_raise(std::uint32_t errorCode);

[[noreturn]] void
win32_raise_last_error();

void
inline win32_check(std::uint32_t errorCode)
{
    if (errorCode != 0) detail::win32_raise(errorCode);
}
void
inline win32_assert(int success)
{
    if (!success) detail::win32_raise_last_error();
}
#endif // _WIN32


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_ERRORS_HPP_
