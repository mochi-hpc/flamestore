#ifndef __FLAMESTORE_SERVER_CONTEXT_H
#define __FLAMESTORE_SERVER_CONTEXT_H

#include <spdlog/spdlog.h>
#include <thallium.hpp>

namespace tl = thallium;

struct flamestore_server_context {
    spdlog::logger* m_logger = nullptr;
    tl::engine*     m_engine = nullptr;
};

#endif
