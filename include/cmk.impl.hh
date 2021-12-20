#ifndef __CMK_IMPL_HH__
#define __CMK_IMPL_HH__

#include "ep.hh"
#include "message.impl.hh"

/* registers all user data-types with the RTS
 */

namespace cmk {
template <entry_fn_t Fn, bool Constructor>
static entry_id_t register_entry_fn_(void) {
  auto id = CsvAccess(entry_table_).size() + 1;
  CsvAccess(entry_table_).emplace_back(Fn, Constructor);
  return id;
}

template <entry_fn_t Fn, bool Constructor>
entry_id_t entry_fn_helper_<Fn, Constructor>::id_ =
    register_entry_fn_<Fn, Constructor>();

template <typename T>
static chare_kind_t register_chare_(void) {
  auto id = CsvAccess(chare_table_).size() + 1;
  CsvAccess(chare_table_).emplace_back(typeid(T).name(), sizeof(T));
  return id;
}

template <typename T>
chare_kind_t chare_kind_helper_<T>::kind_ = register_chare_<T>();

template <typename T, template <class> class Mapper>
static collection_base_* construct_collection_(
    const collection_index_t& id, const collection_options_base_& opts,
    const message* msg) {
  using collection_type = collection<T, Mapper>;
  using index_type = typename collection_type::index_type;
  static_assert(sizeof(collection_options<index_type>) ==
                    sizeof(collection_options_base_),
                "specializations not allowed to vary in size wrt base");
  return new collection_type(
      id, static_cast<const collection_options<index_type>&>(opts), msg);
}

template <typename T, template <class> class Mapper>
static collection_kind_t register_collection_(void) {
  auto id = CsvAccess(collection_kinds_).size() + 1;
  CsvAccess(collection_kinds_).emplace_back(&construct_collection_<T, Mapper>);
  return id;
}

template <typename T, template <class> class Mapper>
collection_kind_t collection_helper_<collection<T, Mapper>>::kind_ =
    register_collection_<T, Mapper>();

template <typename T>
static void message_deleter_impl_(void* msg) {
  delete static_cast<T*>(msg);
}

template <typename T>
static message_kind_t register_message_(void) {
  using properties_type = message_properties_extractor_<T>;
  auto id = CsvAccess(message_table_).size() + 1;
  CsvAccess(message_table_)
      .emplace_back(&message_deleter_impl_<T>, properties_type::packer(),
                    properties_type::unpacker());
  return id;
}

message_kind_t message_helper_<message>::kind_ = 0;

template <typename T>
message_kind_t message_helper_<T>::kind_ = register_message_<T>();

// helper struct to erase type of combiners/callbacks
template <typename Message, template <class> class Function,
          Function<Message> Fn, typename Enable = void>
struct function_wrapper_;

template <template <class> class Function, Function<message> Fn>
struct function_wrapper_<message, Function, Fn> {
  // no type erasure needed for the "base" case
  static constexpr Function<message> fn(void) { return Fn; }
};

template <typename Message, combiner_fn_t<Message> Fn>
struct function_wrapper_<
    Message, combiner_fn_t, Fn,
    typename std::enable_if<!std::is_same<message, Message>::value>::type> {
  static message* impl_(message* lhs, message* rhs) {
    // ( result should be implicitly castable to message )
    return Fn(static_cast<Message*>(lhs), static_cast<Message*>(rhs));
  }

  static constexpr combiner_fn_t<message> fn(void) {
    return &(function_wrapper_<Message, combiner_fn_t, Fn>::impl_);
  }
};

template <typename Message, callback_fn_t<Message> Fn>
struct function_wrapper_<
    Message, callback_fn_t, Fn,
    typename std::enable_if<!std::is_same<message, Message>::value>::type> {
  static void impl_(message* msg) { Fn(static_cast<Message*>(msg)); }

  static constexpr callback_fn_t<message> fn(void) {
    return &(function_wrapper_<Message, callback_fn_t, Fn>::impl_);
  }
};

template <typename Message, template <class> class Function,
          Function<Message> Fn>
static std::size_t register_function_(std::vector<Function<message>>& table) {
  // get a type-erased version of the function
  constexpr auto fn = function_wrapper_<Message, Function, Fn>::fn();
  // then register it
  auto id = table.size() + 1;
  table.emplace_back(fn);
  return id;
}

template <typename Message, combiner_fn_t<Message> Fn>
combiner_id_t combiner_helper_<Message, Fn>::id_ =
    register_function_<Message, combiner_fn_t, Fn>(CsvAccess(combiner_table_));

template <typename Message, callback_fn_t<Message> Fn>
callback_id_t callback_helper_<Message, Fn>::id_ =
    register_function_<Message, callback_fn_t, Fn>(CsvAccess(callback_table_));

}  // namespace cmk

#endif
