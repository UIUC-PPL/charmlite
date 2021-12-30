#ifndef __CMK_TRAITS_HH__
#define __CMK_TRAITS_HH__

#include <charmlite/core/message.hpp>

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

    template <typename T, typename Message>
    using member_fn_t = void (T::*)(cmk::message_ptr<Message>&&);

    template <typename T>
    constexpr bool is_message_(void)
    {
        return std::is_base_of<message, T>::value;
    }

    template <>
    constexpr bool is_message_<void>(void)
    {
        return true;
    }
}    // namespace cmk

#endif
