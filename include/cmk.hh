#ifndef __CMK_HH__
#define __CMK_HH__

#include "collection.hh"
#include "core.hh"
#include "proxy.hh"
#include "reduction.hh"

// ( no reordering )
#include "cmk.impl.hh"

namespace cmk {
// broadcasts an exit message to all pes
void exit(void) {
  auto* msg = new message();
  new (&(msg->dst_))
      destination(callback_helper_<message, exit>::id_, cmk::all);
  send(msg);
}
}  // namespace cmk

#endif
