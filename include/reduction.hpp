#ifndef __CMK_REDUCTION_HH__
#define __CMK_REDUCTION_HH__

#include "callback.hpp"
#include "message.hpp"

// TODO ( converse collectives should be isolated/removed )

namespace cmk {
    void* converse_combiner_(int* size, void* local, void** remote, int count);

    template <typename Message, combiner_fn_t<Message> Combiner,
        callback_fn_t<Message> Callback>
    void reduce(message_ptr<Message>&& msg)
    {
        // callback will be invoked on pe0
        new (&msg->dst_)
            destination(callback_helper_<Message, Callback>::id_, 0);
        msg->has_combiner() = true;
        *(msg->combiner()) = combiner_helper_<Message, Combiner>::id_;
        auto sz = msg->total_size_;
        CmiReduce(msg.release(), sz, converse_combiner_);
    }

    template <typename T = message>
    message_ptr<T> nop(message_ptr<T>&& msg, message_ptr<T>&&)
    {
        return std::move(msg);
    }

    template <typename T>
    message_ptr<data_message<T>> add(
        message_ptr<data_message<T>>&& lhs, message_ptr<data_message<T>>&& rhs)
    {
        lhs->value() += rhs->value();
        return std::move(lhs);
    }
}    // namespace cmk

#endif
