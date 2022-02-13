#ifndef CHARMLITE_SERIALIZATION_SERIALIZATION_HPP
#define CHARMLITE_SERIALIZATION_SERIALIZATION_HPP

#include <charmlite/serialization/marshall_message.hpp>

PUPbytes(cmk::collection_index_t);

namespace cmk {
    template <typename T>
    constexpr auto is_pupable_v = PUP::details::is_pupable<T>::value;
}

#endif
