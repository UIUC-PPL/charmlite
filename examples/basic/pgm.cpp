/* charmlite migration demo
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
    int val, orig;

    foo(cmk::message_ptr<test_message>&& msg)
      : val(msg->val)
      , orig(CmiMyPe())
    {
    }

    foo(PUP::reconstruct) {}

    void bar(cmk::message_ptr<test_message>&& msg)
    {
        CmiAssert((orig != 0) || (CmiNumPes() == 1));
        CmiPrintf("ch%d@pe%d> %d+%d=%d\n", this->index(), orig, this->val,
            msg->val, this->val + msg->val);
        auto dst_pe = (orig + 1) % CmiNumPes();
        if (orig == dst_pe || !this->migrate_me(dst_pe))
        {
            this->on_migrated();
        }
    }

    void on_migrated(void)
    {
        auto curr = CmiMyPe();
        if (orig != curr)
        {
            CmiPrintf("ch%d@pe%d> resumed with %d!\n", this->index(), curr,
                this->val);
        }

        auto cb =
            cmk::callback<cmk::message>::construct<cmk::exit>(cmk::all::pes);
        this->element_proxy().contribute<cmk::nop<cmk::message>>(
            cmk::make_message<cmk::message>(), cb);
    }

    void pup(PUP::er& p)
    {
        p | this->val;
        p | this->orig;
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
        arr.broadcast<&foo::bar>(cmk::make_message<test_message>(n));
        // necessary to enable collective communication
        arr.done_inserting();
    }
    cmk::finalize();
    return 0;
}
