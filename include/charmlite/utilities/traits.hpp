#ifndef CHARMLITE_UTILITIES_TRAITS_HPP
#define CHARMLITE_UTILITIES_TRAITS_HPP

#include <charmlite/utilities/traits/message.hpp>

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

}    // namespace cmk

#endif
