
#ifndef INCLUDED_PATTON_DETAIL_TYPE_TRAITS_HPP_
#define INCLUDED_PATTON_DETAIL_TYPE_TRAITS_HPP_


#include <type_traits>  // for integral_constant<>, void_t<>


namespace patton::detail {


template <template <typename...> class, typename, typename...> struct can_instantiate_ : std::false_type { };
template <template <typename...> class Z, typename... Ts> struct can_instantiate_<Z, std::void_t<Z<Ts...>>, Ts...> : std::true_type { };
template <template <typename...> class Z, typename... Ts> struct can_instantiate : can_instantiate_<Z, void, Ts...> { };
template <template <typename...> class Z, typename... Ts> constexpr bool can_instantiate_v = can_instantiate<Z, Ts...>::value;


} // namespace patton::detail


#endif // INCLUDED_PATTON_DETAIL_TYPE_TRAITS_HPP_
