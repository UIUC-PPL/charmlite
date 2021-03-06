#ifndef CHARMLITE_CORE_COLLECTION_HPP
#define CHARMLITE_CORE_COLLECTION_HPP

#include <charmlite/core/collection_bridge.hpp>
#include <charmlite/core/common.hpp>

#include <charmlite/utilities/traits.hpp>

#include <iostream>

namespace cmk {
    template <typename T, template <class> class Mapper>
    class collection : public collection_bridge_<T, Mapper>
    {
        using parent_type = collection_bridge_<T, Mapper>;
        using location_message = data_message<int>;

    public:
        using index_type = typename parent_type::index_type;

    private:
        std::unordered_map<chare_index_t, message_buffer_t> buffers_;
        message_buffer_t collective_buffer_;

    public:
        static_assert(
            std::is_base_of<chare_base_, T>::value, "expected a chare!");

        collection(const collection_index_t& id,
            const collection_options<index_type>& opts, const message* msg)
          : parent_type(id)
        {
            // need valid message and options or neither
            CmiEnforceMsg(
                ((bool) opts == (bool) msg), "cannot seed collection");
            // all collections start in an inserting state
            this->set_insertion_status(true, {});
            if (msg)
            {
                using view_type = index_view<index_type>;
                typename view_type::range_type range(
                    opts.start(), opts.step(), opts.end());

                // deliver a copy of the message to all "seeds"
                while (range.has_next())
                {
                    auto next = range.advance();
                    auto view = view_type::encode(next);
                    // NOTE ( I'm pretty sure this is no worse than what Charm )
                    //      ( does vis-a-vis CKARRAYMAP_POPULATE_INITIAL       )
                    // TODO ( that said, it should be elim'd for node/groups   )
                    if (this->locmgr_.home_pe(view) == CmiMyPe())
                    {
                        // simulate an insertion event
                        this->produce();
                        // message should be packed
                        auto clone = msg->clone();
                        clone->dst_.endpoint().chare = view;
                        this->deliver_now(std::move(clone));
                    }
                }
                // the default insertion phase concludes...
                this->set_insertion_status(false, this->ready_callback_());
            }
        }

        virtual void* lookup(const chare_index_t& idx) override
        {
            auto find = this->chares_.find(idx);
            if (find == std::end(this->chares_))
            {
                return nullptr;
            }
            else
            {
                return (find->second).get();
            }
        }

        virtual void flush_buffers(const chare_index_t& idx) override
        {
            auto find = this->buffers_.find(idx);
            if (find == std::end(this->buffers_))
            {
                return;
            }
            else
            {
                auto& buffer = find->second;
                while (!buffer.empty())
                {
                    auto& msg = buffer.front();
                    if (this->try_deliver(msg))
                    {
                        // if successful, pop from the queue
                        buffer.pop_front();
                    }
                    else
                    {
                        // if delivery failed, stop attempting
                        // to deliver messages
                        break;
                    }
                }
            }
        }

        bool try_deliver(message_ptr<>& msg)
        {
            auto& ep = msg->dst_.endpoint();
            auto* rec = msg->has_combiner() ? nullptr : record_for(ep.entry);
            // NOTE ( the lifetime of this variable is tied to the message! )
            auto idx = ep.chare;
            auto home_pe = this->locmgr_.home_pe(idx);
            auto my_pe = CmiMyPe();
            if (rec && rec->is_constructor_)
            {
                if (msg->createhere() || home_pe == my_pe)
                {
                    auto* ch = static_cast<T*>((record_for<T>()).allocate());
                    // set properties of the newly created chare
                    property_setter_<T>()(ch, this->id_, idx);
                    // place the chare within our element list
                    [[maybe_unused]] auto ins = this->chares_.emplace(idx, ch);
                    CmiAssertMsg(ins.second, "insertion did not occur!");
                    CmiAssertMsg(!msg->is_broadcast(), "not implemented!");
                    // call constructor on chare
                    rec->invoke(ch, std::move(msg));
                    // trigger all the chare on-arrival events
                    // ( e.g., notify any listeners )
                    this->on_chare_arrival(ch, true);
                }
                else
                {
                    send_helper_(home_pe, std::move(msg));
                }
            }
            else
            {
                auto find = this->chares_.find(idx);
                // if the element isn't found locally
                if (find == std::end(this->chares_))
                {
                    auto loc = this->locmgr_.lookup(idx);
                    // this pe has no idea where idx is,
                    // since it is the home pe, buffer here
                    if (home_pe == my_pe && loc == home_pe)
                        return false;
                    // this pe has the location of idx from either the locmap
                    // or the routing cache,
                    // forward the message to that location
                    else if (loc != home_pe)
                    {
                        if (msg->sender_pe_ >= 0 && msg->sender_pe_ != my_pe)
                            msg->is_forwarded() = true;
                        send_helper_(loc, std::move(msg));
                    }
                    // this pe has no idea of the location
                    // and it is not the home pe, forward message to
                    // home pe
                    // XXX ( update bcast? prolly not. )
                    else
                    {
                        if (msg->sender_pe_ >= 0 && msg->sender_pe_ != my_pe)
                            msg->is_forwarded() = true;
                        send_helper_(home_pe, std::move(msg));
                    }
                }
                else
                {
                    // idx was found on non home pe and sender was not
                    // the home pe and msg was forwarded at least once
                    // send a routing update message to sender pe to
                    // update the routing cache
                    if (home_pe != my_pe && msg->sender_pe_ != home_pe &&
                        msg->is_forwarded())
                        this->send_location_update_(
                            idx, msg->sender_pe_, my_pe);
                    // otherwise, invoke the EP on the chare
                    handle_(rec, (find->second).get(), std::move(msg));
                }
            }

            return true;
        }

        inline void deliver_now(message_ptr<>&& msg)
        {
            auto& ep = msg->dst_.endpoint();
            if (ep.chare == cmk::helper_::chare_bcast_root_)
            {
                auto* root = this->root();
                auto* obj = root ?
                    static_cast<chare_base_*>(this->lookup(*root)) :
                    nullptr;
                if (obj == nullptr)
                {
                    // if the object is unavailable -- we have to reroute/buffer it
                    auto pe = root ? this->locmgr_.lookup(*root) : CmiMyPe();
                    if (pe == CmiMyPe())
                    {
                        this->collective_buffer_.emplace_back(std::move(msg));
                    }
                    else
                    {
                        send_helper_(pe, std::move(msg));
                    }
                }
                else
                {
                    // otherwise, we increment the broadcast count and go!
                    ep.chare = *root;
                    ep.collective = next_collective_(obj->last_bcast_);
                    handle_(record_for(ep.entry), static_cast<T*>(obj),
                        std::move(msg));
                }
            }
            else if (!try_deliver(msg))
            {
                // buffer messages when delivery attempt fails
                this->buffer_(std::move(msg));
            }
        }

        inline void deliver_later(message_ptr<>&& msg)
        {
            auto& idx = msg->dst_.endpoint().chare;
            auto pe = (idx == cmk::helper_::chare_bcast_root_) ?
                CmiMyPe() :
                this->locmgr_.lookup(idx);
            send_helper_(pe, std::move(msg));
        }

        virtual void on_insertion_complete(void) override
        {
            while (!this->collective_buffer_.empty())
            {
                auto msg = std::move(this->collective_buffer_.front());
                this->collective_buffer_.pop_front();
                this->deliver_now(std::move(msg));
            }
        }

        virtual void deliver(message_ptr<>&& msg, bool immediate) override
        {
            if (immediate)
            {
                // collection-bound messages are routed to us, yay!
                if (msg->for_collection())
                {
                    auto& entry = msg->dst_.endpoint().entry;
                    auto* rec = record_for(entry);
                    rec->invoke(this, std::move(msg));
                }
                else
                {
                    this->deliver_now(std::move(msg));
                }
            }
            else
            {
                this->deliver_later(std::move(msg));
            }
        }

        virtual void contribute(
            message_ptr<>&& msg, std::optional<collective_id_t> tag) override
        {
            auto& ep = msg->dst_.endpoint();
            auto& idx = ep.chare;
            auto* obj = static_cast<chare_base_*>(this->lookup(idx));
            CmiAssert(msg->has_combiner());
            // stamp the message with a sequence number
            ep.collective = tag ? *tag : next_collective_(obj->last_redn_);
            this->handle_reduction_message_(obj, std::move(msg));
        }

    private:
        using reducer_iterator_t =
            typename chare_base_::reducer_map_t::iterator;

        void handle_reduction_message_(chare_base_* obj, message_ptr<>&& msg)
        {
            auto& ep = msg->dst_.endpoint();
            auto& redn = ep.collective;
            auto& reducers = obj->reducers_;
            auto search = this->get_reducer_(obj, redn);
            if (search == std::end(reducers))
            {
                // we couldn't get a hold of a reducer -- so move on
                this->collective_buffer_.emplace_back(std::move(msg));
                return;
            }
            auto& reducer = search->second;
            CmiAssert(redn == reducer.redn);
            reducer.received.emplace_back(std::move(msg));
            // when we've received all expected messages
            if (reducer.ready())
            {
                auto comb = combiner_for(ep.entry);
                auto& recvd = reducer.received;
                auto& lhs = recvd.front();
                for (auto it = std::begin(recvd) + 1; it != std::end(recvd);
                     it++)
                {
                    auto& rhs = *it;
                    auto cont = *(rhs->continuation());
                    // combine them by the given function
                    lhs = comb(std::move(lhs), std::move(rhs));
                    // reset the message's continuation
                    // (in case it was overriden)
                    lhs->has_continuation() = true;
                    new (lhs->continuation()) destination(cont);
                }
                // update result's destination (and clear
                // flags) so we can send it along
                auto& down = reducer.downstream;
                if (down.empty())
                {
                    new (&(lhs->dst_)) destination(*(lhs->continuation()));
                    lhs->has_combiner() = lhs->has_continuation() = false;
                    cmk::send(std::move(lhs));
                }
                else
                {
                    CmiAssert(down.size() == 1);
                    lhs->dst_.endpoint().chare = down.front();
                    this->deliver_later(std::move(lhs));
                }
                // erase the reducer (it's job is done)
                reducers.erase(search);
            }
        }

        void handle_broadcast_message_(
            const entry_record_* rec, chare_base_* obj, message_ptr<>&& msg)
        {
            auto* base = static_cast<chare_base_*>(obj);
            auto& idx = base->index_;
            auto& bcast = msg->dst_.endpoint().collective;
            auto next = next_collective_(base->last_bcast_);
            // broadcasts are processed in-order
            if (bcast == next)
            {
                base->last_bcast_ = next;
                CmiAssert(obj->association_);
                const auto& children = obj->association_->children;
                // ensure message is packed so we can safely clone it
                pack_message(msg);
                // send a copy of the message to all our children
                for (auto& child : children)
                {
                    auto clone = msg->clone();
                    clone->dst_.endpoint().chare = child;
                    clone->sender_pe_ = CmiMyPe();
                    this->deliver_later(std::move(clone));
                }
                // process the message locally
                rec->invoke(obj, std::move(msg));
                // try flushing the buffers since...
                this->flush_buffers(idx);
            }
            else
            {
                // we buffer out-of-order broadcasts
                this->buffer_(std::move(msg));
            }
        }

        // get a chare's reducer, creating one if it doesn't already exist
        reducer_iterator_t get_reducer_(chare_base_* obj, collective_id_t redn)
        {
            // relationships are in flux during static insertion phases
            // so we should not create a new reducer!
            auto& reducers = obj->reducers_;
            if (this->is_inserting())
            {
                return std::end(reducers);
            }
            auto find = reducers.find(redn);
            if (find == std::end(reducers))
            {
                // construct using most up-to-date knowledge of spanning tree
                CmiAssert(obj->association_ && obj->association_->valid_parent);
                const auto& up = obj->association_->children;
                const auto& down = obj->association_->parent;
                // CmiPrintf("(%u:%u)@%d> i have %lu children\n", this->id_.pe_, this->id_.id_, CmiMyPe(), up.size());
                auto ins = reducers.emplace(std::piecewise_construct,
                    std::forward_as_tuple(redn),
                    std::forward_as_tuple(redn, up, down));
                find = ins.first;
            }
            return find;
        }

        void handle_(const entry_record_* rec, T* obj, message_ptr<>&& msg)
        {
            // if a message has a combiner...
            if (msg->has_combiner())
            {
                // then it's a reduction message
                this->handle_reduction_message_(obj, std::move(msg));
            }
            else if (msg->is_broadcast())
            {
                this->handle_broadcast_message_(rec, obj, std::move(msg));
            }
            else
            {
                rec->invoke(obj, std::move(msg));
            }
        }

        inline void buffer_(message_ptr<>&& msg)
        {
            auto& idx = msg->dst_.endpoint().chare;
            this->buffers_[idx].emplace_back(std::move(msg));
        }

        static collective_id_t next_collective_(collective_id_t bcast)
        {
            constexpr auto max_bcast =
                std::numeric_limits<collective_id_t>::max();
            if (bcast == max_bcast)
            {
                return 1;
            }
            else
            {
                return bcast + 1;
            }
        }
    };

    template <typename T>
    struct collection_helper_;

    template <typename T, template <class> class Mapper>
    struct collection_helper_<collection<T, Mapper>>
    {
        static collection_kind_t kind_;
    };

    template <typename T, template <class> class Mapper>
    inline collection_kind_t collection_kind(void)
    {
        return collection_helper_<collection<T, Mapper>>::kind_;
    }
}    // namespace cmk

#endif
