#ifndef CHARMLITE_UTILITIES_TRAITS_MESSAGE_HPP
#define CHARMLITE_UTILITIES_TRAITS_MESSAGE_HPP

#include <charmlite/core/message.hpp>

#include <type_traits>

#include <deque>

namespace cmk {
    template <typename T>
    class message_properties_
    {
    private:
        template <typename U>
        static auto check_pack(std::nullptr_t)
            -> decltype(U::pack(std::declval<message_ptr<U>&>()));
        template <typename U>
        static std::nullptr_t check_pack(...);

        template <typename U>
        static auto check_unpack(std::nullptr_t)
            -> decltype(U::unpack(std::declval<message_ptr<U>&>()));
        template <typename U>
        static std::nullptr_t check_unpack(...);

    public:
        static constexpr bool has_pack(void)
        {
            return std::is_same<void, decltype(check_pack<T>(nullptr))>::value;
        }

        static constexpr bool has_unpack(void)
        {
            return std::is_same<void,
                decltype(check_unpack<T>(nullptr))>::value;
        }

        static_assert(message_properties_<T>::has_pack() ==
                message_properties_<T>::has_unpack(),
            "missing an un/pack method");

        static constexpr bool value(void)
        {
            return message_properties_<T>::has_pack() &&
                message_properties_<T>::has_unpack();
        }
    };

    template <typename Message, typename Enable = void>
    class message_properties_extractor_;

    template <typename Message>
    class message_properties_extractor_<Message,
        typename std::enable_if<message_properties_<Message>::value()>::type>
    {
    private:
        static void pack_impl_(message_ptr<>& msg)
        {
            Message::pack(reinterpret_cast<message_ptr<Message>&>(msg));
        }

        static void unpack_impl_(message_ptr<>& msg)
        {
            // TODO ( are these valid conversions? )
            Message::unpack(reinterpret_cast<message_ptr<Message>&>(msg));
        }

    public:
        static constexpr message_packer_t packer(void)
        {
            return &(message_properties_extractor_<Message>::pack_impl_);
        }

        static constexpr message_unpacker_t unpacker(void)
        {
            return &(message_properties_extractor_<Message>::unpack_impl_);
        }
    };

    template <typename Message>
    class message_properties_extractor_<Message,
        typename std::enable_if<!(message_properties_<Message>::value())>::type>
    {
    public:
        static constexpr message_packer_t packer(void)
        {
            return nullptr;
        }

        static constexpr message_unpacker_t unpacker(void)
        {
            return nullptr;
        }
    };

    template <typename T>
    struct is_packable
    {
        static constexpr auto value = message_properties_<T>::value();
    };

    template <typename T, typename Message>
    using member_fn_t = void (T::*)(cmk::message_ptr<Message>&&);

    using message_buffer_t = std::deque<message_ptr<message>>;
    using collection_buffer_t = std::unordered_map<collection_index_t,
        message_buffer_t, collection_index_hasher_>;

    template <typename T>
    struct is_message
      : std::conditional<std::is_base_of<message, T>::value, std::true_type,
            std::false_type>
    {
        static constexpr bool value = is_message<T>::type::value;
    };

    template <typename T>
    using is_message_t = typename is_message<T>::type;

}    // namespace cmk

#endif
