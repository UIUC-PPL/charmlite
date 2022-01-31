#ifndef CHARMLITE_CORE_PROXY_HPP
#define CHARMLITE_CORE_PROXY_HPP

#include <charmlite/core/callback.hpp>
#include <charmlite/core/chare.hpp>
#include <charmlite/core/common.hpp>
#include <charmlite/core/ep.hpp>
#include <charmlite/core/locmgr.hpp>

#include <charmlite/utilities/traits.hpp>

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

        template <typename Message>
        void insert(message_ptr<Message>&& msg, int pe = -1) const;

        template <typename... Args>
        void insert(Args... args) const;

        void insert(int pe = -1) const;

        // template <typename Message, member_fn_t<T, Message> Fn>
        template <auto Fn>
        void send(
            typename cmk::extract_message<decltype(Fn)>::ptr_type&& msg) const;

        // Unmarshalled message types
        template <auto Fn, typename... Args>
        void send(Args&&... args) const;

        // template <typename Message, member_fn_t<T, Message> Fn>
        template <auto Fn>
        auto callback(void) const;

    protected:
        // template <typename Message, combiner_fn_t<Message> Combiner>
        template <auto Combiner>
        void contribute(
            typename cmk::extract_message<decltype(Combiner)>::ptr_type&& msg,
            const cmk::callback<
                typename cmk::extract_message<decltype(Combiner)>::type>& cb)
            const;
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
            auto view = index_view<index_type>::encode(idx);
            return element_proxy<T>(this->id_, view);
        }

        // template <typename Message, member_fn_t<T, Message> Fn>
        template <auto Fn>
        auto callback(void) const
        {
            using message_t = typename cmk::extract_message<decltype(Fn)>::type;

            return cmk::callback<message_t>(
                this->id_, cmk::helper_::chare_bcast_root_, entry<Fn>());
        }

        // template <typename Message, member_fn_t<T, Message> Fn>
        template <auto Fn>
        void broadcast(
            typename cmk::extract_message<decltype(Fn)>::ptr_type&& msg) const
        {
            // send a message to the broadcast root
            new (&msg->dst_) destination(
                this->id_, cmk::helper_::chare_bcast_root_, entry<Fn>());
            cmk::send(std::move(msg));
        }

        template <auto Fn, typename... Args>
        void broadcast(Args&&... args) const
        {
            auto msg =
                cmk::marshall_msg<Args...>::pack(std::forward<Args>(args)...);

            // send a message to the broadcast root
            new (&msg->dst_) destination(
                this->id_, cmk::helper_::chare_bcast_root_, entry<Fn>());
            cmk::send(std::move(msg));
        }

        operator collection_index_t(void) const
        {
            return this->id_;
        }

    protected:
        static void next_index_(collection_index_t& idx)
        {
            new (&idx) collection_index_t{
                CmiMyPe(), CpvAccess(local_collection_count_)++};
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
            message_ptr<Message>&& a_msg, const options_type& opts);

        // TODO ( disable using this with reserved mappers (i.e., node/group) )
        template <template <class> class Mapper = default_mapper>
        static collection_proxy<T> construct(const options_type& opts);

        template <template <class> class Mapper = default_mapper>
        static collection_proxy<T> construct(void);

        // TODO ( encode this information within the proxy -- this is NOT user-friendly )
        template <template <class> class Mapper = default_mapper>
        void done_inserting(void);

    private:
        template <template <class> class Mapper = default_mapper>
        static void call_constructor_(const collection_index_t& id,
            const options_type* opts, message_ptr<>&& a_msg);
    };

    template <typename T, bool NodeGroup = false>
    class group_proxy : public collection_proxy_base_<T>
    {
        using base_type = collection_proxy_base_<T>;

    public:
        using index_type = typename base_type::index_type;

        static_assert(std::is_same<index_type, int>::value,
            "groups must use integer indices");

        template <typename Index>
        using mapper_type = typename std::conditional<NodeGroup,
            nodegroup_mapper<Index>, group_mapper<Index>>::type;

        group_proxy(const collection_index_t& id)
          : base_type(id)
        {
        }

        T* local_branch(void);

        template <typename... Args>
        static group_proxy<T> construct(Args&&... args);
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
