#ifndef __CMK_PROXY_HH__
#define __CMK_PROXY_HH__

#include "callback.hh"
#include "chare.hh"
#include "ep.hh"
#include "locmgr.hh"

namespace cmk {

    template <typename T, typename Enable = void>
    struct message_extractor;

    template <>
    struct message_extractor<void>
    {
        static message_ptr<> get(void)
        {
            return message_ptr<>(new message);
        }
    };

    template <typename Message>
    struct message_extractor<message_ptr<Message>&&,
        typename std::enable_if<std::is_base_of<message, Message>::value>::type>
    {
        static message_ptr<> get(message_ptr<Message>&& msg)
        {
            return std::move(msg);
        }
    };

    template <typename... Args>
    struct pack_helper;

    template <typename T>
    struct pack_helper<T>
    {
        using type = T;
    };

    template <>
    struct pack_helper<>
    {
        using type = void;
    };

    template <typename... Args>
    using pack_helper_t = typename pack_helper<Args...>::type;

    class element_proxy_base_
    {
    private:
        collection_index_t id_;
        chare_index_t idx_;

    public:
        template <typename T>
        friend class element_proxy;

        element_proxy_base_(element_proxy_base_&&) = default;
        element_proxy_base_(const element_proxy_base_&) = default;
        element_proxy_base_(
            const collection_index_t& id, const chare_index_t& idx)
          : id_(id)
          , idx_(idx)
        {
        }
    };

    template <typename T>
    class element_proxy : public element_proxy_base_
    {
    public:
        friend T;

        element_proxy(element_proxy<T>&&) = default;
        element_proxy(const element_proxy<T>&) = default;
        element_proxy(const collection_index_t& id, const chare_index_t& idx)
          : element_proxy_base_(id, idx)
        {
        }

        template <typename... Args>
        void insert(Args&&... args) const
        {
            using arg_type = pack_helper_t<Args&&...>;
            auto msg =
                message_extractor<arg_type>::get(std::forward<Args>(args)...);
            new (&(msg->dst_))
                destination(this->id_, this->idx_, constructor<T, arg_type>());
            cmk::send(std::move(msg));
        }

        template <typename Message, member_fn_t<T, Message> Fn>
        void send(message_ptr<Message>&& msg) const
        {
            new (&(msg->dst_)) destination(
                this->id_, this->idx_, entry<member_fn_t<T, Message>, Fn>());
            cmk::send(std::move(msg));
        }

        template <typename Message, member_fn_t<T, Message> Fn>
        cmk::callback<Message> callback(void) const
        {
            return cmk::callback<Message>(
                this->id_, this->idx_, entry<member_fn_t<T, Message>, Fn>());
        }

    protected:
        template <typename Message, combiner_fn_t<Message> Combiner>
        void contribute(
            message_ptr<Message>&& msg, const cmk::callback<Message>& cb) const
        {
            // set the contribution's combiner
            msg->has_combiner() = true;
            new (&(msg->dst_)) destination(this->id_, this->idx_,
                combiner_helper_<Message, Combiner>::id_);
            // set the contribution's continuation
            auto cont = msg->has_continuation();
            CmiAssertMsg(
                !cont, "continuation of contribution will be overriden");
            cont = true;
            cb.imprint(*(msg->continuation()));
            // send the contribution...
            cmk::lookup(this->id_)->contribute(std::move(msg));
        }
    };

    template <typename T>
    class collection_proxy_base_
    {
    protected:
        collection_index_t id_;

    public:
        using index_type = index_for_t<T>;

        collection_proxy_base_(const collection_index_t& id)
          : id_(id)
        {
        }

        element_proxy<T> operator[](const index_type& idx) const
        {
            auto& view = index_view<index_type>::decode(idx);
            return element_proxy<T>(this->id_, view);
        }

        template <typename Message, member_fn_t<T, Message> Fn>
        cmk::callback<Message> callback(void) const
        {
            return cmk::callback<Message>(this->id_, chare_bcast_root_,
                entry<member_fn_t<T, Message>, Fn>());
        }

        template <typename Message, member_fn_t<T, Message> Fn>
        void broadcast(message_ptr<Message>&& msg) const
        {
            // send a message to the broadcast root
            new (&msg->dst_) destination(this->id_, chare_bcast_root_,
                entry<member_fn_t<T, Message>, Fn>());
            cmk::send(std::move(msg));
        }

        operator collection_index_t(void) const
        {
            return this->id_;
        }

    protected:
        static void next_index_(collection_index_t& idx)
        {
            new (&idx) collection_index_t{(std::uint32_t) CmiMyPe(),
                CpvAccess(local_collection_count_)++};
        }
    };

    template <typename T>
    class collection_proxy : public collection_proxy_base_<T>
    {
        using base_type = collection_proxy_base_<T>;

    public:
        using index_type = typename base_type::index_type;
        using options_type = collection_options<index_type>;

        collection_proxy(const collection_index_t& id)
          : base_type(id)
        {
        }

        template <typename Message,
            template <class> class Mapper = default_mapper>
        static collection_proxy<T> construct(
            message_ptr<Message>&& a_msg, const options_type& opts)
        {
            collection_index_t id;
            base_type::next_index_(id);
            new (&a_msg->dst_)
                destination(id, chare_bcast_root_, constructor<T, Message>());
            call_construtor_<Mapper>(id, &opts, std::move(a_msg));
            return collection_proxy<T>(id);
        }

        // TODO ( disable using this with reserved mappers (i.e., node/group) )
        template <template <class> class Mapper = default_mapper>
        static collection_proxy<T> construct(const options_type& opts)
        {
            collection_index_t id;
            base_type::next_index_(id);
            auto a_msg = cmk::make_message<message<>>();
            new (&a_msg->dst_)
                destination(id, chare_bcast_root_, constructor<T, void>());
            call_construtor_<Mapper>(id, &opts, std::move(a_msg));
            return collection_proxy<T>(id);
        }

        template <template <class> class Mapper = default_mapper>
        static collection_proxy<T> construct(void)
        {
            collection_index_t id;
            base_type::next_index_(id);
            call_construtor_<Mapper>(id, nullptr, message_ptr<>());
            return collection_proxy<T>(id);
        }

        void done_inserting(void) {}

    private:
        template <template <class> class Mapper = default_mapper>
        static void call_construtor_(const collection_index_t& id,
            const options_type* opts, message_ptr<>&& a_msg)
        {
            auto kind = collection_helper_<collection<T, Mapper>>::kind_;
            auto offset = sizeof(message);
            auto a_sz = a_msg ? a_msg->total_size_ : 0;
            message_ptr<> msg(
                new (offset + a_sz + sizeof(options_type)) message);
            auto* base = (char*) msg.get();
            auto* m_opts = reinterpret_cast<options_type*>(base + offset);
            offset += sizeof(options_type);
            new (&msg->dst_) destination(id, chare_bcast_root_, kind);
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
            send_helper_(cmk::all, std::move(msg));
        }
    };

    template <typename T>
    class group_proxy : public collection_proxy_base_<T>
    {
        using base_type = collection_proxy_base_<T>;

    public:
        using index_type = typename base_type::index_type;

        static_assert(std::is_same<index_type, int>::value,
            "groups must use integer indices");

        group_proxy(const collection_index_t& id)
          : base_type(id)
        {
        }

        T* local_branch(void)
        {
            auto* loc = lookup(this->id_);
            return loc ? loc->template lookup<T>(CmiMyPe()) : nullptr;
        }

        template <typename... Args>
        static group_proxy<T> construct(Args&&... args)
        {
            collection_index_t id;
            base_type::next_index_(id);
            auto a_msg = ([&](void) {
                using arg_type = pack_helper_t<Args&&...>;
                auto msg = message_extractor<arg_type>::get(
                    std::forward<Args>(args)...);
                new (&msg->dst_) destination(
                    id, chare_bcast_root_, constructor<T, arg_type>());
                return std::move(msg);
            })();
            {
                using options_type = collection_options<int>;
                auto kind =
                    collection_helper_<collection<T, group_mapper>>::kind_;
                auto offset = sizeof(message) + sizeof(options_type);
                auto total_size = offset + a_msg->total_size_;
                message_ptr<> msg(new (total_size) message);
                // update properties of creation message
                new (&msg->dst_) destination(id, chare_bcast_root_, kind);
                msg->has_collection_kind() = true;
                msg->total_size_ = total_size;
                // set the bounds for the collection
                auto* base = (char*) msg.get();
                auto* opts =
                    reinterpret_cast<options_type*>(base + sizeof(message));
                new (opts) options_type(CmiNumPes());
                // copy the argument message onto it
                pack_and_free_(base + offset, std::move(a_msg));
                // broadcast the conjoined message to all PEs
                send_helper_(cmk::all, std::move(msg));
            }
            return group_proxy<T>(id);
        }
    };

    template <typename T, typename Index>
    class chare : public chare_base_
    {
    public:
        const Index& index(void) const
        {
            return index_view<Index>::decode(this->index_);
        }

        // NOTE ( if we associated chares with particular collections
        //        we could make this a typed proxy )
        const collection_index_t& collection(void) const
        {
            return this->parent_;
        }

        collection_proxy_base_<T> collection_proxy(void) const
        {
            return collection_proxy_base_<T>(this->parent_);
        }

        cmk::element_proxy<T> element_proxy(void) const
        {
            return cmk::element_proxy<T>(this->parent_, this->index_);
        }
    };

}    // namespace cmk

#endif
