#ifndef __CMK_REDUCTION_HH__
#define __CMK_REDUCTION_HH__

#include "callback.hh"
#include "message.hh"

// TODO ( converse collectives should be isolated/removed )

namespace cmk {
    void* converse_combiner_(int* size, void* local, void** remote, int count)
    {
        message_ptr<> lhs(static_cast<message*>(local));
        auto comb = combiner_for(lhs);
        CmiEnforce(comb != nullptr);
        for (auto i = 0; i < count; i++)
        {
            auto& msg = remote[i];
            message_ptr<> rhs(reinterpret_cast<message*>(msg));
            CmiReference(msg);    // yielding will call free!
            lhs = comb(std::move(lhs), std::move(rhs));
        }
        *size = (int) lhs->total_size_;
        return lhs.release();
    }

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

    message_ptr<> nop(message_ptr<>&& msg, message_ptr<>&&)
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
