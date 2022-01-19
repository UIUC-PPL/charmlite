/* charmlite demo
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <charmlite/charmlite.hpp>

// a message with only POD members (constant-size)
struct test_message : public cmk::plain_message<test_message>
{
    int val;

    test_message(int _)
      : val(_)
    {
    }
};

// a chare that uses an int for its index
struct foo : public cmk::chare<foo, int>
{
    int val;

    foo(cmk::message_ptr<test_message>&& msg)
      : val(msg->val)
    {
    }

    void bar(cmk::message_ptr<test_message>&& msg)
    {
        auto mine = CmiMyPe();
        CmiAssert((mine != 0) || (CmiNumPes() == 1));
        CmiPrintf("ch%d@pe%d> %d+%d=%d\n", this->index(), mine, this->val,
            msg->val, this->val + msg->val);
        auto cb =
            cmk::callback<cmk::message>::construct<cmk::exit>(cmk::all::pes);
        this->element_proxy().contribute<cmk::message, cmk::nop>(
            std::move(msg), cb);
    }
};

int main(int argc, char** argv)
{
    cmk::initialize(argc, argv);
    if (CmiMyNode() == 0)
    {
        // create a collection
        auto arr = cmk::collection_proxy<foo>::construct();
        // OVER DECOMPOSE!
        auto nPes = CmiNumPes();
        auto n = 8 * nPes;
        for (auto i = 0; i < n; i++)
        {
            if ((i % nPes == 0) && (nPes != 1))
            {
                n++;
            }
            else
            {
                arr[i].insert(cmk::make_message<test_message>(i));
            }
        }
        // then send 'em a buncha' messages
        arr.broadcast<test_message, &foo::bar>(
            cmk::make_message<test_message>(n));
        // necessary to enable collective communication
        arr.done_inserting();
    }
    cmk::finalize();
    return 0;
}
