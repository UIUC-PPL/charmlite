#ifndef __CMK_CORE_HH__
#define __CMK_CORE_HH__

#include "common.hh"
#if CHARMLITE_TOPOLOGY
#include "TopoManager.h"
#include "spanningTree.h"

void CmiInitCPUTopology(char** argv);
void CmiInitMemAffinity(char** argv);
#endif

namespace cmk {
void start_fn_(int, char**);

inline collection_base_* lookup(collection_index_t idx) {
  auto find = collection_table_.find(idx);
  if (find == std::end(collection_table_)) {
    return nullptr;
  } else {
    return (find->second).get();
  }
}

inline void register_deliver_(void) {
  CpvInitialize(int, deliver_handler_);
  CpvAccess(deliver_handler_) = CmiRegisterHandler(deliver);
}

inline void initialize(int argc, char** argv) {
  ConverseInit(argc, argv, start_fn_, 1, 1);
  register_deliver_();
#if CHARMLITE_TOPOLOGY
  TopoManager_init();
  CmiNodeAllBarrier();
  CmiInitCPUAffinity(argv);
  CmiInitMemAffinity(argv);
  CmiInitCPUTopology(argv);
  TopoManager_reset();  // initialize TopoManager singleton
  _topoTree = ST_RecursivePartition_getTreeInfo(0);
#endif
  CmiNodeAllBarrier();
}

inline void finalize(void) {
  CsdScheduleForever();
  ConverseExit();
}

void exit(message* msg);
}  // namespace cmk

#endif
