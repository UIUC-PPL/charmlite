#ifndef __CMK_CALLBACK_HH__
#define __CMK_CALLBACK_HH__

#include "message.hh"

namespace cmk {

template <typename Message, combiner_fn_t<Message> Fn>
struct combiner_helper_ {
  static combiner_id_t id_;
};

template <typename Message, callback_fn_t<Message> Fn>
struct callback_helper_ {
  static callback_id_t id_;
};

inline combiner_fn_t<message> combiner_for(combiner_id_t id) {
  return id ? CsvAccess(combiner_table_)[id - 1] : nullptr;
}

inline callback_fn_t<message> callback_for(callback_id_t id) {
  return id ? CsvAccess(callback_table_)[id - 1] : nullptr;
}

inline combiner_fn_t<message> combiner_for(message* msg) {
  auto* id = msg->combiner();
  return id ? combiner_for(*id) : nullptr;
}

inline callback_fn_t<message> callback_for(message* msg) {
  return (msg->dst_.kind() == kCallback)
             ? callback_for(msg->dst_.callback_fn().id)
             : nullptr;
}

template <typename Message>
class callback {
  destination dst_;

  template <typename... Args>
  callback(Args&&... args) : dst_(std::forward<Args>(args)...) {}

 public:
  template <typename T>
  friend class collection_proxy_base_;

  template <typename T>
  friend class element_proxy;

  inline void imprint(destination& dst) const {
    new (&dst) destination(this->dst_);
  }

  inline void imprint(message* msg) const { this->imprint(msg->dst_); }

  void send(Message* msg) {
    this->imprint(msg);
    cmk::send(msg);
  }

  template <callback_fn_t<Message> Callback>
  static callback<Message> construct(int pe) {
    return callback<Message>(callback_helper_<Message, Callback>::id_, pe);
  }
};
}  // namespace cmk

#endif
