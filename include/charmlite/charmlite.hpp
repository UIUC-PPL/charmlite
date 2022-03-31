#ifndef CHARMLITE_CHARMLITE_HPP
#define CHARMLITE_CHARMLITE_HPP

#include <charmlite/core/core.hpp>

#include <charmlite/algorithms/reduction.hpp>

#include <charmlite/utilities/math.hpp>
#include <charmlite/utilities/traits.hpp>

#include <charmlite/serialization/serialization.hpp>

#if CMK_SMP
extern int userDrivenMode;
extern int CharmLibInterOperate;
#endif

namespace cmk {

    template <typename Message, typename... Args>
    inline message_ptr<Message> make_message(Args&&... args)
    {
        auto* msg = new Message(std::forward<Args>(args)...);
        return message_ptr<Message>(msg);
    }

    void start_fn_(int, char**);

    inline collection_base_* lookup(collection_index_t idx)
    {
        auto& tab = CpvAccess(collection_table_);
        auto find = tab.find(idx);
        if (find == std::end(tab))
        {
            return nullptr;
        }
        else
        {
            return (find->second).get();
        }
    }

    inline void initialize(int& argc, char** argv)
    {
#if CMK_SMP
        userDrivenMode = CharmLibInterOperate = true;
#endif
        ConverseInit(argc, argv, start_fn_, 1, 1);
        // update the argument counts since
        // converse will reorder them
        argc = CmiGetArgc(argv);
        initialize_globals_();
#if CHARMLITE_TOPOLOGY
        TopoManager_init();
        CmiNodeAllBarrier();
        CmiInitCPUAffinity(argv);
        CmiInitMemAffinity(argv);
        CmiInitCPUTopology(argv);
        TopoManager_reset();    // initialize TopoManager singleton
        _topoTree = ST_RecursivePartition_getTreeInfo(0);
#endif
        CmiNodeAllBarrier();
#if CHARMLITE_TOPOLOGY
        CmiCheckAffinity();
#endif
    }

    inline void send(message_ptr<>&& msg, bool immediate)
    {
        if (immediate)
        {
            converse_handler_(msg.release());
        }
        else
        {
            send(std::move(msg));
        }
    }

    inline void finalize(void)
    {
        CsdScheduleForever();
        ConverseExit();
    }

    void exit(message_ptr<>&& msg);

    // broadcasts an exit message to all pes
    inline void exit(void)
    {
        auto msg = cmk::make_message<message>();
        new (&(msg->dst_))
            destination(callback_helper_<message, exit>::id_, cmk::all::pes);
        send(std::move(msg));
    }
}    // namespace cmk

#endif
