#include "core.hh"

#include "callback.hh"
#include "collection.hh"
#include "ep.hh"

namespace cmk {

CsvDeclare(entry_table_t, entry_table_);
CsvDeclare(chare_table_t, chare_table_);
CsvDeclare(message_table_t, message_table_);
CsvDeclare(callback_table_t, callback_table_);
CsvDeclare(combiner_table_t, combiner_table_);
CsvDeclare(collection_kinds_t, collection_kinds_);

CpvDeclare(collection_table_t, collection_table_);
CpvDeclare(collection_buffer_t, collection_buffer_);
CpvDeclare(std::uint32_t, local_collection_count_);
CpvDeclare(int, deliver_handler_);

void initialize_globals_(void) {
  if (CmiMyRank() == 0) {
    CsvInitialize(entry_table_t, entry_table_);
    CsvInitialize(chare_table_t, chare_table_);
    CsvInitialize(message_table_t, message_table_);
    CsvInitialize(callback_table_t, callback_table_);
    CsvInitialize(combiner_table_t, combiner_table_);
    CsvInitialize(collection_kinds_t, collection_kinds_);
  }
  CpvInitialize(collection_table_t, collection_table_);
  CpvInitialize(collection_buffer_t, collection_buffer_);
  // collection ids start after zero
  CpvInitialize(std::uint32_t, local_collection_count_);
  CpvAccess(local_collection_count_) = 0;
  // register converse handlers
  CpvInitialize(int, deliver_handler_);
  CpvAccess(deliver_handler_) = CmiRegisterHandler(deliver);
}

void start_fn_(int, char** argv) {
  initialize_globals_();
#if CHARMLITE_TOPOLOGY
  CmiNodeAllBarrier();
  CmiInitCPUAffinity(argv);
  CmiInitMemAffinity(argv);
  CmiInitCPUTopology(argv);
  // threads wait until _topoTree has been generated
#endif
  CmiNodeAllBarrier();
  if (CmiInCommThread()) {
    // the comm thread has to get back to work!
    return;
  } else {
    CsdScheduleForever();
  }
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
  auto& buf = CpvAccess(collection_buffer_);
  auto& tab = CpvAccess(collection_table_);
  auto& col = ep.collection;
  if (msg->has_collection_kind()) {
    auto kind = (collection_kind_t)ep.entry;
    auto& rec = CsvAccess(collection_kinds_)[kind - 1];
    auto* obj = rec(col);
    auto ins = tab.emplace(col, obj);
    CmiAssertMsg(ins.second, "insertion did not occur!");
    auto find = buf.find(col);
    // free now that we're done with the endpoint
    message::free(msg);
    // check whether there are buffered messages...
    if (find == std::end(buf)) {
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
    auto search = tab.find(col);
    if (search == std::end(tab)) {
      buf[col].emplace_back(msg);
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