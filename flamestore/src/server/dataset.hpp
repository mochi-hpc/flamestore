#ifndef __FLAMESTORE_DESCRIPTOR_H
#define __FLAMESTORE_DESCRIPTOR_H

#include <string>
#include <vector>
#include <thallium.hpp>

namespace tl = thallium;

template<typename T>
struct flamestore_dataset {
   
    tl::mutex         m_mutex;
    std::string       m_name;
    std::string       m_descriptor;
    std::string       m_metadata;
    std::size_t       m_size;
    T                 m_impl;

    flamestore_dataset() = default;
    flamestore_dataset(const flamestore_dataset&) = delete;
    flamestore_dataset& operator=(const flamestore_dataset&) = delete;
    flamestore_dataset(flamestore_dataset&&) = delete;
    flamestore_dataset& operator=(flamestore_dataset&&) = delete;
    ~flamestore_dataset() = default;
};

#endif
