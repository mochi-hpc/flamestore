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

class Client {

    private:

    struct CachedBulk {
        std::vector<char> m_buffer;
        tl::bulk          m_bulk;
    };

    std::shared_ptr<tl::engine> m_engine;
    std::string                 m_client_addr;
    tl::remote_procedure        m_rpc_shutdown;
    tl::remote_procedure        m_rpc_register_model;
    tl::remote_procedure        m_rpc_reload_model;
    tl::remote_procedure        m_rpc_write_model;
    tl::remote_procedure        m_rpc_read_model;
    tl::remote_procedure        m_rpc_dup_model;
    tl::provider_handle         m_master_provider;
    std::unordered_map<std::string, CachedBulk> m_cache;

    public:

    using return_status = std::pair<int32_t, std::string>;

    Client(pymargo_instance_id mid, const std::string& configfile);

    tl::engine& engine() {
        return *m_engine;
    }

    void cleanup_hg_resources() {
        m_master_provider = tl::provider_handle();
        m_engine.reset();
    }

    /**
     * @brief This function is exposed to Python.
     */
    std::string get_id() const {
        std::stringstream ss;
        ss << reinterpret_cast<intptr_t>(this);
        return ss.str();
    }

    /**
     * @brief This function is used by TMCI.
     */
    static Client* from_id(const std::string& id) {
        intptr_t iid;
        std::istringstream ss(id);
        ss >> iid;
        return reinterpret_cast<Client*>(iid);
    }

    /**
     * @brief Shuts down FlameStore.
     */
    return_status shutdown();

    /**
     * @brief This function is exposed to Python.
     */
    return_status register_model(
            const std::string& model_name,
            const std::string& model_config,
            std::size_t model_data_size,
            const std::string& model_signature);

    /**
     * @brief This function is exposed to Python.
     */
    return_status reload_model(
            const std::string& model_name);

    /**
     * @brief This function is exposed to Python.
     */
    return_status duplicate_model(
            const std::string& model_name,
            const std::string& new_model_name);

    /**
     * This function is used by TMCI.
     */
    return_status write_model_data(
            const std::string& model_name,
            const std::string& signature,
            std::vector<std::pair<void*,size_t>>& memory,
            const std::size_t& size);

    /**
     * This function is used by TMCI.
     */
    return_status read_model_data(
            const std::string& model_name,
            const std::string& signature,
            std::vector<std::pair<void*,size_t>>& memory,
            const std::size_t& size);
};

}

#endif
