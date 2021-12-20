#ifndef __CMK_MSG_HH__
#define __CMK_MSG_HH__

#include <array>
#include <bitset>

#include "common.hh"

namespace cmk {
using message_deleter_t = void (*)(void *);
using message_packer_t = void (*)(message *&);
using message_unpacker_t = void (*)(message *&);

struct message_record_ {
  message_deleter_t deleter_;
  message_packer_t packer_;
  message_unpacker_t unpacker_;

  message_record_(const message_deleter_t &deleter,
                  const message_packer_t &packer,
                  const message_unpacker_t &unpacker)
      : deleter_(deleter), packer_(packer), unpacker_(unpacker) {}
};

using message_table_t = std::vector<message_record_>;
using message_kind_t = typename message_table_t::size_type;
CsvExtern(message_table_t, message_table_);

template <typename T>
struct message_helper_ {
  static message_kind_t kind_;
};

#define CMK_MESSAGE_FIELDS                       \
  std::array<char, CmiMsgHeaderSizeBytes> core_; \
  message_kind_t kind_;                          \
  std::bitset<8> flags_;                         \
  std::size_t total_size_;                       \
  destination dst_;

namespace {
struct message_fields_ {
  CMK_MESSAGE_FIELDS;
};
}  // namespace

// pad the messages with extra room for a continuation
constexpr auto reserve_align =
    (sizeof(message_fields_) + sizeof(destination)) % ALIGN_BYTES;
// TODO ( use std::byte if we upgrade )
using aligned_reserve_t =
    std::array<std::uint8_t, sizeof(destination) + reserve_align>;

struct message {
  CMK_MESSAGE_FIELDS;
  aligned_reserve_t reserve_;

 private:
  static constexpr auto has_combiner_ = 0;
  static constexpr auto has_continuation_ = has_combiner_ + 1;
  static constexpr auto has_collection_kind_ = has_continuation_ + 1;
  static constexpr auto is_packed_ = has_collection_kind_ + 1;

 public:
  using flag_type = std::bitset<8>::reference;

  message(void) : kind_(0), total_size_(sizeof(message)) {
    CmiSetHandler(this, CpvAccess(converse_handler_));
  }

  message(message_kind_t kind, std::size_t total_size)
      : kind_(kind), total_size_(total_size) {
    // FIXME ( DRY failure )
    CmiSetHandler(this, CpvAccess(converse_handler_));
  }

  combiner_id_t *combiner(void) {
    if (this->has_combiner()) {
      return reinterpret_cast<combiner_id_t *>(this->dst_.offset_());
    } else {
      return nullptr;
    }
  }

  destination *continuation(void) {
    if (this->has_continuation()) {
      return reinterpret_cast<destination *>(this->reserve_.data());
    } else {
      return nullptr;
    }
  }

  flag_type has_combiner(void) { return this->flags_[has_combiner_]; }

  flag_type has_continuation(void) { return this->flags_[has_continuation_]; }

  flag_type is_packed(void) { return this->flags_[is_packed_]; }

  flag_type has_collection_kind(void) {
    return this->flags_[has_collection_kind_];
  }

  template <typename T>
  static void free(std::unique_ptr<T> &msg) {
    free(msg.release());
  }

  const message_record_ *record(void) const {
    if (this->kind_ == 0) {
      return nullptr;
    } else {
      return &(CsvAccess(message_table_)[this->kind_ - 1]);
    }
  }

  static void free(void *blk) {
    if (blk == nullptr) {
      return;
    } else {
      auto *msg = static_cast<message *>(blk);
      auto *rec = msg->record();
      if (rec == nullptr) {
        message::operator delete(msg);
      } else {
        rec->deleter_(msg);
      }
    }
  }

  bool is_cloneable(void) const {
    if (const_cast<message *>(this)->is_packed()) {
      return true;
    } else {
      auto* rec = this->record();
      return (rec == nullptr || rec->packer_ == nullptr);
    }
  }

  // clones a PACKED message
  message *clone(void) const {
    CmiAssert(this->is_cloneable());
    return (message *)CmiCopyMsg((char *)this, this->total_size_);
  }

  void *operator new(std::size_t count, std::size_t sz) {
    CmiAssert(sz >= sizeof(message));
    return CmiAlloc(sz);
  }

  void operator delete(void *blk, std::size_t sz) { CmiFree(blk); }

  void *operator new(std::size_t sz) { return CmiAlloc(sz); }

  void operator delete(void *blk) { CmiFree(blk); }

  bool is_broadcast(void) { return this->dst_.is_broadcast(); }
};

inline void pack_message(message *&msg) {
  auto *rec = msg ? msg->record() : nullptr;
  auto *fn = rec ? rec->packer_ : nullptr;
  if (fn && !(msg->is_packed())) {
    fn(msg);
    msg->is_packed() = true;
  }
}

inline void unpack_message(message *&msg) {
  auto *rec = msg ? msg->record() : nullptr;
  auto *fn = rec ? rec->unpacker_ : nullptr;
  if (fn && msg->is_packed()) {
    fn(msg);
    msg->is_packed() = false;
  }
}

inline void pack_and_free_(char *dst, message *src) {
  pack_message(src);
  memcpy(dst, src, src->total_size_);
  message::free(src);
}

static_assert(sizeof(message) % ALIGN_BYTES == 0, "message unaligned");

#undef CMK_MESSAGE_FIELDS

template <typename T>
struct plain_message : public message {
  plain_message(void) : message(message_helper_<T>::kind_, sizeof(T)) {}
};

template <typename T>
struct data_message : public plain_message<data_message<T>> {
  using type = T;

 private:
  using storage_type =
      typename std::aligned_storage<sizeof(T), alignof(T)>::type;
  storage_type storage_;

 public:
  template <typename... Args>
  data_message(Args &&...args) {
    new (&(this->value())) T(std::forward<Args>(args)...);
  }

  T &value(void) { return *(reinterpret_cast<T *>(&(this->storage_))); }

  const T &value(void) const {
    return *(reinterpret_cast<const T *>(&(this->storage_)));
  }
};

// utility function to pick optimal send mechanism
inline void send_helper_(int pe, message *msg) {
  // NOTE ( we only need to pack when we're going off-node )
  if (pe == cmk::all) {
    pack_message(msg);

    CmiSyncBroadcastAllAndFree(msg->total_size_, (char *)msg);
  } else {
    if (CmiNodeOf(pe) == CmiMyNode()) {
      CmiPushPE(pe, msg);
    } else {
      pack_message(msg);

      CmiSyncSendAndFree(pe, msg->total_size_, (char *)msg);
    }
  }
}

template <typename T>
inline bool operator==(const T *lhs, const std::unique_ptr<T> &rhs) {
  return lhs == rhs.get();
}

// helper function for combining the results of reductions
template <typename A, typename B>
inline void pick_message_(A &lhs, A &rhs, B res) {
  if (res == lhs) {
    // result is OK
    return;
  } else if (res == rhs) {
    // result is remote -- so we have
    // to swap lhs so it's freed as remote
    std::swap(lhs, rhs);
  } else {
    // combiner alloc'd a new non-remote
    // message so we have to free
    message::free(lhs);
  }
}
}  // namespace cmk

#endif
