#ifndef CHARMLITE_CORE_EP_HPP
#define CHARMLITE_CORE_EP_HPP

#include <charmlite/core/common.hpp>

#include <charmlite/utilities/traits.hpp>

#include <cassert>

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

    template <typename A, typename MessagePtr>
    struct constructor_caller_
    {
        auto operator()(void* self, message_ptr<>&& msg)
        {
            if constexpr (cmk::message_compatibility_v<MessagePtr>)
            {
                using Message = cmk::get_message_t<MessagePtr>;
                auto* typed = static_cast<Message*>(msg.release());
                message_ptr<Message> owned(typed);
                new (static_cast<A*>(self)) A(std::move(owned));
            }
            else if constexpr (std::is_same_v<MessagePtr, void>)
            {
                new (static_cast<A*>(self)) A;
            }
            else
            {
                assert(false &&
                    "constructor_caller_ called with a message pointer of "
                    "non-message type");
            }
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
