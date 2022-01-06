#include <charmlite/charmlite.hpp>

namespace cmk {

    // Define globals
    constexpr int all::pes;
    constexpr int all::nodes;

    constexpr entry_id_t helper_::nil_entry_;
    constexpr chare_index_t helper_::chare_bcast_root_;

    // these can be nix'd when we upgrade to c++17
    constexpr int default_options<int>::start;
    constexpr int default_options<int>::step;

    CpvDeclare(collection_table_t, collection_table_);
    CpvDeclare(collection_buffer_t, collection_buffer_);
    CpvDeclare(std::uint32_t, local_collection_count_);
    CpvDeclare(int, converse_handler_);

    completion* system_detector_(void)
    {
        collection_index_t sys{.pe_ = cmk::all::pes, .id_ = 0};
        auto& table = CpvAccess(collection_table_);
        auto find = table.find(sys);
        collection_base_* loc = nullptr;
        if (find == std::end(table))
        {
            collection_options<int> opts(CmiNumPes());
            auto msg = cmk::make_message<message>();
            new (&msg->dst_) destination(sys, cmk::helper_::chare_bcast_root_,
                constructor<completion, void>());
            loc =
                new collection<completion, group_mapper>(sys, opts, msg.get());
            table.emplace(sys, loc);
        }
        else
        {
            loc = find->second.get();
        }
        auto idx = index_view<int>::encode(CmiMyPe());
        return loc->template lookup<completion>(idx);
    }

    void initialize_globals_(void)
    {
        CpvInitialize(collection_table_t, collection_table_);
        CpvInitialize(collection_buffer_t, collection_buffer_);
        // collection ids start after zero
        CpvInitialize(std::uint32_t, local_collection_count_);
        CpvAccess(local_collection_count_) = 0;
        // register converse handlers
        CpvInitialize(int, converse_handler_);
        CpvAccess(converse_handler_) = CmiRegisterHandler(converse_handler_);
    }

    void start_fn_(int, [[maybe_unused]] char** argv)
    {
        initialize_globals_();
#if CHARMLITE_TOPOLOGY
        CmiNodeAllBarrier();
        CmiInitCPUAffinity(argv);
        CmiInitMemAffinity(argv);
        CmiInitCPUTopology(argv);
        // threads wait until _topoTree has been generated
#endif
        CmiNodeAllBarrier();
        if (CmiInCommThread())
        {
            // the comm thread has to get back to work!
            return;
        }
        else
        {
            CsdScheduleForever();
        }
    }

    // handle an exit message on each pe
    // circulate it if not received via broadcast
    void exit(message_ptr<>&& msg)
    {
        if (!msg->is_broadcast())
        {
            msg->dst_.callback_fn().pe = cmk::all::pes;
            pack_message(msg);    // XXX ( this is likely overkill )
            CmiSyncBroadcastAndFree(msg->total_size_, (char*) msg.release());
        }
        CsdExitScheduler();
    }

    inline void deliver_to_endpoint_(message_ptr<>&& msg, bool immediate)
    {
        auto& ep = msg->dst_.endpoint();
        auto& buf = CpvAccess(collection_buffer_);
        auto& tab = CpvAccess(collection_table_);
        auto& col = ep.collection;
        if (msg->has_collection_kind())
        {
            auto kind = (collection_kind_t) ep.entry;
            auto& rec = CMK_ACCESS_SINGLETON(collection_kinds_)[kind - 1];
            CmiAssert(rec);
            // determine whether or not the creation message
            // is attached to an argument message
            auto* base = (char*) msg.get();
            auto* opts = base + sizeof(message);
            auto offset = sizeof(message) + sizeof(collection_options_base_);
            auto* arg =
                (msg->total_size_ == offset) ? nullptr : (base + offset);
            auto* obj =
                rec(col, *(reinterpret_cast<collection_options_base_*>(opts)),
                    reinterpret_cast<message*>(arg));
            [[maybe_unused]] auto ins = tab.emplace(col, obj);
            CmiAssertMsg(ins.second, "insertion did not occur!");
            auto find = buf.find(col);
            // free now that we're done with the endpoint
            message::free(msg);
            // check whether there are buffered messages...
            if (find == std::end(buf))
            {
                return;
            }
            else
            {
                auto& buffer = find->second;
                while (!buffer.empty())
                {
                    obj->deliver(std::move(buffer.front()), immediate);
                    buffer.pop_front();
                }
            }
        }
        else
        {
            auto search = tab.find(col);
            if (search == std::end(tab))
            {
                buf[col].emplace_back(std::move(msg));
            }
            else
            {
                search->second->deliver(std::move(msg), immediate);
            }
        }
    }

    inline void deliver_to_callback_(message_ptr<>&& msg)
    {
#if CMK_ERROR_CHECKING
        auto pe = msg->dst_.callback_fn().pe;
        CmiEnforce(pe == cmk::all::pes || pe == CmiMyPe());
#endif
        // prepare message for local-processing
        unpack_message(msg);
        // then process it
        (callback_for(msg))(std::move(msg));
    }

    void converse_handler_(void* raw)
    {
        message_ptr<> msg(static_cast<message*>(raw));
        // then determine how to route it
        switch (msg->dst_.kind())
        {
        case destination_kind::Endpoint:
            deliver_to_endpoint_(std::move(msg), true);
            break;
        case destination_kind::Callback:
            deliver_to_callback_(std::move(msg));
            break;
        default:
            CmiAbort("invalid message destination");
        }
    }

    void send(message_ptr<>&& msg)
    {
        auto& dst = msg->dst_;
        switch (dst.kind())
        {
        case destination_kind::Callback:
        {
            auto& cb = dst.callback_fn();
            send_helper_(cb.pe, std::move(msg));
            break;
        }
        case destination_kind::Endpoint:
            CmiAssert(!msg->has_collection_kind());
            deliver_to_endpoint_(std::move(msg), false);
            break;
        default:
            CmiAbort("invalid message destination");
        }
    }
}    // namespace cmk
