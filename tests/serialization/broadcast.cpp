#include <charmlite/charmlite.hpp>

#include <iostream>
#include <tuple>
#include <type_traits>
#include <vector>

struct invoker : cmk::chare<invoker, int>
{
    invoker()
    {
        CmiPrintf("Constructor called on PE%d!\n", CmiMyPe());
    }

    void invoke(int arg0, double arg1)
    {
        std::cout << arg0 << std::endl;
        std::cout << arg1 << std::endl;
    }
};

int main(int argc, char* argv[])
{
    cmk::initialize(argc, argv);

    if (CmiMyNode() == 0)
    {
        int arg0 = 42;
        double arg1 = 3.14;

        // create a collection
        auto arr = cmk::collection_proxy<invoker>::construct();
        // OVER DECOMPOSE!
        auto n = 8 * CmiNumPes();
        for (auto i = 0; i < n; i++)
        {
            arr[i].insert();
        }
        // then send 'em a buncha' messages
        arr.broadcast<&invoker::invoke>(arg0, arg1);
        // necessary to enable collective communication
        arr.done_inserting();
    }

    cmk::finalize();
    return 0;
}
