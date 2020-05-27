#include <mutex>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <bake-client.hpp>
#include "model.hpp"
#include "backend.hpp"

namespace flamestore {

namespace tl = thallium;

class MochiBackend : public AbstractServerBackend {

        struct location {
            tl::endpoint          m_endpoint;
            uint64_t              m_ssg_member_id;
            bake::provider_handle m_phandle;
            bake::target          m_target;
        };

        struct model_impl {
            std::weak_ptr<location> m_location;
            bake::region            m_region;
            std::size_t             m_size;
        };


    public:

        using model_t = flamestore_model<model_impl>;
        using name_t = std::string;
        using lock_guard_t = std::lock_guard<tl::mutex>;

    private:

        tl::engine*                                 m_engine;
        spdlog::logger*                             m_logger;
        mutable tl::rwlock                          m_models_rwlock;
        std::map<name_t, std::unique_ptr<model_t>>  m_models;
        bake::client                                m_bake_client;

        std::vector<std::shared_ptr<location>>      m_storage_locations;
        tl::rwlock                                  m_storage_locations_lock;

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

        MochiBackend(const ServerContext& ctx, const AbstractServerBackend::config_type& config)
        : m_engine(ctx.m_engine)
        , m_logger(ctx.m_logger)
        , m_bake_client(m_engine->get_margo_instance()) {
            m_logger->debug("Initializing memory backend");
        }

        MochiBackend(const AbstractServerBackend&)            = delete;
        MochiBackend(AbstractServerBackend&&)                 = delete;
        MochiBackend& operator=(const AbstractServerBackend&) = delete;
        MochiBackend& operator=(AbstractServerBackend&&)      = delete;
        ~MochiBackend()                              = default;

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

        virtual void duplicate_model(
                const tl::request& req,
                const std::string& model_name,
                const std::string& new_model_name) override;

        virtual void on_shutdown() override;

        virtual void on_worker_joined(
                uint64_t member_id,
                hg_addr_t addr) override;

        virtual void on_worker_left(
                uint64_t member_id) override;

        virtual void on_worker_died(
                uint64_t member_id) override;
};

REGISTER_FLAMESTORE_BACKEND("mochi",MochiBackend);

void MochiBackend::on_shutdown()
{
    m_logger->debug("Asking all storage servers to shut down");
    m_storage_locations_lock.wrlock();
    for(auto& l : m_storage_locations) {
        auto& ep = l->m_endpoint;
        m_engine->shutdown_remote_engine(ep);
    }
    m_storage_locations_lock.unlock();
    while(true){
        m_storage_locations_lock.rdlock();
        auto x = m_storage_locations.size();
        m_storage_locations_lock.unlock();
        if(x == 0)
            break;
        else
            tl::thread::sleep(*m_engine, 100);
    }
    m_logger->debug("All storage servers have shut down");
}

//////////////////////////////////////////////////////////////////////////
// SSG callbacks
//////////////////////////////////////////////////////////////////////////

void MochiBackend::on_worker_joined(uint64_t member_id, hg_addr_t addr)
{
    tl::endpoint worker_ep(*m_engine, addr, false);
    m_logger->info("Mochi backend received new worker at address {}", (std::string)worker_ep);

    // query the new storage server for its Bake target(s)
    m_logger->debug("Querying new storage server for storage targets...");
    bake::provider_handle ph(m_bake_client, addr);
    std::vector<bake::target> targets = m_bake_client.probe(ph);
    m_logger->info("New storage server has {} target(s)", targets.size());

    for(auto& tgt : targets) {
        auto l = std::make_shared<location>();
        l->m_endpoint = worker_ep;
        l->m_ssg_member_id = member_id;
        l->m_phandle = bake::provider_handle(m_bake_client, addr);
        l->m_target = tgt;
        m_storage_locations.push_back(std::move(l));
    }
}

void MochiBackend::on_worker_left(uint64_t member_id)
{
    m_storage_locations.erase(
        std::remove_if( std::begin(m_storage_locations),
                        std::end(m_storage_locations),
                        [member_id](const std::shared_ptr<location>& l) {
                            return l->m_ssg_member_id == member_id;
                        }),
        std::end(m_storage_locations)
    );
}

void MochiBackend::on_worker_died(uint64_t member_id)
{
    m_storage_locations.erase(
        std::remove_if( std::begin(m_storage_locations),
                        std::end(m_storage_locations),
                        [member_id](const std::shared_ptr<location>& l) {
                            return l->m_ssg_member_id == member_id;
                        }),
        std::end(m_storage_locations)
    );
}

//////////////////////////////////////////////////////////////////////////
// Client requests
//////////////////////////////////////////////////////////////////////////

void MochiBackend::register_model(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name,
        const std::string& model_config,
        std::size_t& model_size,
        const std::string& model_signature)
{
    bool created = false;
    m_logger->info("Entering MochiBackend::register_model");
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

    m_logger->info("Registering model \"{}\"", model_name);

    model->m_model_config    = std::move(model_config);
    model->m_model_signature = std::move(model_signature);
    model->m_impl.m_size     = model_size;

    // select a location for the model
    auto i = std::rand() % m_storage_locations.size();
    m_logger->debug("Selecting storage target {}/{}", i+1, m_storage_locations.size());
    auto loc = m_storage_locations[i];
    model->m_impl.m_location = loc;

    // allocate a region with the right size in Bake
    try {
        m_logger->debug("Creating bake region of size {}", model_size);
        auto region = m_bake_client.create(loc->m_phandle, loc->m_target, model_size);
        m_logger->debug("Region successfuly created");
        model->m_impl.m_region = region;
    } catch(const bake::exception& ex) {
        // TODO remove the model from the database since it wasn't properly created
        m_logger->error("Bake region creation failed: {}", ex.what());
        req.respond(Status(FLAMESTORE_EBAKE, "Bake region creation failed"));
        return;
    }

    req.respond(Status::OK());
}

void MochiBackend::reload_model(
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

void MochiBackend::write_model(
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
    m_logger->debug("Proxy-writing model {}", model_name);
    auto loc = model->m_impl.m_location.lock();
    // TODO check validity of loc
    try {
        m_bake_client.write(loc->m_phandle,
                        loc->m_target,
                        model->m_impl.m_region,
                        0,
                        remote_bulk.get_bulk(),
                        0,
                        client_addr,
                        size);
    } catch(const bake::exception& ex) {
        m_logger->error("Failed to write in Bake: {}", ex.what());
        req.respond(Status(FLAMESTORE_EBAKE, "Failed to write in Bake"));
        return;
    }
    // persisting data
    try {
        m_bake_client.persist(loc->m_phandle,
                            loc->m_target,
                            model->m_impl.m_region,
                            0,
                            size);
    } catch(const bake::exception& ex) {
        
    }
    req.respond(Status::OK());
}

void MochiBackend::read_model(
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
    auto loc = model->m_impl.m_location.lock();
    // TODO check validity of loc
    try {
        m_bake_client.read(loc->m_phandle,
                        loc->m_target,
                        model->m_impl.m_region,
                        0,
                        remote_bulk.get_bulk(),
                        0,
                        client_addr,
                        size);
    } catch(const bake::exception& ex) {
        m_logger->error("Failed to read from Bake: {}", ex.what());
        req.respond(Status(FLAMESTORE_EBAKE, "Failed to read from Bake"));
        return;
    }
    req.respond(Status::OK());
}

void MochiBackend::duplicate_model(
        const tl::request& req,
        const std::string& model_name,
        const std::string& new_model_name)
{
    m_logger->info("Entering MochiBackend::duplicate_model");
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(Status(
                    FLAMESTORE_ENOEXISTS,
                    "No model found with provided name"));
        return;
    }
    bool created = false;
    auto new_model = _find_or_create_model(new_model_name, created);
    if(not created) {
        m_logger->error("Model \"{}\" already exists", new_model_name);
        req.respond(Status(
                    FLAMESTORE_EEXISTS,
                    "A model with the same name is already registered"));
        m_logger->trace("Leaving flamestore_provider::on_duplicate_model");
        return;
    }
    
    new_model->m_model_config    = model->m_model_config;
    new_model->m_model_signature = model->m_model_signature;
    new_model->m_impl.m_size     = model->m_impl.m_size;

    // find out where the source model is
    auto loc = model->m_impl.m_location.lock();

    // select a location for the model
    auto i = std::rand() % m_storage_locations.size();
    m_logger->debug("Selecting storage target {}/{}", i+1, m_storage_locations.size());
    auto new_loc = m_storage_locations[i];
    new_model->m_impl.m_location = loc;

    // allocate a region with the right size in Bake for the new model
    try {
        m_logger->debug("Creating bake region of size {} by migrating existing region", new_model->m_impl.m_size);
        std::string new_addr = tl::endpoint(*m_engine, new_loc->m_phandle.address());
        auto region = m_bake_client.migrate(loc->m_phandle,
                                            loc->m_target,
                                            model->m_impl.m_region,
                                            model->m_impl.m_size,
                                            false,
                                            new_addr,
                                            new_loc->m_phandle.provider_id(),
                                            new_loc->m_target);
        m_logger->debug("Region successfuly created");
        new_model->m_impl.m_region = region;
    } catch(const bake::exception& ex) {
        // TODO remove the model from the database since it wasn't properly created
        m_logger->error("Bake region creation failed: {}", ex.what());
        req.respond(Status(FLAMESTORE_EBAKE, "Bake region migration failed"));
        return;
    }
    req.respond(Status::OK());
}

}

