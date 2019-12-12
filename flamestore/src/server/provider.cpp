#include "server/provider.hpp"

namespace flamestore {

void MasterProvider::on_register_model(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name,
        std::string& model_config,
        std::string& optimizer_config,
        std::size_t model_data_size,
        std::size_t optimizer_data_size,
        std::string& model_signature,
        std::string& optimizer_signature)
{
    // TODO
}

void MasterProvider::on_reload_model(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name)
{
    // TODO
}

void MasterProvider::on_write_model_data(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name,
        const std::string& model_signature,
        tl::bulk& remote_bulk)
{
    // TODO
}

void MasterProvider::on_read_model_data(
        const tl::request& req,
        const std::string& client_addr,
        const std::string& model_name,
        const std::string& model_signature,
        tl::bulk& remote_bulk)
{
    // TODO
}

}
