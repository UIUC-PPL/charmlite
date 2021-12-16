#ifndef __CMK_DESTINATION_HH__
#define __CMK_DESTINATION_HH__

#include "common.hh"

namespace cmk {
struct destination {
 private:
  struct s_callback_fn_ {
    callback_id_t id;
    int pe;
  };

  struct s_endpoint_ {
    collective_index_t collective;
    chare_index_t chare;
    entry_id_t entry;
    // reserved for collective communication
    bcast_id_t bcast;
  };

  // TODO ( use an std::variant if we upgrade )
  union u_impl_ {
    s_endpoint_ endpoint_;
    s_callback_fn_ callback_fn_;
  } impl_;

  destination_kind kind_;

 public:
  friend class message;

  destination(void) : kind_(kInvalid) {}

  destination(callback_id_t id, int pe) : kind_(kCallback) {
    new (&(this->impl_.callback_fn_)) s_callback_fn_{.id = id, .pe = pe};
  }

  destination(const collective_index_t& collective, const chare_index_t& chare,
              entry_id_t entry)
      : kind_(kEndpoint) {
    new (&(this->impl_.endpoint_))
        s_endpoint_{.collective = collective, .chare = chare, .entry = entry};
  }

  inline s_callback_fn_& callback_fn(void) {
    CmiAssert(this->kind_ == kCallback);
    return this->impl_.callback_fn_;
  }

  inline s_endpoint_& endpoint(void) {
    CmiAssert(this->kind_ == kEndpoint);
    return this->impl_.endpoint_;
  }

  inline destination_kind kind(void) const { return this->kind_; }

  inline bool is_broadcast(void) const {
    switch (this->kind_) {
      case kCallback:
        return (this->impl_.callback_fn_.pe == cmk::all);
      case kEndpoint: {
        auto& ep = this->impl_.endpoint_;
        return ep.bcast || (ep.chare == chare_bcast_root_);
      }
      default:
        return false;
    }
  }

 private:
  // message uses this to store data inside the union
  // when it only needs the collective id!
  inline void* offset_(void) { return &(this->impl_.endpoint_.entry); }
};
}  // namespace cmk

#endif
