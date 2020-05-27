#include "server/backend.hpp"

namespace flamestore {

std::unordered_map<std::string,
    std::function<
        std::unique_ptr<AbstractServerBackend>(const ServerContext&,
                        const AbstractServerBackend::config_type&)>>
        AbstractServerBackend::s_backend_factories;

}
