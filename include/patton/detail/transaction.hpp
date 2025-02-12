
#ifndef INCLUDED_PATTON_DETAIL_TRANSACTION_HPP_
#define INCLUDED_PATTON_DETAIL_TRANSACTION_HPP_


#include <utility>      // for move(), exchange()
#include <type_traits>  // for integral_constant<>


namespace patton::detail {


    // TODO: Should this be in gsl-lite? B. Stroustrup rightfully claims that `on_success()`, `on_error()`, and `finally()` are the elementary operations here, but
    //       calling `uncaught_exceptions()` does have a cost, so maybe a transaction is still a useful thing.
template <typename RollbackFuncT>
class transaction_t : private RollbackFuncT
{
private:
    bool done_;

public:
    constexpr transaction_t(RollbackFuncT&& rollbackFunc)
        : RollbackFuncT(std::move(rollbackFunc)), done_(false)
    {
    }
    ~transaction_t()
    {
        if (!done_)
        {
            static_cast<RollbackFuncT&>(*this)();
        }
    }

    constexpr void
    commit() noexcept
    {
        done_ = true;
    }

    constexpr transaction_t(transaction_t&& rhs) // pre-C++17 tax
        : RollbackFuncT(std::move(rhs)), done_(std::exchange(rhs.done_, true))
    {

    }
    transaction_t&
    operator =(transaction_t&&) = delete;
};
class no_op_transaction_t
{
public:
    constexpr void commit() noexcept
    {
    }

    no_op_transaction_t() noexcept = default;

    no_op_transaction_t(no_op_transaction_t&&) = default;
    no_op_transaction_t& operator =(no_op_transaction_t&&) = delete;
};
template <typename RollbackFuncT>
constexpr transaction_t<RollbackFuncT>
make_transaction(RollbackFuncT rollbackFunc)
{
    return { std::move(rollbackFunc) };
}
template <typename RollbackFuncT>
constexpr transaction_t<RollbackFuncT>
make_transaction(std::true_type /*enableRollback*/, RollbackFuncT rollbackFunc)
{
    return { std::move(rollbackFunc) };
}
template <typename RollbackFuncT>
constexpr no_op_transaction_t
make_transaction(std::false_type /*enableRollback*/, RollbackFuncT)
{
    return { };
}


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_TRANSACTION_HPP_
