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
    };

}    // namespace cmk

#endif
