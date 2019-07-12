#include "client.hpp"
#include "status.hpp"

void flamestore_provider_handle::shutdown() const {
    m_engine->shutdown_remote_engine(m_provider_handle);
}

flamestore_provider_handle flamestore_client::lookup(
        const std::string& address,
        uint16_t provider_id) const {

    auto it = m_endpoint_cache.find(address);
    if(it != m_endpoint_cache.end()) {
        return flamestore_provider_handle(m_engine, tl::provider_handle(it->second, provider_id));
    } else {
        auto endpoint = m_engine->lookup(address);
        m_endpoint_cache[address] = endpoint;
        return flamestore_provider_handle(m_engine, tl::provider_handle(endpoint, provider_id));
    }
}

flamestore_client::return_status flamestore_client::register_model(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& model_config,
        const std::string& optimizer_config,
        std::size_t model_data_size,
        std::size_t optimizer_data_size,
        const std::string& model_signature,
        const std::string& optimizer_signature) const {

    flamestore_status status = m_rpc_register_model
        .on(ph.m_provider_handle)(
            m_addr_str,
            model_name,
            model_config,
            optimizer_config,
            model_data_size,
            optimizer_data_size,
            model_signature,
            optimizer_signature);

    return status.move_to_pair();
}

flamestore_client::return_status flamestore_client::get_model_config(
        const flamestore_provider_handle& ph,
        const std::string& model_name) const {

    flamestore_status status = m_rpc_get_model_config
        .on(ph.m_provider_handle)(m_addr_str, model_name);

    return status.move_to_pair();
}

flamestore_client::return_status flamestore_client::get_optimizer_config(
        const flamestore_provider_handle& ph,
        const std::string& model_name) const {

    flamestore_status status = m_rpc_get_optimizer_config
        .on(ph.m_provider_handle)(m_addr_str, model_name);

    return status.move_to_pair();
}

flamestore_client::return_status flamestore_client::write_model_data(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& local_bulk) const {

    flamestore_status status = m_rpc_write_model_data
        .on(ph.m_provider_handle)(
            m_addr_str,
            model_name,
            model_signature,
            local_bulk);

    return status.move_to_pair();
}

flamestore_client::return_status flamestore_client::read_model_data(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& local_bulk) const {

    flamestore_status status = m_rpc_read_model_data
        .on(ph.m_provider_handle)(
            m_addr_str,
            model_name,
            model_signature,
            local_bulk);

    return status.move_to_pair();
}

flamestore_client::return_status flamestore_client::write_optimizer_data(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& optimizer_signature,
        const tl::bulk& local_bulk) const {

    flamestore_status status = m_rpc_write_optimizer_data
        .on(ph.m_provider_handle)(
            m_addr_str,
            model_name,
            optimizer_signature,
            local_bulk);

    return status.move_to_pair();
}

flamestore_client::return_status flamestore_client::read_optimizer_data(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& optimizer_signature,
        const tl::bulk& local_bulk) const {
    
    flamestore_status status = m_rpc_read_optimizer_data
        .on(ph.m_provider_handle)(
            m_addr_str,
            model_name,
            optimizer_signature,
            local_bulk);

    return status.move_to_pair();
}

tl::async_response flamestore_client::async_write_model_data(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& local_bulk) const {

    return m_rpc_write_model_data
        .on(ph.m_provider_handle).async(
            m_addr_str,
            model_name,
            model_signature,
            local_bulk);
}

tl::async_response flamestore_client::async_read_model_data(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& local_bulk) const {

    return m_rpc_read_model_data
        .on(ph.m_provider_handle).async(
            m_addr_str,
            model_name,
            model_signature,
            local_bulk);

}

tl::async_response flamestore_client::async_write_optimizer_data(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& optimizer_signature,
        const tl::bulk& local_bulk) const {

    return m_rpc_write_optimizer_data
        .on(ph.m_provider_handle).async(
            m_addr_str,
            model_name,
            optimizer_signature,
            local_bulk);
}

tl::async_response flamestore_client::async_read_optimizer_data(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& optimizer_signature,
        const tl::bulk& local_bulk) const {

    return m_rpc_read_optimizer_data
        .on(ph.m_provider_handle).async(
            m_addr_str,
            model_name,
            optimizer_signature,
            local_bulk);
}

flamestore_client::return_status flamestore_client::start_sync_model(
        const flamestore_provider_handle& ph,
        const std::string& model_name,
        const std::string& model_signature,
        const std::string& optimizer_signature,
        const tl::bulk& local_bulk) const {

    flamestore_status status = m_rpc_start_sync_model
        .on(ph.m_provider_handle)(
            m_addr_str,
            model_name,
            model_signature,
            optimizer_signature,
            local_bulk);

    return status.move_to_pair();
}

flamestore_client::return_status flamestore_client::stop_sync_model(
        const flamestore_provider_handle& ph,
        const std::string& model_name) const {

    flamestore_status status = m_rpc_stop_sync_model
        .on(ph.m_provider_handle)(m_addr_str, model_name);

    return status.move_to_pair();
}
