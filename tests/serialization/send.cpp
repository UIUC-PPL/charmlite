#include <charmlite/charmlite.hpp>

#include <iostream>
#include <random>
#include <tuple>
#include <type_traits>
#include <vector>

struct invoker : cmk::chare<invoker, int>
{
    invoker(double arg0, const std::vector<int>& arg1)
    {
        CmiPrintf("From PE%d, constructor input: %f, {", CmiMyPe(), arg0);
        for (int elem : arg1)
        {
            CmiPrintf("%d,", elem);
        }
        CmiPrintf("}\n");

        this->element_proxy().send<&invoker::initiate_exit>(std::rand() % 10);
    }

    void initiate_exit(int exit_code)
    {
        CmiPrintf("Initiating Exit from PE%d with exit code %d\n", CmiMyPe(),
            exit_code);
        auto cb =
            cmk::callback<cmk::message>::construct<cmk::exit>(cmk::all::pes);
        this->element_proxy().contribute<cmk::nop<>>(
            cmk::make_message<cmk::message>(), cb);
    }
};

int main(int argc, char* argv[])
{
    cmk::initialize(argc, argv);

    if (CmiMyNode() == 0)
    {
        double arg0 = 3.14;
        std::vector arg1{-1, 0, 1, 42};

        // create a collection
        auto arr = cmk::collection_proxy<invoker>::construct();
        // OVER DECOMPOSE!
        auto n = 8 * CmiNumPes();
        for (auto i = 0; i < n; i++)
        {
            arr[i].insert(arg0, arg1);
        }

        arr.done_inserting();
    }

    cmk::finalize();
    return 0;
}
