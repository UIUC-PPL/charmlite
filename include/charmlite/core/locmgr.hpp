#ifndef __CMK_LOCMGR_HH__
#define __CMK_LOCMGR_HH__

#include <algorithm>

#include <charmlite/core/common.hpp>

namespace cmk {

    template <typename Index>
    struct default_mapper
    {
        int pe_for(const chare_index_t& idx) const
        {
            return (idx % CmiNumPes());
        }

        int num_span_tree_children(int pe)
        {
            return CmiNumSpanTreeChildren(pe);
        }

        void span_tree_children(int pe, int* children)
        {
            CmiSpanTreeChildren(pe, children);
        }

        int span_tree_parent(int pe)
        {
            return CmiSpanTreeParent(pe);
        }
    };

    template <typename Index>
    struct group_mapper;

    template <>
    struct group_mapper<int> : public default_mapper<int>
    {
    };

    template <typename Index>
    struct nodegroup_mapper;

    template <>
    struct nodegroup_mapper<int> : public default_mapper<int>
    {
        int pe_for(const chare_index_t& idx) const
        {
            return CmiNodeFirst(idx);
        }

        int num_span_tree_children(int node)
        {
            return CmiNumNodeSpanTreeChildren(node);
        }

        void span_tree_children(int node, int* children)
        {
            CmiNodeSpanTreeChildren(node, children);
        }

        int span_tree_parent(int node)
        {
            return CmiNodeSpanTreeParent(node);
        }
    };

    template <typename Mapper>
    class locmgr;

    template <typename Mapper>
    class locmgr_base_
    {
    protected:
        Mapper mapper_;

    public:
        int pe_for(const chare_index_t& idx) const
        {
            return this->mapper_.pe_for(idx);
        }
    };

    template <typename Mapper>
    class locmgr : public locmgr_base_<Mapper>
    {
        // TODO ( determine what should go here... )
    };
}    // namespace cmk

#endif
