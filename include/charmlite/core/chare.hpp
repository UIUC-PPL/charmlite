#ifndef CHARMLITE_CORE_CHARE_HPP
#define CHARMLITE_CORE_CHARE_HPP

#include <charmlite/core/common.hpp>
#include <charmlite/core/options.hpp>

#include <memory>
#include <unordered_map>

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
    CsvExtern(chare_table_t, chare_table_);

    template <typename T>
    struct chare_kind_helper_
    {
        static chare_kind_t kind_;
    };

    template <typename T>
    const chare_record_& record_for(void)
    {
        auto id = chare_kind_helper_<T>::kind_;
        return CsvAccess(chare_table_)[id - 1];
    }

    template <typename T, typename Enable = void>
    struct property_setter_
    {
        void operator()(T*, const collection_index_t&, const chare_index_t&) {}
    };

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
    };

    struct reducer_
    {
        std::vector<chare_index_t> upstream;
        std::vector<chare_index_t> downstream;
        std::vector<message_ptr<message>> received;

        reducer_(const std::vector<chare_index_t>& up,
            const std::vector<chare_index_t>& down)
          : upstream(up)
          , downstream(down)
        {
        }

        bool ready(void) const
        {
            // a message from all our children and from us
            return received.size() == (upstream.size() + 1);
        }
    };

    struct chare_base_
    {
    private:
        collection_index_t parent_;
        chare_index_t index_;
        bcast_id_t last_redn_ = 0;
        bcast_id_t last_bcast_ = 0;

        using reducer_map_t = std::unordered_map<bcast_id_t, reducer_>;
        reducer_map_t reducers_;

        std::unique_ptr<cmk::association_> association_;

        std::size_t num_children_(void) const
        {
            return this->association_ ? this->association_->children.size() : 0;
        }

    public:
        template <typename T, typename Index>
        friend class chare;

        template <typename T, template <class> class Mapper>
        friend class collection;

        template <typename T, template <class> class Mapper, typename Enable>
        friend class collection_bridge_;

        template <typename T, typename Enable>
        friend struct property_setter_;
    };

    template <typename T>
    struct property_setter_<T,
        typename std::enable_if<std::is_base_of<chare_base_, T>::value>::type>
    {
        void operator()(
            T* t, const collection_index_t& id, const chare_index_t& idx)
        {
            t->parent_ = id;
            t->index_ = idx;
        }
    };

    template <typename T, typename Index>
    static Index index_for_impl_(const chare<T, Index>*);

    template <typename T>
    using index_for_t = decltype(index_for_impl_(std::declval<T*>()));

}    // namespace cmk

#endif
