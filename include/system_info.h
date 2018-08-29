#pragma once

#include <atomic>
#include <cstdint>
#include <chrono>

namespace graft { class Context; }

namespace graft { namespace supernode {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using SysClockTimePoint = std::chrono::time_point<std::chrono::system_clock>;

class SystemInfoProvider
{
  public:
    SystemInfoProvider(void);
    ~SystemInfoProvider(void);

    // interface for producer
    void count_http_request_total(void)       { ++m_http_req_total_cnt; }
    void count_http_request_routed(void)      { ++m_http_req_routed_cnt; }
    void count_http_request_unrouted(void)    { ++m_http_req_unrouted_cnt; }

    void count_http_resp_status_ok(void)      { ++m_http_resp_status_ok_cnt; }      // 200
    void count_http_resp_status_error(void)   { ++m_http_resp_status_error_cnt; }   // 500
    void count_http_resp_status_drop(void)    { ++m_http_resp_status_drop_cnt; }    // 400
    void count_http_resp_status_busy(void)    { ++m_http_resp_status_busy_cnt; }    // 503

    void count_upstrm_http_req(void)          { ++m_upstrm_http_req_cnt; }
    void count_upstrm_http_resp(void)         { ++m_upstrm_http_resp_cnt; }

    // interface for consumer
    u64 http_request_total_cnt(void) const    { return m_http_req_total_cnt; }
    u64 http_request_routed_cnt(void) const   { return m_http_req_routed_cnt; }
    u64 http_request_unrouted_cnt(void) const { return m_http_req_unrouted_cnt; }

    u64 http_resp_status_ok_cnt(void) const    { return m_http_resp_status_ok_cnt; }      // 200
    u64 http_resp_status_error_cnt(void) const { return m_http_resp_status_error_cnt; }   // 500
    u64 http_resp_status_drop_cnt(void) const  { return m_http_resp_status_drop_cnt; }    // 400
    u64 http_resp_status_busy_cnt(void) const  { return m_http_resp_status_busy_cnt; }    // 503

    u64 upstrm_http_req_cnt(void) const   { return m_upstrm_http_req_cnt; }
    u64 upstrm_http_resp_cnt(void) const  { return m_upstrm_http_resp_cnt; }

    u32 system_uptime_sec(void) const
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - m_system_start_time).count();
    }

  private:
    std::atomic<u64>  m_http_req_total_cnt;
    std::atomic<u64>  m_http_req_routed_cnt;
    std::atomic<u64>  m_http_req_unrouted_cnt;

    std::atomic<u64>  m_http_resp_status_ok_cnt;
    std::atomic<u64>  m_http_resp_status_error_cnt;
    std::atomic<u64>  m_http_resp_status_drop_cnt;
    std::atomic<u64>  m_http_resp_status_busy_cnt;

    std::atomic<u64>  m_upstrm_http_req_cnt;
    std::atomic<u64>  m_upstrm_http_resp_cnt;

    const SysClockTimePoint m_system_start_time;
};

SystemInfoProvider& get_system_info_provider_from_ctx(const graft::Context& ctx);

} }