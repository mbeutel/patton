
#ifndef INCLUDED_PATTON_THREAD_SQUAD_HPP_
#define INCLUDED_PATTON_THREAD_SQUAD_HPP_


#include <span>
#include <utility>     // for move()
#include <concepts>
#include <functional>  // for function<>, identity

#include <gsl-lite/gsl-lite.hpp>  // for not_null<>

#include <patton/detail/thread_squad.hpp>


namespace patton {


namespace gsl = ::gsl_lite;


    //
    // Simple thread squad with support for thread core affinity.
    //
class thread_squad
{
public:
        //
        // Thread squad parameters.
        //
    struct params
    {
            //
            // How many threads to fork. A value of 0 indicates "as many as hardware threads are available".
            //
        int num_threads = 0;

            //
            // Controls whether threads are pinned to hardware threads, i.e. whether threads have a core affinity. Helps maintain
            // data locality.
            //
        bool pin_to_hardware_threads = false;

            //
            // Controls whether thread synchronization uses spin waiting with exponential backoff.
            //
        bool spin_wait = false;

            //
            // Maximal number of hardware threads to pin threads to. A value of 0 indicates "as many as possible".
            //ᅟ
            // If `max_num_hardware_threads` is 0 and `hardware_thread_mappings` is non-empty, `hardware_thread_mappings.size()`
            // is taken as the maximal number of hardware threads to pin threads to.
            // If `hardware_thread_mappings` is not empty, `max_num_hardware_threads` must not be larger than
            // `hardware_thread_mappings.size()`.
            // Setting `max_num_hardware_threads` can be useful to increase reproducibility of synchronization and data race bugs
            // by running multiple threads on the same core.
            //
        int max_num_hardware_threads = 0;

            //
            // Maps thread indices to hardware thread ids. If empty, the thread squad uses thread indices as hardware thread ids.
            //ᅟ
            // If non-empty and if `max_num_hardware_threads == 0`, `hardware_thread_mappings.size()` is taken as the maximal
            // number of hardware threads to pin threads to.
            //
        std::span<int const> hardware_thread_mappings = { };
    };

        //
        // State passed to tasks that are executed in thread squad.
        //
    class task_context
    {
        friend detail::task_context_factory;

    private:
        detail::thread_squad_impl_base& impl_;
        int threadIdx_;
        int numRunningThreads_;

        task_context(detail::thread_squad_impl_base& _impl, int _threadIdx, int _numRunningThreads) noexcept
            : impl_(_impl), threadIdx_(_threadIdx), numRunningThreads_(_numRunningThreads)
        {
        }

        void
        collect(detail::task_context_synchronizer& synchronizer) noexcept;
        void
        broadcast(detail::task_context_synchronizer& synchronizer) noexcept;

    public:
            //
            // The current thread index.
            //
        [[nodiscard]] int
        thread_index() const noexcept
        {
            return threadIdx_;
        }

            //
            // The number of concurrent threads currently executing the task.
            //
        [[nodiscard]] int
        num_threads() const noexcept
        {
            return numRunningThreads_;
        }

            //
            // Synchronizes all threads which execute the current task.
            //ᅟ
            // It is the responsibility of the task to ensure that synchronization operations such as `synchronize()`, `reduce_transform()`,
            // and `reduce()` are executed by all participating threads unconditionally and in the same order.
            //
        void
        synchronize() noexcept
        {
            auto synchronizer = detail::task_context_synchronizer{ };
            collect(synchronizer);
            broadcast(synchronizer);
        }

            //
            // Synchronizes all threads which execute the current task and computes the reduction of `value` for all threads
            // with the reduction operation `reduceOp`. The result of the reduction is transformed with the `transformFunc` operation,
            // communicated back to all threads and returned by the `reduce_transform()` call.
            //ᅟ
            // The function objects `reduceOp` and `transformFunc` are executed on the calling thread only. `transformFunc` is executed
            // only on the thread which is the root of the synchronization operation.
            // It is the responsibility of the task to ensure that synchronization operations such as `synchronize()`, `reduce_transform()`,
            // and `reduce()` are executed by all participating threads unconditionally and in the same order.
            // If either of the function objects `transformFunc` or `reduceOp` throws an exception, `std::terminate()` is called.
            //
        template <std::copyable T, detail::reduction<T> ReduceOpT, std::invocable<T> TransformFuncT>
        auto
        reduce_transform(T value, ReduceOpT reduceOp, TransformFuncT transformFunc) noexcept
        {
            using R = std::decay_t<decltype(transformFunc(value))>;

            auto synchronizer = detail::task_context_reduce_transform_synchronizer<T, ReduceOpT, R>(std::move(value), reduceOp);
            collect(synchronizer);
            if (threadIdx_ == 0)
            {
                synchronizer.data.result = transformFunc(std::move(synchronizer.data.value));
            }
            broadcast(synchronizer);
            return std::move(synchronizer.data).result.value();
        }

            //
            // Synchronizes all threads which execute the current task and computes the reduction of `value` for all threads
            // with the reduction operation `reduceOp`. The result of the reduction is communicated back to all threads and returned
            // by the `reduce()` call.
            //ᅟ
            // The function object `reduceOp` is executed on the calling thread only.
            // It is the responsibility of the task to ensure that synchronization operations such as `synchronize()`, `reduce_transform()`,
            // and `reduce()` are executed by all participating threads unconditionally and in the same order.
            // If the function object `reduceOp` throws an exception, `std::terminate()` is called.
            //
        template <std::copyable T, detail::reduction<T> ReduceOpT>
        T
        reduce(T value, ReduceOpT reduceOp) noexcept
        {
            auto synchronizer = detail::task_context_reduce_synchronizer<T, ReduceOpT>(std::move(value), reduceOp);
            collect(synchronizer);
            broadcast(synchronizer);
            return std::move(synchronizer.data).value;
        }
    };

private:
    gsl::not_null<detail::thread_squad_handle> handle_;

    static params const&
    check_params(params const& p)
    {
        gsl_Expects(p.num_threads >= 0);
        gsl_Expects(p.max_num_hardware_threads >= 0);
        gsl_Expects(p.num_threads == 0 || p.max_num_hardware_threads <= p.num_threads);
        gsl_Expects(p.hardware_thread_mappings.empty() || (p.max_num_hardware_threads <= std::ssize(p.hardware_thread_mappings)
            && p.num_threads <= std::ssize(p.hardware_thread_mappings)));
        return p;
    }

    static detail::thread_squad_handle
    create(thread_squad::params p);

    void
    do_run(detail::thread_squad_task& op);

public:
    explicit thread_squad(params const& p)
        : handle_(create(check_params(p)))
    {
    }

    thread_squad(thread_squad&&) noexcept = default;
    thread_squad&
    operator =(thread_squad&&) noexcept = default;

    thread_squad(thread_squad const&) = delete;
    thread_squad&
    operator =(thread_squad const&) = delete;

        //
        // The number of concurrent threads.
        //
    [[nodiscard]] int
    num_threads() const
    {
        return handle_->numThreads;
    }

        //
        // Runs the given action on `concurrency` threads and waits until all tasks have run to completion.
        //ᅟ
        // `concurrency` must not exceed the number of threads in the thread squad. A value of -1 indicates that all available
        // threads shall be used.
        // The thread squad makes a dedicated copy of `action` for every participating thread and invokes it with an appropriate
        // task context. If `action()` throws an exception, `std::terminate()` is called.
        //
    template <std::invocable<task_context&> ActionT>
    requires std::copy_constructible<ActionT>
    void
    run(ActionT action, int concurrency = -1) &
    {
        gsl_Expects(concurrency >= -1 && concurrency <= handle_->numThreads);

        if (concurrency == -1)
        {
            concurrency = handle_->numThreads;
        }
        auto op = detail::thread_squad_action<task_context, ActionT>(std::move(action));
        op.params.concurrency = concurrency;
        do_run(op);
    }

        //
        // Runs the given action on `concurrency` threads and waits until all tasks have run to completion.
        //ᅟ
        // `concurrency` must not exceed the number of threads in the thread squad. A value of -1 indicates that all available
        // threads shall be used.
        // The thread squad makes a dedicated copy of `action` for every participating thread and invokes it with a thread-specific
        // `task_context&` argument. If `action` throws an exception, `std::terminate()` is called.
        //
    template <std::invocable<task_context&> ActionT>
    requires std::copy_constructible<ActionT>
    void
    run(ActionT action, int concurrency = -1) &&
    {
        gsl_Expects(concurrency >= -1 && concurrency <= handle_->numThreads);

        if (concurrency == -1)
        {
            concurrency = handle_->numThreads;
        }
        auto op = detail::thread_squad_action<task_context, ActionT>(std::move(action));
        op.params.concurrency = concurrency;
        op.params.join_requested = true;
        do_run(op);
    }

        //
        // Runs `transformFunc` on `concurrency` threads and waits until all tasks have run to completion, then reduces
        // the results using the `reduceOp` operator.
        //ᅟ
        // `concurrency` must not exceed the number of threads in the thread squad. A value of -1 indicates that all available
        // threads shall be used.
        // The thread squad makes a dedicated copy of `transformFunc` and `reduceOp` for every participating thread. `transformFunc`
        // is invoked with a thread-specific `task_context&` argument. If either of `transformFunc` or `reduceOp` throws an exception,
        // `std::terminate()` is called.
        //
    template <std::invocable<task_context&> TransformFuncT, detail::reduction<std::invoke_result_t<TransformFuncT, task_context&>> ReduceOpT>
    requires std::copy_constructible<TransformFuncT> && std::copy_constructible<ReduceOpT> && std::copyable<std::invoke_result_t<TransformFuncT, task_context&>>
    [[nodiscard]] std::invoke_result_t<TransformFuncT, task_context&>
    transform_reduce(TransformFuncT transformFunc, std::invoke_result_t<TransformFuncT, task_context&> init, ReduceOpT reduceOp, int concurrency = -1) &&
    {
        using T = std::invoke_result_t<TransformFuncT, task_context&>;

        gsl_Expects(concurrency >= -1 && concurrency <= handle_->numThreads);

        if (concurrency == -1)
        {
            concurrency = handle_->numThreads;
        }

        if (concurrency != 0)
        {
            auto data = std::make_unique<detail::thread_reduce_data<T>[]>(concurrency);
            auto op = detail::thread_squad_transform_reduce_operation<task_context, TransformFuncT, T, ReduceOpT>(std::move(transformFunc), std::move(reduceOp), data.get());
            op.params.concurrency = concurrency;
            op.params.join_requested = true;
            do_run(op);
            return op.reduce_op()(std::move(init), std::move(data[0].value).value());
        }
        else
        {
            return std::move(init);
        }
    }

        //
        // Runs `transformFunc` on `concurrency` threads and waits until all tasks have run to completion, then reduces
        // the results using the `reduceOp` operator.
        //ᅟ
        // `concurrency` must not exceed the number of threads in the thread squad. A value of -1 indicates that all available
        // threads shall be used.
        // The thread squad makes a dedicated copy of `transformFunc` and `reduceOp` for every participating thread. `transformFunc`
        // is invoked with a thread-specific `task_context&` argument. If either of `transformFunc` or `reduceOp` throws an exception,
        // `std::terminate()` is called.
        //
    template <std::invocable<task_context&> TransformFuncT, detail::reduction<std::invoke_result_t<TransformFuncT, task_context&>> ReduceOpT>
    requires std::copy_constructible<TransformFuncT> && std::copy_constructible<ReduceOpT> && std::copyable<std::invoke_result_t<TransformFuncT, task_context&>>
    [[nodiscard]] std::invoke_result_t<TransformFuncT, task_context&>
    transform_reduce(TransformFuncT transformFunc, std::invoke_result_t<TransformFuncT, task_context&> init, ReduceOpT reduceOp, int concurrency = -1) &
    {
        using T = std::invoke_result_t<TransformFuncT, task_context&>;

        gsl_Expects(concurrency >= -1 && concurrency <= handle_->numThreads);

        if (concurrency == -1)
        {
            concurrency = handle_->numThreads;
        }

        if (concurrency != 0)
        {
            auto data = std::make_unique<detail::thread_reduce_data<T>[]>(concurrency);
            auto op = detail::thread_squad_transform_reduce_operation<task_context, TransformFuncT, T, ReduceOpT>(std::move(transformFunc), std::move(reduceOp), data.get());
            op.params.concurrency = concurrency;
            do_run(op);
            return op.reduce_op()(std::move(init), std::move(data[0].value).value());
        }
        else
        {
            return init;
        }
    }

        //
        // Runs `transformFunc` on `concurrency` threads and waits until all tasks have run to completion, then reduces
        // the results using the `reduceOp` operator.
        //ᅟ
        // `concurrency` must not be 0 and must not exceed the number of threads in the thread squad. A value of -1 indicates
        // that all available threads shall be used.
        // The thread squad makes a dedicated copy of `transformFunc` and `reduceOp` for every participating thread. `transformFunc`
        // is invoked with a thread-specific `task_context&` argument. If either of `transformFunc` or `reduceOp` throws an exception,
        // `std::terminate()` is called.
        //
    template <std::invocable<task_context&> TransformFuncT, detail::reduction<std::invoke_result_t<TransformFuncT, task_context&>> ReduceOpT>
    requires std::copy_constructible<TransformFuncT> && std::copy_constructible<ReduceOpT> && std::copyable<std::invoke_result_t<TransformFuncT, task_context&>>
    [[nodiscard]] std::invoke_result_t<TransformFuncT, task_context&>
    transform_reduce_first(TransformFuncT transformFunc, ReduceOpT reduceOp, int concurrency = -1) &&
    {
        using T = std::invoke_result_t<TransformFuncT, task_context&>;

        gsl_Expects(concurrency == -1 || (concurrency >= 1 && concurrency <= handle_->numThreads));

        if (concurrency == -1)
        {
            concurrency = handle_->numThreads;
        }

        auto data = std::make_unique<detail::thread_reduce_data<T>[]>(concurrency);
        auto op = detail::thread_squad_transform_reduce_operation<task_context, TransformFuncT, T, ReduceOpT>(std::move(transformFunc), std::move(reduceOp), data.get());
        op.params.concurrency = concurrency;
        op.params.join_requested = true;
        do_run(op);
        return std::move(data[0].value).value();
    }

        //
        // Runs `transformFunc` on `concurrency` threads and waits until all tasks have run to completion, then reduces
        // the results using the `reduceOp` operator.
        //ᅟ
        // `concurrency` must not be 0 and must not exceed the number of threads in the thread squad. A value of -1 indicates
        // that all available threads shall be used.
        // The thread squad makes a dedicated copy of `transformFunc` and `reduceOp` for every participating thread. `transformFunc`
        // is invoked with a thread-specific `task_context&` argument. If either of `transformFunc` or `reduceOp` throws an exception,
        // `std::terminate()` is called.
        //
    template <std::invocable<task_context&> TransformFuncT, detail::reduction<std::invoke_result_t<TransformFuncT, task_context&>> ReduceOpT>
    requires std::copy_constructible<TransformFuncT> && std::copy_constructible<ReduceOpT> && std::copyable<std::invoke_result_t<TransformFuncT, task_context&>>
    [[nodiscard]] std::invoke_result_t<TransformFuncT, task_context&>
    transform_reduce_first(TransformFuncT transformFunc, ReduceOpT reduceOp, int concurrency = -1) &
    {
        using T = std::invoke_result_t<TransformFuncT, task_context&>;

        gsl_Expects(concurrency == -1 || (concurrency >= 1 && concurrency <= handle_->numThreads));

        if (concurrency == -1)
        {
            concurrency = handle_->numThreads;
        }

        auto data = std::make_unique<detail::thread_reduce_data<T>[]>(concurrency);
        auto op = detail::thread_squad_transform_reduce_operation<task_context, TransformFuncT, T, ReduceOpT>(std::move(transformFunc), std::move(reduceOp), data.get());
        op.params.concurrency = concurrency;
        do_run(op);
        return std::move(data[0].value).value();
    }
};


} // namespace patton


#endif // INCLUDED_PATTON_THREAD_SQUAD_HPP_
