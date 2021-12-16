#ifndef __CMK_COMMMON_HH__
#define __CMK_COMMMON_HH__

#include <converse.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace cmk {

struct message;

struct chare_record_;

struct collective_base_;

struct collective_index_t {
  std::uint32_t pe_;
  std::uint32_t id_;

  inline bool operator==(const collective_index_t& other) const {
    return (reinterpret_cast<const std::size_t&>(*this) ==
            reinterpret_cast<const std::size_t&>(other));
  }
};

using collective_constructor_t =
    collective_base_* (*)(const collective_index_t&);

template <typename T>
struct message_deleter_;

template <typename Message>
using message_ptr = std::unique_ptr<Message>;  // , message_deleter_<Message>>;

struct collective_index_hasher_ {
  static_assert(sizeof(collective_index_t) == sizeof(std::size_t),
                "trivial hashing assumed");
  std::size_t operator()(const collective_index_t& id) const {
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
};

/* general terminology :
 * - kind refers to a family of instances
 * - id/index refers to a specific instance
 */

// TODO ( rename to callback_fn_t )
using callback_t = void (*)(message*);
using callback_table_t = std::vector<callback_t>;
using callback_id_t = typename callback_table_t::size_type;

// TODO ( rename to combiner_fn_t )
using combiner_t = message* (*)(message*, message*);
using combiner_table_t = std::vector<combiner_t>;
using combiner_id_t = typename combiner_table_t::size_type;

using entry_table_t = std::vector<entry_record_>;
using entry_id_t = typename entry_table_t::size_type;

using chare_table_t = std::vector<chare_record_>;
using chare_kind_t = typename chare_table_t::size_type;

using collective_kinds_t = std::vector<collective_constructor_t>;
using collective_kind_t = typename collective_kinds_t::size_type;

using chare_index_t =
    typename std::conditional<std::is_integral<CmiUInt16>::value, CmiUInt16,
                              CmiUInt8>::type;

template <typename T>
using collective_map =
    std::unordered_map<collective_index_t, T, collective_index_hasher_>;

using collective_table_t = collective_map<std::unique_ptr<collective_base_>>;

using message_buffer_t = std::deque<message_ptr<message>>;
using collective_buffer_t =
    std::unordered_map<collective_index_t, message_buffer_t,
                       collective_index_hasher_>;

constexpr entry_id_t nil_entry_ = 0;
constexpr collective_kind_t nil_kind_ = 0;
// TODO ( make this more distinct -- ensure it plays nicely with chare_index_t )
constexpr int all = -1;
constexpr auto chare_bcast_root_ = std::numeric_limits<chare_index_t>::max();

using bcast_id_t = std::uint16_t;

// FIXME ( make these csv variables! )
extern entry_table_t entry_table_;
extern chare_table_t chare_table_;
extern callback_table_t callback_table_;
extern combiner_table_t combiner_table_;
extern collective_kinds_t collective_kinds_;
extern collective_table_t collective_table_;
extern collective_buffer_t collective_buffer_;
extern std::uint32_t local_collective_count_;

CpvExtern(int, deliver_handler_);

struct destination;
enum destination_kind : std::uint8_t { kInvalid = 0, kCallback, kEndpoint };

// TODO (deliver is the converse handler)
void deliver(void*);
// TODO (send is the send fn // needs better names)
void send(message*);
}  // namespace cmk

#include "destination.hh"

#endif