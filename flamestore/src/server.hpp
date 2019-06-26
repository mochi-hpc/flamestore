#ifndef __FLAMESTORE_SERVER_HPP
#define __FLAMESTORE_SERVER_HPP

#include <pybind11/pybind11.h>
#include <thallium.hpp>
#include <iostream>
#include <mutex>
#include <map>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "common.hpp"
#include "model.hpp"
#include "status.hpp"
#include "backend.hpp"

namespace py11 = pybind11;
namespace np = py11;
namespace tl = thallium;
using name_t = std::string;
using lock_guard_t = std::lock_guard<tl::mutex>;

/**
 * @brief This class is here to create and wrap a tl::engine
 * from a pymargo_instance_id. The flamestore_provider indeed cannot
 * be initialized directly with a pymargo_instance_id.
 */
class flamestore_engine_wrapper {

    private:

    tl::engine m_engine;

    public:

    /**
     * @brief Creates a flamestore_engine_wrapper from a margo instance
     * (useful when using py-margo).
     *
     * @param mid Margo instance.
     */
    flamestore_engine_wrapper(pymargo_instance_id mid)
    : m_engine(CAPSULE2MID(mid)) {
        m_engine.enable_remote_shutdown();
    }

    /**
     * @brief Returns the tl::engine this server uses.
     *
     * @return the engine this server uses.
     */
    tl::engine& engine() {
        return m_engine;
    }
};

/**
 * @brief FlameStore Provider.
 */
class flamestore_provider : public tl::provider<flamestore_provider> {

    private:

    flamestore_engine_wrapper&            m_server;
    flamestore_server_context             m_server_context;
    std::unique_ptr<spdlog::logger> m_logger;
    std::unique_ptr<flamestore_backend>   m_backend;

    /**
     * @brief RPC called when a client registers a model.
     *
     * @param req
     * @param model_name
     * @param model_config
     * @param optimizer_config
     */
    void on_register_model(
            const tl::request& req,
            const std::string& model_name,
            std::string& model_config,
            std::string& optimizer_config,
            std::size_t model_data_size,
            std::size_t optimizer_data_size,
            std::string& model_signature,
            std::string& optimizer_signature)
    {
        m_logger->trace("Entering flamestore_provider::on_register_model for model \"{}\"", model_name);
        m_backend->register_model(
            req,
            model_name,
            model_config,
            optimizer_config,
            model_data_size,
            optimizer_data_size,
            model_signature,
            optimizer_signature);
        m_logger->trace("Leaving flamestore_provider::on_register_model");
    }

    /**
     * @brief RPC called when a client wants to retrieve the
     * configuration of a model.
     *
     * @param req
     * @param model_name
     */
    void on_get_model_config(
            const tl::request& req,
            const std::string& model_name) {
        m_logger->trace("Entering flamestore_provider::on_get_model_config for model \"{}\"", model_name);
        m_backend->get_model_config(req, model_name);
        m_logger->trace("Leaving flamestore_provider::on_get_model_config");
    }

    /**
     * @brief RPC called when a client wants to retrieve the
     * configuration of an optimizer of a model.
     *
     * @param req
     * @param model_name
     */
    void on_get_optimizer_config(
            const tl::request& req,
            const std::string& model_name) {
        m_logger->trace("Entering flamestore_provider::on_get_optimizer_config for model \"{}\"", model_name);
        m_backend->get_optimizer_config(req, model_name);
        m_logger->trace("Leaving flamestore_provider::on_get_optimizer_config");
    }

    /**
     * @brief RPC called when a client wants to write data to a model.
     *
     * @param req
     * @param model_name
     * @param remote_bulk
     */
    void on_write_model_data(
            const tl::request& req,
            const std::string& model_name,
            const std::string& model_signature,
            tl::bulk& remote_bulk) {
        m_logger->trace("Entering flamestore_provider::on_write_model_data for model \"{}\"", model_name);
        m_backend->write_model_data(req, model_name, model_signature, remote_bulk);
        m_logger->trace("Leaving flamestore_provider::on_write_model_data");
    }

    /**
     * @brief RPC called when a client wants to read a model.
     *
     * @param req
     * @param model_name
     */
    void on_read_model_data(
            const tl::request& req,
            const std::string& model_name,
            const std::string& model_signature,
            tl::bulk& remote_bulk) {
        m_logger->trace("Entering flamestore_provider::on_read_model_data for model \"{}\"", model_name);
        m_backend->read_model_data(req, model_name, model_signature, remote_bulk);
        m_logger->trace("Leaving flamestore_provider::on_read_model_data");
    }

    /**
     * @brief RPC called when a client wants to write data to a model's optimizer.
     *
     * @param req
     * @param model_name
     * @param remote_bulk
     */
    void on_write_optimizer_data(
            const tl::request& req,
            const std::string& model_name,
            const std::string& optimizer_signature,
            tl::bulk& remote_bulk) {
        m_logger->trace("Entering flamestore_provider::on_write_optimizer_data for model \"{}\"", model_name);
        m_backend->write_optimizer_data(req, model_name, optimizer_signature, remote_bulk);
        m_logger->trace("Leaving flamestore_provider::on_write_optimizer_data");
    }

    /**
     * @brief RPC called when a client wants to read a model.
     *
     * @param req
     * @param model_name
     */
    void on_read_optimizer_data(
            const tl::request& req,
            const std::string& model_name,
            const std::string& optimizer_signature,
            tl::bulk& remote_bulk) {
        m_logger->trace("Entering flamestore_provider::on_read_optimizer_data for model \"{}\"", model_name);
        m_backend->read_optimizer_data(req, model_name, optimizer_signature, remote_bulk);
        m_logger->trace("Leaving flamestore_provider::on_read_optimizer_data");
    }

    /**
     * @brief RPC called to start auto-synchronizing the model.
     *
     * @param req
     * @param model_name
     */
    void on_start_sync_model(
            const tl::request& req,
            const std::string& model_name,
            const std::string& model_signature,
            const std::string& optimizer_signature) {
        // TODO
        req.respond(flamestore_status::OK());
    }

    /**
     * @brief RPC called to stop auto-synchronizing the model.
     *
     * @param req
     * @param model_name
     */
    void on_stop_sync_model(
            const tl::request& req,
            const std::string& model_name) {
        // TODO
        req.respond(flamestore_status::OK());
    }

    public:

    /**
     * @brief Constructor.
     *
     * @param engine flamestore server wrapper
     * @param provider_id provider id
     */
    flamestore_provider(flamestore_engine_wrapper& server, uint16_t provider_id,
                  const std::string& backend = "memory",
                  const std::string& logfile = "", int loglevel=2)
    : tl::provider<flamestore_provider>(server.engine(), provider_id)
    , m_server(server) {
        std::stringstream loggername;
        loggername << "flamestore_provider:" << provider_id;
        if(logfile.size() != 0) {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile, true);
            m_logger = std::make_unique<spdlog::logger>(loggername.str(), file_sink);
        } else {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            m_logger = std::make_unique<spdlog::logger>(loggername.str(), console_sink);
        }
        m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v");
        m_logger->set_level(static_cast<spdlog::level::level_enum>(loglevel));
        m_logger->info("Initializing provider with id {} at address {}", provider_id, (std::string)(get_engine().self()));

        m_server_context.m_logger = m_logger.get();
        m_server_context.m_engine = &get_engine();

        define("flamestore_register_model",       &flamestore_provider::on_register_model);
        define("flamestore_get_model_config",     &flamestore_provider::on_get_model_config);
        define("flamestore_get_optimizer_config", &flamestore_provider::on_get_optimizer_config);
        define("flamestore_write_model_data",     &flamestore_provider::on_write_model_data);
        define("flamestore_read_model_data",      &flamestore_provider::on_read_model_data);
        define("flamestore_write_optimizer_data", &flamestore_provider::on_write_optimizer_data);
        define("flamestore_read_optimizer_data",  &flamestore_provider::on_read_optimizer_data);
        define("flamestore_start_sync_model",     &flamestore_provider::on_start_sync_model);
        define("flamestore_stop_sync_model",      &flamestore_provider::on_stop_sync_model);
        get_engine().on_finalize([this]() {
                    m_logger->trace("Calling finalize callback");
                    m_backend.reset();
                });
        m_logger->trace("Initializing {} backend", backend);
        flamestore_backend::config_type config;
        m_backend = flamestore_backend::create(backend, m_server_context, config);
        m_logger->trace("Done initializing provider");
    }

    ~flamestore_provider() {
        m_logger->info("Provider is terminated");
    }

    std::pair<std::string, uint16_t> get_connection_info() const {
        return std::make_pair<std::string, uint16_t>(
                get_engine().self(), get_provider_id());
    }

};

#endif
