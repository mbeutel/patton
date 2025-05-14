
#ifndef INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_
#define INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_


#include <memory>       // for unique_ptr<>
#include <optional>
#include <concepts>
#include <type_traits>  // for invoke_result<>


namespace patton::detail {


template <typename F, typename T>
concept reduction = std::invocable<F, T, T> && std::same_as<std::invoke_result_t<F, T, T>, T>;

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

    virtual void execute(thread_squad_impl_base& impl, int i, int numRunningThreads) noexcept = 0;
    virtual void merge(int iDst, int iSrc) noexcept;
};

    // We define our own value here instead of referring to `std::hardware_destructive_interference_size` because that can change
    // based on compiler flags and thus cause ABI breakage (which is why GCC issues a warning about it).
static constexpr std::size_t destructive_interference_size = 64;

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4324)  // structure was padded due to alignment specifier
#endif // _MSC_VER
template <typename TaskContextT, typename ActionT>
class alignas(destructive_interference_size) thread_squad_action : public thread_squad_task
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
    execute(thread_squad_impl_base& impl, int i, int numRunningThreads) noexcept override
    {
        auto laction = action_;
        auto ctx = task_context_factory::template make_task_context<TaskContextT>(impl, i, numRunningThreads);
        laction(ctx);
    }
};

template <typename T>
struct alignas(destructive_interference_size) thread_reduce_data
{
    std::optional<T> value;
};

template <typename TaskContextT, typename TransformFuncT, typename T, typename ReduceOpT>
class alignas(destructive_interference_size) thread_squad_transform_reduce_operation : public thread_squad_task
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
    
    ReduceOpT&
    reduce_op()
    {
        return reduce_;
    }

    void
    execute(thread_squad_impl_base& impl, int i, int numRunningThreads) noexcept override
    {
        auto ltransform = transform_;
        auto ctx = task_context_factory::template make_task_context<TaskContextT>(impl, i, numRunningThreads);
        gsl_Assert(!subthreadData_[i].value.has_value());
        subthreadData_[i].value = ltransform(ctx);
    }
    void
    merge(int iDst, int iSrc) noexcept override
    {
        auto lreduce = reduce_;
        subthreadData_[iDst].value = lreduce(std::move(subthreadData_[iDst].value.value()), std::move(subthreadData_[iSrc].value.value()));
        subthreadData_[iSrc].value.reset();
    }
};


struct task_context_synchronizer
{
public:
    ~task_context_synchronizer() = default;  // intentionally non-virtual – the lifetime of the object is not managed through a base class pointer

    virtual void* sync_data() noexcept;
    virtual void collect(void const* src) noexcept;
    virtual void broadcast(void* dst) noexcept;
};

template <typename T>
struct alignas(destructive_interference_size) thread_sync_reduce_data
{
    T value;
};

template <typename T, typename R>
struct alignas(destructive_interference_size) thread_sync_reduce_transform_data
{
    T value;
    std::optional<R> result = std::nullopt;
};

template <typename T, typename ReduceOpT>
struct alignas(destructive_interference_size) task_context_reduce_synchronizer : task_context_synchronizer
{
    thread_sync_reduce_data<T> data;
    ReduceOpT& reduce;

    task_context_reduce_synchronizer(T&& _value, ReduceOpT& _reduce)
        : data{
              .value = std::move(_value)
          },
          reduce(_reduce)
    {
    }

    void*
    sync_data() noexcept override
    {
        return &data;
    }
    void
    collect(void const* src) noexcept override
    {
        data.value = reduce(std::move(data.value), std::move(static_cast<thread_sync_reduce_data<T> const*>(src)->value));
    }
    void
    broadcast(void* dst) noexcept override
    {
        static_cast<thread_sync_reduce_data<T>*>(dst)->value = data.value;
    }
};
template <typename T, typename ReduceOpT, typename R>
struct alignas(destructive_interference_size) task_context_reduce_transform_synchronizer : task_context_synchronizer
{
    thread_sync_reduce_transform_data<T, R> data;
    ReduceOpT& reduce;

    task_context_reduce_transform_synchronizer(T&& _value, ReduceOpT& _reduce)
        : data{
              .value = std::move(_value)
          },
          reduce(_reduce)
    {
    }

    void*
    sync_data() noexcept override
    {
        return &data;
    }
    void
    collect(void const* src) noexcept override
    {
        data.value = reduce(std::move(data.value), std::move(static_cast<thread_sync_reduce_transform_data<T, R> const*>(src)->value));
    }
    void
    broadcast(void* dst) noexcept override
    {
        static_cast<thread_sync_reduce_transform_data<T, R>*>(dst)->result = data.result.value();
    }
};
#ifdef _MSC_VER
# pragma warning(pop)
#endif // _MSC_VER


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_THREAD_SQUAD_HPP_
