#ifndef CHARMLITE_CORE_LOCMGR_HPP
#define CHARMLITE_CORE_LOCMGR_HPP

#include <algorithm>

#include <charmlite/core/common.hpp>

namespace cmk {

    template <typename Index>
    struct default_mapper
    {
        int home_pe(const chare_index_t& idx) const
        {
            return (idx % CmiNumPes());
        }

        int lookup(const chare_index_t& idx)
        {
            return home_pe(idx);
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
        int home_pe(const chare_index_t& idx) const
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
        int home_pe(const chare_index_t& idx) const
        {
            return this->mapper_.home_pe(idx);
        }
    };

    template <typename Mapper>
    class locmgr : public locmgr_base_<Mapper>
    {
    public:
        using location_updater_t =
            std::function<void(chare_index_t const&, int, int)>;

    private:
        using location_map_t = std::unordered_map<chare_index_t, int>;

        location_map_t locmap_;
        location_map_t routing_cache_;
        location_updater_t updater_;

    public:
        locmgr(location_updater_t&& updater)
          : updater_(std::forward<location_updater_t>(updater))
        {
        }

        int lookup(const chare_index_t& idx)
        {
            auto find = this->locmap_.find(idx);
            if (find == std::end(this->locmap_))
            {
                return lookup_cache(idx);
            }
            else
            {
                return find->second;
            }
        }

        int lookup_cache(const chare_index_t& idx)
        {
            auto find = this->routing_cache_.find(idx);
            if (find == std::end(this->routing_cache_))
            {
                return this->home_pe(idx);
            }
            else
            {
                return find->second;
            }
        }

        // TODO - how to handle same element created on multiple PEs?
        void update_location(
            const chare_index_t& idx, int pe, bool inform_home = false)
        {
            auto home = this->home_pe(idx);
            auto mine = CmiMyPe();

            if (mine == home)
            {
                this->locmap_[idx] = pe;
            }
            else
            {
                if (inform_home)
                {
                    this->updater_(idx, home, pe);
                }

                this->routing_cache_[idx] = pe;
            }
        }
    };
}    // namespace cmk

#endif
