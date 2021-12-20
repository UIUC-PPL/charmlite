#include "core.hh"

#include "collection.hh"
#include "proxy.hh"

namespace cmk {
// these can be nix'd when we upgrade to c++17
constexpr int default_options<int>::start;
constexpr int default_options<int>::step;

CsvDeclare(entry_table_t, entry_table_);
CsvDeclare(chare_table_t, chare_table_);
CsvDeclare(message_table_t, message_table_);
CsvDeclare(callback_table_t, callback_table_);
CsvDeclare(combiner_table_t, combiner_table_);
CsvDeclare(collection_kinds_t, collection_kinds_);

CpvDeclare(collection_table_t, collection_table_);
CpvDeclare(collection_buffer_t, collection_buffer_);
CpvDeclare(std::uint32_t, local_collection_count_);
CpvDeclare(int, converse_handler_);

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
  CpvInitialize(int, converse_handler_);
  CpvAccess(converse_handler_) = CmiRegisterHandler(converse_handler_);
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
    pack_message(msg);  // XXX ( this is likely overkill )
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
    // determine whether or not the creation message
    // is attached to an argument message
    auto* opts = (char*)msg + sizeof(message);
    auto offset = sizeof(message) + sizeof(collection_options_base_);
    auto* arg = (msg->total_size_ == offset) ? nullptr : ((char*)msg + offset);
    auto* obj = rec(col, *(reinterpret_cast<collection_options_base_*>(opts)),
                    reinterpret_cast<message*>(arg));
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
  // prepare message for local-processing
  unpack_message(msg);
  // then process it
  (callback_for(msg))(msg);
}

void converse_handler_(void* raw) {
  auto* msg = static_cast<message*>(raw);
  // then determine how to route it
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
      send_helper_(cb.pe, msg);
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