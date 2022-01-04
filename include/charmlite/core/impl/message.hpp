#ifndef CHARMLITE_CORE_IMPL_MESSAGE_HPP
#define CHARMLITE_CORE_IMPL_MESSAGE_HPP

#include <charmlite/core/message.hpp>

namespace cmk {

    inline combiner_id_t* message::combiner(void)
    {
        if (this->has_combiner())
        {
            return reinterpret_cast<combiner_id_t*>(this->dst_.offset_());
        }
        else
        {
            return nullptr;
        }
    }

    inline destination* message::continuation(void)
    {
        if (this->has_continuation())
        {
            return reinterpret_cast<destination*>(this->reserve_.data());
        }
        else
        {
            return nullptr;
        }
    }

    inline message::flag_type message::has_combiner(void)
    {
        return this->flags_[has_combiner_];
    }

    inline bool message::has_combiner(void) const
    {
        return this->flags_[has_combiner_];
    }

    inline message::flag_type message::for_collection(void)
    {
        return this->flags_[for_collection_];
    }

    inline bool message::for_collection(void) const
    {
        return this->flags_[for_collection_];
    }

    inline message::flag_type message::has_continuation(void)
    {
        return this->flags_[has_continuation_];
    }

    inline bool message::has_continuation(void) const
    {
        return this->flags_[has_continuation_];
    }

    inline message::flag_type message::is_packed(void)
    {
        return this->flags_[is_packed_];
    }

    inline bool message::is_packed(void) const
    {
        return this->flags_[is_packed_];
    }

    inline message::flag_type message::has_collection_kind(void)
    {
        return this->flags_[has_collection_kind_];
    }

    inline bool message::has_collection_kind(void) const
    {
        return this->flags_[has_collection_kind_];
    }

    inline const message_record_* message::record(void) const
    {
        if (this->kind_ == 0)
        {
            return nullptr;
        }
        else
        {
            return &(CMK_ACCESS_SINGLETON(message_table_)[this->kind_ - 1]);
        }
    }

    inline void message::free(void* blk)
    {
        if (blk == nullptr)
        {
            return;
        }
        else
        {
            auto* msg = static_cast<message*>(blk);
            auto* rec = msg->record();
            if (rec == nullptr)
            {
                message::operator delete(msg);
            }
            else
            {
                rec->deleter_(msg);
            }
        }
    }

    inline bool message::is_cloneable(void) const
    {
        if (this->is_packed())
        {
            return true;
        }
        else
        {
            auto* rec = this->record();
            return (rec == nullptr || rec->packer_ == nullptr);
        }
    }

    template <typename T>
    message_ptr<T> message::clone(void) const
    {
        CmiAssert(this->is_cloneable());
        auto* typed =
            reinterpret_cast<T*>(CmiCopyMsg((char*) this, this->total_size_));
        return message_ptr<T>(typed);
    }

    inline void* message::operator new(std::size_t count, std::size_t sz)
    {
        CmiAssert(sz >= sizeof(message));
        return CmiAlloc(sz);
    }

    inline void message::operator delete(void* blk, std::size_t sz)
    {
        CmiFree(blk);
    }

    inline void* message::operator new(std::size_t sz)
    {
        return CmiAlloc(sz);
    }

    inline void message::operator delete(void* blk)
    {
        CmiFree(blk);
    }

    inline bool message::is_broadcast(void) const
    {
        return this->dst_.is_broadcast();
    }

    // Utility function definition
    inline void pack_message(message_ptr<>& msg)
    {
        auto* rec = msg ? msg->record() : nullptr;
        auto* fn = rec ? rec->packer_ : nullptr;
        if (fn && !(msg->is_packed()))
        {
            fn(msg);
            msg->is_packed() = true;
        }
    }

    inline void unpack_message(message_ptr<>& msg)
    {
        auto* rec = msg ? msg->record() : nullptr;
        auto* fn = rec ? rec->unpacker_ : nullptr;
        if (fn && msg->is_packed())
        {
            fn(msg);
            msg->is_packed() = false;
        }
    }

    inline void pack_and_free_(char* dst, message_ptr<>&& src)
    {
        pack_message(src);
        memcpy(dst, src.get(), src->total_size_);
    }

    inline void send_helper_(int pe, message_ptr<>&& msg)
    {
        // NOTE ( we only need to pack when we're going off-node )
        if (pe == cmk::all::pes)
        {
            pack_message(msg);

            auto msg_size = msg->total_size_;

            CmiSyncBroadcastAllAndFree(msg_size, (char*) msg.release());
        }
        else if (pe == cmk::all::nodes)
        {
            pack_message(msg);

            auto msg_size = msg->total_size_;

            CmiSyncNodeBroadcastAllAndFree(msg_size, (char*) msg.release());
        }
        else
        {
            if (CmiNodeOf(pe) == CmiMyNode())
            {
                CmiPushPE(pe, msg.release());
            }
            else
            {
                pack_message(msg);

                auto msg_size = msg->total_size_;

                CmiSyncSendAndFree(pe, msg_size, (char*) msg.release());
            }
        }
    }

}    // namespace cmk

#endif
