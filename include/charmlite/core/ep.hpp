#ifndef CHARMLITE_CORE_EP_HPP
#define CHARMLITE_CORE_EP_HPP

#include <charmlite/core/common.hpp>

#include <charmlite/utilities/traits.hpp>

namespace cmk {
    inline const entry_record_* record_for(entry_id_t id)
    {
        if (id == cmk::helper_::nil_entry_)
        {
            return nullptr;
        }
        else
        {
            return &(CMK_ACCESS_SINGLETON(entry_table_)[id - 1]);
        }
    }

    template <entry_fn_t Fn, bool Constructor>
    struct entry_fn_helper_
    {
        static entry_id_t id_;
    };

    // template <typename T>
    // struct get_type;

    template <auto Fn>
    struct entry_fn_impl_
    {
        static void call_(void* self, message_ptr<>&& msg)
        {
            // using type = typename get_type<decltype(Fn)>::type;
            if constexpr (cmk::is_member_fn_t<decltype(Fn)>::value &&
                cmk::is_message<
                    typename cmk::extract_message<decltype(Fn)>::type>::value)
            {
                using message_t =
                    typename cmk::extract_message<decltype(Fn)>::type;
                using template_t =
                    typename cmk::extract_message<decltype(Fn)>::template_t;

                auto* typed = static_cast<message_t*>(msg.release());
                message_ptr<message_t> owned(typed);
                (static_cast<template_t*>(self)->*Fn)(std::move(owned));
            }
        }

        static const entry_id_t& id_(void)
        {
            return entry_fn_helper_<(&call_), false>::id_;
        }
    };

    // template <typename T, T t, typename Enable = void>
    // struct entry_fn_impl_;

    // template <typename T, typename Message, member_fn_t<T, Message> Fn>
    // struct entry_fn_impl_<member_fn_t<T, Message>, Fn,
    //     typename std::enable_if<is_message<Message>::value>::type>
    // {
    //     static void call_(void* self, message_ptr<>&& msg)
    //     {
    //         auto* typed = static_cast<Message*>(msg.release());
    //         message_ptr<Message> owned(typed);
    //         (static_cast<T*>(self)->*Fn)(std::move(owned));
    //     }

    //     static const entry_id_t& id_(void)
    //     {
    //         return entry_fn_helper_<(&call_), false>::id_;
    //     }
    // };

    template <typename Chare, typename Argument>
    struct constructor_caller_
    {
        auto operator()(void* self, message_ptr<>&& msg)
        {
            if constexpr (cmk::message_compatibility_v<Argument>)
            {
                using Message = cmk::get_message_t<Argument>;
                auto* typed = static_cast<Message*>(msg.release());
                message_ptr<Message> owned(typed);
                new (static_cast<Chare*>(self)) Chare(std::move(owned));
            }
            else if constexpr (std::is_same_v<Argument, void>)
            {
                new (static_cast<Chare*>(self)) Chare;
            }
            else
            {
                CmiAbort("constructor_caller_ called with a message pointer of "
                         "non-message type");
            }
        }
    };

    template <typename T, typename Arg>
    void call_constructor_(void* self, message_ptr<>&& msg)
    {
        constructor_caller_<T, Arg>()(self, std::move(msg));
    }

    template <auto Fn>
    entry_id_t entry(void)
    {
        return entry_fn_impl_<Fn>::id_();
    }

    template <typename T, typename Message>
    entry_id_t constructor(void)
    {
        return entry_fn_helper_<(&call_constructor_<T, Message>), true>::id_;
    }

}    // namespace cmk

#endif
