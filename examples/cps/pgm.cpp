/* charmlite continuation passing demo
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include "future.hpp"

int say_hello(void)
{
    CmiPrintf("hello world! have an int...\n");
    return 13;
}

int plus_42(int arg)
{
    return arg + 42;
}

int main(int argc, char** argv)
{
    cmk::initialize(argc, argv);
    if (CmiMyNode() == 0)
    {
        auto sch = scheduler();
        auto begin = schedule(sch);
        auto hi = then(begin, action<void, int>::construct<&say_hello>());
        auto add_42 = then(hi, action<int, int>::construct<&plus_42>());
        auto res = sync_wait(add_42);
        CmiEnforce(res == (13 + 42));
        CmiPrintf("woke up with %d!\n", res);
        cmk::exit();
    }
    cmk::finalize();
    return 0;
}
