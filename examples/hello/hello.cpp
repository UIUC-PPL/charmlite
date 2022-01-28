/* the goal of this exercise is to
 * minimize how many lines it takes to
 * represent a "hello, world" example 
 * 
 * TODOs:
 * - we can add something like `cmk::do_once`
 * - we can add something like `CkCallback(CkCallback::ckExit)`
 * - parameter marshalling will help!
 * - a `nop`-by-default contribute op will help!
 */

#include <charmlite/charmlite.hpp>

class hello : public cmk::chare<hello, int>
{
public:
    void sayHello(cmk::message_ptr<>&& msg)
    {
        CmiPrintf("%d> hello!\n", this->index());

        this->element_proxy().contribute<cmk::nop<>>(std::move(msg),
            cmk::callback<cmk::message>::construct<&cmk::exit>(cmk::all::pes));
    }
};

int main(int argc, char** argv)
{
    cmk::initialize(argc, argv);

    if (CmiMyNode() == 0)
    {
        auto proxy = cmk::group_proxy<hello>::construct();
        proxy.broadcast<&hello::sayHello>(cmk::make_message<cmk::message>());
    }

    cmk::finalize();

    return 0;
}
