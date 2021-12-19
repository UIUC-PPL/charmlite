#ifndef __CMK_COMMMON_HH__
#define __CMK_COMMMON_HH__

#include <converse.h>
#include <execinfo.h>

#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace cmk {

struct message;

struct chare_record_;

template <typename T>
class element_proxy;

class collection_base_;

template <typename T>
class collection_proxy_base_;

struct collection_index_t {
  std::uint32_t pe_;
  std::uint32_t id_;

  inline bool operator==(const collection_index_t& other) const {
    return (reinterpret_cast<const std::size_t&>(*this) ==
            reinterpret_cast<const std::size_t&>(other));
  }

  inline operator std::string(void) const {
    std::stringstream ss;
    ss << "collection(";
    ss << "pe=" << this->pe_ << ",";
    ss << "id=" << this->id_;
    ss << ")";
    return ss.str();
  }
};

using collection_constructor_t =
    collection_base_* (*)(const collection_index_t&, const message*);

template <typename T>
struct message_deleter_;

template <typename Message>
using message_ptr = std::unique_ptr<Message>;  // , message_deleter_<Message>>;

struct collection_index_hasher_ {
  static_assert(sizeof(collection_index_t) == sizeof(std::size_t),
                "trivial hashing assumed");
  std::size_t operator()(const collection_index_t& id) const {
    auto& view = reinterpret_cast<const std::size_t&>(id);
    return std::hash<std::size_t>()(view);
  }
};

using entry_fn_t = void (*)(void*, void*);

struct entry_record_ {
  entry_fn_t fn_;
  bool is_constructor_;

  entry_record_(entry_fn_t fn, bool is_constructor)
      : fn_(fn), is_constructor_(is_constructor) {}

  // helper function to be used for projections tracing
  std::string name(void) const {
    auto* fn = reinterpret_cast<void*>(this->fn_);
    auto** names = backtrace_symbols(&fn, 1);
    // free is only reqd when n ptrs > 1
    return std::string(names[0]);
  }

  void invoke(void* obj, void* msg) const { (this->fn_)(obj, msg); }
};

/* general terminology :
 * - kind refers to a family of instances
 * - id/index refers to a specific instance
 */

// TODO ( rename to callback_fn_t )
template <typename Message>
using callback_fn_t = void (*)(Message*);
using callback_table_t = std::vector<callback_fn_t<message>>;
using callback_id_t = typename callback_table_t::size_type;

// TODO ( rename to combiner_fn_t )
template <typename Message>
using combiner_fn_t = Message* (*)(Message*, Message*);
using combiner_table_t = std::vector<combiner_fn_t<message>>;
using combiner_id_t = typename combiner_table_t::size_type;

using entry_table_t = std::vector<entry_record_>;
using entry_id_t = typename entry_table_t::size_type;

using chare_table_t = std::vector<chare_record_>;
using chare_kind_t = typename chare_table_t::size_type;

using collection_kinds_t = std::vector<collection_constructor_t>;
using collection_kind_t = typename collection_kinds_t::size_type;

using chare_index_t =
    typename std::conditional<std::is_integral<CmiUInt16>::value, CmiUInt16,
                              CmiUInt8>::type;

template <typename T>
using collection_map =
    std::unordered_map<collection_index_t, T, collection_index_hasher_>;

using collection_table_t = collection_map<std::unique_ptr<collection_base_>>;

using message_buffer_t = std::deque<message_ptr<message>>;
using collection_buffer_t =
    std::unordered_map<collection_index_t, message_buffer_t,
                       collection_index_hasher_>;

constexpr entry_id_t nil_entry_ = 0;
constexpr collection_kind_t nil_kind_ = 0;
// TODO ( make these more distinct? )
constexpr int all = -1;
constexpr auto chare_bcast_root_ = std::numeric_limits<chare_index_t>::max();

// TODO ( rename this "collective" id type )
using bcast_id_t = std::uint16_t;

// Shared between workers in a process
CsvExtern(entry_table_t, entry_table_);
CsvExtern(chare_table_t, chare_table_);
CsvExtern(callback_table_t, callback_table_);
CsvExtern(combiner_table_t, combiner_table_);
CsvExtern(collection_kinds_t, collection_kinds_);
// Each worker has its own instance of these
CpvExtern(collection_table_t, collection_table_);
CpvExtern(collection_buffer_t, collection_buffer_);
CpvExtern(std::uint32_t, local_collection_count_);
CpvExtern(int, deliver_handler_);

void initialize_globals_(void);

struct destination;
enum destination_kind : std::uint8_t { kInvalid = 0, kCallback, kEndpoint };

// TODO (deliver is the converse handler)
void deliver(void*);
// TODO (send is the send fn // needs better names)
void send(message*);
}  // namespace cmk

#include "destination.hh"

#endif