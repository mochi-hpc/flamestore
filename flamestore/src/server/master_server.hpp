#ifndef __FLAMESTORE_SERVER_HPP
#define __FLAMESTORE_SERVER_HPP

#include <pybind11/pybind11.h>
#include <thallium.hpp>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <ssg.h>
#include "common/common.hpp"
#include "common/status.hpp"
#include "server/server_context.hpp"
#include "server/master_provider.hpp"
#include "server/backend.hpp"

namespace flamestore {

namespace py11 = pybind11;
namespace tl = thallium;

class MasterServer {

    tl::engine                      m_engine;
    std::unique_ptr<spdlog::logger> m_logger;
    std::unique_ptr<MasterProvider> m_provider;
    ServerContext                   m_server_context;
    std::string                     m_workspace_path;
    ssg_group_id_t                  m_ssg_gid;

    public:

    typedef std::unordered_map<std::string,std::string> backend_config_t;

    MasterServer(pymargo_instance_id mid,
           const std::string& workspace_path = ".",
           const std::string& backend_name = "master-memory",
           const std::string& logfile = "", int loglevel=2,
           const backend_config_t& backend_config = backend_config_t());

    std::string get_connection_info() const;

    ~MasterServer();

    private:

    void _init_ssg();

    void _ssg_finalize();

    static void _ssg_membership_update(void* arg,
            ssg_member_id_t member_id,
            ssg_member_update_type_t update_type);
};

}

#endif
