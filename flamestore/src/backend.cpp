#include "backend.hpp"

std::unordered_map<std::string,
            std::function<
                std::unique_ptr<flamestore_backend>(const flamestore_server_context&,
                        const flamestore_backend::config_type&)>> flamestore_backend::s_backend_factories;
