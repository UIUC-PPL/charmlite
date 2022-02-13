#ifndef CHARMLITE_UTILITIES_TRAITS_CHARE_HPP
#define CHARMLITE_UTILITIES_TRAITS_CHARE_HPP

#include <type_traits>

namespace cmk { namespace impl {
    template <typename T, typename U = void>
    struct has_on_migrated : std::false_type
    {
    };

    template <typename T>
    struct has_on_migrated<T,
        std::void_t<decltype(std::declval<T>().on_migrated())>> : std::true_type
    {
    };

    template <typename T>
    constexpr auto has_on_migrated_v = has_on_migrated<T>::value;
}}    // namespace cmk::impl

#endif
