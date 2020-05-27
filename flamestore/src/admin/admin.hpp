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

class Admin {

    private:

    std::shared_ptr<tl::engine> m_engine;
    std::string                 m_admin_addr;
    tl::remote_procedure        m_rpc_shutdown;
    tl::provider_handle         m_master_provider;

    public:

    using return_status = std::pair<int32_t, std::string>;

    Admin(pymargo_instance_id mid, const std::string& configfile);

    tl::engine& engine() {
        return *m_engine;
    }

    void cleanup_hg_resources() {
        m_master_provider = tl::provider_handle();
        m_engine.reset();
    }

    /**
     * @brief Shuts down FlameStore.
     */
    return_status shutdown();

};

}

#endif
