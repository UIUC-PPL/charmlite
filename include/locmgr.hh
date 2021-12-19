#ifndef __CMK_LOCMGR_HH__
#define __CMK_LOCMGR_HH__

#include <algorithm>

#include "common.hh"

namespace cmk {

template <typename Index>
struct default_mapper {
  int pe_for(const chare_index_t& idx) const { return (idx % CmiNumPes()); }
};

template <typename Index>
struct group_mapper;

template <>
struct group_mapper<int> : public default_mapper<int> {};

template <typename Mapper>
class locmgr;

template <typename Mapper>
class locmgr_base_ {
 protected:
  Mapper mapper_;

 public:
  int pe_for(const chare_index_t& idx) const {
    return this->mapper_.pe_for(idx);
  }
};

template <typename Mapper>
class locmgr : public locmgr_base_<Mapper> {
 public:
  // NOTE ( these methods will have to be expanded if/when
  //        we add support for sections. )
  chare_index_t root(void) const { CmiAbort("not implemented."); }

  std::vector<chare_index_t> upstream(const chare_index_t& idx) const {
    CmiAbort("not implemented.");
  }

  std::vector<chare_index_t> downstream(const chare_index_t& idx) const {
    CmiAbort("not implemented.");
  }
};

template <>
class locmgr<group_mapper<int>> : public locmgr_base_<group_mapper<int>> {
 public:
  chare_index_t root(void) const {
    CmiAssert(CmiSpanTreeParent(0) < 0);
    return index_view<int>::encode(0);
  }

  std::vector<chare_index_t> upstream(const chare_index_t& idx) const {
    auto pe = this->pe_for(idx);
    auto n_children = CmiNumSpanTreeChildren(pe);
    if (n_children > 0) {
      // copied from qd.h -- memcheck seems to be legacy?
      std::vector<int> child_pes(n_children);
      _MEMCHECK(child_pes.data());
      CmiSpanTreeChildren(CmiMyPe(), child_pes.data());
      std::vector<chare_index_t> children;
      children.reserve(n_children);
      std::transform(std::begin(child_pes), std::end(child_pes),
                     std::back_inserter(children), index_view<int>::encode);
      return children;
    } else {
      return {};
    }
  }

  std::vector<chare_index_t> downstream(const chare_index_t& idx) const {
    auto pe = this->pe_for(idx);
    auto parent = CmiSpanTreeParent(pe);
    if (parent >= 0) {
      return {index_view<int>::encode(parent)};
    } else {
      return {};
    }
  }
};
}  // namespace cmk

#endif
