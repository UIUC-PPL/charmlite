#include "core.hh"

#include "callback.hh"
#include "collection.hh"
#include "ep.hh"

namespace cmk {
CpvDeclare(int, deliver_handler_);

entry_table_t entry_table_;
chare_table_t chare_table_;
message_table_t message_table_;
callback_table_t callback_table_;
combiner_table_t combiner_table_;
collection_kinds_t collection_kinds_;
collection_table_t collection_table_;
collection_buffer_t collection_buffer_;
std::uint32_t local_collection_count_ = 0;

void start_fn_(int, char** argv) {
  register_deliver_();
#if CHARMLITE_TOPOLOGY
  CmiNodeAllBarrier();
  CmiInitCPUAffinity(argv);
  CmiInitMemAffinity(argv);
  CmiInitCPUTopology(argv);
  // threads wait until _topoTree has been generated
#endif
  CmiNodeAllBarrier();
  CsdScheduleForever();
}

// handle an exit message on each pe
// circulate it if not received via broadcast
void exit(message* msg) {
  if (msg->is_broadcast()) {
    message::free(msg);
  } else {
    msg->dst_.callback_fn().pe = cmk::all;
    CmiSyncBroadcastAndFree(msg->total_size_, (char*)msg);
  }
  CsdExitScheduler();
}

inline void deliver_to_endpoint_(message* msg, bool immediate) {
  auto& ep = msg->dst_.endpoint();
  auto& col = ep.collection;
  if (msg->has_collection_kind()) {
    auto kind = (collection_kind_t)ep.entry;
    auto& rec = collection_kinds_[kind - 1];
    auto* obj = rec(col);
    auto ins = collection_table_.emplace(col, obj);
    CmiAssertMsg(ins.second, "insertion did not occur!");
    auto find = collection_buffer_.find(col);
    if (find == std::end(collection_buffer_)) {
      return;
    } else {
      auto& buffer = find->second;
      while (!buffer.empty()) {
        auto* tmp = buffer.front().release();
        buffer.pop_front();
        obj->deliver(tmp, immediate);
      }
    }
  } else {
    auto search = collection_table_.find(col);
    if (search == std::end(collection_table_)) {
      collection_buffer_[col].emplace_back(msg);
    } else {
      search->second->deliver(msg, immediate);
    }
  }
}

inline void deliver_to_callback_(message* msg) {
#if CMK_ERROR_CHECKING
  auto pe = msg->dst_.callback_fn().pe;
  CmiEnforce(pe == cmk::all || pe == CmiMyPe());
#endif
  (callback_for(msg))(msg);
}

void deliver(void* raw) {
  auto* msg = static_cast<message*>(raw);
  switch (msg->dst_.kind()) {
    case kEndpoint:
      deliver_to_endpoint_(msg, true);
      break;
    case kCallback:
      deliver_to_callback_(msg);
      break;
    default:
      CmiAbort("invalid message destination");
  }
}

void send(message* msg) {
  auto& dst = msg->dst_;
  switch (dst.kind()) {
    case kCallback: {
      auto& cb = dst.callback_fn();
      if (cb.pe == cmk::all) {
        CmiSyncBroadcastAllAndFree(msg->total_size_, (char*)msg);
      } else {
        // then forward it along~
        CmiSyncSendAndFree(cb.pe, msg->total_size_, (char*)msg);
      }
      break;
    }
    case kEndpoint:
      CmiAssert(!msg->has_collection_kind());
      deliver_to_endpoint_(msg, false);
      break;
    default:
      CmiAbort("invalid message destination");
  }
}
}  // namespace cmk