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
  auto& tab = CpvAccess(collection_table_);
  auto find = tab.find(idx);
  if (find == std::end(tab)) {
    return nullptr;
  } else {
    return (find->second).get();
  }
}

inline void initialize(int argc, char** argv) {
  ConverseInit(argc, argv, start_fn_, 1, 1);
  initialize_globals_();
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
