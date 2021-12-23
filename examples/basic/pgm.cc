/* charmlite demo
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <cmk.hh>

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
        CmiPrintf("ch%d@pe%d> %d+%d=%d\n", this->index(), CmiMyPe(), this->val,
            msg->val, this->val + msg->val);
        auto cb = cmk::callback<cmk::message>::construct<cmk::exit>(cmk::all);
        this->element_proxy()
            .contribute<cmk::message, cmk::nop>(std::move(msg), cb);
    }
};

int main(int argc, char** argv)
{
    cmk::initialize(argc, argv);
    if (CmiMyNode() == 0)
    {
        // create a collection
        auto arr = cmk::collection_proxy<foo>::construct();
        // for each pe...
        for (auto i = 0; i < CmiNumPes(); i++)
        {
            auto elt = arr[i];
            // insert an element
            elt.insert(cmk::make_message<test_message>(i));
            // and send it a message
            elt.send<test_message, &foo::bar>(
                cmk::make_message<test_message>(i + 1));
        }
        // currently does nothing, will unblock reductions
        arr.done_inserting();
    }
    cmk::finalize();
    return 0;
}
