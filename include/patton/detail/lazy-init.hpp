
#ifndef INCLUDED_PATTON_DETAIL_LAZY_INIT_HPP_
#define INCLUDED_PATTON_DETAIL_LAZY_INIT_HPP_


#include <atomic>


namespace patton::detail {


template <typename T, typename F>
T
lazy_init(
    std::atomic<T>& value,
    T defaultValue,
    F&& initFunc,
    std::memory_order releaseOrder = std::memory_order_seq_cst)  // A release fence would be sufficient here, but we use sequential
                                                                 // consistency by default to have other threads see the results of
                                                                 // our hard work as soon as possible.
{
    T result = value.load(std::memory_order_relaxed);
    if (result == defaultValue)
    {
        result = initFunc();
        value.store(result, std::memory_order_relaxed);
        std::atomic_thread_fence(releaseOrder);
    }
    return result;
}


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_LAZY_INIT_HPP_
