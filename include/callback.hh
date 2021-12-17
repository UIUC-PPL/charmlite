#ifndef __CMK_CALLBACK_HH__
#define __CMK_CALLBACK_HH__

#include "message.hh"

namespace cmk {

template <combiner_t Fn>
struct combiner_helper_ {
  static combiner_id_t id_;
};

template <callback_t Fn>
struct callback_helper_ {
  static callback_id_t id_;
};

inline combiner_t combiner_for(combiner_id_t id) {
  return id ? CsvAccess(combiner_table_)[id - 1] : nullptr;
}

inline callback_t callback_for(callback_id_t id) {
  return id ? CsvAccess(callback_table_)[id - 1] : nullptr;
}

inline combiner_t combiner_for(message* msg) {
  auto* id = msg->combiner();
  return id ? combiner_for(*id) : nullptr;
}

inline callback_t callback_for(message* msg) {
  return (msg->dst_.kind() == kCallback)
             ? callback_for(msg->dst_.callback_fn().id)
             : nullptr;
}

class callback {
  destination dst_;

  template <typename... Args>
  callback(Args&&... args) : dst_(std::forward<Args>(args)...) {}

 public:
  void send(message* msg) {
    new (&(msg->dst_)) destination(this->dst_);
    cmk::send(msg);
  }

  template <callback_t Callback>
  static callback construct(int pe) {
    return callback(callback_helper_<Callback>::id_, pe);
  }
};
}  // namespace cmk

#endif
