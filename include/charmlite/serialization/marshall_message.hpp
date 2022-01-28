#ifndef CHARMLITE_SERIALIZATION_MARSHALL_MESSAGE_HPP
#define CHARMLITE_SERIALIZATION_MARSHALL_MESSAGE_HPP

#include <charmlite/core/message.hpp>

#include <pup.h>
#include <pup_stl.h>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace cmk {

    template <typename... Args>
    struct marshall_msg : cmk::message
    {
    private:
        template <typename Serializer, typename Arg0, typename... Args_>
        static void args_parser(
            Serializer&& serializer, Arg0&& arg0, Args_&&... args)
        {
            serializer | (typename std::decay_t<Arg0>&) arg0;
            args_parser(std::forward<Serializer>(serializer),
                std::forward<Args_>(args)...);
        }

        template <typename Serializer, typename Arg0>
        static void args_parser(Serializer&& serializer, Arg0&& arg0)
        {
            serializer | (typename std::decay_t<Arg0>&) arg0;
        }

        template <typename Serializer>
        static void args_parser(Serializer&&)
        {
        }

    public:
        using marshall_args = std::tuple<std::decay_t<Args>...>;

        marshall_msg(int size)
          : cmk::message(
                cmk::message_helper_<marshall_msg<Args...>>::kind_, size)
        {
        }

        template <typename... Args_>
        static message_ptr<marshall_msg<marshall_args>> pack(Args_&&... args)
        {
            PUP::sizer sizer_;
            args_parser(sizer_, std::forward<Args_>(args)...);
            int size = sizer_.size();

            CmiPrintf("Size of marshalled args: %d", size);

            message_ptr<marshall_msg<marshall_args>> msg =
                make_message<marshall_msg<marshall_args>>(
                    sizeof(message) + size);

            int msg_offset = sizeof(message);
            PUP::toMem pack_to_mem((void*) (msg.get() + msg_offset));
            args_parser(pack_to_mem, std::forward<Args_>(args)...);

            return msg;
        }
    };
}    // namespace cmk

#endif
