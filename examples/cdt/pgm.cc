/* charmlite completion detection demo
 *
 * ( completion detection is important
 *   for hypercomm collections, hence
 *   giving priority to this! )
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <cmk.hh>
#include <completion.hh>

// a callback to resume the main thread
void resume_main_(cmk::message_ptr<>&& msg);

struct test : cmk::chare<test, int>
{
    using completion_proxy = cmk::group_proxy<cmk::completion>;

    completion_proxy detector;
    bool detection_started_;

    test(cmk::message_ptr<cmk::data_message<completion_proxy>>&& msg)
      : detector(msg->value())
    {
    }

    void produce(cmk::message_ptr<>&& msg)
    {
        // reset completion detection status
        // (only at "root" element)
        this->detection_started_ = (this->index() != 0);
        // check whether we have a local branch
        auto* local = detector.local_branch();
        if (local == nullptr)
        {
            auto elt = this->element_proxy();
            // put the message back to if we don't
            elt.send<cmk::message, &test::produce>(std::move(msg));
        }
        else
        {
            // each pe will expect a message from each pe (inclusive)
            CmiPrintf("%d> producing %d value(s)...\n", CmiMyPe(), CmiNumPes());
            local->produce(this->collection(), CmiNumPes());
            // so send the messages
            cmk::group_proxy<test> col(this->collection());
            col.broadcast<cmk::message, &test::consume>(std::move(msg));
        }
    }

    void consume(cmk::message_ptr<>&& msg)
    {
        auto* local = detector.local_branch();
        if (local == nullptr)
        {
            auto elt = this->element_proxy();
            // put the message back to await local branch creation
            elt.send<cmk::message, &test::consume>(std::move(msg));
        }
        else
        {
            // indicate that we received an expected message
            CmiPrintf("%d> consuming a value...\n", CmiMyPe());
            local->consume(this->collection());

            // start completion detection if we haven't already
            if (!detection_started_)
            {
                // goal : wake up the main pe!
                auto cb =
                    cmk::callback<cmk::message>::construct<resume_main_>(0);
                auto dm = cmk::make_message<cmk::completion::detection_message>(
                    this->collection(), cb);
                // (each pe could start its own completion detection
                //  but this checks that broadcasts are working!)
                detector.broadcast<cmk::completion::detection_message,
                    &cmk::completion::start_detection>(std::move(dm));

                detection_started_ = true;
            }
        }
    }
};

CthThread th;

void resume_main_(cmk::message_ptr<>&& msg)
{
    CthAwaken(th);
}

int main(int argc, char** argv)
{
    cmk::initialize(argc, argv);
    if (CmiMyNode() == 0)
    {
        // assert that this test will not explode
        th = CthSelf();
        CmiAssert(th && CthIsSuspendable(th));
        // establish detector and participant groups
        auto detector = cmk::group_proxy<cmk::completion>::construct();
        auto dm =
            cmk::make_message<cmk::data_message<decltype(detector)>>(detector);
        auto elts = cmk::group_proxy<test>::construct(std::move(dm));
        auto nIts = CmiNumPes();
        for (auto it = 0; it < nIts; it++)
        {
            // send each element a "produce" message to start the process
            elts.broadcast<cmk::message, &test::produce>(
                cmk::make_message<cmk::message>());
            // sleep until detection completes
            CthSuspend();
            CmiPrintf("main> iteration %d complete!\n", it + 1);
        }
        // all done...
        cmk::exit();
    }
    cmk::finalize();
    return 0;
}
