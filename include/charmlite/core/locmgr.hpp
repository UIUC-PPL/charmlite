#ifndef CHARMLITE_CORE_LOCMGR_HPP
#define CHARMLITE_CORE_LOCMGR_HPP

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

        int get_location(const chare_index_t& idx) const
        {
            return this->pe_for(idx);
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

        int num_span_tree_children(int node)
        {
            return this->mapper_.num_span_tree_children(node);
        }

        void span_tree_children(int node, int* children)
        {
            this->mapper_.span_tree_children(node, children);
        }

        int span_tree_parent(int node)
        {
            return this->mapper_.span_tree_parent(node);
        }

        int get_location(const chare_index_t& idx)
        {
            return this->pe_for(idx);
        }
    };

    template <typename Mapper>
    class locmgr : public locmgr_base_<Mapper>
    {
        using location_map_t = std::unordered_map<chare_index_t, int>;

    private:
        location_map_t locmap_; 

    public:
        int get_location(const chare_index_t& idx)
        {
            auto find = this->locmap_.find(idx);
            if(find == std::end(this->locmap_))
            {
                return this->pe_for(idx);
            }
            else
            {
                return find->second;
            }
        }

        void update_location(const chare_index_t& idx, const int pe)
        {
            this->locmap_[idx] = pe;
        }
    };
}    // namespace cmk

#endif
