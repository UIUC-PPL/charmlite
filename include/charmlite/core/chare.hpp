#ifndef CHARMLITE_CORE_CHARE_HPP
#define CHARMLITE_CORE_CHARE_HPP

#include <charmlite/core/common.hpp>
#include <charmlite/core/options.hpp>
#include <charmlite/serialization/serialization.hpp>

namespace cmk {

    struct chare_record_
    {
        const char* name_;
        std::size_t size_;

        chare_record_(const char* name, std::size_t size)
          : name_(name)
          , size_(size)
        {
        }

        void* allocate(void) const
        {
            return ::operator new(this->size_);
        }

        void deallocate(void* obj) const
        {
            ::operator delete(obj);
        }
    };

    using chare_table_t = std::vector<chare_record_>;
    using chare_kind_t = typename chare_table_t::size_type;

    // Shared between workers in a process
    CMK_GENERATE_SINGLETON(chare_table_t, chare_table_);

    template <typename T>
    struct chare_kind_helper_
    {
        static chare_kind_t kind_;
    };

    template <typename T>
    const chare_record_& record_for(void)
    {
        auto id = chare_kind_helper_<T>::kind_;
        return CMK_ACCESS_SINGLETON(chare_table_)[id - 1];
    }

    template <typename T>
    struct property_setter_;

    template <typename T, typename Index>
    class chare;

    template <typename T, template <class> class Mapper>
    class collection;

    struct association_
    {
        std::vector<chare_index_t> parent;
        std::vector<chare_index_t> children;
        bool valid_parent;

        association_(void)
          : valid_parent(false)
        {
        }

        void put_child(const chare_index_t& index)
        {
            this->children.emplace_back(index);
        }

        void put_parent(const chare_index_t& index)
        {
            this->valid_parent = true;
            this->parent.emplace_back(index);
        }

        void pup(PUP::er& p)
        {
            p | this->parent;
            p | this->children;
            p | this->valid_parent;
        }
    };

    struct reducer_
    {
        collective_id_t redn;
        std::vector<chare_index_t> upstream;
        std::vector<chare_index_t> downstream;
        std::vector<message_ptr<message>> received;

        reducer_(collective_id_t redn_, const std::vector<chare_index_t>& up,
            const std::vector<chare_index_t>& down)
          : redn(redn_)
          , upstream(up)
          , downstream(down)
        {
        }

        reducer_(PUP::reconstruct) {}

        bool ready(void) const
        {
            // a message from all our children and from us
            return received.size() == (upstream.size() + 1);
        }

        void pup(PUP::er& p)
        {
            p | this->redn;
            p | this->upstream;
            p | this->downstream;
            // TODO ( lift this restriction by making messages PUP'able )
            CmiEnforceMsg(this->received.empty(),
                "chares with buffered reduction messages cannot migrate");
        }
    };

    struct chare_base_
    {
    private:
        collection_index_t parent_;
        chare_index_t index_;
        collective_id_t last_bcast_ = 0;
        collective_id_t last_redn_ = 0;

        using reducer_map_t = std::unordered_map<collective_id_t, reducer_>;
        reducer_map_t reducers_;

        std::unique_ptr<cmk::association_> association_;

        std::size_t num_children_(void) const
        {
            return this->association_ ? this->association_->children.size() : 0;
        }

        void parent_pup_(PUP::er& p)
        {
            p | this->parent_;
            p | this->index_;
            p | this->last_bcast_;
            p | this->last_redn_;
            p | this->reducers_;
            p | this->association_;
        }

    public:
        template <typename T, typename Index>
        friend class chare;

        template <typename T, template <class> class Mapper>
        friend class collection;

        template <typename T, template <class> class Mapper, typename Enable>
        friend class collection_bridge_;

        template <typename T>
        friend struct property_setter_;
    };

    template <typename T>
    struct property_setter_
    {
        void operator()(
            T* t, const collection_index_t& id, const chare_index_t& idx)
        {
            if constexpr (std::is_base_of<chare_base_, T>::value)
            {
                t->parent_ = id;
                t->index_ = idx;
            }
        }
    };

    template <typename T, typename Index>
    static Index index_for_impl_(const chare<T, Index>*);

    template <typename T>
    using index_for_t = decltype(index_for_impl_(std::declval<T*>()));

}    // namespace cmk

#endif
