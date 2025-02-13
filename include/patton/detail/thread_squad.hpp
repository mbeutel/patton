
#ifndef INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_
#define INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_


#include <new>
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



struct task_context_factory
{
    template <typename TaskContextT>
    static TaskContextT
    make_task_context(detail::thread_squad_impl_base& impl, int threadIdx)
    {
        return TaskContextT(impl, threadIdx);
    }
};

struct thread_squad_task
{
protected:
    ~thread_squad_task() = default;  // intentionally non-virtual – the lifetime of the object is not managed through a base class pointer

public:
    int concurrency = 0;
    bool join_requested = false;

    virtual void
    execute(thread_squad_impl_base& impl, int i) = 0;
};

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4324)  // structure was padded due to alignment specifier
#endif // _MSC_VER
template <typename TaskContextT, typename ActionT>
class alignas(std::hardware_destructive_interference_size) thread_squad_action : public thread_squad_task
{
private:
    ActionT action_;

public:
    thread_squad_action(ActionT&& _action)
        : action_(std::move(_action))
    {
    }
    ~thread_squad_action() = default;

    void
    execute(thread_squad_impl_base& impl, int i) override
    {
        action_(task_context_factory::template make_task_context<TaskContextT>(impl, i));
    }
};
#ifdef _MSC_VER
# pragma warning(pop)
#endif // _MSC_VER


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_
