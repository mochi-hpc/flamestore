#ifndef __FLAMESTORE_CLIENT_H
#define __FLAMESTORE_CLIENT_H

#include <iostream>
#include <mutex>
#include <map>
#include <thallium.hpp>
#include "common.hpp"
#include "status.hpp"

namespace py11 = pybind11;
namespace tl = thallium;

class flamestore_client;

class flamestore_provider_handle {
    
    friend class flamestore_client;

    private:

    std::shared_ptr<tl::engine> m_engine;
    tl::provider_handle         m_provider_handle;

    flamestore_provider_handle(std::shared_ptr<tl::engine> engine, const tl::provider_handle& ph)
    : m_engine(std::move(engine))
    , m_provider_handle(ph) {}

    public:

    flamestore_provider_handle()                                             = default;
    flamestore_provider_handle(const flamestore_provider_handle& other)            = default;
    flamestore_provider_handle(flamestore_provider_handle&& other)                 = default;
    flamestore_provider_handle& operator=(const flamestore_provider_handle& other) = default;
    flamestore_provider_handle& operator=(flamestore_provider_handle&& other)      = default;
    ~flamestore_provider_handle()                                            = default;

    void shutdown() const;

    std::string get_addr() const {
        return (std::string)m_provider_handle;
    }

    int16_t get_id() const {
        return m_provider_handle.provider_id();
    }
};

class flamestore_client {

    private:

    std::shared_ptr<tl::engine> m_engine;
    std::string                 m_addr_str; // address of this client
    tl::remote_procedure m_rpc_register_model;
    tl::remote_procedure m_rpc_get_model_config;
    tl::remote_procedure m_rpc_get_optimizer_config;
    tl::remote_procedure m_rpc_write_model_data;
    tl::remote_procedure m_rpc_read_model_data;
    tl::remote_procedure m_rpc_write_optimizer_data;
    tl::remote_procedure m_rpc_read_optimizer_data;
    tl::remote_procedure m_rpc_start_sync_model;
    tl::remote_procedure m_rpc_stop_sync_model;
    mutable std::unordered_map<std::string, tl::endpoint> m_endpoint_cache;

    public:

    using return_status = std::pair<int32_t, std::string>;

    flamestore_client(pymargo_instance_id mid)
    : m_engine(std::make_shared<tl::engine>(CAPSULE2MID(mid)))
    , m_addr_str(m_engine->self())
    , m_rpc_register_model(m_engine->define("flamestore_register_model"))
    , m_rpc_get_model_config(m_engine->define("flamestore_get_model_config"))
    , m_rpc_get_optimizer_config(m_engine->define("flamestore_get_optimizer_config"))
    , m_rpc_write_model_data(m_engine->define("flamestore_write_model_data"))
    , m_rpc_read_model_data(m_engine->define("flamestore_read_model_data"))
    , m_rpc_write_optimizer_data(m_engine->define("flamestore_write_optimizer_data"))
    , m_rpc_read_optimizer_data(m_engine->define("flamestore_read_optimizer_data"))
    , m_rpc_start_sync_model(m_engine->define("flamestore_start_sync_model"))
    , m_rpc_stop_sync_model(m_engine->define("flamestore_stop_sync_model"))
    { }

    tl::engine& engine() {
        return *m_engine;
    }

    std::string get_id() const {
        std::stringstream ss;
        ss << reinterpret_cast<intptr_t>(this);
        return ss.str();
    }

    static flamestore_client* from_id(const std::string& id) {
        intptr_t iid;
        std::istringstream ss(id);
        ss >> iid;
        return reinterpret_cast<flamestore_client*>(iid);
    }

    flamestore_provider_handle lookup(
            const std::string& address,
            uint16_t provider_id) const;

    return_status register_model(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& model_config,
            const std::string& optimizer_config,
            std::size_t model_data_size,
            std::size_t optimizer_data_size,
            const std::string& model_signature,
            const std::string& optimizer_signature) const;

    return_status get_model_config(
            const flamestore_provider_handle& ph,
            const std::string& model_name) const;

    return_status get_optimizer_config(
            const flamestore_provider_handle& ph,
            const std::string& model_name) const;

    return_status write_model_data(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& model_signature,
            const tl::bulk& local_bulk) const;

    return_status read_model_data(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& model_signature,
            const tl::bulk& local_bulk) const;

    return_status write_optimizer_data(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& optimizer_signature,
            const tl::bulk& local_bulk) const;

    return_status read_optimizer_data(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& optimizer_signature,
            const tl::bulk& local_bulk) const;

    tl::async_response async_write_model_data(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& model_signature,
            const tl::bulk& local_bulk) const;

    tl::async_response async_read_model_data(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& model_signature,
            const tl::bulk& local_bulk) const;

    tl::async_response async_write_optimizer_data(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& optimizer_signature,
            const tl::bulk& local_bulk) const;

    tl::async_response async_read_optimizer_data(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& optimizer_signature,
            const tl::bulk& local_bulk) const;

    return_status start_sync_model(
            const flamestore_provider_handle& ph,
            const std::string& model_name,
            const std::string& model_signature,
            const std::string& optimizer_signature,
            const tl::bulk& local_bulk) const;

    return_status stop_sync_model(
            const flamestore_provider_handle& ph,
            const std::string& model_name) const;

};

#endif
