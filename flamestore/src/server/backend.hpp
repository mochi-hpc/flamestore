#ifndef __FLAMESTORE_BACKEND
#define __FLAMESTORE_BACKEND

#include <iostream>
#include <string>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <thallium.hpp>
#include "common/status.hpp"
#include "server/server_context.hpp"

namespace flamestore {

namespace tl = thallium;

template<typename T>
class FactoryRegistration;

class AbstractServerBackend {

    public:

        using config_type = std::unordered_map<std::string, std::string>;

    private:

        template<typename T>
        friend class FactoryRegistration;

        static std::unordered_map<
            std::string,
            std::function<
                std::unique_ptr<AbstractServerBackend>(const ServerContext&, const config_type&)>> s_backend_factories;
    
    protected:

        AbstractServerBackend() = default;

    public:


        AbstractServerBackend(const AbstractServerBackend&)            = delete;
        AbstractServerBackend(AbstractServerBackend&&)                 = delete;
        AbstractServerBackend& operator=(const AbstractServerBackend&) = delete;
        AbstractServerBackend& operator=(AbstractServerBackend&&)      = delete;
        virtual ~AbstractServerBackend()                            = default;

        static std::unique_ptr<AbstractServerBackend> create(
                const std::string& name,
                const ServerContext& ctx,
                const config_type& config,
                spdlog::logger* logger)
        {
            auto factory = s_backend_factories.find(name);
            if(factory == s_backend_factories.end()) {
                logger->critical("Could not find factory for backend {}", name);
                return std::unique_ptr<AbstractServerBackend>(nullptr);
            } else {
                logger->info("Creating backend {}", name);
                return factory->second(ctx, config);
            }
        }

        virtual void register_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_config,
                std::size_t& model_size,
                const std::string& model_signature) = 0;

        virtual void reload_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name) = 0;

        virtual void write_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_signature,
                const tl::bulk& remote_bulk,
                const std::size_t& size) = 0;

        virtual void read_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_signature,
                const tl::bulk& remote_bulk,
                const std::size_t& size) = 0;

        virtual void duplicate_model(
                const tl::request& req,
                const std::string& model_name,
                const std::string& new_model_name) = 0;

        virtual void on_shutdown() {}

        virtual void on_worker_joined(
                uint64_t member_id,
                hg_addr_t addr) {}

        virtual void on_worker_left(
                uint64_t member_id) {}

        virtual void on_worker_died(
                uint64_t member_id) {}
};

template<typename T>
class FactoryRegistration {

    public:

    FactoryRegistration(const std::string& name) {
        AbstractServerBackend::s_backend_factories[name] =
            [](const ServerContext& ctx, const AbstractServerBackend::config_type& config) {
                return std::make_unique<T>(ctx, config);
            };
    }
};

}

#define REGISTER_FLAMESTORE_BACKEND(__name_str__, __type__) \
    static FactoryRegistration<__type__> __registration(__name_str__)

#endif
