#ifndef __CMK_REDUCTION_HH__
#define __CMK_REDUCTION_HH__

#include <charmlite/core/callback.hpp>
#include <charmlite/core/message.hpp>

/* collection of combiners for
 * reduction operations
 *
 * TODO( support wrapping
 *       std::plus and their ilk )
 *
 */

namespace cmk {
    template <typename T = message>
    message_ptr<T> nop(message_ptr<T>&& msg, message_ptr<T>&&)
    {
        return std::move(msg);
    }

    template <typename T>
    message_ptr<data_message<T>> add(
        message_ptr<data_message<T>>&& lhs, message_ptr<data_message<T>>&& rhs)
    {
        lhs->value() += rhs->value();
        return std::move(lhs);
    }
}    // namespace cmk

#endif
