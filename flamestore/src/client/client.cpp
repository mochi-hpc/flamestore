#include "client/client.hpp"
#include <fstream>

namespace flamestore {

Client::Client(pymargo_instance_id mid, const std::string& connectionfile)
    : m_engine(std::make_shared<tl::engine>(CAPSULE2MID(mid)))
    , m_client_addr(m_engine->self())
    , m_rpc_shutdown(m_engine->define("flamestore_shutdown"))
    , m_rpc_register_model(m_engine->define("flamestore_register_model"))
    , m_rpc_reload_model(m_engine->define("flamestore_reload_model"))
    , m_rpc_write_model(m_engine->define("flamestore_write_model_data"))
    , m_rpc_read_model(m_engine->define("flamestore_read_model_data"))
    , m_rpc_dup_model(m_engine->define("flamestore_dup_model"))
{
    std::ifstream ifs(connectionfile);
    if(!ifs.good())
        throw std::runtime_error(std::string("File ")+connectionfile+" not found");
    std::string master_provider_address;
    ifs >> master_provider_address;
    auto endpoint = m_engine->lookup(master_provider_address);
    m_master_provider = tl::provider_handle(endpoint, 0);
}

Client::return_status Client::shutdown()
{
    Status status = m_rpc_shutdown
        .on(m_master_provider)();
    return status.move_to_pair();
}

Client::return_status Client::register_model(
        const std::string& model_name,
        const std::string& model_config,
        std::size_t model_data_size,
        const std::string& model_signature)
{
    Status status = m_rpc_register_model
        .on(m_master_provider)(
            m_client_addr,
            model_name,
            model_config,
            model_data_size,
            model_signature);
    return status.move_to_pair();
}

Client::return_status Client::reload_model(
        const std::string& model_name)
{
    Status status = m_rpc_reload_model
        .on(m_master_provider)(
            m_client_addr,
            model_name);
    return status.move_to_pair();
}

Client::return_status Client::write_model_data(
        const std::string& model_name,
        const std::string& signature,
        std::vector<std::pair<void*,size_t>>& memory,
        const std::size_t& size)
{
    auto& cached_buffer = m_cache[model_name];
    if(cached_buffer.m_bulk.is_null()
    || cached_buffer.m_buffer.size() != size) {
        cached_buffer.m_buffer.resize(size);
        cached_buffer.m_bulk = engine().expose(memory, tl::bulk_mode::read_write);
    }

    size_t offset = 0;
    for(auto& p : memory) {
        std::memcpy(cached_buffer.m_buffer.data()+offset, p.first, p.second);
        offset += p.second;
    }

    Status status = m_rpc_write_model
        .on(m_master_provider)(
            m_client_addr,
            model_name,
            signature,
            cached_buffer.m_bulk,
            size);

    return status.move_to_pair();
}

Client::return_status Client::read_model_data(
        const std::string& model_name,
        const std::string& signature,
        std::vector<std::pair<void*,size_t>>& memory,
        const std::size_t& size)
{
    auto& cached_buffer = m_cache[model_name];
    if(cached_buffer.m_bulk.is_null()
    || cached_buffer.m_buffer.size() != size) {
        cached_buffer.m_buffer.resize(size);
        cached_buffer.m_bulk = engine().expose(memory, tl::bulk_mode::read_write);
    }

    tl::bulk local_bulk = engine().expose(memory, tl::bulk_mode::write_only);
    Status status = m_rpc_read_model
        .on(m_master_provider)(
            m_client_addr,
            model_name,
            signature,
            cached_buffer.m_bulk,
            size);

    size_t offset = 0;
    for(auto& p : memory) {
        std::memcpy(p.first, cached_buffer.m_buffer.data()+offset, p.second);
        offset += p.second;
    }

    return status.move_to_pair();
}

Client::return_status Client::duplicate_model(
        const std::string& model_name,
        const std::string& new_model_name)
{
    Status status = m_rpc_dup_model
        .on(m_master_provider)(
            model_name,
            new_model_name);
    return status.move_to_pair();
}

}
