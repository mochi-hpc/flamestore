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
#include "common/common.hpp"
#include "common/status.hpp"
#include "server/server_context.hpp"
#include "server/provider.hpp"
#include "server/backend.hpp"

namespace flamestore {

namespace py11 = pybind11;
namespace tl = thallium;

class Server {

    tl::engine                      m_engine;
    std::unique_ptr<spdlog::logger> m_logger;
    std::unique_ptr<MasterProvider> m_provider;
    ServerContext                   m_server_context;

    public:

    typedef std::unordered_map<std::string,std::string> backend_config_t;

    Server(pymargo_instance_id mid,
           const std::string& backend_name = "memory",
           const std::string& logfile = "", int loglevel=2,
           const backend_config_t& backend_config = backend_config_t())
    : m_engine(CAPSULE2MID(mid)) {
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
        m_logger->info("Initializing MasterProvider at address {}", (std::string)(m_engine.self()));
        // Setting up the finalize callback
        m_engine.push_finalize_callback([this]() {
            m_logger->trace("Finalizing...");
            m_provider.reset();
            m_logger->trace("MasterProvider destroyed");
        });
        // Setting up the MasterProvider
        m_engine.enable_remote_shutdown();
        m_provider = std::make_unique<MasterProvider>(m_engine, m_logger.get());
        // Setting up the backend
        m_server_context.m_engine = &m_engine;
        m_server_context.m_logger = m_logger.get();
        m_logger->info("Setting up backend as \"{}\"", backend_name);
        m_provider->backend() = AbstractServerBackend::create(
                backend_name, m_server_context, backend_config, m_logger.get());
    }

    std::string get_connection_info() const {
        return m_engine.self();
    }
};

}

#endif
