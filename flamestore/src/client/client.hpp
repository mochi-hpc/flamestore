#ifndef __FLAMESTORE_CLIENT_H
#define __FLAMESTORE_CLIENT_H

#include <iostream>
#include <mutex>
#include <map>
#include <thallium.hpp>
#include "common/common.hpp"
#include "common/status.hpp"

namespace py11 = pybind11;
namespace tl = thallium;

namespace flamestore {

class Client;

class ProviderHandle {
    
    friend class Client;

    private:

    std::shared_ptr<tl::engine> m_engine;
    tl::provider_handle         m_provider_handle;

    public:

    ProviderHandle(std::shared_ptr<tl::engine> engine, const tl::provider_handle& ph)
    : m_engine(std::move(engine))
    , m_provider_handle(ph) {}

    ProviderHandle()                                       = default;
    ProviderHandle(const ProviderHandle& other)            = default;
    ProviderHandle(ProviderHandle&& other)                 = default;
    ProviderHandle& operator=(const ProviderHandle& other) = default;
    ProviderHandle& operator=(ProviderHandle&& other)      = default;
    ~ProviderHandle()                                      = default;

    void shutdown() const;

    std::string get_addr() const {
        return (std::string)m_provider_handle;
    }

    int16_t get_id() const {
        return m_provider_handle.provider_id();
    }
};

class Client {

    private:

    std::shared_ptr<tl::engine> m_engine;
    std::string                 m_addr_str; // address of this client
    tl::remote_procedure m_rpc_register_model;
    tl::remote_procedure m_rpc_reload_model;
    ProviderHandle m_metadata_provider_handle;
    mutable std::string some_model_config;

    public:

    using return_status = std::pair<int32_t, std::string>;

    Client(pymargo_instance_id mid, const std::string& configfile);

    tl::engine& engine() {
        return *m_engine;
    }

    std::string get_id() const {
        std::stringstream ss;
        ss << reinterpret_cast<intptr_t>(this);
        return ss.str();
    }

    static Client* from_id(const std::string& id) {
        intptr_t iid;
        std::istringstream ss(id);
        ss >> iid;
        return reinterpret_cast<Client*>(iid);
    }

    return_status register_model(
            const std::string& model_name,
            const std::string& model_config,
            std::size_t model_data_size,
            const std::string& model_signature) const;

    return_status reload_model(
            const std::string& model_name,
            bool include_optimizer) const;
};

}

#endif
