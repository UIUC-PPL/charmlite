#ifndef CHARMLITE_CORE_CALLBACK_HPP
#define CHARMLITE_CORE_CALLBACK_HPP

#include <charmlite/core/common.hpp>
#include <charmlite/core/message.hpp>

namespace cmk {

    template <typename Message, combiner_fn_t<Message> Fn>
    struct combiner_helper_
    {
        static combiner_id_t id_;
    };

    template <typename Message, callback_fn_t<Message> Fn>
    struct callback_helper_
    {
        static callback_id_t id_;
    };

    inline combiner_fn_t<message> combiner_for(combiner_id_t id)
    {
        return id ? CMK_ACCESS_SINGLETON(combiner_table_)[id - 1] : nullptr;
    }

    inline callback_fn_t<message> callback_for(callback_id_t id)
    {
        return id ? CMK_ACCESS_SINGLETON(callback_table_)[id - 1] : nullptr;
    }

    inline combiner_fn_t<message> combiner_for(const message_ptr<>& msg)
    {
        auto* id = msg->combiner();
        return id ? combiner_for(*id) : nullptr;
    }

    inline callback_fn_t<message> callback_for(const message_ptr<>& msg)
    {
        return (msg->dst_.kind() == destination_kind::kCallback) ?
            callback_for(msg->dst_.callback_fn().id) :
            nullptr;
    }

    template <typename Message>
    class callback
    {
        destination dst_;

        template <typename... Args>
        callback(Args&&... args)
          : dst_(std::forward<Args>(args)...)
        {
        }

    public:
        callback(void) = default;
        callback(callback<Message>&&) = default;
        callback(const callback<Message>&) = default;

        template <typename T>
        friend class collection_proxy_base_;

        template <typename T>
        friend class element_proxy;

        inline void imprint(destination& dst) const
        {
            new (&dst) destination(this->dst_);
        }

        inline void imprint(const message_ptr<Message>& msg) const
        {
            this->imprint(msg->dst_);
        }

        void send(message_ptr<Message>&& msg) const
        {
            this->imprint(msg);
            cmk::send(std::move(msg));
        }

        template <callback_fn_t<Message> Callback>
        static callback<Message> construct(int pe)
        {
            return callback<Message>(
                callback_helper_<Message, Callback>::id_, pe);
        }

        operator bool(void) const
        {
            return (bool) this->dst_;
        }
    };
}    // namespace cmk

#endif
