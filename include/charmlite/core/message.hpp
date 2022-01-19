#ifndef CHARMLITE_CORE_MESSAGE_HPP
#define CHARMLITE_CORE_MESSAGE_HPP

#include <array>
#include <bitset>
#include <memory>
#include <vector>

#include <converse.h>

#include <charmlite/core/common.hpp>
#include <charmlite/core/destination.hpp>

namespace cmk {

    using message_deleter_t = void (*)(void*);
    using message_packer_t = void (*)(message_ptr<>&);
    using message_unpacker_t = void (*)(message_ptr<>&);

    struct message_record_
    {
        message_deleter_t deleter_;
        message_packer_t packer_;
        message_unpacker_t unpacker_;

        message_record_(const message_deleter_t& deleter,
            const message_packer_t& packer, const message_unpacker_t& unpacker)
          : deleter_(deleter)
          , packer_(packer)
          , unpacker_(unpacker)
        {
        }
    };

    using message_table_t = std::vector<message_record_>;
    using message_kind_t = typename message_table_t::size_type;
    CMK_GENERATE_SINGLETON(message_table_t, message_table_);

    template <typename T>
    struct message_helper_
    {
        static message_kind_t kind_;
    };

    namespace {
        struct message_fields_
        {
            std::array<char, CmiMsgHeaderSizeBytes> core_;
            message_kind_t kind_;
            std::bitset<8> flags_;
            std::size_t total_size_;
            int sender_pe_;
            destination dst_;
        };
    }    // namespace

    // pad the messages with extra room for a continuation
    struct alignment
    {
        static constexpr auto reserve_align =
            (sizeof(message_fields_) + sizeof(destination)) % ALIGN_BYTES;
    };
    // TODO ( use std::byte if we upgrade )
    using aligned_reserve_t = std::array<std::uint8_t,
        sizeof(destination) + alignment::reserve_align>;

    struct message
    {
        std::array<char, CmiMsgHeaderSizeBytes> core_;
        message_kind_t kind_;
        std::bitset<8> flags_;
        std::size_t total_size_;
        int sender_pe_;
        destination dst_;
        aligned_reserve_t reserve_;

    private:
        static constexpr auto has_combiner_ = 0;
        static constexpr auto has_continuation_ = has_combiner_ + 1;
        static constexpr auto has_collection_kind_ = has_continuation_ + 1;
        static constexpr auto is_packed_ = has_collection_kind_ + 1;
        static constexpr auto for_collection_ = is_packed_ + 1;
        static constexpr auto createhere_ = for_collection_ + 1;
        static constexpr auto is_forwarded_ = createhere_ + 1;

    public:
        using flag_type = std::bitset<8>::reference;

        message(void)
          : kind_(0)
          , total_size_(sizeof(message))
          , sender_pe_(-1)
        {
            CmiSetHandler(this, CpvAccess(converse_handler_));
        }

        message(message_kind_t kind, std::size_t total_size)
          : kind_(kind)
          , total_size_(total_size)
          , sender_pe_(-1)
        {
            // FIXME ( DRY failure )
            CmiSetHandler(this, CpvAccess(converse_handler_));
        }

        combiner_id_t* combiner(void);

        destination* continuation(void);

        flag_type has_combiner(void);
        bool has_combiner(void) const;

        flag_type for_collection(void);
        bool for_collection(void) const;

        flag_type createhere(void);
        bool createhere(void) const;

        flag_type is_forwarded(void);
        bool is_forwarded(void) const;

        flag_type has_continuation(void);
        bool has_continuation(void) const;

        flag_type is_packed(void);
        bool is_packed(void) const;

        flag_type has_collection_kind(void);
        bool has_collection_kind(void) const;

        template <typename T>
        static void free(std::unique_ptr<T>& msg)
        {
            free(msg.release());
        }

        const message_record_* record(void) const;

        static void free(void* blk);

        bool is_cloneable(void) const;

        // clones a PACKED message
        template <typename T = message>
        message_ptr<T> clone(void) const;

        void* operator new(std::size_t count, std::size_t sz);

        void operator delete(void* blk, std::size_t sz);

        void* operator new(std::size_t sz);

        void operator delete(void* blk);

        bool is_broadcast(void) const;
    };

    static_assert(sizeof(message) % ALIGN_BYTES == 0, "message unaligned");

    template <typename T>
    struct plain_message : public message
    {
        plain_message(void)
          : message(message_helper_<T>::kind_, sizeof(T))
        {
        }
    };

    template <typename T>
    struct data_message : public plain_message<data_message<T>>
    {
        using type = T;

    private:
        using storage_type =
            typename std::aligned_storage<sizeof(T), alignof(T)>::type;
        storage_type storage_;

    public:
        template <typename... Args>
        data_message(Args&&... args)
        {
            new (&(this->value())) T(std::forward<Args>(args)...);
        }

        T& value(void)
        {
            return *(reinterpret_cast<T*>(&(this->storage_)));
        }

        const T& value(void) const
        {
            return *(reinterpret_cast<const T*>(&(this->storage_)));
        }
    };

    // utility function to pick optimal send mechanism
    inline void send_helper_(int pe, message_ptr<>&& msg);

    inline void pack_message(message_ptr<>& msg);

    inline void unpack_message(message_ptr<>& msg);

    inline void pack_and_free_(char* dst, message_ptr<>&& src);

}    // namespace cmk

#endif
