#ifndef __CMK_DEF_HH__
#define __CMK_DEF_HH__

#include "ep.hh"

/* registers all user data-types with the RTS
 */

namespace cmk {
template <entry_fn_t Fn, bool Constructor>
static entry_id_t register_entry_fn_(void) {
  auto id = entry_table_.size() + 1;
  entry_table_.emplace_back(Fn, Constructor);
  return id;
}

template <entry_fn_t Fn, bool Constructor>
entry_id_t entry_fn_helper_<Fn, Constructor>::id_ =
    register_entry_fn_<Fn, Constructor>();

template <typename T>
static chare_kind_t register_chare_(void) {
  auto id = chare_table_.size() + 1;
  chare_table_.emplace_back(typeid(T).name(), sizeof(T));
  return id;
}

template <typename T>
chare_kind_t chare_kind_helper_<T>::kind_ = register_chare_<T>();

template <typename T, typename Mapper>
static collective_base_* construct_collective_(const collective_index_t& id) {
  return new collective<T, Mapper>(id);
}

template <typename T, typename Mapper>
static collective_kind_t register_collective_(void) {
  auto id = collective_kinds_.size() + 1;
  collective_kinds_.emplace_back(&construct_collective_<T, Mapper>);
  return id;
}

template <typename T, typename Mapper>
collective_kind_t collective_helper_<collective<T, Mapper>>::kind_ =
    register_collective_<T, Mapper>();

template <typename T>
static void message_deleter_impl_(void* msg) {
  delete static_cast<T*>(msg);
}

template <typename T>
static message_kind_t register_message_(void) {
  auto id = message_table_.size() + 1;
  message_table_.emplace_back(&message_deleter_impl_<T>);
  return id;
}

template <typename T>
message_kind_t message_helper_<T>::kind_ = register_message_<T>();

template <typename T, T t>
static std::size_t register_function_(std::vector<T>& table) {
  auto id = table.size() + 1;
  table.emplace_back(t);
  return id;
}

template <combiner_t Fn>
combiner_id_t combiner_helper_<Fn>::id_ =
    register_function_<combiner_t, Fn>(combiner_table_);

template <callback_t Fn>
callback_id_t callback_helper_<Fn>::id_ =
    register_function_<callback_t, Fn>(callback_table_);

}  // namespace cmk

#endif
