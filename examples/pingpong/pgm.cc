/* charmlite demo
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <cmk.hh>

// a message with only POD members (constant-size)
struct payload_message : public cmk::message
{
    std::size_t size;
    char* data;

    payload_message(std::size_t size_)
      : cmk::message(cmk::message_helper_<payload_message>::kind_,
            sizeof(message) + size_)
      , size(size_)
      , data((char*) this + sizeof(message))
    {
    }

    static void pack(cmk::message_ptr<payload_message>& msg)
    {
        msg->data = msg->data - (std::uintptr_t) msg.get();
    }

    static void unpack(cmk::message_ptr<payload_message>& msg)
    {
        msg->data = msg->data + (std::uintptr_t) msg.get();
    }
};

static_assert(cmk::is_packable<payload_message>::value,
    "expected payload mesasge to be packable");

void phase_completed_(cmk::message_ptr<cmk::data_message<double>>&&);

// a tuple with (sz, nIts) as its members
using run_message_t = cmk::data_message<std::tuple<std::size_t, std::size_t>>;

// a chare that uses an int for its index
struct pingpong : public cmk::chare<pingpong, int>
{
    cmk::element_proxy<pingpong> peer;
    std::size_t it, nIts;
    double startTime;

    pingpong(void)
      : peer((this->collection_proxy())[(this->index() + 1) % CmiNumPes()])
    {
    }

    void run(cmk::message_ptr<run_message_t>&& msg)
    {
        auto& val = msg->value();
        auto& sz = std::get<1>(val);
        // allocate the payload
        cmk::message_ptr<payload_message> payload(
            new (sizeof(cmk::message) + sz) payload_message(sz));
        // reset the iteration counts
        this->it = 0;
        this->nIts = std::get<1>(val);
        // free the run-message (since we're done with it)
        cmk::message::free(msg);
        // then GO!
        this->startTime = CmiWallTimer();
        peer.send<payload_message, &pingpong::receive_message>(
            std::move(payload));
    }

    void receive_message(cmk::message_ptr<payload_message>&& msg)
    {
        if ((this->index() == 0) && (++(this->it) == this->nIts))
        {
            auto endTime = CmiWallTimer();
            auto cb = cmk::callback<cmk::data_message<double>>::construct<
                phase_completed_>(0);
            cb.send(cmk::make_message<cmk::data_message<double>>(
                endTime - this->startTime));
        }
        else
        {
            peer.send<payload_message, &pingpong::receive_message>(
                std::move(msg));
        }
    }
};

CthThread th;
double lastTime;

void phase_completed_(cmk::message_ptr<cmk::data_message<double>>&& msg)
{
    lastTime = msg->value();
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
        // create a collection
        auto grp = cmk::group_proxy<pingpong>::construct();
        // get the runtime parameters
        std::size_t sz = (argc >= 2 && argv) ? atoll(argv[1]) : 4096;
        std::size_t nIts = (argc >= 3 && argv) ? atoll(argv[2]) : 128;
        CmiPrintf(
            "main> pingpong with %luB payload and %lu iterations\n", sz, nIts);
        // allocate the launch pack
        auto msg = cmk::make_message<run_message_t>(sz, nIts);
        // then run through a warm up phase
        grp[0].send<run_message_t, &pingpong::run>(msg->clone<run_message_t>());
        CthSuspend();
        // then run through the measurement phase
        grp[0].send<run_message_t, &pingpong::run>(std::move(msg));
        CthSuspend();
        // print the final round-trip time
        CmiPrintf("main> roundtrip time was %g us\n",
            (1e6 * lastTime) / (double) nIts);
        // then exit
        cmk::exit();
    }
    cmk::finalize();
    return 0;
}
