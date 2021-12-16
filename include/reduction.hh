#ifndef __CMK_REDUCTION_HH__
#define __CMK_REDUCTION_HH__

#include "callback.hh"
#include "message.hh"

namespace cmk {
void* converse_combiner_(int* size, void* local, void** remote, int count) {
  auto* sum = static_cast<message*>(local);
  auto comb = combiner_for(sum);
  for (auto i = 0; i < count; i++) {
    auto& lhs = sum;
    auto& rhs = reinterpret_cast<message*&>(remote[i]);
    auto* res = comb(lhs, rhs);
    if (res == lhs) {
      // result is OK
      continue;
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
  *size = (int)sum->total_size_;
  return sum;
}

template <callback_t Callback>
void all_reduce_impl_(message* msg) {
  // update destination
  new (&msg->dst_) destination(callback_helper_<Callback>::id_, cmk::all);
  // broadcast message to other pes
  CmiSyncBroadcast(msg->total_size_, (char*)msg);
  // then deliver it locally
  deliver(msg);
}

template <combiner_t Combiner, callback_t Callback>
void reduce(message* msg) {
  // callback will be invoked on pe0
  new (&msg->dst_) destination(callback_helper_<Callback>::id_, 0);
  msg->has_combiner() = true;
  *(msg->combiner()) = combiner_helper_<Combiner>::id_;
  CmiReduce(msg, msg->total_size_, converse_combiner_);
}

template <combiner_t Combiner, callback_t Callback>
void all_reduce(message* msg) {
  reduce<Combiner, all_reduce_impl_<Callback>>(msg);
}

message* nop(message* msg, message*) { return msg; }

template <typename T>
message* add(message* lhs, message* rhs) {
  using type = data_message<T>*;
  static_cast<type>(lhs)->value() += static_cast<type>(rhs)->value();
  return lhs;
}
}  // namespace cmk

#endif
