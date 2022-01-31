#ifndef CHARMLITE_UTILITIES_TRAITS_SERIALIZATION_HPP
#define CHARMLITE_UTILITIES_TRAITS_SERIALIZATION_HPP

#include <tuple>
#include <type_traits>

namespace cmk { namespace impl {

    template <typename Serializer>
    void args_parser(Serializer&&)
    {
    }

    template <typename Serializer, typename Arg0>
    void args_parser(Serializer&& serializer, Arg0&& arg0)
    {
        serializer | (typename std::decay_t<Arg0>&) arg0;
    }

    template <typename Serializer, typename Arg0, typename... Args_>
    void args_parser(Serializer&& serializer, Arg0&& arg0, Args_&&... args)
    {
        serializer | (typename std::decay_t<Arg0>&) arg0;
        args_parser(
            std::forward<Serializer>(serializer), std::forward<Args_>(args)...);
    }

    template <typename Serializer>
    void args_unfolder_impl_helper(Serializer&&)
    {
    }

    template <typename Serializer, typename T1>
    void args_unfolder_impl_helper(Serializer&& serializer, T1&& t1)
    {
        serializer | (typename std::decay_t<T1>&) t1;
    }

    template <typename Serializer, typename T1, typename... Ts>
    void args_unfolder_impl_helper(Serializer&& serializer, T1&& t1, Ts&&... ts)
    {
        serializer | (typename std::decay_t<T1>&) t1;

        args_unfolder_impl_helper(
            std::forward<Serializer>(serializer), std::forward<Ts>(ts)...);
    }

    template <typename Serializer, typename Tuple, std::size_t... Indices>
    void args_unfolder_impl(
        Serializer&& serializer, Tuple&& t, std::index_sequence<Indices...>)
    {
        args_unfolder_impl_helper(std::forward<Serializer>(serializer),
            std::get<Indices>(std::forward<Tuple>(t))...);
    }

    template <typename Serializer, typename Tuple>
    void args_unfolder(Serializer&& serializer, Tuple&& ts)
    {
        using tuple_s = std::tuple_size<typename std::decay_t<Tuple>>;

        args_unfolder_impl(std::forward<Serializer>(serializer),
            std::forward<Tuple>(ts),
            std::make_index_sequence<tuple_s::value>());
    }
}}    // namespace cmk::impl

#endif
