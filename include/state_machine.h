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
    CHK_WORKER_ACTION_DONE,
    POST_ACTION,
    CHK_POST_ACTION,
    AGAIN,
    EXIT,
};

class StateMachine
{
public:
    using State = graft::State;
    using Status = graft::Status;
    using Guard = std::function<bool (BaseTaskPtr bt)>;
    using Action = std::function<void (BaseTaskPtr bt)>;

    StateMachine(State initial_state = EXECUTE)
    {
        state(initial_state);
    }

    State state() const { return m_state; }
    State state(State state) { return m_state = state; }
    Status status(BaseTaskPtr bt) const { return bt->getLastStatus(); }
    void process(BaseTaskPtr bt)
    {
        Status cur_stat = status(bt);
        for(int i = 0; i < table_size; ++i)
        {
            row& r = table[i];
            if(m_state != std::get<0>(r)) continue;
            Status& s = std::get<1>(r);
            if(s != Status::Any && cur_stat != std::get<1>(r)) continue;
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
    State m_state;
    using row = std::tuple<State, Status, State, Guard, Action>;
    static row table[];
    static int table_size;

    using H3 = Router::Handler3;

    static Guard has(Router::Handler H3::* act)
    {
        return [act](BaseTaskPtr bt)->bool
        {
            return (bt->getHandler3().*act != nullptr);
        };
    }

    static Action action(State act_state)
    {
        return [act_state](BaseTaskPtr bt)
        {
            bt->getManager().process_action(bt, int(act_state));
        };
    }
};

}//namespace graft

