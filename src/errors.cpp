
#include <cerrno>

#if defined(_WIN32)
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <Windows.h> // GetLastError()
#endif // defined(_WIN32)

#include <patton/detail/errors.hpp>


namespace patton::detail {


[[noreturn]] void
posix_raise(int errorCode)
{
    throw std::system_error(std::error_code(errorCode, std::generic_category()));
}

[[noreturn]] void
posix_raise_last_error()
{
    detail::posix_raise(errno);
}

#ifdef _WIN32
[[noreturn]] void
win32_raise(std::uint32_t errorCode)
{
    throw std::system_error(std::error_code(errorCode, std::system_category())); // TODO: does this work correctly?
}

[[noreturn]] void
win32_raise_last_error()
{
    detail::win32_raise(::GetLastError());
}
#endif // _WIN32


} // namespace patton::detail
