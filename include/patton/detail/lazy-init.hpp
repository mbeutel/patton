
#ifndef INCLUDED_PATTON_DETAIL_LAZY_INIT_HPP_
#define INCLUDED_PATTON_DETAIL_LAZY_INIT_HPP_


#include <atomic>


namespace patton::detail {


template <typename T, typename F>
T
lazy_init(
    std::atomic<T>& value,
    T defaultValue,
    F&& initFunc)
{
    T result = value.load(std::memory_order_relaxed);
    if (result == defaultValue)
    {
        result = initFunc();
        value.store(result, std::memory_order_release);
    }
    return result;
}


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_LAZY_INIT_HPP_
