
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
    make_task_context(detail::thread_squad_impl_base& impl, int threadIdx, int numRunningThreads)
    {
        return TaskContextT(impl, threadIdx, numRunningThreads);
    }
};

struct thread_squad_task_params
{
    int concurrency = 0;
    bool join_requested = false;
};

struct thread_squad_task
{
protected:
    ~thread_squad_task() = default;  // intentionally non-virtual – the lifetime of the object is not managed through a base class pointer

public:
    thread_squad_task_params params;

    virtual void
    execute(thread_squad_impl_base& impl, int i, int numRunningThreads) = 0;
    virtual void
    merge(int iDst, int iSrc);
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
    execute(thread_squad_impl_base& impl, int i, int numRunningThreads) override
    {
        auto laction = action_;
        laction(task_context_factory::template make_task_context<TaskContextT>(impl, i, numRunningThreads));
    }
};


template <typename T>
struct alignas(std::hardware_destructive_interference_size) thread_reduce_data
{
    T value;
};

template <typename TaskContextT, typename TransformFuncT, typename T, typename ReduceOpT>
class alignas(std::hardware_destructive_interference_size) thread_squad_transform_reduce_operation : public thread_squad_task
{
private:
    TransformFuncT transform_;
    ReduceOpT reduce_;
    thread_reduce_data<T>* subthreadData_;

public:
    thread_squad_transform_reduce_operation(TransformFuncT&& _transform, ReduceOpT&& _reduce, thread_reduce_data<T>* _subthreadData)
        : transform_(std::move(_transform)), reduce_(std::move(_reduce)), subthreadData_(_subthreadData)
    {
    }
    ~thread_squad_transform_reduce_operation() = default;

    void
    execute(thread_squad_impl_base& impl, int i, int numRunningThreads) override
    {
        auto ltransform = transform_;
        subthreadData_[i].value = ltransform(task_context_factory::template make_task_context<TaskContextT>(impl, i, numRunningThreads));
    }
    void
    merge(int iDst, int iSrc) override
    {
        subthreadData_[iDst].value = reduce_(std::move(subthreadData_[iDst].value), std::move(subthreadData_[iSrc].value));
    }
};
#ifdef _MSC_VER
# pragma warning(pop)
#endif // _MSC_VER


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_
