#ifndef __CMK_EP_HH__
#define __CMK_EP_HH__

#include "traits.hh"

namespace cmk {
inline const entry_record_ *record_for(entry_id_t id) {
  if (id == nil_entry_) {
    return nullptr;
  } else {
    return &(entry_table_[id - 1]);
  }
}

inline void invoke(void *self, entry_id_t id, void *msg) {
  auto *rec = record_for(id);
  CmiAssert(rec != nullptr);
  (rec->fn_)(self, msg);
}

template <entry_fn_t Fn, bool Constructor>
struct entry_fn_helper_ {
  static entry_id_t id_;
};

template <typename T, T t, typename Enable = void>
struct entry_fn_impl_;

template <typename T, typename Message, member_fn_t<T, Message> Fn>
struct entry_fn_impl_<member_fn_t<T, Message>, Fn,
                      typename std::enable_if<is_message_<Message>()>::type> {
  static void call_(void *self, void *msg) {
    (static_cast<T *>(self)->*Fn)(static_cast<Message *>(msg));
  }

  static const entry_id_t &id_(void) {
    return entry_fn_helper_<(&call_), false>::id_;
  }
};

template <typename A, typename B>
struct constructor_caller_;

template <typename A>
struct constructor_caller_<A, void> {
  void operator()(void *self, void *msg) {
    new (static_cast<A *>(self)) A;
    message::free(msg);
  }
};

template <typename A, typename Message>
struct constructor_caller_<A, Message *> {
  typename std::enable_if<is_message_<Message>()>::type operator()(void *self,
                                                                   void *msg) {
    new (static_cast<A *>(self)) A(static_cast<Message *>(msg));
  }
};

template <typename T, typename Message>
void call_constructor_(void *self, void *msg) {
  constructor_caller_<T, Message>()(self, msg);
}

template <typename T, T t>
entry_id_t entry(void) {
  return entry_fn_impl_<T, t>::id_();
}

template <typename T, typename Message>
entry_id_t constructor(void) {
  return entry_fn_helper_<(&call_constructor_<T, Message>), true>::id_;
}
}  // namespace cmk

#endif