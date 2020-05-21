#include "admin/admin.hpp"
#include <fstream>

namespace flamestore {

Admin::Admin(pymargo_instance_id mid, const std::string& connectionfile)
    : m_engine(std::make_shared<tl::engine>(CAPSULE2MID(mid)))
    , m_rpc_shutdown(m_engine->define("flamestore_shutdown"))
{
    std::ifstream ifs(connectionfile);
    if(!ifs.good())
        throw std::runtime_error(std::string("File ")+connectionfile+" not found");
    std::string master_provider_address;
    ifs >> master_provider_address;
    auto endpoint = m_engine->lookup(master_provider_address);
    m_master_provider = tl::provider_handle(endpoint, 0);
}

Admin::return_status Admin::shutdown()
{
    Status status = m_rpc_shutdown
        .on(m_master_provider)();
    return status.move_to_pair();
}

}
