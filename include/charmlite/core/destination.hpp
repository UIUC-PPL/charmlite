#ifndef CHARMLITE_CORE_DESTINATION_HPP
#define CHARMLITE_CORE_DESTINATION_HPP

#include <charmlite/core/common.hpp>

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
            collective_id_t collective;
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
          : kind_(destination_kind::Invalid)
        {
        }

        destination(callback_id_t id, int pe)
          : kind_(destination_kind::Callback)
        {
            new (&(this->impl_.callback_fn_))
                s_callback_fn_{.id = id, .pe = pe};
        }

        destination(const collection_index_t& collection,
            const chare_index_t& chare, entry_id_t entry)
          : kind_(destination_kind::Endpoint)
        {
            new (&(this->impl_.endpoint_)) s_endpoint_{.collection = collection,
                .chare = chare,
                .entry = entry,
                .collective = 0};
        }

        destination(destination&& dst) = default;
        destination(const destination& dst) = default;

        /* this function is reserved for internal use
         * and should NOT be used in user-code
         *
         * ( it shouldn't even be here, really! )
         */
        inline char* extra(void)
        {
            CmiAssert(this->kind_ == destination_kind::Callback);
            auto* xtra = (char*) &(this->impl_.endpoint_.entry);
            CmiAssert((std::uintptr_t) xtra >
                (std::uintptr_t)(
                    (char*) &(this->impl_.callback_fn_.pe) + sizeof(int)));
            return xtra;
        }

        inline s_callback_fn_& callback_fn(void)
        {
            CmiAssert(this->kind_ == destination_kind::Callback);
            return this->impl_.callback_fn_;
        }

        inline s_endpoint_& endpoint(void)
        {
            CmiAssert(this->kind_ == destination_kind::Endpoint);
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
            case destination_kind::Callback:
                return (this->impl_.callback_fn_.pe == cmk::all::pes);
            case destination_kind::Endpoint:
            {
                auto& ep = this->impl_.endpoint_;
                return ep.collective ||
                    (ep.chare == cmk::helper_::chare_bcast_root_);
            }
            default:
                return false;
            }
        }

        operator bool(void) const
        {
            return !(this->kind_ == destination_kind::Invalid);
        }

        operator std::string(void) const
        {
            std::stringstream ss;
            ss << "destination(";
            switch (kind_)
            {
            case destination_kind::Callback:
            {
                auto& cb = this->impl_.callback_fn_;
                ss << "cb=" << cb.id << ",";
                ss << "pe=" << cb.pe;
                break;
            }
            case destination_kind::Endpoint:
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
    };
}    // namespace cmk

#endif
