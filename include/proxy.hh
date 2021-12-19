#ifndef __CMK_PROXY_HH__
#define __CMK_PROXY_HH__

#include "callback.hh"
#include "chare.hh"
#include "ep.hh"
#include "locmgr.hh"

namespace cmk {

template <typename T, typename Enable = void>
struct message_extractor;

template <>
struct message_extractor<void> {
  static message* get(void) { return new message; }
};

template <typename Message>
struct message_extractor<
    Message*,
    typename std::enable_if<std::is_base_of<message, Message>::value>::type> {
  static message* get(Message* msg) { return msg; }
};

template <typename... Args>
struct pack_helper;

template <typename T>
struct pack_helper<T> {
  using type = T;
};

template <>
struct pack_helper<> {
  using type = void;
};

template <typename... Args>
using pack_helper_t = typename pack_helper<Args...>::type;

class element_proxy_base_ {
 private:
  collection_index_t id_;
  chare_index_t idx_;

 public:
  template <typename T>
  friend class element_proxy;

  element_proxy_base_(element_proxy_base_&&) = default;
  element_proxy_base_(const element_proxy_base_&) = default;
  element_proxy_base_(const collection_index_t& id, const chare_index_t& idx)
      : id_(id), idx_(idx) {}
};

template <typename T>
class element_proxy : public element_proxy_base_ {
 public:
  friend T;

  element_proxy(element_proxy<T>&&) = default;
  element_proxy(const element_proxy<T>&) = default;
  element_proxy(const collection_index_t& id, const chare_index_t& idx)
      : element_proxy_base_(id, idx) {}

  template <typename... Args>
  void insert(Args... args) const {
    using arg_type = pack_helper_t<Args...>;
    auto* msg = message_extractor<arg_type>::get(args...);
    new (&(msg->dst_))
        destination(this->id_, this->idx_, constructor<T, arg_type>());
    deliver(msg);
  }

  template <typename Message, member_fn_t<T, Message> Fn>
  void send(Message* msg) const {
    new (&(msg->dst_)) destination(this->id_, this->idx_,
                                   entry<member_fn_t<T, Message>, Fn>());
    deliver(msg);
  }

  template <typename Message, member_fn_t<T, Message> Fn>
  cmk::callback<Message> callback(void) const {
    return cmk::callback<Message>(this->id_, this->idx_,
                                  entry<member_fn_t<T, Message>, Fn>());
  }

 protected:
  template <typename Message, combiner_fn_t<Message> Combiner>
  void contribute(Message* msg, const cmk::callback<Message>& cb) const {
    // set the contribution's combiner
    msg->has_combiner() = true;
    new (&(msg->dst_)) destination(this->id_, this->idx_,
                                   combiner_helper_<Message, Combiner>::id_);
    // set the contribution's continuation
    auto cont = msg->has_continuation();
    CmiAssertMsg(!cont, "continuation of contribution will be overriden");
    cont = true;
    cb.imprint(*(msg->continuation()));
    // send the contribution...
    cmk::lookup(this->id_)->contribute(msg);
  }
};

template <typename T>
class collection_proxy_base_ {
 protected:
  collection_index_t id_;

 public:
  using index_type = index_for_t<T>;

  collection_proxy_base_(const collection_index_t& id) : id_(id) {}

  element_proxy<T> operator[](const index_type& idx) const {
    auto& view = index_view<index_type>::decode(idx);
    return element_proxy<T>(this->id_, view);
  }

  template <typename Message, member_fn_t<T, Message> Fn>
  cmk::callback<Message> callback(void) const {
    return cmk::callback<Message>(this->id_, chare_bcast_root_,
                                  entry<member_fn_t<T, Message>, Fn>());
  }

  template <typename Message, member_fn_t<T, Message> Fn>
  void broadcast(Message* msg) const {
    // send a message to the broadcast root
    new (&msg->dst_) destination(this->id_, chare_bcast_root_,
                                 entry<member_fn_t<T, Message>, Fn>());
    send(msg);
  }

  operator collection_index_t(void) const { return this->id_; }
};

template <typename T>
class collection_proxy : public collection_proxy_base_<T> {
  using base_type = collection_proxy_base_<T>;

 public:
  using index_type = typename base_type::index_type;

  collection_proxy(const collection_index_t& id) : base_type(id) {}

  // TODO ( disable using this with reserved mappers (i.e., node/group) )
  template <typename Mapper = default_mapper>
  static collection_proxy<T> construct(void) {
    collection_index_t id{(std::uint32_t)CmiMyPe(),
                          CpvAccess(local_collection_count_)++};
    auto kind = collection_helper_<collection<T, Mapper>>::kind_;
    auto* msg = new message();
    new (&msg->dst_) destination(id, cmk::all, kind);
    msg->has_collection_kind() = true;
    CmiSyncBroadcastAllAndFree(msg->total_size_, (char*)msg);
    return collection_proxy<T>(id);
  }

  void done_inserting(void) {}
};

template <typename T>
class group_proxy : public collection_proxy_base_<T> {
  using base_type = collection_proxy_base_<T>;

 public:
  using index_type = typename base_type::index_type;

  static_assert(std::is_same<index_type, int>::value,
                "groups must use integer indices");

  group_proxy(const collection_index_t& id) : base_type(id) {}

  T* local_branch(void) {
    auto* loc = lookup(this->id_);
    return loc ? loc->template lookup<T>(CmiMyPe()) : nullptr;
  }

  template <typename... Args>
  static group_proxy<T> construct(Args... args) {
    collection_index_t id{(std::uint32_t)CmiMyPe(),
                          ++CpvAccess(local_collection_count_)};
    auto* a_msg = ([&](void) -> message* {
      using arg_type = pack_helper_t<Args...>;
      auto* msg = message_extractor<arg_type>::get(args...);
      new (&msg->dst_)
          destination(id, chare_bcast_root_, constructor<T, arg_type>());
      return msg;
    })();
    {
      auto kind = collection_helper_<collection<T, group_mapper>>::kind_;
      auto* msg = new (sizeof(message) + a_msg->total_size_) message();
      // update properties of creation message
      new (&msg->dst_) destination(id, chare_bcast_root_, kind);
      msg->has_collection_kind() = true;
      msg->total_size_ += a_msg->total_size_;
      // copy the argument message onto it
      memcpy((char*)msg + sizeof(message), a_msg, a_msg->total_size_);
      // then free a_msg since we're done with it
      message::free(a_msg);
      // broadcast the conjoined message to all PEs
      CmiSyncBroadcastAllAndFree(msg->total_size_, (char*)msg);
    }
    return group_proxy<T>(id);
  }
};

template <typename T, typename Index>
class chare : public chare_base_ {
 public:
  const Index& index(void) const {
    return index_view<Index>::decode(this->index_);
  }

  // NOTE ( if we associated chares with particular collections
  //        we could make this a typed proxy )
  const collection_index_t& collection(void) const { return this->parent_; }

  collection_proxy_base_<T> collection_proxy(void) const {
    return collection_proxy_base_<T>(this->parent_);
  }

  cmk::element_proxy<T> element_proxy(void) const {
    return cmk::element_proxy<T>(this->parent_, this->index_);
  }
};

}  // namespace cmk

#endif
