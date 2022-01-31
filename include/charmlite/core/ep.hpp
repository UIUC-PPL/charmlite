#ifndef CHARMLITE_CORE_EP_HPP
#define CHARMLITE_CORE_EP_HPP

#include <charmlite/core/common.hpp>

#include <charmlite/utilities/traits.hpp>

#include <typeindex>
#include <typeinfo>

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

    template <auto Fn>
    struct entry_fn_impl_
    {
    private:
        template <typename... Args_>
        static void func_invoker_impl_helper(void* self, Args_&&... args)
        {
            using template_t =
                typename cmk::generate_marshall_msg<decltype(Fn)>::template_t;
            (static_cast<template_t*>(self)->*Fn)(std::forward<Args_>(args)...);
        }

        template <typename Tuple, std::size_t... Indices>
        static void func_invoker_impl(
            void* self, Tuple&& t, std::index_sequence<Indices...>)
        {
            func_invoker_impl_helper(
                self, std::get<Indices>(std::forward<Tuple>(t))...);
        }

        template <typename Tuple>
        static void func_invoker(void* self, Tuple&& t)
        {
            func_invoker_impl(self, std::forward<Tuple>(t),
                std::make_index_sequence<
                    std::tuple_size<std::decay_t<Tuple>>::value>());
        }

    public:
        static void call_(void* self, message_ptr<>&& msg)
        {
            if constexpr (cmk::is_member_fn_t<decltype(Fn)>::value)
            {
                if constexpr (cmk::is_message<typename cmk::extract_message<
                                  decltype(Fn)>::type>::value)
                {
                    using message_t =
                        typename cmk::extract_message<decltype(Fn)>::type;
                    using template_t =
                        typename cmk::extract_message<decltype(Fn)>::template_t;

                    auto* typed = static_cast<message_t*>(msg.release());
                    message_ptr<message_t> owned(typed);
                    (static_cast<template_t*>(self)->*Fn)(std::move(owned));
                }
                else
                {
                    CmiAbort("entry_fn_impl_::call_ called with incompatible "
                             "message type");
                }
            }
            else if constexpr (cmk::is_member_fn_args_t<decltype(Fn)>::value)
            {
                using message_t =
                    typename cmk::generate_marshall_msg<decltype(Fn)>::type;
                using tuple_t = typename cmk::generate_marshall_msg<
                    decltype(Fn)>::tuple_type;

                auto* typed = static_cast<message_t*>(msg.release());
                tuple_t t{};

                // Unpack to tuple_t
                PUP::fromMem unpacker(((char*) (typed)) + sizeof(message));

                impl::args_unfolder(unpacker, t);

                func_invoker(self, t);
            }
            else
            {
                CmiAbort("no matching call to compatible functiono call.");
            }
        }

        static const entry_id_t& id_(void)
        {
            return entry_fn_helper_<(&call_), false>::id_;
        }
    };

    template <typename Chare, typename Argument>
    struct constructor_caller_
    {
    private:
        template <typename... Args_>
        static void func_invoker_impl_helper(void* self, Args_&&... args)
        {
            new (static_cast<Chare*>(self)) Chare(std::forward<Args_>(args)...);
        }

        template <typename Tuple, std::size_t... Indices>
        static void func_invoker_impl(
            void* self, Tuple&& t, std::index_sequence<Indices...>)
        {
            func_invoker_impl_helper(
                self, std::get<Indices>(std::forward<Tuple>(t))...);
        }

        template <typename Tuple>
        static void func_invoker(void* self, Tuple&& t)
        {
            func_invoker_impl(self, std::forward<Tuple>(t),
                std::make_index_sequence<
                    std::tuple_size<std::decay_t<Tuple>>::value>());
        }

    public:
        auto operator()(void* self, message_ptr<>&& msg)
        {
            if constexpr (cmk::is_marshall_type_v<std::decay_t<Argument>>)
            {
                using Message = cmk::get_message_t<Argument>;
                auto* typed = static_cast<Message*>(msg.release());
                using tuple_t = marshall_args_t<Message>;

                tuple_t t{};

                // Unpack to tuple_t
                PUP::fromMem unpacker(((char*) (typed)) + sizeof(message));

                impl::args_unfolder(unpacker, t);

                func_invoker(self, t);
            }
            else if constexpr (cmk::message_compatibility_v<Argument>)
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
