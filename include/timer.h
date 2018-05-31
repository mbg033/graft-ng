#pragma once

#include <chrono>
#include <queue>
#include <functional>
#include <iostream>
#include <string>

namespace graft
{
    namespace ch = std::chrono;
    using Func = std::function<void(void)>;
/*
    template<typename Rep, typename Period>
    struct posix_duration_cast< ch::duration<Rep, Period>, struct timeval >
    {
        static struct timeval cast(ch::duration<Rep, Period> const& d)
        {
            struct timeval tv;
            ch::seconds const sec = ch::duration_cast<ch::seconds>(d);

            tv.tv_sec  = sec.count();
            tv.tv_usec = ch::duration_cast<ch::microseconds>(d - sec).count();

            return std::move(tv);
        }
    };
*/
    template<typename TR_ptr>
    class TimerList
    {
        struct timer
        {
            ch::seconds timeout;
            ch::seconds lap;
            TR_ptr ptr;
//            Func f;

            timer(ch::seconds timeout, TR_ptr ptr)
                : timeout(timeout)
                , lap(ch::seconds::max())
                , ptr(ptr) {}

            timer() = delete;

            void start()
            {
                lap = ch::time_point_cast<ch::seconds>(
                    ch::steady_clock::now()
                ).time_since_epoch() + timeout;
            }

            bool fired()
            {
                return lap <=
                    ch::time_point_cast<ch::seconds>(
                        ch::steady_clock::now()
                    ).time_since_epoch();
            }
/*
            struct timeval timeval()
            {
                return posix_duration_cast<ch::seconds>(lap);
            }
*/
            void dump(const std::string &pref) const
            {
                std::cout << pref << " timeout: " << timeout.count()
                    << "; lap: " << lap.count() << std::endl;
            }

            bool operator < (const timer& other) const
            {
                return lap < other.lap;
            }
            bool operator > (const timer& other) const { return ! operator <(other); }
        };
/*
        struct TimerCompare
        {
            bool operator()(
                    const std::shared_ptr<TimerList::timer> &t1,
                    const std::shared_ptr<TimerList::timer> &t2
            ) const
            {
                return t1->lap > t2->lap;
            }
        };
*/
        class priority_queue : public std::priority_queue<timer, std::vector<timer>, std::greater<timer>>
        {
            using base_t = std::priority_queue<timer, std::vector<timer>, std::greater<timer>>;
        public:
            const typename base_t::container_type& get_c() const { return base_t::c; }
        };

        priority_queue m_pq;
/*
        std::vector<timer> m_vec;
//        std::priority_queue<TimerList::timer, std::vector<TimerList::timer>, std::greater<TimerList::timer>> m_pq;
        std::priority_queue<timer> m_pq;
//            std::shared_ptr<TimerList::timer>,
    public:
        TimerList() : m_pq(std::greater<timer>(), m_vec)
        { }
*/

    public:
//        void push(ch::seconds timeout, Func func)
        void push(ch::seconds timeout, TR_ptr ptr)
        {
            timer t(timeout, ptr);
            t.start();
            m_pq.emplace(std::move(t));
        }

        void eval()
        {
            while(!m_pq.empty())
            {
                auto t = m_pq.top();
                if(!t.fired()) break;
                t.ptr->onEvent();
                m_pq.pop();
                t.start();
                m_pq.emplace(t);
//                if(t.f) t.f();
            }
        }
/*
        void start()
        {
            std::for_each(m_pq.begin(), m_pq.end(),
                [](const std::shared_ptr<TimerList::timer>& t) { t->start(); }
            );
        }
*/
        void dump(const std::string &pref) const
        {
            auto& vec = m_pq.get_c();
            std::for_each(vec.begin(), vec.end(),
                [&pref](auto& t) { t.dump(pref); }
            );
/*
            std::priority_queue<TimerList::timer> pq(m_pq);
            while(!pq.empty())
            {
                auto& t = pq.top();
                t.dump(pref);
                pq.pop();
            }
*/
        }
    };
}

