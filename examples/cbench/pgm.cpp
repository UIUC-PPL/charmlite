/* benchmark for testing collective performance
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <charmlite/charmlite.hpp>

void resume_main_(cmk::message_ptr<>&&);

struct communicator : public cmk::chare<communicator, int>
{
    int nChares;
    int nIters;
    int it;

    communicator(cmk::message_ptr<cmk::data_message<std::tuple<int, int>>>&& msg)
    : nChares(std::get<0>(msg->value())),
      nIters(std::get<1>(msg->value())),
      it(0)
    {
    }

    void run(cmk::message_ptr<>&& msg)
    {
        if ((++it) == nIters)
        {
            it = 0;

            auto cb = cmk::callback<cmk::message>::construct<resume_main_>(0);
            this->element_proxy().contribute<cmk::message, cmk::nop>(
                std::move(msg), cb);
        }
        else if ((it % nChares) == this->index())
        {
            this->collection_proxy()
                .broadcast<cmk::message, &communicator::recv_broadcast>(
                    std::move(msg));
        }
    }

    void recv_broadcast(cmk::message_ptr<>&& msg)
    {
        auto cb = this->collection_proxy()
                      .callback<cmk::message, &communicator::run>();
        this->element_proxy().contribute<cmk::message, cmk::nop>(
            std::move(msg), cb);
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
        // set up the initial parameters
        // TODO ( pull these from command-line )
        auto nChares = 4 * CmiNumPes();
        auto nReps = 11;
        auto nIters = 129;
        auto nSkip = nReps / 2;
        // create the collection
        using message_type = cmk::data_message<std::tuple<int, int>>;
        auto imsg = cmk::make_message<message_type>(nChares, nIters);
        auto opts = cmk::collection_options<int>(nChares);
        auto arr = cmk::collection_proxy<communicator>::construct<message_type>(std::move(imsg), opts);

        auto totalReps = nSkip + nReps;
        double totalTime = 0;

        for (auto rep = 0; rep < totalReps; rep++)
        {
            CmiPrintf("main> rep %d of %d\n", rep + 1, totalReps);

            auto startTime = CmiWallTimer();

            arr.broadcast<cmk::message, &communicator::run>(
                cmk::make_message<cmk::message>());
            CthSuspend();

            auto endTime = CmiWallTimer();

            if (rep >= nSkip)
            {
                totalTime += (endTime - startTime);
            }
        }

        CmiPrintf("info> interleaved %d broadcasts and reductions\n", 4);
        CmiPrintf("info> with %d chares\n", nChares);
        CmiPrintf("info> average time per repetition: %g ms\n",
            1e3 * (totalTime / nReps));

        cmk::exit();
    }
    cmk::finalize();
    return 0;
}
