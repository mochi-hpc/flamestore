#include "client/client.hpp"
#include <fstream>

namespace flamestore {

void ProviderHandle::shutdown() const {
    m_engine->shutdown_remote_engine(m_provider_handle);
}

Client::Client(pymargo_instance_id mid, const std::string& configfile)
    : m_engine(std::make_shared<tl::engine>(CAPSULE2MID(mid)))
    , m_addr_str(m_engine->self())
    , m_rpc_register_model(m_engine->define("flamestore_register_model"))
    , m_rpc_reload_model(m_engine->define("flamestore_reload_model"))
{
    std::ifstream ifs(configfile);
    if(!ifs.good())
        throw std::runtime_error(std::string("File ")+configfile+" not found");
    std::string metadata_provider_address;
    uint16_t metadata_provider_id;
    ifs >> metadata_provider_address >> metadata_provider_id;
//    auto endpoint = m_engine->lookup(metadata_provider_address);
//    m_metadata_provider_handle = ProviderHandle(m_engine,
//            tl::provider_handle(endpoint, metadata_provider_id));
}

Client::return_status Client::register_model(
        const std::string& model_name,
        const std::string& model_config,
        std::size_t model_data_size,
        const std::string& model_signature) const
{
    Status status(0, "");
    std::cerr << "[CLIENT] Registering model " << model_name << std::endl;
    some_model_config = model_config;
#if 0
    flamestore_status status = m_rpc_register_model
        .on(m_metadata_provider_handle.m_provider_handle)(
            m_addr_str,
            model_name,
            model_config,
            optimizer_config,
            model_data_size,
            optimizer_data_size,
            model_signature,
            optimizer_signature);
#endif
    return status.move_to_pair();
}

Client::return_status Client::reload_model(
        const std::string& model_name,
        bool include_optimizer) const
{
    std::cerr << "[CLIENT] Reloading model " << model_name << std::endl;
    Status status(0, some_model_config);
#if 0
    flamestore_status status = m_rpc_reload_model
        .on(m_metadata_provider_handle.m_provider_handle)(
            model_name,
            include_optimizer);
#endif
    return status.move_to_pair();
}

}
