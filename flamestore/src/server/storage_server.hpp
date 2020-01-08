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
    std::unique_ptr<MasterProvider> m_provider;
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
           const backend_config_t& backend_config = backend_config_t())
    : m_engine(CAPSULE2MID(mid))
    , m_workspace_path(workspace_path) {
        // Setting up logging
        if(logfile.size() != 0) {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile, true);
            m_logger = std::make_unique<spdlog::logger>("FlameStore", file_sink);
        } else {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            m_logger = std::make_unique<spdlog::logger>("FlameStore", console_sink);
        }
        m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v");
        m_logger->set_level(static_cast<spdlog::level::level_enum>(loglevel));
        m_logger->info("Initializing StorageProvider at address {}", (std::string)(m_engine.self()));
        m_logger->info("Workspace is {}", m_workspace_path);
        // Initialize Bake
        auto it = backend_config.find("storage-path");
        if(it == backend_config.end()) {
            m_logger->critical("Path not provided for Bake target");
            throw std::runtime_error("Path not provided for Bake target");
        }
        std::string target_path = it->second;
        _init_bake(target_path);
        // Initializing SSG
        _init_ssg();
        // Setting up the finalize callbacks
        m_engine.push_prefinalize_callback([this]() {
            _finalize_ssg();
        });
        m_engine.push_finalize_callback([this]() {
            m_logger->trace("Finalizing...");
            m_provider.reset();
            m_logger->trace("MasterProvider destroyed");
        });
        m_engine.enable_remote_shutdown();
    }

    ~StorageServer() {
        m_logger->debug("Destroying StorageServer instance");
    }

    private:

    void _init_bake(const std::string& target_path) {
        // Initializing Bake provider
        m_logger->info("Initializing Bake with target {}", target_path);
        try {
            m_bake_provider = bake::provider::create(m_engine.get_margo_instance());
        } catch(const bake::exception& ex) {
            m_logger->critical("Could not create Bake provider (Bake exception: {})", ex.what());
            throw std::runtime_error("Could not create Bake provider");
        }
        m_logger->debug("Bake provider correctly created");
        try {
            m_bake_provider->add_storage_target(target_path);
        } catch(const bake::exception& ex) {
            m_logger->critical("Could not add Bake storage target (Bake exception: {})", ex.what());
            throw std::runtime_error("Could not add Bake storage target");
        }
        m_logger->debug("Bake target correctly added to provider");
    }

    void _init_ssg() {
        // Checking that the SSG file exists
        std::string filename = m_workspace_path + "/.flamestore/group.ssg";
        {
            std::ifstream f(filename.c_str());
            if(!f.good()) {
                m_logger->critical("Could not open SSG file {}", filename);
                throw std::runtime_error("Could not open file "+filename);
            }
        }
        // Initialize ssg
        m_logger->debug("Initializing SSG");
        int ret = ssg_init();
        if(ret != 0) {
            m_logger->critical("Could not initialize SSG (ssg_init returned error code {})", ret);
            throw std::runtime_error("Could not initialize SSG");
        }
        // Opening SSG group
        int num_addrs = 128;
        ret = ssg_group_id_load(filename.c_str(), &num_addrs, &m_ssg_gid);
        if(ret != 0) {
            m_logger->critical("ssg_group_id_load failed with error code {}", ret);
            throw std::runtime_error("Could not load SSG group file");
        }
        // Join the group
        ret = ssg_group_join(m_engine.get_margo_instance(), m_ssg_gid,
                _ssg_membership_update, (void*)this);
        if(ret != 0) {
            m_logger->critical("Could not join SSG group (ssg_group_join returned {})", ret);
            throw std::runtime_error("Could join SSG group");
        }
        // Get the member id of the master
        m_master_member_id = ssg_get_group_member_id_from_rank(m_ssg_gid, 0);
        m_logger->debug("Master's SSG member id is {}", m_master_member_id);
    }

    void _finalize_ssg() {
        m_logger->debug("Leaving SSG group");
        int ret = ssg_group_leave(m_ssg_gid);
        if(ret != 0) {
            m_logger->error("SSG could not leave group (ssg_group_leave returned error code {})", ret);
        }
        m_logger->debug("Finalizing SSG");
        ret = ssg_finalize();
        if(ret != 0) {
            m_logger->error("SSG could not be finalized (ssg_finalize returned error code {})", ret);
        }
        m_logger->debug("SSG finalized");
    }

    static void _ssg_membership_update(void* arg,
            ssg_member_id_t member_id,
            ssg_member_update_type_t update_type) {
        StorageServer* server = static_cast<StorageServer*>(arg);
        server->m_logger->debug("Entering _ssg_membership_update");
        if(update_type == SSG_MEMBER_LEFT
        || update_type == SSG_MEMBER_DIED) {
            if(member_id == server->m_master_member_id) {
                server->m_logger->error("Master abandoned its workers, shutting down, good bye cruel world");
                // the reason we 
                tl::xstream::self().make_thread([server]() {
                        server->m_logger->debug("Finalizing engine");
                        server->m_engine.finalize();
                }, tl::anonymous());
                server = nullptr;
            }
        }
        // After the above, if the server may have been finalized, we have to
        // be careful not to use it anymore. We therefore check if it is null.
        if(server)
            server->m_logger->debug("Leaving _ssg_membership_update");
    }
};

}

#endif
