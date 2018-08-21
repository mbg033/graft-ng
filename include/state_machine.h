#pragma once

#include "task.h"

namespace graft
{

enum State
{
    EXECUTE,
    PRE_ACTION,
    CHK_PRE_ACTION,
    WORKER_ACTION,
    CHK_WORKER_ACTION,
    WORKER_ACTION_DONE,
    POST_ACTION,
    CHK_POST_ACTION,
    AGAIN,
    EXIT,
};

class StateMachine final
{
public:
    using State = graft::State;
    using St = graft::Status;
    using Statuses = std::initializer_list<graft::Status>;
    using Guard = std::function<bool (BaseTaskPtr bt)>;
    using Action = std::function<void (BaseTaskPtr bt)>;

    StateMachine(State initial_state = EXECUTE)
    {
        state(initial_state);
    }

    State state() const { return m_state; }
    State state(State state) { return m_state = state; }
    St status(BaseTaskPtr bt) const { return bt->getLastStatus(); }
    void process(BaseTaskPtr bt)
    {
        St cur_stat = status(bt);

        for(auto& r : m_table)
        {
            if(m_state != std::get<0>(r)) continue;

            Statuses& ss = std::get<1>(r);
            if(ss.size()!=0)
            {
                bool res = false;
                for(auto s : ss)
                {
                    if(s == cur_stat)
                    {
                        res = true;
                        break;
                    }
                }
                if(!res) continue;
            }

            Guard& g = std::get<3>(r);
            if(g && !g(bt)) continue;

            Action& a = std::get<4>(r);
            if(a) a(bt);
            m_state = std::get<2>(r);
            return;
        }
        throw std::runtime_error("State machine table is not complete");
    }
private:

    using H3 = Router::Handler3;

    Guard has(Router::Handler H3::* act)
    {
        return [act](BaseTaskPtr bt)->bool
        {
            return (bt->getHandler3().*act != nullptr);
        };
    }

    Guard hasnt(Router::Handler H3::* act)
    {
        return [act](BaseTaskPtr bt)->bool
        {
            return (bt->getHandler3().*act == nullptr);
        };
    }

    const Action run_forward = [](BaseTaskPtr bt)
    {
        bt->getManager().processForward(bt);
    };

    const Action run_response = [](BaseTaskPtr bt)
    {
        assert(St::Again == bt->getLastStatus());
        bt->getManager().respondAndDie(bt, bt->getOutput().data(), false);
    };

    const Action run_error_response = [](BaseTaskPtr bt)
    {
        assert(St::Error == bt->getLastStatus() ||
               St::InternalError == bt->getLastStatus() ||
               St::Stop == bt->getLastStatus());
        bt->getManager().respondAndDie(bt, bt->getOutput().data());
    };

    const Action run_drop = [](BaseTaskPtr bt)
    {
        assert(St::Drop == bt->getLastStatus());
        bt->getManager().respondAndDie(bt, "Job done Drop."); //TODO: Expect HTTP Error Response
    };

    const Action run_ok_response = [](BaseTaskPtr bt)
    {
        assert(St::Ok == bt->getLastStatus());
        bt->getManager().processOk(bt);
    };

    const Action run_postpone = [](BaseTaskPtr bt)
    {
        assert(St::Postpone == bt->getLastStatus());
        bt->getManager().postponeTask(bt);
    };

    const Action check_overflow = [](BaseTaskPtr bt)
    {
        bt->getManager().checkThreadPoolOverflow(bt);
    };

    const Action run_preaction = [](BaseTaskPtr bt)
    {
        bt->getManager().runPreAction(bt);
    };

    const Action run_workeraction = [](BaseTaskPtr bt)
    {
        bt->getManager().runWorkerAction(bt);
    };

    const Action run_postaction = [](BaseTaskPtr bt)
    {
        bt->getManager().runPostAction(bt);
    };

    State m_state;
    using row = std::tuple<State, Statuses, State, Guard, Action>;

#define ANY { }
#define ANY_ERROR {St::Error, St::InternalError, St::Drop}

    std::vector<row> m_table =
    {
//      Start                   Status          Target              Guard               Action
        {EXECUTE,               ANY,            PRE_ACTION,         nullptr,            check_overflow },
        {PRE_ACTION,            {St::Busy},     EXIT,               nullptr,            nullptr },
        {PRE_ACTION,            {St::None, St::Ok, St::Forward, St::Postpone},
                                                CHK_PRE_ACTION,     nullptr,            run_preaction },
        {CHK_PRE_ACTION,        {St::Again},    PRE_ACTION,         nullptr,            run_response },
        {CHK_PRE_ACTION,        {St::Ok},       WORKER_ACTION,      has(&H3::pre_action), nullptr },
        {CHK_PRE_ACTION,        {St::Forward},  POST_ACTION,        has(&H3::pre_action), nullptr },
        {CHK_PRE_ACTION,        {St::Error, St::InternalError, St::Stop},
                                                EXIT,               has(&H3::pre_action), run_error_response },
        {CHK_PRE_ACTION,        {St::Drop},     EXIT,               has(&H3::pre_action), run_drop },
        {CHK_PRE_ACTION,        {St::None, St::Ok, St::Forward, St::Postpone},
                                                WORKER_ACTION,      nullptr,            nullptr },
        {WORKER_ACTION,         ANY,            CHK_WORKER_ACTION,  nullptr,            run_workeraction },
        {CHK_WORKER_ACTION,     ANY,            EXIT,               has(&H3::worker_action), nullptr },
        {CHK_WORKER_ACTION,     ANY,            POST_ACTION,        nullptr,            nullptr },

        {WORKER_ACTION_DONE,    {St::Again},    WORKER_ACTION,      nullptr,            run_response },
        {WORKER_ACTION_DONE,    ANY,            POST_ACTION,        nullptr,            nullptr },
        {POST_ACTION,           ANY,            CHK_POST_ACTION,    nullptr,            run_postaction },
        {CHK_POST_ACTION,       {St::Again},    POST_ACTION,        nullptr,            run_response },
        {CHK_POST_ACTION,       {St::Forward},  EXIT,               nullptr,            run_forward },
        {CHK_POST_ACTION,       {St::Ok},       EXIT,               nullptr,            run_ok_response },
        {CHK_POST_ACTION,       {St::Error, St::InternalError, St::Stop},
                                                EXIT,               nullptr,            run_error_response },
        {CHK_POST_ACTION,       {St::Drop},     EXIT,               nullptr,            run_drop },
        {CHK_POST_ACTION,       {St::Postpone}, EXIT,               nullptr,            run_postpone },
    };
};

}//namespace graft

