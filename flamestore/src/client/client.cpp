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
    , m_rpc_register_dataset(m_engine->define("flamestore_register_dataset"))
    , m_rpc_get_dataset_descriptor(m_engine->define("flamestore_get_dataset_descriptor"))
    , m_rpc_get_dataset_size(m_engine->define("flamestore_get_dataset_size"))
    , m_rpc_get_dataset_metadata(m_engine->define("flamestore_get_dataset_metadata"))
    , m_rpc_add_samples(m_engine->define("flamestore_add_samples"))
    , m_rpc_load_samples(m_engine->define("flamestore_load_samples"))
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
        std::vector<std::pair<void*, size_t>> mem(1);
        mem[0].first = cached_buffer.m_buffer.data();
        mem[0].second = cached_buffer.m_buffer.size();
        cached_buffer.m_bulk = engine().expose(mem, tl::bulk_mode::read_write);
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
        std::vector<std::pair<void*, size_t>> mem(1);
        mem[0].first = cached_buffer.m_buffer.data();
        mem[0].second = cached_buffer.m_buffer.size();
        cached_buffer.m_bulk = engine().expose(mem, tl::bulk_mode::read_write);
    }

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

Client::return_status Client::register_dataset(
        const std::string& dataset_name,
        const std::string& descriptor,
        const std::string& metadata)
{
    Status status = m_rpc_register_dataset
        .on(m_master_provider)(
                dataset_name,
                descriptor,
                metadata);
    return status.move_to_pair();
}

Client::return_status Client::get_dataset_descriptor(
        const std::string& dataset_name)
{
    Status status = m_rpc_get_dataset_descriptor
        .on(m_master_provider)(dataset_name);
    return status.move_to_pair();
}

Client::return_status Client::get_dataset_size(
        const std::string& dataset_name)
{
    Status status = m_rpc_get_dataset_size
        .on(m_master_provider)(dataset_name);
    return status.move_to_pair();
}

Client::return_status Client::get_dataset_metadata(
        const std::string& dataset_name)
{
    Status status = m_rpc_get_dataset_metadata
        .on(m_master_provider)(dataset_name);
    return status.move_to_pair();
}


Client::return_status Client::add_samples(
        const std::string& dataset_name,
        const std::string& descriptor,
        const std::vector<std::string>& field_names,
        const std::vector<np::array>& arrays)
{
    // Don't forget to pass client_address to the RPC
    std::cout << "add_samples: " << dataset_name
        << " " << descriptor << " with fields " << std::endl;
    for(const auto& f : field_names)
        std::cout << "   " << f << std::endl;
    std::cout << "and arrays size " << arrays.size() << std::endl;
}

Client::return_status Client::load_samples(
        const std::string& dataset_name,
        const std::string& descriptor,
        const std::vector<std::string>& field_names,
        const std::vector<std::vector<np::array>>& arrays)
{
    // Don't forget to pass client_address to the RPC
    std::cout << "load_samples: " << dataset_name
        << " " << descriptor << " with fields " << std::endl;
    for(const auto& f : field_names)
        std::cout << "   " << f << std::endl;
    std::cout << "and arrays size " << arrays.size() << std::endl;
}

}
