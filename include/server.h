#pragma once

#include "connection.h"

namespace graft {

class GraftServer
{
public:
    bool init(const ConfigOpts& opts);
    bool serve();
protected:
    virtual bool initConfigOption(const std::string& config_filename);
    virtual void initConnectionManagers();
private:
    void initSignals();
    void initLog(int log_level);
    void addGlobalCtxCleaner();
    void setHttpRouters(HttpConnectionManager& httpcm);
    void setCoapRouters(CoapConnectionManager& coapcm);
    static void checkRoutes(graft::ConnectionManager& cm);

    ConfigOpts m_configOpts;
    std::unique_ptr<graft::Looper> m_looper;
    std::vector<std::unique_ptr<graft::ConnectionManager>> m_conManagers;
};

}//namespace graft

