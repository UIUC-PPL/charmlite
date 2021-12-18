#ifndef __CMK_REDUCTION_HH__
#define __CMK_REDUCTION_HH__

#include "callback.hh"
#include "message.hh"

// TODO ( converse collectives should be isolated/removed )

namespace cmk {
void* converse_combiner_(int* size, void* local, void** remote, int count) {
  auto* sum = static_cast<message*>(local);
  auto comb = combiner_for(sum);
  for (auto i = 0; i < count; i++) {
    auto& lhs = sum;
    auto& rhs = reinterpret_cast<message*&>(remote[i]);
    auto* res = comb(lhs, rhs);
    pick_message_(lhs, rhs, res);
  }
  *size = (int)sum->total_size_;
  return sum;
}

template <typename Message, combiner_fn_t<Message> Combiner,
          callback_fn_t<Message> Callback>
void reduce(Message* msg) {
  // callback will be invoked on pe0
  new (&msg->dst_) destination(callback_helper_<Message, Callback>::id_, 0);
  msg->has_combiner() = true;
  *(msg->combiner()) = combiner_helper_<Message, Combiner>::id_;
  CmiReduce(msg, msg->total_size_, converse_combiner_);
}

message* nop(message* msg, message*) { return msg; }

template <typename T>
data_message<T>* add(data_message<T>* lhs, data_message<T>* rhs) {
  lhs->value() += rhs->value();
  return lhs;
}
}  // namespace cmk

#endif
