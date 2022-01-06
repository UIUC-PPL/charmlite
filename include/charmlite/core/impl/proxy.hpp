#ifndef CHARMLITE_CORE_IMPL_PROXY_HPP
#define CHARMLITE_CORE_IMPL_PROXY_HPP

#include <charmlite/core/collection.hpp>
#include <charmlite/core/proxy.hpp>

namespace cmk {

    // Element Proxy member functions
    template <typename T>
    template <typename Message>
    void element_proxy<T>::insert(message_ptr<Message>&& msg, int pe) const
    {
        new (&(msg->dst_))
            destination(this->id_, this->idx_, constructor<T, message_ptr<Message>&&>(), pe);
        cmk::send(std::move(msg));
    }

    template <typename T>
    void element_proxy<T>::insert(int pe) const
    {
        auto msg = message_extractor<void>::get();
        new (&(msg->dst_))
            destination(this->id_, this->idx_, constructor<T, void>(), pe);
        cmk::send(std::move(msg));
    }

    template <typename T>
    template <typename Message, member_fn_t<T, Message> Fn>
    void element_proxy<T>::send(message_ptr<Message>&& msg) const
    {
        new (&(msg->dst_)) destination(
            this->id_, this->idx_, entry<member_fn_t<T, Message>, Fn>());
        cmk::send(std::move(msg));
    }

    template <typename T>
    template <typename Message, member_fn_t<T, Message> Fn>
    cmk::callback<Message> element_proxy<T>::callback(void) const
    {
        return cmk::callback<Message>(
            this->id_, this->idx_, entry<member_fn_t<T, Message>, Fn>());
    }

    template <typename T>
    template <typename Message, combiner_fn_t<Message> Combiner>
    void element_proxy<T>::contribute(
        message_ptr<Message>&& msg, const cmk::callback<Message>& cb) const
    {
        // set the contribution's combiner
        msg->has_combiner() = true;
        new (&(msg->dst_)) destination(
            this->id_, this->idx_, combiner_helper_<Message, Combiner>::id_);
        // set the contribution's continuation
        auto cont = msg->has_continuation();
        CmiAssertMsg(!cont, "continuation of contribution will be overriden");
        cont = true;
        cb.imprint(*(msg->continuation()));
        // send the contribution...
        cmk::lookup(this->id_)->contribute(std::move(msg));
    }

    // Collection proxy member functions
    template <typename T>
    template <typename Message, template <class> class Mapper>
    collection_proxy<T> collection_proxy<T>::construct(
        message_ptr<Message>&& a_msg, const options_type& opts)
    {
        collection_index_t id;
        base_type::next_index_(id);
        new (&a_msg->dst_) destination(id, cmk::helper_::chare_bcast_root_,
            constructor<T, message_ptr<Message>&&>());
        call_constructor_<Mapper>(id, &opts, std::move(a_msg));
        return collection_proxy<T>(id);
    }

    template <typename T>
    template <template <class> class Mapper>
    collection_proxy<T> collection_proxy<T>::construct(const options_type& opts)
    {
        collection_index_t id;
        base_type::next_index_(id);
        auto a_msg = cmk::make_message<message>();
        new (&a_msg->dst_) destination(
            id, cmk::helper_::chare_bcast_root_, constructor<T, void>());
        call_constructor_<Mapper>(id, &opts, std::move(a_msg));
        return collection_proxy<T>(id);
    }

    template <typename T>
    template <template <class> class Mapper>
    collection_proxy<T> collection_proxy<T>::construct(void)
    {
        collection_index_t id;
        base_type::next_index_(id);
        call_constructor_<Mapper>(id, nullptr, message_ptr<>());
        return collection_proxy<T>(id);
    }

    template <typename T>
    template <template <class> class Mapper>
    void collection_proxy<T>::done_inserting(void)
    {
        auto msg = make_message<data_message<bool>>(false);
        new (&msg->dst_) destination(this->id_, cmk::helper_::chare_bcast_root_,
            collection<T, Mapper>::receive_status());
        msg->for_collection() = true;
        auto size = msg->total_size_;
        CmiSyncBroadcastAllAndFree(size, (char*) msg.release());
    }

    template <typename T>
    template <template <class> class Mapper>
    void collection_proxy<T>::call_constructor_(const collection_index_t& id,
        const options_type* opts, message_ptr<>&& a_msg)
    {
        auto kind = collection_kind<T, Mapper>();
        auto offset = sizeof(message);
        auto a_sz = a_msg ? a_msg->total_size_ : 0;
        message_ptr<> msg(new (offset + a_sz + sizeof(options_type)) message);
        auto* base = (char*) msg.get();
        auto* m_opts = reinterpret_cast<options_type*>(base + offset);
        offset += sizeof(options_type);
        new (&msg->dst_) destination(id, cmk::helper_::chare_bcast_root_, kind);
        if (opts)
        {
            new (m_opts) options_type(*opts);
            pack_and_free_(base + offset, std::move(a_msg));
        }
        else
        {
            new (m_opts) options_type();
            CmiAssert(a_msg == nullptr);
        }
        msg->total_size_ += (a_sz + sizeof(options_type));
        msg->has_collection_kind() = true;
        send_helper_(cmk::all::pes, std::move(msg));
    }

    // Group proxy member functions
    template <typename T, bool NodeGroup>
    T* group_proxy<T, NodeGroup>::local_branch(void)
    {
        auto* loc = lookup(this->id_);
        auto idx = index_view<int>::encode(NodeGroup ? CmiMyNode() : CmiMyPe());
        return loc ? loc->template lookup<T>(idx) : nullptr;
    }

    template <typename T, bool NodeGroup>
    template <typename... Args>
    group_proxy<T> group_proxy<T, NodeGroup>::construct(Args&&... args)
    {
        collection_index_t id;
        base_type::next_index_(id);
        auto a_msg = ([&](void) {
            using arg_type = pack_helper_t<Args&&...>;
            auto msg =
                message_extractor<arg_type>::get(std::forward<Args>(args)...);
            new (&msg->dst_) destination(id, cmk::helper_::chare_bcast_root_,
                constructor<T, arg_type>());
            return std::move(msg);
        }) ();
        {
            using options_type = collection_options<int>;
            auto kind = collection_kind<T, mapper_type>();
            auto offset = sizeof(message) + sizeof(options_type);
            auto total_size = offset + a_msg->total_size_;
            message_ptr<> msg(new (total_size) message);
            // update properties of creation message
            new (&msg->dst_)
                destination(id, cmk::helper_::chare_bcast_root_, kind);
            msg->has_collection_kind() = true;
            msg->total_size_ = total_size;
            // set the bounds for the collection
            auto* base = (char*) msg.get();
            auto* opts =
                reinterpret_cast<options_type*>(base + sizeof(message));
            new (opts) options_type(NodeGroup ? CmiNumNodes() : CmiNumPes());
            // copy the argument message onto it
            pack_and_free_(base + offset, std::move(a_msg));
            // broadcast the conjoined message to all PEs/nodes
            send_helper_(
                NodeGroup ? cmk::all::nodes : cmk::all::pes, std::move(msg));
        }
        return group_proxy<T>(id);
    }

}    // namespace cmk

#endif
