
#ifndef INCLUDED_PATTON_THREAD_HPP_
#define INCLUDED_PATTON_THREAD_HPP_


#include <span>


namespace patton {


    //
    // Reports the number of concurrent physical cores available.
    //ᅟ
    // Unlike `std::thread::hardware_concurrency()`, this only returns the number of cores, not the number of hardware threads.
    // On systems with simultaneous multithreading ("hyper-threading") enabled, `std::thread::hardware_concurrency()` typically
    // returns some multiple of `physical_concurrency()`.
    //
[[nodiscard]] unsigned
physical_concurrency() noexcept;


    //
    // Returns a list of thread ids, where each thread is situated on a distinct physical core. Can be used to select thread
    // affinity if no simultaneous multithreading ("hyper-threading") is desired.
    //
    // Returns an empty span if thread affinity is not supported by the OS.
    //
[[nodiscard]] std::span<int const>
physical_core_ids() noexcept;


} // namespace patton


#endif // INCLUDED_PATTON_THREAD_HPP_
