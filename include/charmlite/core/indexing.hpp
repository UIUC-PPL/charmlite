#ifndef CHARMLITE_CORE_INDEXING_HPP
#define CHARMLITE_CORE_INDEXING_HPP

#include <charmlite/core/common.hpp>
#include <iostream>
namespace cmk {

    template <typename... Args>
    class index_range;

    template <typename... Args>
    struct index_view;

    template <typename... Args>
    struct index_view<std::tuple<Args...>>
    {
        using type = std::tuple<Args...>;
        using range_type = index_range<type>;

        struct less_type
        {
        private:
            template <std::size_t I = 0>
            inline constexpr bool is_less(
                const type& lhs, const type& rhs) const
            {
                if constexpr (I == std::tuple_size<type>::value)
                {
                    return true;
                }
                else
                {
                    return ((std::get<I>(lhs) < std::get<I>(rhs)) &&
                        is_less<I + 1>(lhs, rhs));
                }
            }

        public:
            inline constexpr bool operator()(
                const type& lhs, const type& rhs) const
            {
                return is_less(lhs, rhs);
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
            to_string_impl(ss, idx);
            return ss.str();
        }

    private:
        template <std::size_t I = 0>
        inline static void to_string_impl(
            std::stringstream& ss, const type& idx)
        {
            if constexpr (I == 0)
            {
                ss << "(" << std::get<I>(idx);
                to_string_impl<I + 1>(ss, idx);
            }
            else if constexpr (I == std::tuple_size<type>::value)
            {
                ss << ")";
            }
            else
            {
                ss << "," << std::get<I>(idx);
                to_string_impl<I + 1>(ss, idx);
            }
        }
    };

    template <typename T>
    struct index_view<T>
    {
        static_assert(
            std::is_integral_v<T>, "index_view<T> with non-integral type T");

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

    template <typename... Args>
    class index_range<std::tuple<Args...>>
    {
        using less_type = typename index_view<std::tuple<Args...>>::less_type;
        using index_type = std::tuple<Args...>;

        less_type less_;
        index_type curr_, step_, end_;

    public:
        constexpr index_range(index_type start, index_type step, index_type end)
          : curr_(start)
          , step_(step)
          , end_(end)
        {
        }

        inline constexpr bool has_next(void) const
        {
            return less_(this->curr_, this->end_);
        }

        inline constexpr bool has_next(index_type const& curr) const
        {
            return less_(curr, this->end_);
        }

        index_type advance(void)
        {
            auto prev = this->curr_;
            this->curr_ = next_impl(prev, prev);
            return prev;
        }

        constexpr index_type next(index_type const& curr) const
        {
            return next_impl(curr, curr);
        }

    private:
        template <std::size_t I = 0>
        constexpr index_type next_impl(
            index_type const& curr_tuple, index_type const& temp_tuple) const
        {
            constexpr std::size_t Tuple_size =
                std::tuple_size<index_type>::value;

            if constexpr (I == 0)
            {
                // Specialization for non-1D chare indexing
                index_type curr_tuple_ = tuple_concat(std::tuple<>{},
                    ((std::get<I>(curr_tuple) + std::get<I>(step_)) %
                        std::get<I>(end_)),
                    tuple_from<I + 1>(curr_tuple));

                index_type temp_tuple_ = tuple_concat(std::tuple<>{},
                    ((std::get<I>(curr_tuple) + std::get<I>(step_)) >=
                        std::get<I>(end_)),
                    tuple_from<I + 1>(curr_tuple));

                return next_impl<I + 1>(curr_tuple_, temp_tuple_);
            }
            else if constexpr (I != Tuple_size - 1)
            {
                // Specialization for non-1D mid indexing
                index_type curr_tuple_ =
                    tuple_concat(tuple_till<I - 1>(curr_tuple),
                        ((std::get<I>(curr_tuple) +
                             std::get<I>(step_) * std::get<I - 1>(temp_tuple)) %
                            std::get<I>(end_)),
                        tuple_from<I + 1>(curr_tuple));

                index_type temp_tuple_ = tuple_concat(
                    tuple_till<I - 1>(curr_tuple),
                    ((std::get<I>(curr_tuple) +
                         std::get<I>(step_) * std::get<I - 1>(temp_tuple)) >=
                        std::get<I>(end_)),
                    tuple_from<I + 1>(curr_tuple));

                return next_impl<I + 1>(curr_tuple_, temp_tuple_);
            }
            else
            {
                // Specialization for end index
                return tuple_concat(tuple_till<I - 1>(curr_tuple),
                    (std::get<I>(curr_tuple) +
                        std::get<I>(step_) * std::get<I - 1>(temp_tuple)),
                    std::tuple<>{});
            }
        }

        template <typename Tuple1, typename Arg, typename Tuple2>
        constexpr index_type tuple_concat(
            Tuple1&& t1, Arg const& arg, Tuple2&& t2) const
        {
            if constexpr (std::is_same_v<Tuple1, std::tuple<>>)
                return std::tuple_cat(
                    std::make_tuple(arg), std::forward<Tuple2>(t2));
            else if constexpr (std::is_same_v<Tuple2, std::tuple<>>)
                return std::tuple_cat(
                    std::forward<Tuple1>(t1), std::make_tuple(arg));
            else
                return std::tuple_cat(std::forward<Tuple1>(t1),
                    std::make_tuple(arg), std::forward<Tuple2>(t2));
        }

        template <std::size_t I>
        constexpr auto tuple_till(index_type const& t) const
        {
            return tuple_till_impl(t, std::make_index_sequence<I + 1>{});
        }

        template <std::size_t... Is>
        constexpr auto tuple_till_impl(
            index_type const& t, std::index_sequence<Is...>) const
        {
            return std::make_tuple(std::get<Is>(t)...);
        }

        template <std::size_t I>
        constexpr auto tuple_from(index_type const& t) const
        {
            constexpr std::size_t Tuple_size =
                std::tuple_size<index_type>::value;

            constexpr std::size_t New_size = Tuple_size - I;

            return tuple_from_impl<I>(t, std::make_index_sequence<New_size>{});
        }

        template <std::size_t Start, std::size_t... Is>
        constexpr auto tuple_from_impl(
            index_type const& t, std::index_sequence<Is...>) const
        {
            return std::make_tuple(std::get<Is + Start>(t)...);
        }
    };

    template <typename T>
    class index_range<T>
    {
        using less_type = typename index_view<T>::less_type;

        less_type less_;
        T curr_, step_, end_;

    public:
        constexpr index_range(T start, T step, T end)
          : curr_(start)
          , step_(step)
          , end_(end)
        {
        }

        inline constexpr bool has_next(void) const
        {
            return this->less_(this->curr_, this->end_);
        }

        inline T advance(void)
        {
            auto prev = this->curr_;
            this->curr_ += this->step_;
            return prev;
        }

        inline constexpr bool has_next(T const& curr) const
        {
            return this->less_(curr, this->end_);
        }

        inline constexpr T next(T const& curr) const
        {
            return curr + step_;
        }
    };

}    // namespace cmk

#endif
