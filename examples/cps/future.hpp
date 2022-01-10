#ifndef CMK_FUTURE_HPP
#define CMK_FUTURE_HPP

#include <charmlite/charmlite.hpp>
#include <map>

struct future_base_
{
    int pe;
    int id;

    future_base_(int pe_, int id_)
      : pe(pe_)
      , id(id_)
    {
    }

    bool operator<(const future_base_& other) const
    {
        return (this->pe < other.pe) || (this->id < other.id);
    }
};

template <typename T>
struct future : public future_base_
{
    future(int pe_, int id_)
      : future_base_(pe_, id_)
    {
    }
};

struct future_manager_;

CpvDeclare(future_manager_, man_);

struct future_manager_
{
    using value_type = cmk::message_ptr<cmk::message>;
    using action_type = cmk::callback<cmk::message>;

    void chain(const future_base_& f, const future_base_& g)
    {
        auto ins = this->chained_.emplace(f, g);
        CmiEnforce(ins.second);
    }

    void put(const future_base_& f, value_type&& val)
    {
        auto search = this->waiting_.find(f);
        if (search == std::end(this->waiting_))
        {
            auto ins = this->avail_.emplace(f, std::move(val));
            CmiAssertMsg(ins.second, "duplicate value for future!");
        }
        else
        {
            this->fire_(f, search->second, val);
            this->waiting_.erase(search);
        }
    }

    void get(const future_base_& f, const action_type& action)
    {
        auto search = this->avail_.find(f);
        if (search == std::end(this->avail_))
        {
            auto ins = this->waiting_.emplace(f, action);
            CmiAssertMsg(ins.second, "duplicate value for future!");
        }
        else
        {
            this->fire_(f, action, search->second);
            this->avail_.erase(search);
        }
    }

    template <typename T>
    future<T> issue(void)
    {
        return future<T>(CmiMyPe(), ++this->last_);
    }

private:
    static void receive_(cmk::message_ptr<>&& msg)
    {
        auto* dst = &(msg->dst_);
        auto* g = reinterpret_cast<future_base_*>(dst->extra());
        CpvAccess(man_).put(*g, std::move(msg));
    }

    void fire_(
        const future_base_& f, const action_type& action, value_type& val)
    {
        auto search = this->chained_.find(f);
        if (search != std::end(this->chained_))
        {
            auto& g = search->second;
            auto cb = cmk::callback<cmk::message>::construct<
                &future_manager_::receive_>(g.pe);
            val->has_continuation() = true;
            auto* dst = val->continuation();
            cb.imprint(*dst);
            memcpy(dst->extra(), &g, sizeof(future_base_));
            this->chained_.erase(search);
        }
        action.send(std::move(val));
    }

    int last_ = 0;
    std::map<future_base_, value_type> avail_;
    std::map<future_base_, future_base_> chained_;
    std::map<future_base_, action_type> waiting_;
};

template <typename T, typename Rval>
struct fn_ptr_
{
    using type = Rval (*)(T);
};

template <typename Rval>
struct fn_ptr_<void, Rval>
{
    using type = Rval (*)();
};

template <typename T, typename Rval>
using fn_ptr_t = typename fn_ptr_<T, Rval>::type;

template <typename T, typename Rval>
struct action
{
    using fn_t = fn_ptr_t<T, Rval>;

    cmk::callback<cmk::message> cb_;

    template <fn_t Fn>
    static void impl_(cmk::message_ptr<>&& msg);

    template <fn_t Fn>
    static action<T, Rval> construct(void)
    {
        action<T, Rval> rval{.cb_ = cmk::callback<cmk::message>::construct<
                                 &action<T, Rval>::impl_<Fn>>(CmiMyPe())};
        return rval;
    }
};

template <typename T, typename Rval>
struct invoker_
{
    template <fn_ptr_t<T, Rval> Fn>
    static Rval invoke(cmk::data_message<T>* msg)
    {
        return Fn(msg->value());
    }
};

template <typename Rval>
struct invoker_<void, Rval>
{
    template <fn_ptr_t<void, Rval> Fn>
    static Rval invoke(cmk::data_message<void>*)
    {
        return Fn();
    }
};

template <typename T, typename Rval>
template <typename action<T, Rval>::fn_t Fn>
void action<T, Rval>::impl_(cmk::message_ptr<>&& msg)
{
    auto* cont = msg->continuation();
    auto* typed = static_cast<cmk::data_message<T>*>(msg.release());
    auto rval = invoker_<T, Rval>::template invoke<Fn>(typed);

    if (cont)
    {
        auto rmsg = cmk::make_message<cmk::data_message<Rval>>(rval);
        new (&rmsg->dst_) cmk::destination(*cont);
        cmk::send(std::move(rmsg));
    }

    delete typed;
}

struct scheduler
{
    scheduler(void) = default;
};

future<void> schedule(scheduler&)
{
    if (!CpvInitialized(man_))
    {
        CpvInitialize(future_manager_, man_);
    }

    auto& man = CpvAccess(man_);
    auto f = man.issue<void>();
    man.put(f, cmk::make_message<cmk::message>());
    return f;
}

template <typename T, typename Rval>
future<Rval> then(const future<T>& f, const action<T, Rval>& action)
{
    auto& man = CpvAccess(man_);
    auto g = man.issue<Rval>();
    man.chain(f, g);
    man.get(f, action.cb_);
    return g;
}

// NOTE ( this is NOT a robust solution )
using limbo_t = std::pair<CthThread, void*>;
CpvDeclare(limbo_t, limbo);

template <typename T>
void resume_(cmk::message_ptr<>&& msg)
{
    auto& limbo = CpvAccess(limbo);
    auto* typed = static_cast<cmk::data_message<T>*>(msg.release());
    *(reinterpret_cast<T*>(limbo.second)) = typed->value();
    CthAwaken(limbo.first);
    delete typed;
}

template <typename T>
T sync_wait(const future<T>& f)
{
    T t;
    if (!CpvInitialized(limbo))
    {
        CpvInitialize(limbo_t, limbo);
    }
    auto& limbo = CpvAccess(limbo);
    limbo.first = CthSelf();
    limbo.second = &t;
    CpvAccess(man_).get(
        f, cmk::callback<cmk::message>::construct<&resume_<T>>(CmiMyPe()));
    CthSuspend();
    return t;
}

#endif
