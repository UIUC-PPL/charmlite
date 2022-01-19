#ifndef CHARMLITE_CORE_INDEXING_HPP
#define CHARMLITE_CORE_INDEXING_HPP

#include <charmlite/core/common.hpp>

namespace cmk {
    template <typename T, typename Enable = void>
    class index_range;

    template <typename T, typename Enable = void>
    struct index_view
    {
        using type = T;
        using less_type = std::less<T>;
        using range_type = index_range<type>;

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

        static std::string to_string(const T& idx)
        {
            return std::to_string(idx);
        }
    };

    template <>
    struct index_view<std::tuple<int, int>>
    {
        using type = std::tuple<int, int>;
        using range_type = index_range<type>;

        // std::less is not usable here since we
        // require strictly less in ALL dimensions!
        struct less_type
        {
            inline bool operator()(const type& lhs, const type& rhs) const
            {
                return (std::get<0>(lhs) < std::get<0>(rhs)) &&
                    (std::get<1>(lhs) < std::get<1>(rhs));
            }
        };

        static type& decode(chare_index_t& idx)
        {
            return reinterpret_cast<type&>(idx);
        }

        static const type& decode(const chare_index_t& idx)
        {
            return reinterpret_cast<const type&>(idx);
        }

        static chare_index_t encode(const type& idx)
        {
            chare_index_t res = 0;
            memcpy(&res, &idx, sizeof(type));
            return res;
        }

        static std::string to_string(const type& idx)
        {
            std::stringstream ss;
            ss << "(" << std::get<0>(idx) << "," << std::get<1>(idx) << ")";
            return ss.str();
        }
    };

    template <typename T, typename Enable>
    class index_range
    {
        using less_type = typename index_view<T>::less_type;

        less_type less_;
        T curr_, step_, end_;

    public:
        index_range(T start, T step, T end)
          : curr_(start)
          , step_(step)
          , end_(end)
        {
        }

        inline bool has_next(void) const
        {
            return this->less_(this->curr_, this->end_);
        }

        inline T next(void)
        {
            auto res = this->curr_;
            this->curr_ += this->step_;
            return res;
        }
    };

    template <>
    class index_range<std::tuple<int, int>>
    {
        using type = std::tuple<int, int>;
        using less_type = typename index_view<type>::less_type;

        less_type less_;
        type curr_, step_, end_;

    public:
        index_range(const type& start, const type& step, const type& end)
          : curr_(start)
          , step_(step)
          , end_(end)
        {
        }

        inline bool has_next(void) const
        {
            return this->less_(this->curr_, this->end_);
        }

        type next(void)
        {
            auto res = this->curr_;
            auto lsb = std::get<1>(this->curr_) + std::get<1>(this->step_);
            auto msb = std::get<0>(this->curr_) +
                std::get<0>(this->step_) * (lsb >= std::get<1>(this->end_));
            this->curr_ = {msb, lsb % std::get<1>(this->end_)};
            return res;
        }
    };
}    // namespace cmk

#endif
