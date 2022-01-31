#ifndef CHARMLITE_SERIALIZATION_MARSHALL_MESSAGE_HPP
#define CHARMLITE_SERIALIZATION_MARSHALL_MESSAGE_HPP

#include <charmlite/core/message.hpp>

#include <charmlite/utilities/traits/serialization.hpp>

#include <pup.h>
#include <pup_stl.h>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace cmk {

    template <typename... Args>
    struct marshall_msg : cmk::message
    {
        using marshall_args = std::tuple<std::decay_t<Args>...>;

        marshall_msg(int size)
          : cmk::message(
                cmk::message_helper_<marshall_msg<Args...>>::kind_, size)
        {
        }

        template <typename... Args_>
        static message_ptr<marshall_msg<std::decay_t<Args_>...>> pack(
            Args_&&... args)
        {
            PUP::sizer sizer_;
            impl::args_parser(sizer_, std::forward<Args_>(args)...);
            int size = sizer_.size();

            message_ptr<marshall_msg<std::decay_t<Args_>...>> msg(
                new (sizeof(cmk::message) + size)
                    marshall_msg<std::decay_t<Args_>...>(
                        sizeof(cmk::message) + size));

            int msg_offset = sizeof(message);
            PUP::toMem pack_to_mem((void*) ((char*) (msg.get()) + msg_offset));
            impl::args_parser(pack_to_mem, std::forward<Args_>(args)...);

            return msg;
        }
    };
}    // namespace cmk

#endif
