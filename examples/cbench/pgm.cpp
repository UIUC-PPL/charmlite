/* benchmark for testing collective performance
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include <charmlite/charmlite.hpp>
#include <unistd.h>

void resume_main(cmk::message_ptr<>&&);

struct communicator : public cmk::chare<communicator, int>
{
    int nChares;
    int nIters;
    int it;

    communicator(
        cmk::message_ptr<cmk::data_message<std::tuple<int, int>>>&& msg)
      : nChares(std::get<0>(msg->value()))
      , nIters(std::get<1>(msg->value()))
      , it(0)
    {
    }

    void run(cmk::message_ptr<>&& msg)
    {
        if ((++it) == nIters)
        {
            it = 0;

            auto cb = cmk::callback<cmk::message>::construct<resume_main>(0);
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

void resume_main(cmk::message_ptr<>&& msg)
{
    CthAwaken(th);
}

struct arguments
{
    int nChares = 4 * CmiNumPes();
    int nReps = 11;
    int nIters = 129;
};

arguments parse_arguments(int argc, char** argv);

int main(int argc, char** argv)
{
    cmk::initialize(argc, argv);
    if (CmiMyNode() == 0)
    {
        // assert that this test will not explode
        th = CthSelf();
        CmiAssert(th && CthIsSuspendable(th));
        // set up the initial parameters
        auto args = parse_arguments(argc, argv);
        // create the collection
        using message_type = cmk::data_message<std::tuple<int, int>>;
        auto imsg = cmk::make_message<message_type>(args.nChares, args.nIters);
        auto opts = cmk::collection_options<int>(args.nChares);
        auto arr = cmk::collection_proxy<communicator>::construct<message_type>(
            std::move(imsg), opts);

        int nSkip = args.nReps / 2;
        auto totalReps = args.nReps + nSkip;
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

        CmiPrintf(
            "info> interleaved %d broadcasts and reductions across %d chares\n",
            args.nIters, args.nChares);
        CmiPrintf("info> average time per repetition: %g ms\n",
            1e3 * (totalTime / args.nReps));
        CmiPrintf("info> average time per broadcast+reduction: %g ns\n",
            1e6 * (totalTime / (args.nIters * args.nReps)));

        cmk::exit();
    }
    cmk::finalize();
    return 0;
}

arguments parse_arguments(int argc, char** argv)
{
    arguments args;

    int opt;
    opterr = 0;

    while ((opt = getopt(argc, argv, "i:r:k:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            args.nIters = atoi(optarg);
            break;
        case 'r':
            args.nReps = atoi(optarg);
            break;
        case 'k':
            args.nChares = atoi(optarg);
            break;
        case '?':
            CmiError("error> unknown option, '%c'!\n", opt);
            break;
        }
    }

    return args;
}
