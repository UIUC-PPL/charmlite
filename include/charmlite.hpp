#ifndef __CMK_HH__
#define __CMK_HH__

#include "collection.hpp"
#include "core.hpp"
#include "proxy.hpp"
#include "reduction.hpp"

// ( no reordering )
#include "charmlite.impl.hpp"

namespace cmk {
    // broadcasts an exit message to all pes
    inline void exit(void)
    {
        auto msg = cmk::make_message<message>();
        new (&(msg->dst_))
            destination(callback_helper_<message, exit>::id_, cmk::all_pes);
        send(std::move(msg));
    }
}    // namespace cmk

#endif
