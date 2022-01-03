#ifndef CHARMLITE_UTILITIES_MATH_HPP
#define CHARMLITE_UTILITIES_MATH_HPP

#include <vector>

namespace cmk { namespace binary_tree {
    template <typename T>
    inline T left_child(const T& i)
    {
        return (2 * i) + 1;
    }

    template <typename T>
    inline T right_child(const T& i)
    {
        return (2 * i) + 2;
    }

    template <typename T>
    inline T parent(const T& i)
    {
        return (i > 0) ? ((i - 1) / 2) : -1;
    }

    template <typename T>
    std::vector<T> leaves(const T& which, const T& max)
    {
        auto left = left_child(which);
        auto right = right_child(which);
        std::vector<T> res;
        if (left >= 0 && left < max)
        {
            res.push_back(left);
        }
        if (right >= 0 && right < max)
        {
            res.push_back(right);
        }
        return res;
    }
}}    // namespace cmk::binary_tree

#endif
