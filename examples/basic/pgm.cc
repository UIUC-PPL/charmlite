/* charmlite demo
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <cmk.hh>

// a message with only POD members (constant-size)
struct test_message : public cmk::plain_message<test_message> {
  int val;

  test_message(int _) : val(_) {}
};

// a chare that uses an int for its index
struct foo : public cmk::chare<foo, int> {
  int val;

  foo(test_message* msg) : val(msg->val) { delete msg; }

  void bar(test_message* msg) {
    CmiPrintf("ch%d@pe%d> %d+%d=%d\n", this->index(), CmiMyPe(), this->val,
              msg->val, this->val + msg->val);
    // note -- this is a cross-pe reduction
    // (cross-chare reductions not yet implemented)
    cmk::reduce<cmk::nop, cmk::exit>(msg);
  }
};

int main(int argc, char** argv) {
  cmk::initialize(argc, argv);
  if (CmiMyNode() == 0) {
    // create a collection
    auto arr = cmk::collection_proxy<foo>::construct();
    // for each pe...
    for (auto i = 0; i < CmiNumPes(); i++) {
      auto elt = arr[i];
      // insert an element
      elt.insert(new test_message(i));
      // and send it a message
      elt.send<test_message, &foo::bar>(new test_message(i + 1));
    }
    // currently does nothing, will unblock reductions
    arr.done_inserting();
  }
  cmk::finalize();
  return 0;
}
