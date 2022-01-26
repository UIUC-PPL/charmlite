#include <charmlite/charmlite.hpp>

#include <tuple>

int main(int argc, char* argv[])
{
    cmk::initialize(argc, argv);

    if (CmiMyNode() == 0)
    {
        // 1D range
        constexpr cmk::index_range<int> index_range_1d{0, 1, 5};
        constexpr int next_1d = index_range_1d.next(0);
        static_assert(next_1d == 1, "Next computed incorrectly for 1D range!");

        constexpr cmk::index_range<std::tuple<int, int>> index_range_2d{
            std::tuple<int, int>{0, 0}, std::tuple<int, int>{0, 1},
            std::tuple<int, int>{2, 2}};
        constexpr std::tuple<int, int> next_2d =
            index_range_2d.next(std::make_tuple(0, 0));
        static_assert(std::get<0>(next_2d) == 0 && std::get<1>(next_2d) == 0,
            "Next computed incorrectly for 2D range!");
    }

    cmk::finalize();
    return 0;
}
