#ifndef __CMK_LOCMGR_HH__
#define __CMK_LOCMGR_HH__

#include <algorithm>

#include "common.hh"

namespace cmk {

    template <typename Index>
    struct default_mapper
    {
        int pe_for(const chare_index_t& idx) const
        {
            return (idx % CmiNumPes());
        }
    };

    template <typename Index>
    struct group_mapper;

    template <>
    struct group_mapper<int> : public default_mapper<int>
    {
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
