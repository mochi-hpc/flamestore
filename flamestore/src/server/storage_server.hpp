#ifndef __FLAMESTORE_STORAGE_SERVER_HPP
#define __FLAMESTORE_STORAGE_SERVER_HPP

#include <pybind11/pybind11.h>
#include <thallium.hpp>
#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <ssg.h>
#include <bake-server.hpp>
#include "common/common.hpp"
#include "common/status.hpp"
#include "server/server_context.hpp"
#include "server/backend.hpp"

namespace flamestore {

namespace py11 = pybind11;
namespace tl = thallium;

class StorageServer {

    tl::engine                      m_engine;
    std::unique_ptr<spdlog::logger> m_logger;
    bake::provider*                 m_bake_provider;
    ServerContext                   m_server_context;
    std::string                     m_workspace_path;
    ssg_group_id_t                  m_ssg_gid;
    ssg_member_id_t                 m_master_member_id;

    public:

    typedef std::unordered_map<std::string,std::string> backend_config_t;

    StorageServer(pymargo_instance_id mid,
           const std::string& workspace_path = ".",
           const std::string& backend_name = "master-memory",
           const std::string& logfile = "", int loglevel=2,
           const backend_config_t& backend_config = backend_config_t());

    ~StorageServer();

    private:

    void _init_bake(const std::string& target_path);

    void _init_ssg();

    void _finalize_ssg();

    static void _ssg_membership_update(void* arg,
            ssg_member_id_t member_id,
            ssg_member_update_type_t update_type);
};

}

#endif
