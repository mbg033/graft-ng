#include "task.h"
#include "connection.h"
#include "router.h"
#include "fsm.h"

namespace graft {

thread_local bool TaskManager::io_thread = false;
TaskManager* TaskManager::g_upstreamManager{nullptr};

//pay attension, input is output and vice versa
void TaskManager::sendUpstreamBlocking(Output& output, Input& input, std::string& err)
{
    if(io_thread) throw std::logic_error("the function sendUpstreamBlocking should not be called in IO thread");
    assert(g_upstreamManager);
    std::promise<Input> promise;
    std::future<Input> future = promise.get_future();
    std::pair< std::promise<Input>, Output> pair = std::make_pair( std::move(promise), output);
    g_upstreamManager->m_promiseQueue->push( std::move(pair) );
    g_upstreamManager->notifyJobReady();
    err.clear();
    try
    {
        input = future.get();
    }
    catch(std::exception& ex)
    {
        err = ex.what();
    }
}

void TaskManager::checkUpstreamBlockingIO()
{
    while(true)
    {
        PromiseItem pi;
        bool res = m_promiseQueue->pop(pi);
        if(!res) break;
        UpstreamTask::Ptr bt = BaseTask::Create<UpstreamTask>(*this, std::move(pi));
        UpstreamSender::Ptr uss = UpstreamSender::Create<UpstreamSender>();
        uss->send(*this, bt);
    }
}

void TaskManager::sendUpstream(BaseTaskPtr bt)
{
    ++m_cntUpstreamSender;
    UpstreamSender::Ptr uss = UpstreamSender::Create();
    uss->send(*this, bt);
}

void TaskManager::onTimer(BaseTaskPtr bt)
{
    assert(dynamic_cast<PeriodicTask*>(bt.get()));
    Execute(bt);
}

void TaskManager::respondAndDie(BaseTaskPtr bt, const std::string& s, bool keep_alive)
{
    ClientTask* ct = dynamic_cast<ClientTask*>(bt.get());
    if(ct)
    {
        ct->m_connectionManager->respond(ct, s);
    }
    else
    {
        assert( dynamic_cast<PeriodicTask*>(bt.get()) );
    }

    Context::uuid_t uuid = bt->getCtx().getId();
    auto it = m_postponedTasks.find(uuid);
    if (it != m_postponedTasks.end())
        m_postponedTasks.erase(it);

    if(!keep_alive)
        bt->finalize();
}

void TaskManager::schedule(PeriodicTask* pt)
{
    m_timerList.push(pt->getTimeout(), pt->getSelf());
}

/*
enum State : int
{
    DONE = 1,
    EXECUTE,
    PRE_ACTION,
    WORKER_ACTION,
    WORKER_ACTION_DONE,
    POST_ACTION,
    ERROR,
    FORWARD,
    AGAIN,
    EXIT,
};
*/

template<int N>
struct eventStatus
{
     enum { ev = N };

     eventStatus(BaseTaskPtr bt) : bt(bt) { }
     BaseTaskPtr bt;
};

auto is_two = [](eventStatus<0> i) { Status s = Status::Ok; /* = i.bt->getHandler3().pre_action(...);*/ i.bt->setLastStatus(s); return; };
auto is_two_1 = [](eventStatus<1> i) { Status s = Status::Error; /* = i.bt->getHandler3().pre_action(...);*/ i.bt->setLastStatus(s); return; };

class state_machine: public fsmlite::fsm<state_machine>
{
    friend class fsm;  // base class needs access to transition_table
public:
//    enum State : int
    enum states
    {
        DONE = 1,
        EXECUTE,
        PRE_ACTION,
        WORKER_ACTION,
        WORKER_ACTION_DONE,
        POST_ACTION,
        ERROR,
        FORWARD,
        AGAIN,
        EXIT,
    };
public:
    state_machine() : fsm(0) { }
    state_machine(states initial_state) : fsm(initial_state)
    {

    }
//    enum states { Init, Even, Odd };

//    typedef int event;
//    using event = BaseTaskPtr;
    typedef BaseTaskPtr event;

private:
/*
    bool is_even(const event& e) const {
        return e % 2 == 0;
    }

    bool is_odd(const event& e) const {
        return e % 2 != 0;
    }
*/
    bool can(BaseTaskPtr bt)
    {
        int prev_state = current_state();
        switch(states(prev_state))
        {
        case EXECUTE: return PRE_ACTION;
        case PRE_ACTION:
        {
            if(bt->getParams().h3.pre_action)
            {
                switch(bt->getLastStatus())
                {
                case Status::None: assert(false);
                case Status::Ok: return WORKER_ACTION;
                case Status::Forward: return WORKER_ACTION;
                case Status::Again: return states(AGAIN | (PRE_ACTION << 8));
                default: return states(AGAIN | (EXIT << 8));
    /*
                case Status::Error: return states(AGAIN | (EXIT << 8));
                case Status::Drop: return states(AGAIN | (EXIT << 8));
                case Status::Busy: return states(AGAIN | (EXIT << 8));
                case Status::InternalError: return states(AGAIN | (EXIT << 8));
                case Status::Postpone: return states(AGAIN | (EXIT << 8));
                case Status::Stop: return states(AGAIN | (EXIT << 8));
    */
                }
            }
            return WORKER_ACTION;
        }
        case WORKER_ACTION:
        {
            if(bt->getParams().h3.worker_action) return EXIT;
            else return POST_ACTION;
        }
        case WORKER_ACTION_DONE:
        {
            switch(bt->getLastStatus())
            {
            case Status::Again: return states(AGAIN | (WORKER_ACTION << 8));
            default: return POST_ACTION;
            }
        }
        case POST_ACTION:
        {
            switch(bt->getLastStatus())
            {
            case Status::Again: return states(AGAIN | (POST_ACTION << 8));
            default: return states(AGAIN | (EXIT << 8));
            }
        }
        default: assert(false);
        }
    }

    bool ret_true(const BaseTaskPtr& bt) const { return true; }
    void do_nothing(const BaseTaskPtr& bt) { return; }
    bool can_PRE_ACTION_to_WORKER_ACTION(const BaseTaskPtr& bt) const
    {
        if(bt->getParams().h3.pre_action)
        {
            switch(bt->getLastStatus())
            {
            case Status::None: assert(false);
            case Status::Ok: return true; // return SM::WORKER_ACTION;
            case Status::Forward: return true; // return SM::WORKER_ACTION;
            default: return false;
/*
            case Status::Again: return SM::states(SM::AGAIN | (SM::PRE_ACTION << 8));
            default: return SM::states(SM::AGAIN | (SM::EXIT << 8));
*/
/*
            case Status::Error: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Drop: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Busy: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::InternalError: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Postpone: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Stop: return SM::states(SM::AGAIN | (SM::EXIT << 8));
*/
            }
        }
        return false;
    }

    template<int FROM, int TO>
    bool can(const BaseTaskPtr& bt) const
    {
        if(bt->getParams().h3.pre_action)
        {
            switch(bt->getLastStatus())
            {
            case Status::None: assert(false);
            case Status::Ok: return true; // return SM::WORKER_ACTION;
            case Status::Forward: return true; // return SM::WORKER_ACTION;
            default: return false;
/*
            case Status::Again: return SM::states(SM::AGAIN | (SM::PRE_ACTION << 8));
            default: return SM::states(SM::AGAIN | (SM::EXIT << 8));
*/
/*
            case Status::Error: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Drop: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Busy: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::InternalError: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Postpone: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Stop: return SM::states(SM::AGAIN | (SM::EXIT << 8));
*/
            }
        }
        return false;
    }

    void do_PRE_ACTION_to_WORKER_ACTION(const BaseTaskPtr& bt)
    {
        auto& params = bt->getParams();
        auto& ctx = bt->getCtx();
        auto& output = bt->getOutput();

//        if(params.h3.pre_action)
        {
            try
            {
                Status status = params.h3.pre_action(params.vars, params.input, ctx, output);
                bt->setLastStatus(status);
                if(Status::Ok == status && (params.h3.worker_action || params.h3.post_action)
                        || Status::Forward == status)
                {
                    params.input.assign(output);
                }
            }
            catch(const std::exception& e)
            {
                bt->setError(e.what());
                params.input.reset();
            }
            catch(...)
            {
                bt->setError("unknown exception");
                params.input.reset();
            }
            LOG_PRINT_RQS_BT(3,bt,"pre_action completed with result " << bt->getStrStatus());
        }
    }
private:

    void x(const eventStatus<0>& v)
    {
//        bt->

    }

    struct X
    {
        void operator() (eventStatus<0>& v)
        {

        }
        BaseTaskPtr bt;
    } my_x;

    BaseTaskPtr bt;


private:
    typedef state_machine m;

    using transition_table = table<
//              Start Event  Target Action   Guard
//  -----------+-----+------+------+--------+--------------+-
    row< PRE_ACTION, eventStatus<0>, WORKER_ACTION, decltype(is_two),   &is_two  >,
    row< PRE_ACTION, eventStatus<1>, WORKER_ACTION, decltype(is_two_1),   &is_two_1  >
//    row< Init,    event, Running, void,   nullptr                                   >,
//    mem_fn_row< PRE_ACTION, eventStatus<0>, WORKER_ACTION, void,  [](const eventStatus<0>& v) { return; }, nullptr   >
/*
    mem_fn_row< DONE, event, EXECUTE,  &m::do_nothing, &m::ret_true >,
    mem_fn_row< EXECUTE, event, PRE_ACTION,  nullptr, nullptr    >,
    mem_fn_row< PRE_ACTION, event, WORKER_ACTION,  &m::do_PRE_ACTION_to_WORKER_ACTION, &m::can_PRE_ACTION_to_WORKER_ACTION    >,
    mem_fn_row< PRE_ACTION, eventStatus<Status::Ok>, WORKER_ACTION, nullptr, nullptr   >,
/ *
    mem_fn_row< Init, event, Even,  nullptr, &m::is_even    >,
    mem_fn_row< Init, event, Odd,   nullptr, &m::is_odd     >,
    mem_fn_row< Even, event, Odd,   nullptr, &m::is_odd     >,
    mem_fn_row< Even, event, Even,  nullptr, &m::is_even    >,
    mem_fn_row< Odd,  event, Even,  nullptr, &m::is_even    >,
* /
    mem_fn_row< EXECUTE, event, PRE_ACTION, nullptr  / * fallback * / >
*/
//  -----------+-----+------+------+--------+--------------+-
    >;
};

using SM = state_machine;

/*
std::tuple<State, bool> nextState(BaseTaskPtr bt, State prev_state)
{
//    static bool copy_output = false;
    switch(bt->getLastStatus())
    {
    case Status::None:
    {
        assert(prev_state == DONE);
        return std::make_tuple(PRE_ACTION, false);
    } break;
    case Status::Ok:
    {
        switch(prev_state)
        {
        case PRE_ACTION : return std::make_tuple(WORKER_ACTION, true);
        case WORKER_ACTION : return std::make_tuple(POST_ACTION, true);
        case POST_ACTION : return std::make_tuple(DONE, false);
        case ERROR : assert(false); //return std::make_tuple(DONE, false);
        case FORWARD : return std::make_tuple(PRE_ACTION, true);
        case DONE : return std::make_tuple(PRE_ACTION, true); //timer //assert(false);
        }
    } break;
    case Status::Again:
    {
        switch(prev_state)
        {
        case PRE_ACTION : return std::make_tuple(State(AGAIN | (PRE_ACTION << 8)), true);
        case WORKER_ACTION : return std::make_tuple(State(AGAIN | (WORKER_ACTION << 8)), true);
        case POST_ACTION : return std::make_tuple(State(AGAIN | (POST_ACTION << 8)), false);
        case ERROR : //return std::make_tuple(DONE, false);
        case FORWARD : //return std::make_tuple(PRE_ACTION, true);
        case DONE : assert(false);
        }
    } break;
    case Status::Error:
    {
        switch(prev_state)
        {
//        case DONE : assert(false);
        case PRE_ACTION : return std::make_tuple(ERROR, false);
        case WORKER_ACTION : return std::make_tuple(ERROR, false);
        case POST_ACTION : return std::make_tuple(ERROR, false);
        case ERROR : //return std::make_tuple(DONE, false);
        case FORWARD : return std::make_tuple(ERROR, true);
        case DONE : assert(false);
        }
    } break;
    case Status::Forward:
    {
        switch(prev_state)
        {
//        case DONE : assert(false);
        case PRE_ACTION : return std::make_tuple(FORWARD, true);
        case WORKER_ACTION : return std::make_tuple(FORWARD, true);
        case POST_ACTION : return std::make_tuple(FORWARD, true);
        case ERROR : assert(false); ///: return std::make_tuple(PRE_ACTION, false);
        case FORWARD : return std::make_tuple(PRE_ACTION, false); //assert(false);
        case DONE : assert(false);
        }
    } break;
    default: assert(false);
    }
}
*/

SM::states nextState(BaseTaskPtr bt, SM::states prev_state)
{
    switch(prev_state)
    {
    case SM::EXECUTE: return SM::PRE_ACTION;
    case SM::PRE_ACTION:
    {
        if(bt->getParams().h3.pre_action)
        {
            switch(bt->getLastStatus())
            {
            case Status::None: assert(false);
            case Status::Ok: return SM::WORKER_ACTION;
            case Status::Forward: return SM::WORKER_ACTION;
            case Status::Again: return SM::states(SM::AGAIN | (SM::PRE_ACTION << 8));
            default: return SM::states(SM::AGAIN | (SM::EXIT << 8));
/*
            case Status::Error: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Drop: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Busy: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::InternalError: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Postpone: return SM::states(SM::AGAIN | (SM::EXIT << 8));
            case Status::Stop: return SM::states(SM::AGAIN | (SM::EXIT << 8));
*/
            }
        }
        return SM::WORKER_ACTION;
    }
    case SM::WORKER_ACTION:
    {
        if(bt->getParams().h3.worker_action) return SM::EXIT;
        else return SM::POST_ACTION;
    }
    case SM::WORKER_ACTION_DONE:
    {
        switch(bt->getLastStatus())
        {
        case Status::Again: return SM::states(SM::AGAIN | (SM::WORKER_ACTION << 8));
        default: return SM::POST_ACTION;
        }
    }
    case SM::POST_ACTION:
    {
        switch(bt->getLastStatus())
        {
        case Status::Again: return SM::states(SM::AGAIN | (SM::POST_ACTION << 8));
        default: return SM::states(SM::AGAIN | (SM::EXIT << 8));
        }
    }
    default: assert(false);
    }
}

void TaskManager::dispatch_x(BaseTaskPtr bt, int initial_state)
{
/*
    auto& params = bt->getParams();
    auto& ctx = bt->getCtx();
    auto& output = bt->getOutput();
*/
    SM sm{SM::states(initial_state)};
//    SM sm;
//    auto cs = sm.current_state();
    while(SM::states(sm.current_state()) != SM::EXIT)
    {
        sm.process_event(eventStatus<0>(bt));
    }
//    sm.m_state
}


void TaskManager::dispatch(BaseTaskPtr bt, int initial_state, int initial_use_state)
{
    auto& params = bt->getParams();
    auto& ctx = bt->getCtx();
    auto& output = bt->getOutput();

    SM::states use_state = SM::states(initial_use_state);
    SM::states state = initial_state? SM::states(initial_state) : SM::DONE;
    while(true)
    {
        bool copy_output;
        if(use_state)
        {
            state = use_state;
            use_state = SM::states(0);
        }
        else
        {
            SM::states prev_state = state;
            state = SM::states(state >> 8);
            if(state == 0)
                state = nextState(bt, prev_state);
//                std::tie(state, copy_output) = nextState(bt, prev_state);
        }

        switch(state & 0xFF)
        {
        case SM::EXECUTE:
        {
            assert(m_cntJobDone <= m_cntJobSent);
            if(m_cntJobSent - m_cntJobDone == m_threadPoolInputSize)
            {//check overflow
                bt->getCtx().local.setError("Service Unavailable", Status::Busy);
                respondAndDie(bt,"Thread pool overflow");

                use_state = SM::EXIT;
                break;
            }
            assert(m_cntJobSent - m_cntJobDone < m_threadPoolInputSize);
        } break;
        case SM::PRE_ACTION:
        {
            if(params.h3.pre_action)
            {
                try
                {
                    Status status = params.h3.pre_action(params.vars, params.input, ctx, output);
                    bt->setLastStatus(status);
                    if(Status::Ok == status && (params.h3.worker_action || params.h3.post_action)
                            || Status::Forward == status)
                    {
                        params.input.assign(output);
                    }
                }
                catch(const std::exception& e)
                {
                    bt->setError(e.what());
                    params.input.reset();
                }
                catch(...)
                {
                    bt->setError("unknown exception");
                    params.input.reset();
                }
                LOG_PRINT_RQS_BT(3,bt,"pre_action completed with result " << bt->getStrStatus());
            }
        } break;
        case SM::WORKER_ACTION:
        {
            if(params.h3.worker_action)
            {
                ++m_cntJobSent;
                m_threadPool->post(
                            GJPtr( bt, m_resQueue.get(), this ),
                            true
                            );
            }
        } break;
        case SM::WORKER_ACTION_DONE:
        {
            LOG_PRINT_RQS_BT(2,bt,"worker_action completed with result " << bt->getStrStatus());

            if(Status::Again == bt->getLastStatus())
            {
                use_state = SM::states(SM::AGAIN | (SM::WORKER_ACTION << 8));
                break;
            }
        } break;
        case SM::POST_ACTION:
        {
            if(params.h3.post_action)
            {
                try
                {
                    Status status = params.h3.post_action(params.vars, params.input, ctx, output);
                    bt->setLastStatus(status);
                    if(Status::Forward == status)
                    {
                        params.input.assign(output);
                    }
                }
                catch(const std::exception& e)
                {
                    bt->setError(e.what());
                    params.input.reset();
                }
                catch(...)
                {
                    bt->setError("unknown exception");
                    params.input.reset();
                }
                LOG_PRINT_RQS_BT(3,bt,"post_action completed with result " << bt->getStrStatus());
            }
        } break;
        case SM::AGAIN:
        {
            processResult(bt);
        } break;
        case SM::EXIT:
        {
            return;
        } break;
/*
        case ERROR:
        {
            respondAndDie(bt, bt->getOutput().data());
            bt->finalize();
            return;
        } break;
//        case THREAD_POOL:
        case DONE:
        {
            respondAndDie(bt, bt->getOutput().data());
            bt->finalize();
            return;
        } break;
*/
/*
        case FORWARD:
        {
            LOG_PRINT_RQS_BT(3,bt,"Sending request to CryptoNode");
            sendUpstream(bt);
            return;
        } break;
*/
        default: assert(false);
        }
    }
}

void TaskManager::Execute(BaseTaskPtr bt)
{
    dispatch(bt, 0, SM::EXECUTE);
}

bool TaskManager::canStop()
{
    return (m_cntBaseTask == m_cntBaseTaskDone)
            && (m_cntUpstreamSender == m_cntUpstreamSenderDone)
            && (m_cntJobSent == m_cntJobDone);
}

bool TaskManager::tryProcessReadyJob()
{
    GJPtr gj;
    bool res = m_resQueue->pop(gj);
    if(!res) return res;
    ++m_cntJobDone;
    BaseTaskPtr bt = gj->getTask();

    dispatch(bt, 0, SM::WORKER_ACTION_DONE);

    return true;
}

void TaskManager::postponeTask(BaseTaskPtr bt)
{
    Context::uuid_t uuid = bt->getCtx().getId();
    assert(!uuid.is_nil());
    assert(m_postponedTasks.find(uuid) == m_postponedTasks.end());
    m_postponedTasks[uuid] = bt;
    std::chrono::duration<double> timeout(m_copts.http_connection_timeout);
    std::chrono::steady_clock::time_point tpoint = std::chrono::steady_clock::now()
            + std::chrono::duration_cast<std::chrono::steady_clock::duration>( timeout );
    m_expireTaskQueue.push(std::make_pair(
                                    tpoint,
                                    uuid)
                                );
}

void TaskManager::executePostponedTasks()
{
    while(!m_readyToResume.empty())
    {
        BaseTaskPtr& bt = m_readyToResume.front();
        Execute(bt);
        m_readyToResume.pop_front();
    }

    if(m_expireTaskQueue.empty()) return;

    auto now = std::chrono::steady_clock::now();
    while(!m_expireTaskQueue.empty())
    {
        auto& pair = m_expireTaskQueue.top();
        if(now <= pair.first) break;

        auto it = m_postponedTasks.find(pair.second);
        if(it != m_postponedTasks.end())
        {
            BaseTaskPtr& bt = it->second;
            std::string msg = "Postpone task response timeout";
            bt->setError(msg.c_str(), Status::Error);
            respondAndDie(bt, msg);
        }

        m_expireTaskQueue.pop();
    }
}

void TaskManager::processResult(BaseTaskPtr bt)
{
    switch(bt->getLastStatus())
    {
    case Status::Forward:
    {
        LOG_PRINT_RQS_BT(3,bt,"Sending request to CryptoNode");
        sendUpstream(bt);
    } break;
    case Status::Again:
    {
        respondAndDie(bt, bt->getOutput().data(), true);
    } break;
    case Status::Ok:
    {
        Context::uuid_t nextUuid = bt->getCtx().getNextTaskId();
        if(!nextUuid.is_nil())
        {
            auto it = m_postponedTasks.find(nextUuid);
            assert(it != m_postponedTasks.end());
            m_readyToResume.push_back(it->second);
            m_postponedTasks.erase(it);
        }
        respondAndDie(bt, bt->getOutput().data());
    } break;
    case Status::InternalError:
    case Status::Error:
    case Status::Stop:
    {
        respondAndDie(bt, bt->getOutput().data());
    } break;
    case Status::Drop:
    {
        respondAndDie(bt, "Job done Drop."); //TODO: Expect HTTP Error Response
    } break;
    case Status::Postpone:
    {
        postponeTask(bt);
    } break;
    default:
    {
        assert(false);
    } break;
    }
}

void TaskManager::addPeriodicTask(const Router::Handler3& h3, std::chrono::milliseconds interval_ms)
{
    addPeriodicTask(h3, interval_ms, interval_ms);
}

void TaskManager::addPeriodicTask(
        const Router::Handler3& h3, std::chrono::milliseconds interval_ms, std::chrono::milliseconds initial_interval_ms)
{
    BaseTask* bt = BaseTask::Create<PeriodicTask>(*this, h3, interval_ms, initial_interval_ms).get();
    PeriodicTask* pt = dynamic_cast<PeriodicTask*>(bt);
    assert(pt);
    schedule(pt);
}

TaskManager *TaskManager::from(mg_mgr *mgr)
{
    void* user_data = getUserData(mgr);
    assert(user_data);
    return static_cast<TaskManager*>(user_data);
}

void TaskManager::onNewClient(BaseTaskPtr bt)
{
    ++m_cntBaseTask;
    Execute(bt);
}

void TaskManager::onClientDone(BaseTaskPtr bt)
{
    ++m_cntBaseTaskDone;
}

void TaskManager::initThreadPool(int threadCount, int workersQueueSize)
{
    if(threadCount <= 0) threadCount = std::thread::hardware_concurrency();
    if(workersQueueSize <= 0) workersQueueSize = 32;

    tp::ThreadPoolOptions th_op;
    th_op.setThreadCount(threadCount);
    th_op.setQueueSize(workersQueueSize);
    graft::ThreadPoolX thread_pool(th_op);

    size_t resQueueSize;
    {//nearest ceiling power of 2
        size_t val = th_op.threadCount()*th_op.queueSize();
        size_t bit = 1;
        for(; bit<val; bit <<= 1);
        resQueueSize = bit;
    }

    const size_t maxinputSize = th_op.threadCount()*th_op.queueSize();
    graft::TPResQueue resQueue(resQueueSize);

    m_threadPool = std::make_unique<ThreadPoolX>(std::move(thread_pool));
    m_resQueue = std::make_unique<TPResQueue>(std::move(resQueue));
    m_threadPoolInputSize = maxinputSize;
    m_promiseQueue = std::make_unique<PromiseQueue>(threadCount);

    LOG_PRINT_L1("Thread pool created with " << threadCount
                 << " workers with " << workersQueueSize
                 << " queue size each. The output queue size is " << resQueueSize);
}

void TaskManager::setIOThread(bool current)
{
    if(current)
    {
        io_thread = true;
        assert(!g_upstreamManager);
        g_upstreamManager = this;
    }
    else
    {
        g_upstreamManager = nullptr;
        io_thread = false;
    }
}

void TaskManager::cb_event(uint64_t cnt)
{
    //When multiple threads write to the output queue of the thread pool.
    //It is possible that a hole appears when a thread has not completed to set
    //the cell data in the queue. The hole leads to failure of pop operations.
    //Thus, it is better to process as many cells as we can without waiting when
    //the cell will be filled, instead of basing on the counter.
    //We cannot lose any cell because a notification follows the hole completion.

    while(true)
    {
        bool res = tryProcessReadyJob();
        if(!res) break;
    }
}

void TaskManager::onUpstreamDone(UpstreamSender& uss)
{
    BaseTaskPtr bt = uss.getTask();
    UpstreamTask* ust = dynamic_cast<UpstreamTask*>(bt.get());
    if(ust)
    {
        try
        {
            if(Status::Ok != uss.getStatus())
            {
                throw std::runtime_error(uss.getError().c_str());
            }
            ust->m_pi.first.set_value(bt->getInput());
        }
        catch(std::exception&)
        {
            ust->m_pi.first.set_exception(std::current_exception());
        }
        return;
    }
    if(Status::Ok != uss.getStatus())
    {
        bt->setError(uss.getError().c_str(), uss.getStatus());
        LOG_PRINT_RQS_BT(2,bt, "CryptoNode done with error: " << uss.getError().c_str());
        processResult(bt);
        ++m_cntUpstreamSenderDone;
        return;
    }
    //here you can send a job to the thread pool or send response to client
    //uss will be destroyed on exit, save its result
    {//now always create a job and put it to the thread pool after CryptoNode
        LOG_PRINT_RQS_BT(2,bt, "CryptoNode answered ");
        if(!bt->getSelf()) return; //it is possible that a client has closed connection already
//        dispatch(bt, FORWARD, PRE_ACTION);
        Execute(bt);
        ++m_cntUpstreamSenderDone;
    }
//    ++m_cntUpstreamSenderDone;
    //uss will be destroyed on exit
}

BaseTask::BaseTask(TaskManager& manager, const Router::JobParams& params)
    : m_manager(manager)
    , m_params(params)
    , m_ctx(manager.getGcm())
{
}

const char* BaseTask::getStrStatus(Status s)
{
    assert(s<=Status::Stop);
    static const char *status_str[] = { GRAFT_STATUS_LIST(EXP_TO_STR) };
    return status_str[static_cast<int>(s)];
}

const char* BaseTask::getStrStatus()
{
    return getStrStatus(m_ctx.local.getLastStatus());
}

void UpstreamTask::finalize()
{
    releaseItself();
}

void PeriodicTask::finalize()
{
    if(m_ctx.local.getLastStatus() == Status::Stop)
    {
        LOG_PRINT_L2("Timer request stopped with result " << getStrStatus());
        releaseItself();
        return;
    }
    this->m_manager.schedule(this);
}

std::chrono::milliseconds PeriodicTask::getTimeout()
{
    auto ret = (m_initial_run) ? m_initial_timeout_ms : m_timeout_ms;
    m_initial_run = false;
    return ret;
}

ClientTask::ClientTask(ConnectionManager* connectionManager, mg_connection *client, Router::JobParams& prms)
    : BaseTask(*TaskManager::from( getMgr(client) ), prms)
    , m_connectionManager(connectionManager)
    , m_client(client)
{
}

void ClientTask::finalize()
{
    releaseItself();
}

}//namespace graft
