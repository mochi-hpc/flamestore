#ifndef __FLAMESTORE_BACKEND
#define __FLAMESTORE_BACKEND

#include <iostream>
#include <string>
#include <unordered_map>
#include <thallium.hpp>
#include "status.hpp"
#include "server_context.hpp"

template<typename T>
class flamestore_factory_registration;

namespace tl = thallium;

class flamestore_backend {

    public:

        using config_type = std::unordered_map<std::string, std::string>;

    private:

        template<typename T>
        friend class flamestore_factory_registration;

        static std::unordered_map<
            std::string,
            std::function<
                std::unique_ptr<flamestore_backend>(const flamestore_server_context&, const config_type&)>> s_backend_factories;
    
    protected:

        flamestore_backend() = default;

    public:


        flamestore_backend(const flamestore_backend&)            = delete;
        flamestore_backend(flamestore_backend&&)                 = delete;
        flamestore_backend& operator=(const flamestore_backend&) = delete;
        flamestore_backend& operator=(flamestore_backend&&)      = delete;
        virtual ~flamestore_backend()                            = default;

        static std::unique_ptr<flamestore_backend> create(
                const std::string& name,
                const flamestore_server_context& ctx,
                const config_type& config)
        {
            auto factory = s_backend_factories.find(name);
            if(factory == s_backend_factories.end()) {
                return std::unique_ptr<flamestore_backend>(nullptr);
            } else {
                return factory->second(ctx, config);
            }
        }

        virtual void register_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_config,
                const std::string& optimizer_config,
                std::size_t model_data_size,
                std::size_t optimizer_data_size,
                const std::string& model_signature,
                const std::string& optimizer_signature) = 0;

        virtual void get_model_config(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name) = 0;

        virtual void get_optimizer_config(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name) = 0;

        virtual void write_model_data(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_signature,
                const tl::bulk& remote_bulk) = 0;

        virtual void read_model_data(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_signature,
                const tl::bulk& remote_bulk) = 0;

        virtual void write_optimizer_data(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& optimizer_signature,
                tl::bulk& remote_bulk) = 0;

        virtual void read_optimizer_data(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& optimizer_signature,
                tl::bulk& remote_bulk) = 0;
};

template<typename T>
class flamestore_factory_registration {

    public:

        flamestore_factory_registration(const std::string& name) {
            flamestore_backend::s_backend_factories[name] = [](const flamestore_server_context& ctx, const flamestore_backend::config_type& config) {
                return std::make_unique<T>(ctx, config);
            };
        }
};

#define REGISTER_FLAMESTORE_BACKEND(__name_str__, __type__) \
    static flamestore_factory_registration<__type__> __registration(__name_str__)

#endif
