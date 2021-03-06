#ifndef CHARMLITE_CORE_COLLECTION_BRIDGE_HPP
#define CHARMLITE_CORE_COLLECTION_BRIDGE_HPP

#include <charmlite/core/chare.hpp>
#include <charmlite/core/completion.hpp>
#include <charmlite/core/locmgr.hpp>
#include <charmlite/core/message.hpp>
#include <charmlite/core/options.hpp>

#include <charmlite/utilities/math.hpp>
#include <charmlite/utilities/traits.hpp>

#include <functional>
#include <optional>

namespace cmk {
    class collection_base_
    {
    protected:
        collection_index_t id_;

        virtual void on_insertion_complete(void) = 0;

    public:
        virtual ~collection_base_() = default;

        collection_base_(const collection_index_t& id)
          : id_(id)
        {
        }

        virtual void* lookup(const chare_index_t&) = 0;
        virtual void deliver(message_ptr<>&& msg, bool immediate) = 0;
        virtual void contribute(
            message_ptr<>&& msg, std::optional<collective_id_t> tag) = 0;

        virtual bool emigrate(const chare_index_t&, int) = 0;

        template <typename T>
        inline T* lookup(const chare_index_t& idx)
        {
            return static_cast<T*>(this->lookup(idx));
        }

        virtual void flush_buffers(const chare_index_t&) = 0;
    };

    using collection_constructor_t =
        collection_base_* (*) (const collection_index_t&,
            const collection_options_base_&, const message*);

    using collection_kinds_t = std::vector<collection_constructor_t>;
    using collection_kind_t = typename collection_kinds_t::size_type;

    using collection_table_t =
        collection_map<std::unique_ptr<collection_base_>>;

    using collection_buffer_t = std::unordered_map<collection_index_t,
        message_buffer_t, collection_index_hasher_>;

    // Shared between workers in a process
    CMK_GENERATE_SINGLETON(collection_kinds_t, collection_kinds_);
    // Each worker has its own instance of these
    CpvExtern(collection_table_t, collection_table_);
    CpvExtern(collection_buffer_t, collection_buffer_);

    // implements mapper-specific behaviors for
    // chare-collections -- i.e., listeners and spanning tree construction
    template <typename T, template <class> class Mapper, typename Enable = void>
    class collection_bridge_;

    template <typename T, template <class> class Mapper>
    class collection_bridge_<T, Mapper,
        typename std::enable_if<(std::is_same<nodegroup_mapper<index_for_t<T>>,
                                     Mapper<index_for_t<T>>>::value ||
            std::is_same<group_mapper<index_for_t<T>>,
                Mapper<index_for_t<T>>>::value)>::type>
      : public collection_base_
    {
    protected:
        std::unordered_map<chare_index_t, std::unique_ptr<T>> chares_;
        chare_index_t endpoint_;

        using index_type = index_for_t<T>;
        Mapper<index_type> locmgr_;

        collection_bridge_(const collection_index_t& id)
          : collection_base_(id)
          , endpoint_(index_view<index_type>::encode(0))
        {
        }

        void on_chare_arrival(T* obj, [[maybe_unused]] bool created)
        {
            auto* elt = static_cast<chare_base_*>(obj);
            this->associate(elt);
            this->flush_buffers(elt->index_);
            CmiAssertMsg(created, "group-like chares cannot migrate");
        }

        std::nullptr_t ready_callback_(void)
        {
            return nullptr;
        }

        bool is_inserting(void) const
        {
            return false;
        }

        void produce(void) {}

        void set_insertion_status(bool, std::nullptr_t) {}

        const chare_index_t* root(void) const
        {
            return &(this->endpoint_);
        }

        void send_location_update_(const chare_index_t&, int, int) {}

        void associate(chare_base_* elt)
        {
            auto& assoc = elt->association_;
            auto pe = this->locmgr_.lookup(elt->index_);
            auto parent = this->locmgr_.span_tree_parent(pe);
            auto n_children = this->locmgr_.num_span_tree_children(pe);

            CmiAssert((pe == CmiMyPe()) && !assoc);
            assoc.reset(new association_);

            if (n_children > 0)
            {
                std::vector<index_type> child_pes(n_children);
                this->locmgr_.span_tree_children(pe, child_pes.data());
                auto& children = assoc->children;
                children.reserve(n_children);
                std::transform(std::begin(child_pes), std::end(child_pes),
                    std::back_inserter(children),
                    index_view<index_type>::encode);
                CmiAssert(n_children == static_cast<int>(children.size()));
            }

            if (parent >= 0)
            {
                assoc->put_parent(index_view<index_type>::encode(parent));
            }
            else
            {
                assoc->valid_parent = true;
            }
        }

        virtual bool emigrate(const chare_index_t&, int) override
        {
            return false;
        }
    };

    // this more or less implements the logic of hypercomm's
    // tree builder... the code there is better commented for the time being:
    // https://github.com/jszaday/hypercomm/blob/main/include/hypercomm/tree_builder/tree_builder.hpp
    // TODO ( copy the comments from there)
    template <typename T, template <class> class Mapper, typename Enable>
    class collection_bridge_ : public collection_base_
    {
        using self_type = collection_bridge_<T, Mapper>;
        using element_type = chare_base_*;

        using index_message = data_message<std::pair<int, chare_index_t>>;
        using location_message = data_message<int>;

    public:
        static entry_id_t receive_status(void)
        {
            using receiver_type = member_fn_t<self_type, data_message<bool>>;
            return cmk::entry<static_cast<receiver_type>(
                &self_type::receive_status)>();
        }

        static entry_id_t receive_location_update(void)
        {
            using receiver_type = member_fn_t<self_type, location_message>;
            return cmk::entry<static_cast<receiver_type>(
                &self_type::receive_location_update)>();
        }

        static entry_id_t receive_chare(void)
        {
            using receiver_type = member_fn_t<self_type, message>;
            return cmk::entry<static_cast<receiver_type>(
                &self_type::receive_chare)>();
        }

    protected:
        using index_type = index_for_t<T>;

        std::unordered_map<chare_index_t, std::unique_ptr<T>> chares_;
        locmgr<Mapper<index_type>> locmgr_;
        chare_index_t endpoint_;

        collection_bridge_(const collection_index_t& id)
          : collection_base_(id)
          , locmgr_(std::bind(&self_type::send_location_update_, this,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3))
          , endpoint_(cmk::helper_::chare_bcast_root_)
        {
        }

        void produce(std::int64_t count = 1)
        {
            if (count != 0)
            {
                system_detector_()->produce(this->id_, count);
            }
        }

        bool is_inserting(void) const
        {
            return this->is_inserting_;
        }

        void on_chare_depature(T*, bool)
        {
            // TODO ( trigger listeners when they're added  )
            // TODO ( remove element's location from table? )
        }

        void on_chare_arrival(T* obj, bool created)
        {
            auto* elt = static_cast<element_type>(obj);
            auto& idx = elt->index_;
            auto my_pe = CmiMyPe();
            auto home_pe = this->locmgr_.home_pe(idx);
            // if this event is due to migration, then
            // the home pe has already been updated
            if (created && (home_pe != my_pe))
            {
                this->send_location_update_(idx, home_pe, my_pe);
            }
            // try associating the chare (via the tree-builder)
            this->associate(elt);
            // flush any messages we have for it
            this->flush_buffers(idx);
        }

        void set_insertion_status(bool status, const callback<message>& cb)
        {
            if (status)
            {
                this->is_inserting_ = status;

                if ((bool) cb)
                {
                    CmiAbort("not implemented");
                }
            }
            else if ((bool) cb)
            {
                auto msg = cmk::make_message<completion::detection_message>(
                    this->id_, cb);
                system_detector_()->start_detection(std::move(msg));
            }
            else
            {
                this->is_inserting_ = false;
            }
        }

        const chare_index_t* root(void) const
        {
            if (this->is_inserting_ ||
                (this->endpoint_ == cmk::helper_::chare_bcast_root_))
            {
                return nullptr;
            }
            else
            {
                return &(this->endpoint_);
            }
        }

        callback<message> ready_callback_(void)
        {
            return callback<message>::construct<
                &self_type::insertion_complete_>(cmk::all::pes);
        }

        void send_location_update_(
            const chare_index_t& idx, int dst_pe, int loc)
        {
            auto msg = cmk::make_message<location_message>(loc);
            new (&msg->dst_) destination(this->id_, idx,
                collection<T, Mapper>::receive_location_update());
            msg->for_collection() = true;
            send_helper_(dst_pe, std::move(msg));
        }

        virtual bool emigrate(const chare_index_t& idx, int dst_pe) override
        {
            auto search = this->chares_.find(idx);
            if (search == std::end(this->chares_))
            {
                return false;
            }
            else
            {
                auto* t = (search->second).get();
                // abort migration if we can't do the thing
                if (!chare_can_migrate_(t))
                {
                    return false;
                }
                // 0) inform listeners that the chare's leaving
                //    TODO ( add about_to_migrate? )
                this->on_chare_depature(t, false);
                // 1) size the chare
                PUP::sizer s(PUP::er::IS_MIGRATION);
                pup_chare_(s, t);
                // 2) so we can allocate an appropriately-sized message
                auto total_size = s.size() + sizeof(message);
                auto* msg = new (total_size) message(0x0, total_size);
                // 3) then, pack the chare into the message
                PUP::toMem p(
                    (char*) msg + sizeof(message), PUP::er::IS_MIGRATION);
                pup_chare_(p, t);
                CmiAssert(p.size() == (total_size - sizeof(message)));
                // 4) update its destination then send it
                new (&msg->dst_) destination(
                    this->id_, idx, collection<T, Mapper>::receive_chare());
                msg->for_collection() = true;
                send_helper_(dst_pe, message_ptr<>(msg));
                // 5) then update our local tables
                //   `true` indicates that we need to inform the home pe
                this->locmgr_.update_location(idx, dst_pe, true);
                this->chares_.erase(search);
                // 6) re-route messages to new location
                this->flush_buffers(idx);
                // TODO ( pack any buffered messages with the chare )
                //      ( this will fold n-messages down to 1       )
                return true;
            }
        }

    private:
        static bool chare_can_migrate_(const T* t)
        {
            auto* elt = static_cast<const chare_base_*>(t);
            auto status = elt->parent_can_migrate_();

            if constexpr (impl::has_can_migrate_v<T>)
            {
                status = status && t->can_migrate();
            }

            return status;
        }

        static void reconstruct_chare_(T* t)
        {
            if constexpr (std::is_constructible_v<T, PUP::reconstruct>)
            {
                new (t) T(PUP::reconstruct{});
            }
            else if constexpr (std::is_default_constructible_v<T>)
            {
                new (t) T;
            }
            else
            {
                CmiAbort(
                    "could not reconstruct chare of type %s", typeid(T).name());
            }
        }

        static void pup_chare_(PUP::er& p, T* t)
        {
            if (p.isUnpacking())
            {
                reconstruct_chare_(t);
            }

            static_cast<chare_base_*>(t)->parent_pup_(p);

            if constexpr (is_pupable_v<T>)
            {
                t->pup(p);
            }
        }

        void receive_chare(message_ptr<>&& msg)
        {
            auto& ep = msg->dst_.endpoint();
            auto& idx = ep.chare;
            auto mine = CmiMyPe();
            // restore the chare from the message
            auto* t = (T*) (::operator new(sizeof(T)));
            PUP::fromMem p(
                (char*) msg.get() + sizeof(message), PUP::er::IS_MIGRATION);
            pup_chare_(p, t);
            CmiAssert((idx == static_cast<chare_base_*>(t)->index_) &&
                (p.size() == (msg->total_size_ - sizeof(message))));
            // update our local tables
            this->locmgr_.update_location(idx, mine);
            this->chares_.emplace(idx, t);
            // call the chares "on_migrated" function if it has one
            if constexpr (impl::has_on_migrated_v<T>)
            {
                t->on_migrated();
            }
            // trigger the chare arrival events
            this->on_chare_arrival(t, false);
        }

        void receive_status(message_ptr<data_message<bool>>&& msg)
        {
            if (msg->value())
            {
                CmiAbort("not implemented");
            }
            else
            {
                this->set_insertion_status(false, this->ready_callback_());
            }
        }

        static void insertion_complete_(message_ptr<>&& msg)
        {
            using dm_t = completion::detection_message;
            auto dm_kind = message_helper_<dm_t>::kind_;
            auto* dm = (dm_kind == msg->kind_) ? static_cast<dm_t*>(msg.get()) :
                                                 nullptr;
            auto& id = std::get<0>(dm->value());
            auto* self = static_cast<self_type*>(cmk::lookup(id));
            CmiAssert(self->is_inserting_);
            self->is_inserting_ = false;
            self->on_insertion_complete();
        }

        bool is_inserting_;

        struct facade_
        {
            int pe_;
            element_type elt_;

            facade_(int pe)
              : pe_(pe)
              , elt_(nullptr)
            {
            }
            facade_(element_type elt)
              : pe_(cmk::all::pes)
              , elt_(elt)
            {
            }
        };

        template <typename Message, member_fn_t<self_type, Message> Fn,
            typename... Args>
        cmk::message_ptr<Message> make_message(Args&&... args)
        {
            auto msg = cmk::make_message<Message>(std::forward<Args>(args)...);
            auto entry = cmk::entry<Fn>();
            new (&(msg->dst_))
                destination(this->id_, cmk::helper_::chare_bcast_root_, entry);
            msg->for_collection() = true;
            return msg;
        }

        void consume(void)
        {
            this->produce(-1);
        }

        void receive_location_update(cmk::message_ptr<location_message>&& msg)
        {
            auto& pe = msg->value();
            auto& idx = msg->dst_.endpoint().chare;
            this->locmgr_.update_location(idx, pe);
            this->flush_buffers(idx);
        }

        void receive_upstream(cmk::message_ptr<index_message>&& msg)
        {
            auto& val = msg->value();
            auto& src = val.first;
            auto& idx = val.second;
            this->register_upstream(facade_(src), idx);
            this->consume();
        }

        void receive_downstream(cmk::message_ptr<index_message>&& msg)
        {
            auto& val = msg->value();
            auto& src = val.first;
            auto& idx = val.second;
            auto target = this->register_downstream(facade_(src), idx);
            if (target == nullptr)
            {
                this->consume();
            }
            else
            {
                auto msg = this->make_message<index_message,
                    &self_type::receive_upstream>(CmiMyPe(), target->index_);
                send_helper_(src, std::move(msg));
            }
        }

        void receive_endpoint(cmk::message_ptr<index_message>&& msg)
        {
            auto& val = msg->value();
            auto& src = val.first;
            auto& idx = val.second;
            this->register_endpoint(facade_(src), idx);
        }

        void register_endpoint(const facade_& f, const chare_index_t& idx)
        {
            CmiAssert(this->endpoint_ == helper_::chare_bcast_root_);
            this->endpoint_ = idx;
            auto elt =
                f.elt_ ? f.elt_ : this->template lookup<chare_base_>(idx);
            CmiAssert((elt == nullptr) || (idx == elt->index_));
            if (elt)
            {
                elt->association_->valid_parent = true;
            }
            auto mine = CmiMyPe();
            auto leaves = binary_tree::leaves(mine, CmiNumPes());
            for (auto& leaf : leaves)
            {
                auto msg = this->make_message<index_message,
                    &self_type::receive_endpoint>(CmiMyPe(), idx);
                send_helper_(leaf, std::move(msg));
            }
            this->produce((std::int64_t) leaves.size() - 1);
        }

        void send_downstream(const facade_&, const chare_index_t&)
        {
            // this should not occur under normal circumstances since
            // all messages _should_ have valid destinations
            CmiAbort("not implemented");
        }

        void send_upstream(const facade_& f, const chare_index_t& idx)
        {
            auto mine = CmiMyPe();
            auto parent = binary_tree::parent(mine);

            auto send = [&](int pe) {
                auto src = f.elt_ == nullptr ? f.pe_ : mine;
                auto msg = this->make_message<index_message,
                    &self_type::receive_downstream>(src, idx);
                send_helper_(pe, std::move(msg));
            };

            if (parent >= 0)
            {
                send(parent);
            }
            else if (this->endpoint_ == helper_::chare_bcast_root_)
            {
                // set the element as the endpoint if it
                // hasn't been set before
                this->register_endpoint(f, idx);
            }
            else
            {
                // TODO ( this is a fascimile of send_downstream )
                send((this->locmgr_).lookup(this->endpoint_));
            }

            this->produce();
        }

        element_type register_downstream(
            const facade_& f, const chare_index_t& idx)
        {
            element_type min = nullptr;
            for (auto& pair : this->chares_)
            {
                auto& ch = pair.second;
                if (idx == ch->index_)
                {
                    continue;
                }
                else if ((min == nullptr) ||
                    (min->num_children_() > ch->num_children_()))
                {
                    min = ch.get();
                }
            }

            if (min == nullptr)
            {
                this->send_upstream(f, idx);

                return nullptr;
            }
            else
            {
                min->association_->put_child(idx);

                return min;
            }
        }

        element_type register_upstream(
            const facade_& f, const chare_index_t& idx)
        {
            element_type found = nullptr;
            for (auto& pair : this->chares_)
            {
                auto& ch = pair.second;
                if (idx == ch->index_)
                {
                    continue;
                }
                // TODO ( investigate whether this should be relaxed? )
                //      ( maybe include chares without _ANY_ associatons? )
                else if (!(ch->association_ && ch->association_->valid_parent))
                {
                    found = ch.get();
                }
            }

            if (found == nullptr)
            {
                this->send_downstream(f, idx);

                return nullptr;
            }
            else
            {
                found->association_->put_parent(idx);

                return found;
            }
        }

        void associate(element_type elt)
        {
            auto& assoc = elt->association_;
            if (this->is_inserting_ && !assoc)
            {
                assoc.reset(new association_);
                auto* target =
                    this->register_downstream(facade_(elt), elt->index_);
                if (target != nullptr)
                {
                    assoc->put_parent(target->index_);
                }
                // confirm the element's been created
                this->consume();
            }
            else
            {
                CmiAssertMsg(assoc, "dynamic insertions must be associated");
            }
        }
    };
}    // namespace cmk

#endif
