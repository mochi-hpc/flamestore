#include <mutex>
#include <map>
#include <spdlog/spdlog.h>
#include "model.hpp"
#include "backend.hpp"

namespace tl = thallium;

class flamestore_memory_backend : public flamestore_backend {

    public:

        using model_t = flamestore_model<std::vector<char>>;
        using name_t = std::string;
        using lock_guard_t = std::lock_guard<tl::mutex>;

    private:

        tl::engine*                                   m_engine;
        spdlog::logger*                               m_logger;
        mutable tl::rwlock                            m_models_rwlock;
        std::map<name_t, std::unique_ptr<model_t>>    m_models;


        /**
         * @brief Finds a model with the provided name in the map.
         * If the model doesn't exist, returns nullptr.
         *
         * @param model_name Name of the model.
         *
         * @return pointer to the model.
         */
        inline model_t* _find_model(const std::string& model_name) const {
            m_models_rwlock.rdlock();
            auto it = m_models.find(model_name);
            if(it == m_models.end()) {
                m_models_rwlock.unlock();
                return nullptr;
            } else  {
                auto result = it->second.get();
                m_models_rwlock.unlock();
                return result;
            }
        }

        /**
         * @brief Finds a model with the provided name in the map,
         * or create it if it did not exist before.
         *
         * @param model_name Name of the model.
         * @param created whether the model has been created or not.
         *
         * @return pointer to the model.
         */
        inline model_t* _find_or_create_model(const std::string& model_name, bool& created) {
            m_models_rwlock.wrlock();
            auto it = m_models.find(model_name);
            if(it == m_models.end()) {
                auto model = std::make_unique<model_t>();
                auto result = model.get();
                model->m_name = model_name;
                m_models.emplace(model_name, std::move(model));
                m_models_rwlock.unlock();
                created = true;
                return result;
            } else  {
                auto result = it->second.get();
                m_models_rwlock.unlock();
                created = false;
                return result;
            }
        }

    public:

        flamestore_memory_backend(const flamestore_server_context& ctx, const flamestore_backend::config_type& config)
        : m_engine(ctx.m_engine)
        , m_logger(ctx.m_logger) {
            m_logger->info("Initializing memory backend");
        }

        flamestore_memory_backend(const flamestore_backend&)            = delete;
        flamestore_memory_backend(flamestore_backend&&)                 = delete;
        flamestore_memory_backend& operator=(const flamestore_backend&) = delete;
        flamestore_memory_backend& operator=(flamestore_backend&&)      = delete;
        ~flamestore_memory_backend()                              = default;

        virtual void register_model(
                const tl::request& req,
                const std::string& model_name,
                const std::string& model_config,
                const std::string& optimizer_config,
                std::size_t model_data_size,
                std::size_t optimizer_data_size,
                const std::string& model_signature,
                const std::string& optimizer_signature) override;

        virtual void get_model_config(
                const tl::request& req,
                const std::string& model_name) override;

        virtual void get_optimizer_config(
                const tl::request& req,
                const std::string& model_name) override;

        virtual void write_model_data(
                const tl::request& req,
                const std::string& model_name,
                const std::string& model_signature,
                const tl::bulk& remote_bulk) override;

        virtual void read_model_data(
                const tl::request& req,
                const std::string& model_name,
                const std::string& model_signature,
                const tl::bulk& remote_bulk) override;

        virtual void write_optimizer_data(
                const tl::request& req,
                const std::string& model_name,
                const std::string& optimizer_signature,
                tl::bulk& remote_bulk) override;

        virtual void read_optimizer_data(
                const tl::request& req,
                const std::string& model_name,
                const std::string& optimizer_signature,
                tl::bulk& remote_bulk) override;
};

REGISTER_FLAMESTORE_BACKEND("memory",flamestore_memory_backend);
        
void flamestore_memory_backend::register_model(
        const tl::request& req,
        const std::string& model_name,
        const std::string& model_config,
        const std::string& optimizer_config,
        std::size_t model_data_size,
        std::size_t optimizer_data_size,
        const std::string& model_signature,
        const std::string& optimizer_signature)
{
    bool created = false;
    auto model = _find_or_create_model(model_name, created);
    if(not created) {
        m_logger->error("Model \"{}\" already exists", model_name);
        req.respond(flamestore_status(
                    TSRA_EEXISTS,
                    "A model with the same name is already registered"));
        m_logger->trace("Leaving flamestore_provider::on_register_model");
        return;
    }

    lock_guard_t guard(model->m_mutex);
    req.respond(flamestore_status::OK());

    m_logger->info("Registering model \"{}\"", model_name);

    try {

        model->m_model_config        = std::move(model_config);
        model->m_optimizer_config    = std::move(optimizer_config);
        model->m_model_signature     = std::move(model_signature);
        model->m_optimizer_signature = std::move(optimizer_signature);
        model->m_model_data.resize(model_data_size);
        model->m_optimizer_data.resize(optimizer_data_size);

        if(model->m_model_data.size() != 0) {
            std::vector<std::pair<void*, size_t>> model_data_ptr(1);
            model_data_ptr[0].first  = (void*)(model->m_model_data.data());
            model_data_ptr[0].second = model->m_model_data.size();
            model->m_model_data_bulk = m_engine->expose(model_data_ptr, tl::bulk_mode::read_write);
        }

        if(model->m_optimizer_data.size() != 0) {
            std::vector<std::pair<void*, size_t>> optimizer_data_ptr(1);
            optimizer_data_ptr[0].first  = (void*)(model->m_optimizer_data.data());
            optimizer_data_ptr[0].second = model->m_optimizer_data.size();
            model->m_optimizer_data_bulk = m_engine->expose(optimizer_data_ptr, tl::bulk_mode::read_write);
        }

    } catch(const tl::exception& e) {
        m_logger->critical("Exception caught in flamestore_provider::on_register_model: {}", e.what());
    }
}

void flamestore_memory_backend::get_model_config(
        const tl::request& req,
        const std::string& model_name) 
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_provider::on_get_model_config");
        return;
    }
    m_logger->info("Getting model config for model \"{}\"", model_name);
    req.respond(flamestore_status::OK(model->m_model_config));
}

void flamestore_memory_backend::get_optimizer_config(
        const tl::request& req,
        const std::string& model_name) 
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_provider::on_get_optimizer_config");
        return;
    }
    m_logger->info("Getting optimizer config for model \"{}\"", model_name);
    req.respond(flamestore_status::OK(model->m_optimizer_config));
}

void flamestore_memory_backend::write_model_data(
        const tl::request& req,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& remote_bulk)
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_provider::on_write_model_data");
        return;
    }
    m_logger->info("Pulling data from model \"{}\"", model_name);
    lock_guard_t guard(model->m_mutex);
    if(model->m_model_signature != model_signature) {
        m_logger->error("Unmatching signatures when writing model \"{}\"", model_name);
        req.respond(flamestore_status(
                    TSRA_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving flamestore_provider::on_write_model_data");
        return;
    }
    model->m_model_data_bulk << remote_bulk.on(req.get_endpoint());
    req.respond(flamestore_status::OK());
}

void flamestore_memory_backend::read_model_data(
        const tl::request& req,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& remote_bulk)
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        return;
    }

    lock_guard_t guard(model->m_mutex);
    if(model->m_model_signature != model_signature) {
        m_logger->error("Unmatching signatures when reading model \"{}\"", model_name);
        req.respond(flamestore_status(
                    TSRA_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving flamestore_provider::on_read_model_data");
        return;
    }
    m_logger->info("Pushing data to model \"{}\"", model_name);
    model->m_model_data_bulk >> remote_bulk.on(req.get_endpoint());
    req.respond(flamestore_status::OK());
}

void flamestore_memory_backend::write_optimizer_data(
        const tl::request& req,
        const std::string& model_name,
        const std::string& optimizer_signature,
        tl::bulk& remote_bulk) 
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_provider::on_write_optimizer_data");
        return;
    }

    lock_guard_t guard(model->m_mutex);
    if(model->m_optimizer_signature != optimizer_signature) {
        m_logger->error("Unmatching signatures when writing optimizer for model \"{}\"", model_name);
        req.respond(flamestore_status(
                    TSRA_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving flamestore_provider::on_write_optimizer_data");
        return;
    }
    m_logger->info("Pulling data from model optimizer \"{}\"", model_name);
    model->m_optimizer_data_bulk << remote_bulk.on(req.get_endpoint());
    req.respond(flamestore_status::OK());
}

void flamestore_memory_backend::read_optimizer_data(
        const tl::request& req,
        const std::string& model_name,
        const std::string& optimizer_signature,
        tl::bulk& remote_bulk) 
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_provider::on_read_optimizer_data");
        return;
    }

    lock_guard_t guard(model->m_mutex);
    if(model->m_optimizer_signature != optimizer_signature) {
        m_logger->error("Unmatching signatures when reading optimizer for model \"{}\"", model_name);
        req.respond(flamestore_status(
                    TSRA_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving flamestore_provider::on_read_optimizer_data");
        return;
    }
    m_logger->info("Pushing data to model optimizer \"{}\"", model_name);
    model->m_optimizer_data_bulk >> remote_bulk.on(req.get_endpoint());
    req.respond(flamestore_status::OK());
}
