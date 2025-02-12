
#ifndef INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_
#define INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_


#include <memory>  // for unique_ptr<>


namespace patton::detail {


struct thread_squad_impl_base
{
    int numThreads;
};
class thread_squad_impl;

struct thread_squad_impl_deleter
{
    void
    operator ()(thread_squad_impl_base* base);
};

using thread_squad_handle = std::unique_ptr<detail::thread_squad_impl_base, thread_squad_impl_deleter>;


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_
