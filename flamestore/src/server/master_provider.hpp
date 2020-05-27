#ifndef __FLAMESTORE_PROVIDER_HPP
#define __FLAMESTORE_PROVIDER_HPP

#include <pybind11/pybind11.h>
#include <thallium.hpp>
#include <iostream>
#include <mutex>
#include <map>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "common/common.hpp"
#include "common/status.hpp"
#include "server/model.hpp"
#include "server/backend.hpp"

namespace flamestore {

namespace py11 = pybind11;
namespace tl = thallium;
using name_t = std::string;
using lock_guard_t = std::lock_guard<tl::mutex>;

/**
 * @brief FlameStore Provider.
 */
class MasterProvider : public tl::provider<MasterProvider> {

    private:

    spdlog::logger* m_logger = nullptr;
    std::unique_ptr<AbstractServerBackend> m_backend;


    void on_shutdown(const tl::request& req)
    {
        m_logger->debug("Received a request to shut down");
        if(m_backend) {
            m_backend->on_shutdown();
        }
        get_engine().finalize();
        req.respond(Status::OK());
    }

    /**
     * @brief RPC called when a client registers a model.
     *
     * @param req Thallium request
     * @param client_addr Address of the client
     * @param name Model name
     * @param config Model configuration (architecture + optimizer)
     * @param signature Model signature for consistency checking
     */
    void on_register_model(
            const tl::request& req,
            const std::string& client_addr,
            const std::string& name,
            std::string& config,
            std::size_t& size,
            std::string& signature)
    {
        m_logger->debug("Registering model {} from client {}", name, client_addr);
        if(m_backend) {
            m_backend->register_model(req, client_addr, name, config, size, signature);
        } else {
            m_logger->error("No backend found!");
            req.respond(Status(FLAMESTORE_EBACKEND, "No FlameStore backend found"));
        }
    }

    /**
     * @brief RPC called when a client wants to retrieve the
     * configuration of a model.
     *
     * @param req Thallium request
     * @param client_addr Address of the client
     * @param name Model name
     */
    void on_reload_model(
            const tl::request& req,
            const std::string& client_addr,
            const std::string& name)
    {
        m_logger->debug("Reloading model {} to client {}", name, client_addr);
        if(m_backend) {
            m_backend->reload_model(req, client_addr, name);
        } else {
            m_logger->error("No backend found!");
            req.respond(Status(FLAMESTORE_EBACKEND, "No FlameStore backend found"));
        }
    }

    /**
     * @brief RPC called when a client wants to write data to a model.
     *
     * @param req Thallium request
     * @param client_addr Address of the client
     * @param name Name of the model
     * @param signature Signature of the model
     * @param remote_bulk Bulk handle pointing to the model's memory
     * @param size Size of the model data, in bytes
     */
    void on_write_model_data(
            const tl::request& req,
            const std::string& client_addr,
            const std::string& name,
            const std::string& signature,
            tl::bulk& remote_bulk,
            const std::size_t& size)
    {
        m_logger->debug("Writing model data for model {} from client {}", name, client_addr);
        if(m_backend) {
            m_backend->write_model(req, client_addr, name, signature, remote_bulk, size);
        } else {
            m_logger->error("No backend found!");
            req.respond(Status(FLAMESTORE_EBACKEND, "No FlameStore backend found"));
        }
    }

    /**
     * @brief RPC called when a client wants to read a model.
     *
     * @param req Thallium request
     * @param client_addr Address of the client
     * @param name Name of the model
     * @param signature Signature of the model
     * @param remote_bulk Bulk handle pointing to the model's memory
     * @param size Size of the model data, in bytes
     */
    void on_read_model_data(
            const tl::request& req,
            const std::string& client_addr,
            const std::string& name,
            const std::string& signature,
            tl::bulk& remote_bulk,
            const std::size_t& size)
    {
        m_logger->debug("Reading model data for model {} requested by client {}", name, client_addr);
        if(m_backend) {
            m_backend->read_model(req, client_addr, name, signature, remote_bulk, size);
        } else {
            m_logger->error("No backend found!");
            req.respond(Status(FLAMESTORE_EBACKEND, "No FlameStore backend found"));
        }
    }

    /**
     * @brief RPC called when a client duplicates a model.
     *
     * @param req Thallium request
     * @param name Model name
     * @param new_name Duplicated model name
     */
    void on_duplicate_model(
            const tl::request& req,
            const std::string& name,
            const std::string& new_name)
    {
        m_logger->debug("Duplicating model {} as {}", name, new_name);
        if(m_backend) {
            m_backend->duplicate_model(req, name, new_name);
        } else {
            m_logger->error("No backend found!");
            req.respond(Status(FLAMESTORE_EBACKEND, "No FlameStore backend found"));
        }
    }

    public:

    /**
     * @brief Constructor.
     *
     * @param engine Thallium engine
     * @param logger Logger
     * @param provider_id provider id
     */
    MasterProvider(tl::engine& engine, spdlog::logger* logger, uint16_t provider_id = 0)
    : tl::provider<MasterProvider>(engine, provider_id)
    , m_logger(logger) {
        m_logger->debug("Registering RPCs on MasterProvider with provider id {}", provider_id);
        define("flamestore_shutdown",         &MasterProvider::on_shutdown);
        define("flamestore_register_model",   &MasterProvider::on_register_model);
        define("flamestore_reload_model",     &MasterProvider::on_reload_model);
        define("flamestore_write_model_data", &MasterProvider::on_write_model_data);
        define("flamestore_read_model_data",  &MasterProvider::on_read_model_data);
        define("flamestore_dup_model",        &MasterProvider::on_duplicate_model);
        m_logger->debug("RPCs registered");
    }

    /**
     * @brief Destructor.
     */
    ~MasterProvider() {
        m_logger->debug("Destroying MasterProvider");
    }

    inline std::unique_ptr<AbstractServerBackend>& backend() {
        return m_backend;
    }

    inline const std::unique_ptr<AbstractServerBackend>& backend() const {
        return m_backend;
    }
};

}

#endif
