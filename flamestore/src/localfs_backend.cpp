#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <mutex>
#include <map>
#include <fstream>
#include <spdlog/spdlog.h>
#include "model.hpp"
#include "backend.hpp"

namespace tl = thallium;

class flamestore_localfs_backend : public flamestore_backend {

    public:

        using name_t = std::string;
        using lock_guard_t = std::lock_guard<tl::mutex>;

    private:

        struct mmap_entry {
            void*       m_data = nullptr;
            std::size_t m_size = 0;

            ~mmap_entry() {
                if(m_data)
                    munmap(m_data, m_size);
            }
        };

    public:

        using model_t = flamestore_model<mmap_entry>;

    private:

        tl::engine*                                   m_engine;
        spdlog::logger*                               m_logger;
        mutable tl::rwlock                            m_models_rwlock;
        std::map<name_t, std::unique_ptr<model_t>>    m_models;
        std::string                                   m_path = ".";


        inline model_t* _reload_model_from_disk(const std::string& model_name) {
            std::stringstream basedir;
            basedir << m_path << "/" << model_name;
            // check if files exists
            std::string model_data_filename = basedir.str() + "/model.bin";
            std::string model_conf_filename = basedir.str() + "/model.json";
            std::string model_sig_filename  = basedir.str() + "/model.sig";
            std::string optimizer_data_filename = basedir.str() + "/optimizer.bin";
            std::string optimizer_conf_filename = basedir.str() + "/optimizer.json";
            std::string optimizer_sig_filename  = basedir.str() + "/optimizer.sig";

            m_logger->info("Searching for model \"{}\" on disk", model_name);
            if(access(model_data_filename.c_str(), F_OK)     == -1
            || access(model_conf_filename.c_str(), F_OK)     == -1
            || access(model_sig_filename.c_str(), F_OK)      == -1
            || access(optimizer_data_filename.c_str(), F_OK) == -1
            || access(optimizer_conf_filename.c_str(), F_OK) == -1
            || access(optimizer_sig_filename.c_str(), F_OK)  == -1) {
                m_logger->info("Model not found on disk");
                return nullptr;
            }
            // create model
            auto model = std::make_unique<model_t>();
            auto result = model.get();
            model->m_name = model_name;
            // read model config
            {
                std::ifstream t(model_conf_filename.c_str());
                t.seekg(0, std::ios::end);   
                model->m_model_config.reserve(t.tellg());
                t.seekg(0, std::ios::beg);
                model->m_model_config.assign(
                        (std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());
            }
            // read model signature
            {
                std::ifstream t(model_sig_filename.c_str());
                t.seekg(0, std::ios::end);   
                model->m_model_signature.reserve(t.tellg());
                t.seekg(0, std::ios::beg);
                model->m_model_signature.assign(
                        (std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());
            }
            // open, mmap, and create bulk for data
            {
                struct stat st;
                stat(model_data_filename.c_str(), &st);
                model->m_model_data.m_size = st.st_size;
                int fd = open(model_data_filename.c_str(), O_RDWR);
                model->m_model_data.m_data
                    = mmap(0, model->m_model_data.m_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                close(fd);
                try {
                    if(model->m_model_data.m_size != 0) {
                        std::vector<std::pair<void*, size_t>> model_data_ptr(1);
                        model_data_ptr[0].first  = (void*)(model->m_model_data.m_data);
                        model_data_ptr[0].second = model->m_model_data.m_size;
                        model->m_model_data_bulk = m_engine->expose(model_data_ptr, tl::bulk_mode::read_write);
                    }
                } catch(const tl::exception& e) {
                    m_logger->critical("Exception caught in flamestore_provider::_reload_model_from_disk: {}", e.what());
                    return nullptr;
                }
            }
            // read optimizer config
            {
                std::ifstream t(optimizer_conf_filename.c_str());
                t.seekg(0, std::ios::end);   
                model->m_optimizer_config.reserve(t.tellg());
                t.seekg(0, std::ios::beg);
                model->m_optimizer_config.assign(
                        (std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());
            }
            // read optimizer signature
            {
                std::ifstream t(optimizer_sig_filename.c_str());
                t.seekg(0, std::ios::end);   
                model->m_optimizer_signature.reserve(t.tellg());
                t.seekg(0, std::ios::beg);
                model->m_optimizer_signature.assign(
                        (std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());
            }
            // open, mmap, and create bulk for data
            {
                struct stat st;
                stat(optimizer_data_filename.c_str(), &st);
                model->m_optimizer_data.m_size = st.st_size;
                int fd = open(optimizer_data_filename.c_str(), O_RDWR);
                model->m_optimizer_data.m_data
                    = mmap(0, model->m_optimizer_data.m_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                close(fd);
                try {
                    if(model->m_optimizer_data.m_size != 0) {
                        std::vector<std::pair<void*, size_t>> optimizer_data_ptr(1);
                        optimizer_data_ptr[0].first  = (void*)(model->m_optimizer_data.m_data);
                        optimizer_data_ptr[0].second = model->m_optimizer_data.m_size;
                        model->m_optimizer_data_bulk = m_engine->expose(optimizer_data_ptr, tl::bulk_mode::read_write);
                    }
                } catch(const tl::exception& e) {
                    m_logger->critical("Exception caught in flamestore_provider::_reload_model_from_disk: {}", e.what());
                    return nullptr;
                }
            }
            // insert in the map
            m_models.emplace(model_name, std::move(model));
            m_logger->info("Found model \"{}\" on disk", model_name);
            return result;
        }


        inline model_t* _find_model(const std::string& model_name) {
            m_models_rwlock.rdlock();
            auto it = m_models.find(model_name);
            if(it == m_models.end()) {
                auto on_disk = _reload_model_from_disk(model_name);
                m_models_rwlock.unlock();
                return on_disk; // may be nullptr if not on disk
            } else  {
                auto result = it->second.get();
                m_models_rwlock.unlock();
                return result;
            }
        }


        inline model_t* _find_or_create_model(const std::string& model_name, bool& created) {
            m_models_rwlock.wrlock();
            auto it = m_models.find(model_name);
            if(it == m_models.end()) {
                auto on_disk = _reload_model_from_disk(model_name);
                if(on_disk) 
                    return on_disk;
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

        flamestore_localfs_backend(const flamestore_server_context& ctx, const flamestore_backend::config_type& config)
        : m_engine(ctx.m_engine)
        , m_logger(ctx.m_logger) {
            auto it = config.find("path");
            if(it != config.end()) {
                m_path  = it->second;
            }
        }

        flamestore_localfs_backend(const flamestore_backend&)            = delete;
        flamestore_localfs_backend(flamestore_backend&&)                 = delete;
        flamestore_localfs_backend& operator=(const flamestore_backend&) = delete;
        flamestore_localfs_backend& operator=(flamestore_backend&&)      = delete;
        ~flamestore_localfs_backend()                              = default;

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

REGISTER_FLAMESTORE_BACKEND("localfs",flamestore_localfs_backend);
        
void flamestore_localfs_backend::register_model(
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
        model->m_model_data.m_size   = model_data_size;
        model->m_optimizer_data.m_size = optimizer_data_size;

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
            model->m_model_data.m_data
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
            model->m_optimizer_data.m_data
                = mmap(0, optimizer_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            // XXX check mmap worked
            close(fd);
        }

        if(model->m_model_data.m_size != 0) {
            std::vector<std::pair<void*, size_t>> model_data_ptr(1);
            model_data_ptr[0].first  = (void*)(model->m_model_data.m_data);
            model_data_ptr[0].second = model->m_model_data.m_size;
            model->m_model_data_bulk = m_engine->expose(model_data_ptr, tl::bulk_mode::read_write);
        }

        if(model->m_optimizer_data.m_size != 0) {
            std::vector<std::pair<void*, size_t>> optimizer_data_ptr(1);
            optimizer_data_ptr[0].first  = (void*)(model->m_optimizer_data.m_data);
            optimizer_data_ptr[0].second = model->m_optimizer_data.m_size;
            model->m_optimizer_data_bulk = m_engine->expose(optimizer_data_ptr, tl::bulk_mode::read_write);
        }

    } catch(const tl::exception& e) {
        m_logger->critical("Exception caught in flamestore_provider::on_register_model: {}", e.what());
    }
}

void flamestore_localfs_backend::get_model_config(
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

void flamestore_localfs_backend::get_optimizer_config(
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

void flamestore_localfs_backend::write_model_data(
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
    msync(model->m_model_data.m_data, model->m_model_data.m_size, MS_SYNC);
}

void flamestore_localfs_backend::read_model_data(
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

void flamestore_localfs_backend::write_optimizer_data(
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
    msync(model->m_optimizer_data.m_data, model->m_optimizer_data.m_size, MS_SYNC);
}

void flamestore_localfs_backend::read_optimizer_data(
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
