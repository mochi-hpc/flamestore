#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <mutex>
#include <map>
#include <fstream>
#include <spdlog/spdlog.h>
#include <json/json.h>
#include <bake-client.h>
#include <sdskv-client.h>
#include "model.hpp"
#include "backend.hpp"

namespace tl = thallium;

class flamestore_mochi_backend : public flamestore_backend {

    public:

        using name_t = std::string;
        using lock_guard_t = std::lock_guard<tl::mutex>;

    private:

        /**
         * Information about a bake region: target id, region id, and size.
         */
        struct region_info {
            bake_target_id_t m_tid;
            bake_region_id_t m_rid;
            size_t           m_size;
        };

        /**
         * Model implementation. Contains the region information to retrieve
         * the optimizer and model data.
         */
        struct model_impl {
            bake_provider_handle_t m_ph;
            region_info            m_model_region;
            region_info            m_optimizer_region;
        };

        /**
         * Encapsulation of SDSKV information.
         */
        struct sdskv_info {
            sdskv_client_t          m_client = SDSKV_CLIENT_NULL;
            sdskv_provider_handle_t m_ph     = SDSKV_PROVIDER_HANDLE_NULL;
            sdskv_database_id_t     m_db_id;
        };

        /**
         * Encapsulation of BAKE target information.
         */
        struct bake_target_info {
            bake_provider_handle_t m_ph = BAKE_PROVIDER_HANDLE_NULL;
            bake_target_id_t       m_target;
        };

        /**
         * Encapsulation of BAKE information.
         */
        struct bake_info {
            bake_client_t             m_client = BAKE_CLIENT_NULL;
            std::vector<bake_target_info> m_targets;
        };

    public:

        using model_t = flamestore_model<model_impl>;

    private:

        tl::engine*                                   m_engine;
        spdlog::logger*                               m_logger;
        mutable tl::rwlock                            m_models_rwlock;
        std::map<name_t, std::unique_ptr<model_t>>    m_models;
        sdskv_info                                    m_sdskv_info;
        bake_info                                     m_bake_info;

        /**
         * This functon cleans up the content of the m_sdskv_info and
         * m_bake_info structures, erasing all provide handles and clients.
         */
        inline void _cleanup() {
            if(m_sdskv_info.m_ph != SDSKV_PROVIDER_HANDLE_NULL)
                sdskv_provider_handle_release(m_sdskv_info.m_ph);
            if(m_sdskv_info.m_client != SDSKV_CLIENT_NULL)
                sdskv_client_finalize(m_sdskv_info.m_client);
            for(auto& tgt_info : m_bake_info.m_targets) {
                bake_provider_handle_release(tgt_info.m_ph);
            }
            if(m_bake_info.m_client != BAKE_CLIENT_NULL)
                bake_client_finalize(m_bake_info.m_client);
        }

        /**
         * This function reads a JSON config file and configures the backend.
         * Example of JSON configuration:
         *   {
         *       sdskv : {
         *           address  : "tcp://x.y.z:1234",
         *           provider : 1,
         *           database : "mydatabase"
         *       },
         *       bake : [
         *           { address  : "tcp://x.y.z:1234",
         *             provider : 1,
         *             target   : "mytarget" },
         *           { ... },
         *           ...
         *       ]
         *   }
         */
        inline void _configure_backend(const std::string& config) {
            m_logger->trace("Configuring Mochi backend using JSON data");
            Json::Reader reader;
            Json::Value root;
            reader.parse(config.c_str(), root);
            // Check validity of the JSON file
            if(!root.isMember("sdskv"))     throw std::runtime_error("Could not find sdskv entry");
            auto& sdskv = root["sdskv"];
            if(!sdskv.isObject())           throw std::runtime_error("sdskv entry should be an object");
            if(!sdskv.isMember("address"))  throw std::runtime_error("Could not find address in sdskv entry");
            if(!sdskv.isMember("provider")) throw std::runtime_error("Could not find provider in sdskv entry");
            if(!sdskv.isMember("database")) throw std::runtime_error("Could not find database in sdskv entry");
            auto& sdskv_address  = sdskv["address"];
            auto& sdskv_provider = sdskv["provider"];
            auto& sdskv_database = sdskv["database"];
            if(!sdskv_address.isString())   throw std::runtime_error("sdskv address should be a string");
            if(!sdskv_provider.isUInt())    throw std::runtime_error("sdskv provider should be an unsigned int");
            if(!sdskv_database.isString())  throw std::runtime_error("sdskv database should be a string");
            unsigned sdskv_provider_id = sdskv_provider.asUInt();
            if(sdskv_provider_id > 65535)   throw std::runtime_error("sdskv provider value too high");
            if(!root.isMember("bake"))      throw std::runtime_error("Could not find bake entry in JSON file");
            auto& bake = root["bake"];
            if(!bake.isArray())             throw std::runtime_error("bake entry should be an array");
            if(bake.size() == 0)            throw std::runtime_error("bake entry should list at least one provider");
            for(unsigned int i = 0; i < bake.size(); i++) {
                auto& bake_instance = bake[i];
                if(!bake_instance.isObject())           throw std::runtime_error("bake instances should be object");
                if(!bake_instance.isMember("address"))  throw std::runtime_error("Could not find address in bake entry");
                if(!bake_instance.isMember("provider")) throw std::runtime_error("Could not find provider in bake entry");
                if(!bake_instance.isMember("target"))   throw std::runtime_error("Could not find target in bake entry");
                auto& bake_address  = bake_instance["address"];
                auto& bake_provider = bake_instance["provider"];
                auto& bake_target   = bake_instance["target"];
                if(!bake_address.isString()) throw std::runtime_error("bake address should be a string");
                if(!bake_provider.isUInt())  throw std::runtime_error("bake provider should be an unsigned int");
                if(!bake_target.isString())  throw std::runtime_error("bake target should be a string");
                unsigned bake_provider_id = bake_provider.asUInt();
                if(bake_provider_id > 65535) throw std::runtime_error("bake provider value too high");
            }
            // now that we've checked the file, read it and initialize the provider handles
            int ret;
            margo_instance_id mid = m_engine->get_margo_instance();
            // initialize the sdskv info
            m_logger->trace("Creating SDSKV client");
            ret = sdskv_client_init(mid, &(m_sdskv_info.m_client));
            if(ret != SDSKV_SUCCESS) {
                _cleanup();
                throw std::runtime_error("sdskv_client_init failed");
            }
            m_logger->trace("Looking up SDSKV provider address");
            auto ep = m_engine->lookup(sdskv_address.asString());
            ret = sdskv_provider_handle_create(
                    m_sdskv_info.m_client,
                    ep.get_addr(),
                    sdskv_provider.asUInt(),
                    &(m_sdskv_info.m_ph));
            if(ret != SDSKV_SUCCESS) {
                _cleanup();
                throw std::runtime_error("sdskv_provider_handle_create failed");
            }
            m_logger->trace("Opening SDSKV database {}", sdskv_database.asString());
            ret = sdskv_open(
                    m_sdskv_info.m_ph,
                    sdskv_database.asString().c_str(),
                    &(m_sdskv_info.m_db_id));
            if(ret != SDSKV_SUCCESS) {
                _cleanup();
                throw std::runtime_error("sdskv_open failed");
            }
            // initialize the bake info
            m_logger->trace("Creating BAKE client");
            ret = bake_client_init(mid, &(m_bake_info.m_client));
            if(ret != BAKE_SUCCESS) {
                _cleanup();
                throw std::runtime_error("bake_client_init failed");
            }
            m_logger->trace("Creating BAKE {} providers", bake.size());
            std::unordered_map<std::string, bake_provider_handle_t> bake_providers;
            m_bake_info.m_targets.reserve(bake.size());
            for(unsigned int i = 0; i < bake.size(); i++) {
                auto& bake_instance = bake[i];
                auto& bake_address  = bake_instance["address"];
                auto& bake_provider = bake_instance["provider"];
                auto& bake_target   = bake_instance["target"];
                unsigned bake_provider_id = bake_provider.asUInt();
                std::stringstream ss;
                ss << bake_address.asString() << ":" << bake_provider.asUInt();
                auto it = bake_providers.find(ss.str());
                bake_target_info target_info;
                target_info.m_ph = BAKE_PROVIDER_HANDLE_NULL;
                if(it == bake_providers.end()) {
                    m_logger->trace("Looking up BAKE address {}", bake_address.asString());
                    auto ep = m_engine->lookup(bake_address.asString().c_str());
                    m_logger->trace("Creating BAKE provider handle ({},{}) for target {}",
                            bake_address.asString(), bake_provider_id, bake_target.asString());
                    ret = bake_provider_handle_create(
                            m_bake_info.m_client,
                            ep.get_addr(),
                            bake_provider_id,
                            &(target_info.m_ph));
                    if(ret != BAKE_SUCCESS) {
                        _cleanup();
                        throw std::runtime_error("bake_provider_handle_create failed");
                    }
                    bake_providers[ss.str()] = target_info.m_ph;
                } else {
                    m_logger->trace("Reusing BAKE provider handle ({},{}) for target {}",
                            bake_address.asString(), bake_provider_id, bake_target.asString());
                    target_info.m_ph = it->second;
                    bake_provider_handle_ref_incr(target_info.m_ph);
                }
                ret = bake_target_id_from_string(
                        bake_target.asString().c_str(),
                        &(target_info.m_target));
                if(ret != BAKE_SUCCESS) {
                    bake_provider_handle_release(target_info.m_ph);
                    _cleanup();
                    throw std::runtime_error("bake_target_id_from_string failed");
                }
                m_bake_info.m_targets.push_back(target_info);
            }
            m_logger->trace("Done with configuration of Mochi backend");
        }

        /**
         * This function tries to reload a model's information from the database.
         * If found, the model is cached in m_models and a pointer to it is returned.
         */
        inline model_t* _reload_model_from_database(const std::string& model_name) {
            m_logger->trace("Reloading model {} from database", model_name);
            std::string model_config_key     = model_name + "/model/config";
            std::string optimizer_config_key = model_name + "/optimizer/config";
            std::string model_region_key     = model_name + "/model/data";
            std::string optimizer_region_key = model_name + "/optimizer/data";
            std::string model_sig_key        = model_name + "/model/sig";
            std::string optimizer_sig_key    = model_name + "/optimizer/sig";

            auto model = std::make_unique<model_t>();
            auto result = model.get();

            auto get_keyval_str = [this](const std::string& key, std::string& value) {
                hg_size_t vsize;
                m_logger->trace("Looking up length for key {}", key);
                int ret = sdskv_length(
                        m_sdskv_info.m_ph,
                        m_sdskv_info.m_db_id,
                        (const void*)key.c_str(),
                        key.size(),
                        &vsize);
                if(ret != SDSKV_SUCCESS) {
                    m_logger->trace("Lookup failed (return code {})", ret);
                    return false;
                }
                value.resize(vsize+1,'\0');
                m_logger->trace("Getting value for key {}", key);
                ret = sdskv_get(
                        m_sdskv_info.m_ph,
                        m_sdskv_info.m_db_id,
                        (const void*)key.c_str(),
                        key.size(),
                        (void*)value.data(),
                        &vsize);
                if(ret != SDSKV_SUCCESS) {
                    m_logger->trace("Getting value failed (return code {})", ret);
                    return false;
                }
                return true;
            };

            auto get_keyval_auto = [this](const std::string& key, auto& value) {
                hg_size_t vsize;
                m_logger->trace("Looking up length for key {}", key);
                int ret = sdskv_length(
                        m_sdskv_info.m_ph,
                        m_sdskv_info.m_db_id,
                        (const void*)key.c_str(),
                        key.size(),
                        &vsize);
                if(ret != SDSKV_SUCCESS) {
                    m_logger->trace("Lookup failed (return code {})", ret);
                    return false;
                }
                m_logger->trace("Getting value for key {}", key);
                ret = sdskv_get(
                        m_sdskv_info.m_ph,
                        m_sdskv_info.m_db_id,
                        (const void*)key.c_str(),
                        key.size(),
                        (void*)&value,
                        &vsize);
                if(ret != SDSKV_SUCCESS) {
                    m_logger->trace("Getting value failed (return code {})", ret);
                    return false;
                }
                return true;
            };

            // try loading the model config
            if(!get_keyval_str(model_config_key, model->m_model_config)) return nullptr;
            // try loading the model signature
            if(!get_keyval_str(model_sig_key, model->m_model_signature)) return nullptr;
            // try loading the model region info
            if(!get_keyval_auto(model_region_key, model->m_impl.m_model_region)) return nullptr; 
            // try loading the optimizer config 
            if(!get_keyval_str(optimizer_config_key, model->m_optimizer_config)) return nullptr; 
            // try loading the optimizer signature
            if(!get_keyval_str(optimizer_sig_key, model->m_optimizer_signature)) return nullptr;
            // try loading the optimizer region info
            if(!get_keyval_auto(optimizer_region_key, model->m_impl.m_optimizer_region)) return nullptr;
            
            m_models[model_name] = std::move(model);

            return result;
        }

        /**
         * This function tries to find a model in the local m_models cache.
         * If not found, it tries to reload the model from the database.
         */
        inline model_t* _find_model(const std::string& model_name) {
            m_logger->trace("Trying to find model {}", model_name);
            m_models_rwlock.wrlock();
            auto it = m_models.find(model_name);
            if(it == m_models.end()) {
                auto on_database = _reload_model_from_database(model_name);
                m_models_rwlock.unlock();
                return on_database; // may be nullptr if not on disk
            } else  {
                auto result = it->second.get();
                m_models_rwlock.unlock();
                return result;
            }
        }

        /**
         * This function tries to find a model in the local m_models cache.
         * If not found, it tries to reload the model from the database.
         * If the model does not exist, it is created and put in cache in addition to the database.
         */
        inline model_t* _find_or_create_model(
                const std::string& model_name,
                const std::string& model_config,
                const std::string& optimizer_config,
                std::size_t model_data_size,
                std::size_t optimizer_data_size,
                const std::string& model_signature,
                const std::string& optimizer_signature)
        {
            int ret;
            m_models_rwlock.wrlock();
            model_t* model = nullptr;
            auto it = m_models.find(model_name);
            if(it != m_models.end()) { // model is already cached
                model =  it->second.get();
                m_models_rwlock.unlock();
                return model;
            }
            model = _reload_model_from_database(model_name);
            if(model) { // model is in datatabase
                if(model_signature != model->m_model_signature
                || optimizer_signature != model->m_optimizer_signature) {
                    model = nullptr; // signatures don't match, return null
                }
                m_models_rwlock.unlock();
                return model;
            }
            // model is not in cache nor in database, create it
            auto model_uptr = std::make_unique<model_t>();
            model = model_uptr.get();
            // copy the base information
            model->m_name                = model_name;
            model->m_model_config        = model_config;
            model->m_optimizer_config    = optimizer_config;
            model->m_model_signature     = model_signature;
            model->m_optimizer_signature = optimizer_signature;
            // compute the index of the bake target that will store the model
            unsigned i = std::hash<std::string>()(model_name) % m_bake_info.m_targets.size();
            auto& tgt_info = m_bake_info.m_targets[i];
            model->m_impl.m_ph                      = tgt_info.m_ph;
            model->m_impl.m_model_region.m_tid      = tgt_info.m_target;
            model->m_impl.m_model_region.m_size     = model_data_size;
            model->m_impl.m_optimizer_region.m_tid  = tgt_info.m_target;
            model->m_impl.m_optimizer_region.m_size = optimizer_data_size;

            auto& ph  = tgt_info.m_ph;
            auto& tid = tgt_info.m_target;

            // TODO: in the two calls bellow if something goes wrong we should cleanup the mess
            // create a region with the required size for the model data
            ret = bake_create(ph, tid, model_data_size, &(model->m_impl.m_model_region.m_rid));
            if(ret != BAKE_SUCCESS) return nullptr;
            // create a region with the required size for the optimizer data
            ret = bake_create(ph, tid, optimizer_data_size, &(model->m_impl.m_optimizer_region.m_rid));
            if(ret != BAKE_SUCCESS) return nullptr;

            auto put_key_str = [this](const std::string& key, const std::string& value) {
                int ret = sdskv_put(
                        m_sdskv_info.m_ph,
                        m_sdskv_info.m_db_id,
                        (const void*)key.c_str(),
                        key.size(),
                        (const void*)value.c_str(),
                        value.size());
                if(ret == SDSKV_SUCCESS) return true;
                else return false;
            };

            auto put_key_auto = [this](const std::string& key, const auto& value) {
                int ret = sdskv_put(
                        m_sdskv_info.m_ph,
                        m_sdskv_info.m_db_id,
                        (const void*)key.c_str(),
                        key.size(),
                        (const void*)&value,
                        sizeof(value));
                if(ret == SDSKV_SUCCESS) return true;
                else return false;
            };

            std::string model_config_key     = model_name + "/model/config";
            std::string model_region_key     = model_name + "/model/data";
            std::string model_sig_key        = model_name + "/model/sig";
            std::string optimizer_config_key = model_name + "/optimizer/config";
            std::string optimizer_region_key = model_name + "/optimizer/data";
            std::string optimizer_sig_key    = model_name + "/optimizer/sig";

            if(!put_key_str(model_config_key, model_config))                          return nullptr;
            if(!put_key_str(model_sig_key, model_signature))                          return nullptr;
            if(!put_key_auto(model_region_key, model->m_impl.m_model_region))         return nullptr;
            if(!put_key_str(optimizer_config_key, optimizer_config))                  return nullptr;
            if(!put_key_str(optimizer_sig_key, optimizer_signature))                  return nullptr;
            if(!put_key_auto(optimizer_region_key, model->m_impl.m_optimizer_region)) return nullptr;
            // TODO technically if something goes wrong we should clean up the mess

            m_models.emplace(model_name, std::move(model));
            m_models_rwlock.unlock();
            return model;
        }

    public:

        flamestore_mochi_backend(const flamestore_server_context& ctx, const flamestore_backend::config_type& config)
        : m_engine(ctx.m_engine)
        , m_logger(ctx.m_logger) {
            std::string json_config;
            auto it = config.find("config");
            if(it != config.end()) {
                json_config  = it->second;
            } else {
                m_logger->critical("Configuration not provided to Mochi backend");
                throw std::runtime_error("Configuration not provided to Mochi backend");
            }
            try {
                _configure_backend(json_config);
            } catch(const std::runtime_error& e) {
                m_logger->critical("Configuration failed: {}", std::string(e.what()));
                throw;
            }
        }

        flamestore_mochi_backend(const flamestore_backend&)            = delete;
        flamestore_mochi_backend(flamestore_backend&&)                 = delete;
        flamestore_mochi_backend& operator=(const flamestore_backend&) = delete;
        flamestore_mochi_backend& operator=(flamestore_backend&&)      = delete;
        ~flamestore_mochi_backend()                              = default;

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

REGISTER_FLAMESTORE_BACKEND("mochi",flamestore_mochi_backend);
        
void flamestore_mochi_backend::register_model(
        const tl::request& req,
        const std::string& model_name,
        const std::string& model_config,
        const std::string& optimizer_config,
        std::size_t model_data_size,
        std::size_t optimizer_data_size,
        const std::string& model_signature,
        const std::string& optimizer_signature)
{
#if 0
    bool created = false;
    auto model = _find_or_create_model(model_name, created);
    if(not created) {
        m_logger->error("Model \"{}\" already exists", model_name);
        req.respond(flamestore_status(
                    TSRA_EEXISTS,
                    "A model with the same name is already registered"));
        m_logger->trace("Leaving flamestore_mochi_backend::register_model");
        return;
    }

    lock_guard_t guard(model->m_mutex);
    req.respond(flamestore_status::OK());

    m_logger->info("Registering model \"{}\"", model_name);

    try {

        model->m_model_config          = std::move(model_config);
        model->m_optimizer_config      = std::move(optimizer_config);
        model->m_model_signature       = std::move(model_signature);
        model->m_optimizer_signature   = std::move(optimizer_signature);
        model->m_impl.m_model_size     = model_data_size;
        model->m_impl.m_optimizer_size = optimizer_data_size;

        std::stringstream basedir;
        basedir << m_path << "/" << model_name;
        int status = mkdir(basedir.str().c_str(), 0700);
        if(status == -1) {
            req.respond(flamestore_status(TSRA_EMKDIR, "Could not create directory for model"));
            m_logger->error("Coult not create directory for model \"{}\"", model_name);
            return;
        }

        // write model config and signature
        {
            std::string filename = basedir.str() + "/model.json";
            std::ofstream ofs(filename.c_str(), std::ofstream::out);
            ofs << model->m_model_config;

            std::string filename_sig = basedir.str() + "/model.sig";
            std::ofstream ofs_sig(filename_sig.c_str(), std::ofstream::out);
            ofs_sig << model->m_model_signature;
        }

        // write optimizer config and signature
        {
            std::string filename = basedir.str() + "/optimizer.json";
            std::ofstream ofs(filename.c_str(), std::ofstream::out);
            ofs << model->m_optimizer_config;

            std::string filename_sig = basedir.str() + "/optimizer.sig";
            std::ofstream ofs_sig(filename_sig.c_str(), std::ofstream::out);
            ofs_sig << model->m_optimizer_signature;
        }

        // create and mmap model data
        {
            std::string filename = basedir.str() + "/model.bin";
            int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
            if(fd == -1)
            {
                m_logger->error("Error opening model.bin for model \"{}\"", model_name);
                req.respond(flamestore_status(TSRA_EIO, "Could not create model.bin"));
                // XXX cleanup
                return;
            }
            int status = ftruncate(fd, model_data_size);
            if(status != 0)
            {
                m_logger->error("Error truncating model.bin for model \"{}\"", model_name);
                req.respond(flamestore_status(TSRA_EIO, "Could not resize model.bin"));
                // XXX cleanup
                return;
            }
            model->m_impl.m_model_data
                = mmap(0, model_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            // XXX check mmap worked
            close(fd);
        }

        // create and mmap optimizer data
        if(optimizer_data_size != 0) {
            std::string filename = basedir.str() + "/optimizer.bin";
            int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
            if(fd == -1)
            {
                m_logger->error("Error opening optimizer.bin for model \"{}\"", model_name);
                req.respond(flamestore_status(TSRA_EIO, "Could not create optimizer.bin"));
                // XXX cleanup
                return;
            }
            int status = ftruncate(fd, optimizer_data_size);
            if(status != 0)
            {
                m_logger->error("Error truncating optimizer.bin for model \"{}\"", model_name);
                req.respond(flamestore_status(TSRA_EIO, "Could not resize optimizer.bin"));
                // XXX cleanup
                return;
            }
            model->m_impl.m_optimizer_data
                = mmap(0, optimizer_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            // XXX check mmap worked
            close(fd);
        }

        if(model->m_impl.m_model_size != 0) {
            std::vector<std::pair<void*, size_t>> model_data_ptr(1);
            model_data_ptr[0].first  = (void*)(model->m_impl.m_model_data);
            model_data_ptr[0].second = model->m_impl.m_model_size;
            model->m_impl.m_model_data_bulk = m_engine->expose(model_data_ptr, tl::bulk_mode::read_write);
        }

        if(model->m_impl.m_optimizer_size != 0) {
            std::vector<std::pair<void*, size_t>> optimizer_data_ptr(1);
            optimizer_data_ptr[0].first  = (void*)(model->m_impl.m_optimizer_data);
            optimizer_data_ptr[0].second = model->m_impl.m_optimizer_size;
            model->m_impl.m_optimizer_data_bulk = m_engine->expose(optimizer_data_ptr, tl::bulk_mode::read_write);
        }

    } catch(const tl::exception& e) {
        m_logger->critical("Exception caught in flamestore_mochi_backend::register_model: {}", e.what());
    }
#endif
}

void flamestore_mochi_backend::get_model_config(
        const tl::request& req,
        const std::string& model_name)
{
#if 0
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_mochi_backend::get_model_config");
        return;
    }
    m_logger->info("Getting model config for model \"{}\"", model_name);
    req.respond(flamestore_status::OK(model->m_model_config));
#endif
}

void flamestore_mochi_backend::get_optimizer_config(
        const tl::request& req,
        const std::string& model_name)
{
#if 0
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_mochi_backend::get_optimizer_config");
        return;
    }
    m_logger->info("Getting optimizer config for model \"{}\"", model_name);
    req.respond(flamestore_status::OK(model->m_optimizer_config));
#endif
}

void flamestore_mochi_backend::write_model_data(
        const tl::request& req,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& remote_bulk)
{
#if 0
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_mochi_backend::write_model_data");
        return;
    }
    m_logger->info("Pulling data from model \"{}\"", model_name);
    lock_guard_t guard(model->m_mutex);
    if(model->m_model_signature != model_signature) {
        m_logger->error("Unmatching signatures when writing model \"{}\"", model_name);
        req.respond(flamestore_status(
                    TSRA_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving flamestore_mochi_backend::write_model_data");
        return;
    }
    model->m_impl.m_model_data_bulk << remote_bulk.on(req.get_endpoint());
    req.respond(flamestore_status::OK());
    msync(model->m_impl.m_model_data, model->m_impl.m_model_size, MS_SYNC);
#endif
}

void flamestore_mochi_backend::read_model_data(
        const tl::request& req,
        const std::string& model_name,
        const std::string& model_signature,
        const tl::bulk& remote_bulk)
{
#if 0
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
        m_logger->trace("Leaving flamestore_mochi_backend::read_model_data");
        return;
    }
    m_logger->info("Pushing data to model \"{}\"", model_name);
    model->m_impl.m_model_data_bulk >> remote_bulk.on(req.get_endpoint());
    req.respond(flamestore_status::OK());
#endif
}

void flamestore_mochi_backend::write_optimizer_data(
        const tl::request& req,
        const std::string& model_name,
        const std::string& optimizer_signature,
        tl::bulk& remote_bulk) 
{
#if 0
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_mochi_backend::write_optimizer_data");
        return;
    }

    lock_guard_t guard(model->m_mutex);
    if(model->m_optimizer_signature != optimizer_signature) {
        m_logger->error("Unmatching signatures when writing optimizer for model \"{}\"", model_name);
        req.respond(flamestore_status(
                    TSRA_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving flamestore_mochi_backend::write_optimizer_data");
        return;
    }
    m_logger->info("Pulling data from model optimizer \"{}\"", model_name);
    model->m_impl.m_optimizer_data_bulk << remote_bulk.on(req.get_endpoint());
    req.respond(flamestore_status::OK());
    msync(model->m_impl.m_optimizer_data, model->m_impl.m_optimizer_size, MS_SYNC);
#endif
}

void flamestore_mochi_backend::read_optimizer_data(
        const tl::request& req,
        const std::string& model_name,
        const std::string& optimizer_signature,
        tl::bulk& remote_bulk) 
{
#if 0
    auto model = _find_model(model_name);
    if(model == nullptr) {
        m_logger->error("Model \"{}\" does not exist", model_name);
        req.respond(flamestore_status(
                    TSRA_ENOEXISTS,
                    "No model found with provided name"));
        m_logger->trace("Leaving flamestore_mochi_backend::read_optimizer_data");
        return;
    }

    lock_guard_t guard(model->m_mutex);
    if(model->m_optimizer_signature != optimizer_signature) {
        m_logger->error("Unmatching signatures when reading optimizer for model \"{}\"", model_name);
        req.respond(flamestore_status(
                    TSRA_ESIGNATURE,
                    "Unmatching signatures"));
        m_logger->trace("Leaving flamestore_mochi_backend::read_optimizer_data");
        return;
    }
    m_logger->info("Pushing data to model optimizer \"{}\"", model_name);
    model->m_impl.m_optimizer_data_bulk >> remote_bulk.on(req.get_endpoint());
    req.respond(flamestore_status::OK());
#endif
}
