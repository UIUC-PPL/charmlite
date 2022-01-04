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

    template <typename T, T t, typename Enable = void>
    struct entry_fn_impl_;

    template <typename T, typename Message, member_fn_t<T, Message> Fn>
    struct entry_fn_impl_<member_fn_t<T, Message>, Fn,
        typename std::enable_if<is_message<Message>::value>::type>
    {
        static void call_(void* self, message_ptr<>&& msg)
        {
            auto* typed = static_cast<Message*>(msg.release());
            message_ptr<Message> owned(typed);
            (static_cast<T*>(self)->*Fn)(std::move(owned));
        }

        static const entry_id_t& id_(void)
        {
            return entry_fn_helper_<(&call_), false>::id_;
        }
    };

    template <typename A, typename B>
    struct constructor_caller_;

    template <typename A>
    struct constructor_caller_<A, void>
    {
        void operator()(void* self, message_ptr<>&&)
        {
            new (static_cast<A*>(self)) A;
        }
    };

    template <typename A, typename Message>
    struct constructor_caller_<A, message_ptr<Message>&&>
    {
        typename std::enable_if<is_message<Message>::value>::type operator()(
            void* self, message_ptr<>&& msg)
        {
            auto* typed = static_cast<Message*>(msg.release());
            message_ptr<Message> owned(typed);
            new (static_cast<A*>(self)) A(std::move(owned));
        }
    };

    template <typename T, typename Arg>
    void call_constructor_(void* self, message_ptr<>&& msg)
    {
        constructor_caller_<T, Arg>()(self, std::move(msg));
    }

    template <typename T, T t>
    entry_id_t entry(void)
    {
        return entry_fn_impl_<T, t>::id_();
    }

    template <typename T, typename Message>
    entry_id_t constructor(void)
    {
        return entry_fn_helper_<(&call_constructor_<T, Message>), true>::id_;
    }

}    // namespace cmk

#endif