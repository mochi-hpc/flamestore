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

    std::unique_ptr<AbstractServerBackend> m_backend;

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
            const std::string& client_addr,
            const std::string& model_name,
            std::string& model_config,
            std::string& optimizer_config,
            std::size_t model_data_size,
            std::size_t optimizer_data_size,
            std::string& model_signature,
            std::string& optimizer_signature);

    /**
     * @brief RPC called when a client wants to retrieve the
     * configuration of a model.
     *
     * @param req
     * @param model_name
     */
    void on_reload_model(
            const tl::request& req,
            const std::string& client_addr,
            const std::string& model_name);

    /**
     * @brief RPC called when a client wants to write data to a model.
     *
     * @param req
     * @param model_name
     * @param remote_bulk
     */
    void on_write_model_data(
            const tl::request& req,
            const std::string& client_addr,
            const std::string& model_name,
            const std::string& model_signature,
            tl::bulk& remote_bulk);

    /**
     * @brief RPC called when a client wants to read a model.
     *
     * @param req
     * @param model_name
     */
    void on_read_model_data(
            const tl::request& req,
            const std::string& client_addr,
            const std::string& model_name,
            const std::string& model_signature,
            tl::bulk& remote_bulk);

    public:

    /**
     * @brief Constructor.
     *
     * @param engine Thallium engine
     * @param provider_id provider id
     */
    MasterProvider(tl::engine& engine, uint16_t provider_id = 0)
    : tl::provider<MasterProvider>(engine, provider_id) {
        define("flamestore_register_model",   &MasterProvider::on_register_model);
        define("flamestore_reload_model",     &MasterProvider::on_reload_model);
        define("flamestore_write_model_data", &MasterProvider::on_write_model_data);
        define("flamestore_read_model_data",  &MasterProvider::on_read_model_data);
    }

    ~MasterProvider() = default;

    inline std::unique_ptr<AbstractServerBackend>& backend() {
        return m_backend;
    }

    inline const std::unique_ptr<AbstractServerBackend>& backend() const {
        return m_backend;
    }
};

}

#endif
