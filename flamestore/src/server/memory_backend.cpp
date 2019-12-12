#include <mutex>
#include <map>
#include <spdlog/spdlog.h>
#include "model.hpp"
#include "backend.hpp"

namespace flamestore {

namespace tl = thallium;

class MemoryBackend : public AbstractServerBackend {

        struct model_impl {
            std::vector<char> m_model_data;
            tl::bulk          m_model_data_bulk;
        };

    public:

        using model_t = flamestore_model<model_impl>;
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
            m_logger->info("Entering _find_or_create_model");
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

        MemoryBackend(const ServerContext& ctx, const AbstractServerBackend::config_type& config)
        : m_engine(ctx.m_engine)
        , m_logger(ctx.m_logger) {
            m_logger->debug("Initializing memory backend");
        }

        MemoryBackend(const AbstractServerBackend&)            = delete;
        MemoryBackend(AbstractServerBackend&&)                 = delete;
        MemoryBackend& operator=(const AbstractServerBackend&) = delete;
        MemoryBackend& operator=(AbstractServerBackend&&)      = delete;
        ~MemoryBackend()                              = default;

        virtual void register_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_config,
                std::size_t& model_size,
                const std::string& model_signature) override;

        virtual void reload_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name) override;

        virtual void write_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_signature,
                const tl::bulk& remote_bulk,
                const std::size_t& size) override;

        virtual void read_model(
                const tl::request& req,
                const std::string& client_addr,
                const std::string& model_name,
                const std::string& model_signature,
                const tl::bulk& remote_bulk,
                const std::size_t& size) override;
};

REGISTER_FLAMESTORE_BACKEND("memory",MemoryBackend);

void MemoryBackend::register_model(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name,
        const std::string& model_config,
        std::size_t& model_size,
        const std::string& model_signature)
{
    bool created = false;
    m_logger->info("Entering MemoryBackend::register_model");
    auto model = _find_or_create_model(model_name, created);
    if(not created) {
        m_logger->error("Model \"{}\" already exists", model_name);
        req.respond(Status(
                    FLAMESTORE_EEXISTS,
                    "A model with the same name is already registered"));
        m_logger->trace("Leaving flamestore_provider::on_register_model");
        return;
    }
    m_logger->info("Model \"{}\" created", model_name);

    lock_guard_t guard(model->m_mutex);
    req.respond(Status::OK());

    m_logger->info("Registering model \"{}\"", model_name);

    try {

        model->m_model_config        = std::move(model_config);
        model->m_model_signature     = std::move(model_signature);
        model->m_impl.m_model_data.resize(model_size);

        if(model->m_impl.m_model_data.size() != 0) {
            std::vector<std::pair<void*, size_t>> model_data_ptr(1);
            model_data_ptr[0].first  = (void*)(model->m_impl.m_model_data.data());
            model_data_ptr[0].second = model->m_impl.m_model_data.size();
            model->m_impl.m_model_data_bulk = m_engine->expose(model_data_ptr, tl::bulk_mode::read_write);
        }

    } catch(const tl::exception& e) {
        m_logger->critical("Exception caught in flamestore_provider::on_register_model: {}", e.what());
    }
}

void MemoryBackend::reload_model(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name) 
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(Status(
                    FLAMESTORE_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_provider::reload_model");
        return;
    }
    m_logger->info("Getting model config for model \"{}\"", model_name);
    req.respond(Status::OK(model->m_model_config));
}

void MemoryBackend::write_model(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& remote_bulk,
        const std::size_t& size)
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(Status(
                    FLAMESTORE_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving write_model");
        return;
    }
    m_logger->info("Pulling data from model \"{}\"", model_name);
    lock_guard_t guard(model->m_mutex);
    if(model->m_model_signature != model_signature) {
        m_logger->error("Unmatching signatures when writing model \"{}\"", model_name);
        req.respond(Status(
                    FLAMESTORE_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving write_model");
        return;
    }
    model->m_impl.m_model_data_bulk << remote_bulk.on(req.get_endpoint());
    req.respond(Status::OK());
}

void MemoryBackend::read_model(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& remote_bulk,
        const std::size_t& size)
{
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(Status(
                    FLAMESTORE_ENOEXISTS,
                    "No model found with provided name"));
        return;
    }

    lock_guard_t guard(model->m_mutex);
    if(model->m_model_signature != model_signature) {
        m_logger->error("Unmatching signatures when reading model \"{}\"", model_name);
        req.respond(Status(
                    FLAMESTORE_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving on_read_model_data");
        return;
    }
    m_logger->info("Pushing data to model \"{}\"", model_name);
    model->m_impl.m_model_data_bulk >> remote_bulk.on(req.get_endpoint());
    req.respond(Status::OK());
}

}

