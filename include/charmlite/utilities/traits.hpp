#ifndef CHARMLITE_UTILITIES_TRAITS_HPP
#define CHARMLITE_UTILITIES_TRAITS_HPP

#include <charmlite/utilities/traits/message.hpp>
#include <charmlite/utilities/traits/serialization.hpp>

#include <charmlite/serialization/marshall_message.hpp>

namespace cmk {

    template <typename T>
    struct viable_argument
    {
        using type = T*;
    };

    template <>
    struct viable_argument<void>
    {
        using type = void;
    };

    template <typename T>
    using viable_argument_t = typename viable_argument<T>::type;

    template <typename T>
    struct extract_message;

    template <typename T, typename Message>
    struct extract_message<member_fn_t<T, Message>>
    {
        using type = Message;
        using ptr_type = message_ptr<Message>;
        using template_t = T;
    };

    template <typename Message>
    struct extract_message<combiner_fn_t<Message>>
    {
        using type = Message;
        using ptr_type = message_ptr<Message>;
    };

    template <typename T>
    struct is_member_fn_t : std::false_type
    {
    };

    template <typename T, typename Message>
    struct is_member_fn_t<member_fn_t<T, Message>> : std::true_type
    {
    };

    template <typename T>
    struct is_member_fn_args_t : std::false_type
    {
    };

    template <typename T, typename... Args>
    struct is_member_fn_args_t<member_fn_args_t<T, Args...>> : std::true_type
    {
    };

    template <typename T>
    struct generate_marshall_msg;

    template <typename T, typename... Args>
    struct generate_marshall_msg<member_fn_args_t<T, Args...>>
    {
        using type = marshall_msg<std::decay_t<Args>...>;
        using template_t = T;
        using tuple_type = std::tuple<std::decay_t<Args>...>;
    };

    template <typename Tuple>
    struct decay_tuple;

    template <typename... Args>
    struct decay_tuple<std::tuple<Args...>>
    {
        using type = std::tuple<std::decay_t<Args>...>;
    };

    template <typename Tuple>
    using decay_tuple_t = typename decay_tuple<Tuple>::type;

    template <typename T>
    struct is_marshall_type : std::false_type
    {
    };

    template <typename... Args>
    struct is_marshall_type<marshall_msg<Args...>> : std::true_type
    {
    };

    template <typename Argument>
    inline constexpr bool is_marshall_type_v =
        is_marshall_type<Argument>::value;

    template <typename T>
    struct marshall_args;

    template <typename... Args>
    struct marshall_args<marshall_msg<Args...>>
    {
        using tuple_type = std::tuple<std::decay_t<Args>...>;
    };

    template <typename T>
    using marshall_args_t = typename marshall_args<T>::type;

}    // namespace cmk

#endif
