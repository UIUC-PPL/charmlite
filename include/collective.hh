#ifndef __CMK_COLLECTIVE_HH__
#define __CMK_COLLECTIVE_HH__

#include "chare.hh"
#include "ep.hh"
#include "locmgr.hh"
#include "message.hh"

namespace cmk {
struct collective_base_ {
  collective_index_t id_;
  collective_base_(const collective_index_t& id) : id_(id) {}
  virtual ~collective_base_() = default;
  virtual void* lookup(const chare_index_t&) = 0;
  virtual void deliver(message* msg, bool immediate) = 0;

  template <typename T>
  inline T* lookup(const chare_index_t& idx) {
    return static_cast<T*>(this->lookup(idx));
  }
};

template <typename T, typename Mapper>
struct collective : public collective_base_ {
  locmgr<Mapper> locmgr_;

  std::unordered_map<chare_index_t, message_buffer_t> buffers_;
  std::unordered_map<chare_index_t, std::unique_ptr<T>> chares_;

  static_assert(std::is_base_of<chare_base_, T>::value, "expected a chare!");

  collective(const collective_index_t& id) : collective_base_(id) {}

  virtual void* lookup(const chare_index_t& idx) override {
    auto find = this->chares_.find(idx);
    if (find == std::end(this->chares_)) {
      return nullptr;
    } else {
      return (find->second).get();
    }
  }

  void flush_buffers(const chare_index_t& idx) {
    auto find = this->buffers_.find(idx);
    if (find == std::end(this->buffers_)) {
      return;
    } else {
      auto& buffer = find->second;
      while (!buffer.empty()) {
        auto& msg = buffer.front();
        if (this->try_deliver(msg.get())) {
          // release since we consumed the message
          msg.release();
          // the pop it from the queue
          buffer.pop_front();
        } else {
          // if delivery failed, stop attempting
          // to deliver messages
          break;
        }
      }
    }
  }

  bool try_deliver(message* msg) {
    auto& ep = msg->dst_.endpoint();
    auto* rec = record_for(ep.entry);
    auto& idx = ep.chare;
    auto pe = this->locmgr_.pe_for(idx);
    // TODO ( temporary constraint, elements only created on home pe )
    if (rec->is_constructor_ && (pe == CmiMyPe())) {
      auto* ch = static_cast<T*>((record_for<T>()).allocate());
      // set properties of the newly created chare
      property_setter_<T>()(ch, this->id_, idx);
      // place the chare within our element list
      auto ins = chares_.emplace(idx, ch);
      // call constructor on chare
      (rec->fn_)(ch, msg);
      // flush any messages we have for it
      flush_buffers(idx);
    } else {
      auto find = chares_.find(idx);
      // if the element isn't found locally
      if (find == std::end(this->chares_)) {
        // and it's our chare...
        if (pe == CmiMyPe()) {
          // it hasn't been created yet, so buffer
          return false;
        } else {
          // otherwise route to the home pe
          // XXX ( update bcast? prolly not. )
          CmiSyncSendAndFree(pe, msg->total_size_, (char*)msg);
        }
      } else {
        // otherwise, invoke the EP on the chare
        handle_(rec, (find->second).get(), msg);
      }
    }

    return true;
  }

  inline void deliver_now(message* msg) {
    auto& ep = msg->dst_.endpoint();
    if (ep.chare == chare_bcast_root_) {
      auto root = locmgr_.root();
      auto* obj = static_cast<chare_base_*>(this->lookup(root));
      if (obj == nullptr) {
        // if the object is unavailable -- we have to reroute it
        // ( this could be a loopback, then we try again later )
        send_helper_(locmgr_.pe_for(root), msg);
      } else {
        // otherwise, we increment the broadcast count and go!
        ep.chare = root;
        ep.bcast = obj->last_bcast_ + 1;
        handle_(record_for(ep.entry), static_cast<T*>(obj), msg);
      }
    } else if (!try_deliver(msg)) {
      // buffer messages when delivery attempt fails
      this->buffer_(msg);
    }
  }

  inline void deliver_later(message* msg) {
    auto& idx = msg->dst_.endpoint().chare;
    auto pe = this->locmgr_.pe_for(idx);
    send_helper_(pe, msg);
  }

  virtual void deliver(message* msg, bool immediate) override {
    if (immediate) {
      this->deliver_now(msg);
    } else {
      this->deliver_later(msg);
    }
  }

 private:
  void handle_(const entry_record_* rec, T* obj, message* msg) {
    if (msg->is_broadcast()) {
      auto* base = static_cast<chare_base_*>(obj);
      auto& idx = base->index_;
      auto& bcast = msg->dst_.endpoint().bcast;
      // broadcasts are processed in-order
      if (bcast == (base->last_bcast_ + 1)) {
        base->last_bcast_++;
        auto children = this->locmgr_.upstream(idx);
        // send a copy of the message to all our children
        for (auto& child : children) {
          auto* clone = msg->clone();
          clone->dst_.endpoint().chare = child;
          this->deliver_later(clone);
        }
        // process the message locally
        (rec->fn_)(obj, msg);
        // try flushing the buffers since...
        this->flush_buffers(idx);
      } else {
        // we buffer out-of-order broadcasts
        this->buffer_(msg);
      }
    } else {
      (rec->fn_)(obj, msg);
    }
  }

  inline void buffer_(message* msg) {
    auto& idx = msg->dst_.endpoint().chare;
    this->buffers_[idx].emplace_back(msg);
  }
};

template <typename T>
struct collective_helper_;

template <typename T, typename Mapper>
struct collective_helper_<collective<T, Mapper>> {
  static collective_kind_t kind_;
};
}  // namespace cmk

#endif
