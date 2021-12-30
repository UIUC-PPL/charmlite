#ifndef __CMK_DESTINATION_HH__
#define __CMK_DESTINATION_HH__

#include "common.hh"

namespace cmk {
    struct destination
    {
    private:
        struct s_callback_fn_
        {
            callback_id_t id;
            int pe;
        };

        struct s_endpoint_
        {
            collection_index_t collection;
            chare_index_t chare;
            entry_id_t entry;
            // reserved for collection communication
            // TODO ( so rename it as such! )
            bcast_id_t bcast;
        };

        // TODO ( use an std::variant if we upgrade )
        union u_impl_
        {
            s_endpoint_ endpoint_;
            s_callback_fn_ callback_fn_;
        } impl_;

        destination_kind kind_;

    public:
        friend struct message;

        destination(void)
          : kind_(kInvalid)
        {
        }

        destination(callback_id_t id, int pe)
          : kind_(kCallback)
        {
            new (&(this->impl_.callback_fn_))
                s_callback_fn_{.id = id, .pe = pe};
        }

        destination(const collection_index_t& collection,
            const chare_index_t& chare, entry_id_t entry)
          : kind_(kEndpoint)
        {
            new (&(this->impl_.endpoint_)) s_endpoint_{.collection = collection,
                .chare = chare,
                .entry = entry,
                .bcast = 0};
        }

        destination(destination&& dst) = default;
        destination(const destination& dst) = default;

        inline s_callback_fn_& callback_fn(void)
        {
            CmiAssert(this->kind_ == kCallback);
            return this->impl_.callback_fn_;
        }

        inline s_endpoint_& endpoint(void)
        {
            CmiAssert(this->kind_ == kEndpoint);
            return this->impl_.endpoint_;
        }

        inline destination_kind kind(void) const
        {
            return this->kind_;
        }

        inline bool is_broadcast(void) const
        {
            switch (this->kind_)
            {
            case kCallback:
                return (this->impl_.callback_fn_.pe == cmk::all_pes);
            case kEndpoint:
            {
                auto& ep = this->impl_.endpoint_;
                return ep.bcast || (ep.chare == chare_bcast_root_);
            }
            default:
                return false;
            }
        }

        operator bool(void) const
        {
            return !(this->kind_ == kInvalid);
        }

        operator std::string(void) const
        {
            std::stringstream ss;
            ss << "destination(";
            switch (kind_)
            {
            case kCallback:
            {
                auto& cb = this->impl_.callback_fn_;
                ss << "cb=" << cb.id << ",";
                ss << "pe=" << cb.pe;
                break;
            }
            case kEndpoint:
            {
                auto& ep = this->impl_.endpoint_;
                ss << (std::string) ep.collection << ",";
                // ss << "index=" << ep.chare << ",";
                ss << "entry=" << ep.entry;
                break;
            }
            default:
                ss << "???";
                break;
            }
            ss << ")";
            return ss.str();
        }

    private:
        // message uses this to store data inside the union
        // when it only needs the collection id!
        inline void* offset_(void)
        {
            return &(this->impl_.endpoint_.entry);
        }
    };
}    // namespace cmk

#endif
