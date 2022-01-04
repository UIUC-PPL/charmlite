#ifndef CHARMLITE_CORE_COMMMON_HPP
#define CHARMLITE_CORE_COMMMON_HPP

#include <converse.h>
#include <execinfo.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if CHARMLITE_TOPOLOGY
#include "TopoManager.h"
#include "spanningTree.h"

void CmiInitCPUTopology(char** argv);
void CmiInitMemAffinity(char** argv);
#endif

namespace cmk {

    // Helper Singleton Class
    template <typename T, typename SingletonClass>
    class singleton
    {
    public:
        using value_type = T;
        using class_type = SingletonClass;

        // Non-copyable, non-movable
        singleton(singleton const&) = delete;
        singleton(singleton&&) = delete;
        singleton& operator=(singleton const&) = delete;
        singleton& operator=(singleton&&) = delete;

        static const std::unique_ptr<value_type>& instance()
        {
            static std::unique_ptr<value_type> inst{new value_type()};

            return inst;
        }

    private:
        singleton() = default;
    };

#define CMK_GENERATE_SINGLETON(type, name)                                     \
    class name : public singleton<type, name>                                  \
    {                                                                          \
    private:                                                                   \
        name() = default;                                                      \
    }

#define CMK_ACCESS_SINGLETON(name) (*name::instance())

    struct message;

    template <typename Message = message>
    using message_ptr =
        std::unique_ptr<Message>;    // , message_deleter_<Message>>;

    template <typename Message, typename... Args>
    inline message_ptr<Message> make_message(Args&&... args);

    // TODO ( rename to callback_fn_t )
    template <typename Message>
    using callback_fn_t = void (*)(message_ptr<Message>&&);
    using callback_table_t = std::vector<callback_fn_t<message>>;
    using callback_id_t = typename callback_table_t::size_type;

    // TODO ( rename to combiner_fn_t )
    template <typename Message>
    using combiner_fn_t = message_ptr<Message> (*)(
        message_ptr<Message>&&, message_ptr<Message>&&);
    using combiner_table_t = std::vector<combiner_fn_t<message>>;
    using combiner_id_t = typename combiner_table_t::size_type;

    // Shared between workers in a process
    CMK_GENERATE_SINGLETON(combiner_table_t, combiner_table_);
    CMK_GENERATE_SINGLETON(callback_table_t, callback_table_);

    // Each worker has its own instance of these
    CpvExtern(std::uint32_t, local_collection_count_);
    CpvExtern(int, converse_handler_);
    void converse_handler_(void*);

    struct collection_index_t
    {
        std::int32_t pe_;
        std::uint32_t id_;

        inline bool operator==(const collection_index_t& other) const
        {
            return (reinterpret_cast<const std::size_t&>(*this) ==
                reinterpret_cast<const std::size_t&>(other));
        }

        inline operator std::string(void) const
        {
            std::stringstream ss;
            ss << "collection(";
            ss << "pe=" << this->pe_ << ",";
            ss << "id=" << this->id_;
            ss << ")";
            return ss.str();
        }
    };

    struct collection_index_hasher_
    {
        static_assert(sizeof(collection_index_t) == sizeof(std::size_t),
            "trivial hashing assumed");
        std::size_t operator()(const collection_index_t& id) const
        {
            auto& view = reinterpret_cast<const std::size_t&>(id);
            return std::hash<std::size_t>()(view);
        }
    };

    template <typename T>
    using collection_map =
        std::unordered_map<collection_index_t, T, collection_index_hasher_>;

    inline void pack_message(message_ptr<>& msg);
    inline void unpack_message(message_ptr<>& msg);

    using entry_fn_t = void (*)(void*, message_ptr<>&&);

    struct entry_record_
    {
        const entry_fn_t fn_;
        bool is_constructor_;

        entry_record_(entry_fn_t fn, bool is_constructor)
          : fn_(fn)
          , is_constructor_(is_constructor)
        {
        }

        // helper function to be used for projections tracing
        std::string name(void) const
        {
            auto* fn = reinterpret_cast<void*>(this->fn_);
            auto** names = backtrace_symbols(&fn, 1);
            // free is only reqd when n ptrs > 1
            return std::string(names[0]);
        }

        void invoke(void* obj, message_ptr<>&& raw) const
        {
            unpack_message(raw);
            (this->fn_)(obj, std::move(raw));
        }
    };

    using entry_table_t = std::vector<entry_record_>;
    using entry_id_t = typename entry_table_t::size_type;

    enum class destination_kind : std::uint8_t
    {
        Invalid = 0,
        Callback,
        Endpoint
    };

    void converse_handler_(void*);

    void initialize_globals_(void);

    void send(message_ptr<>&&);

    inline void send(message_ptr<>&& msg, bool immediate);

    using chare_index_t =
        typename std::conditional<std::is_integral<CmiUInt16>::value, CmiUInt16,
            CmiUInt8>::type;

    // TODO ( rename this "collective" id type )
    using bcast_id_t = std::uint16_t;

    // Shared between workers in a process (contd.)
    CMK_GENERATE_SINGLETON(entry_table_t, entry_table_);

    struct all
    {
        static constexpr int pes = -1;
        static constexpr int nodes = -2;
    };

    // TODO: Find a better name
    struct helper_
    {
        static constexpr entry_id_t nil_entry_ = 0;
        static constexpr chare_index_t chare_bcast_root_ =
            std::numeric_limits<chare_index_t>::max();
    };

}    // namespace cmk

#endif
