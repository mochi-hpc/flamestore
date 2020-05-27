#ifndef __FLAMESTORE_MODEL_H
#define __FLAMESTORE_MODEL_H

#include <string>
#include <vector>
#include <thallium.hpp>

namespace tl = thallium;

template<typename T>
struct flamestore_model {
   
    tl::mutex         m_mutex;
    std::string       m_name;
    std::string       m_model_config;
    std::string       m_model_signature;
    T                 m_impl;

    flamestore_model() = default;
    flamestore_model(const flamestore_model&) = delete;
    flamestore_model& operator=(const flamestore_model&) = delete;
    flamestore_model(flamestore_model&&) = delete;
    flamestore_model& operator=(flamestore_model&&) = delete;
    ~flamestore_model() = default;
};

#endif
