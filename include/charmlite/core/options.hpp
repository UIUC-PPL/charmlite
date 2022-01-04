#ifndef CHARMLITE_CORE_OPTIONS_HPP
#define CHARMLITE_CORE_OPTIONS_HPP

#include <charmlite/core/common.hpp>

namespace cmk {
    template <typename T, typename Enable = void>
    struct index_view
    {
        static_assert(sizeof(T) <= sizeof(chare_index_t),
            "index must fit in constraints!");

        static T& decode(chare_index_t& idx)
        {
            return reinterpret_cast<T&>(idx);
        }

        static const T& decode(const chare_index_t& idx)
        {
            return reinterpret_cast<const T&>(idx);
        }

        static chare_index_t encode(const T& idx)
        {
            return static_cast<chare_index_t>(idx);
        }
    };

    template <typename Index>
    class default_options;

    template <>
    class default_options<int>
    {
    public:
        static constexpr int start = 0;
        static constexpr int step = 1;
    };

    class collection_options_base_
    {
    protected:
        chare_index_t start_, end_, step_;

    public:
        collection_options_base_(const chare_index_t& start,
            const chare_index_t& end, const chare_index_t& step)
          : start_(start)
          , end_(end)
          , step_(step)
        {
        }

        operator bool(void) const
        {
            return (this->start_) || (this->end_) || (this->step_);
        }
    };

    template <typename Index>
    class collection_options : public collection_options_base_
    {
        using converter = index_view<Index>;
        using defaults = default_options<Index>;

    public:
        collection_options(void)
          : collection_options_base_(0, 0, 0)
        {
        }

        collection_options(const Index& start, const Index& end,
            const Index& step = defaults::step)
          : collection_options_base_(converter::encode(start),
                converter::encode(end), converter::encode(step))
        {
        }

        collection_options(const Index& end)
          : collection_options(defaults::start, end)
        {
        }

        Index& start(void)
        {
            return converter::decode(this->start_);
        }

        Index& end(void)
        {
            return converter::decode(this->end_);
        }

        Index& step(void)
        {
            return converter::decode(this->step_);
        }

        const Index& start(void) const
        {
            return converter::decode(this->start_);
        }

        const Index& end(void) const
        {
            return converter::decode(this->end_);
        }

        const Index& step(void) const
        {
            return converter::decode(this->step_);
        }
    };

}    // namespace cmk

#endif
