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

}    // namespace cmk

#endif
