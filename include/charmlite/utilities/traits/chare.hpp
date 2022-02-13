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

    template <typename T, typename U = void>
    struct has_can_migrate : std::false_type
    {
    };

    template <typename T>
    struct has_can_migrate<T,
        std::enable_if_t<
            std::is_same_v<decltype(std::declval<const T>().can_migrate()), bool>>>
      : std::true_type
    {
    };

    template <typename T>
    constexpr auto has_can_migrate_v = has_can_migrate<T>::value;
}}    // namespace cmk::impl

#endif
