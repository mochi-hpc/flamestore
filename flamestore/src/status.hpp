#ifndef __FLAMESTORE_STATUS_H
#define __FLAMESTORE_STATUS_H

#include <map>
#include <thallium/serialization/stl/string.hpp>

struct flamestore_status {

    int32_t     m_error = 0;
    std::string m_message;

    flamestore_status() = default;

    flamestore_status(int32_t error, std::string message)
    : m_error(error)
    , m_message(std::move(message)) {}

    template<typename A>
    void serialize(A& ar) {
        ar & m_error;
        ar & m_message;
    }

    static flamestore_status OK(const std::string& msg) {
        return flamestore_status{0, msg};
    }

    static const flamestore_status& OK() {
        static flamestore_status ok{ 0, "OK" };
        return ok;
    }

    std::pair<int32_t, std::string> copy_to_pair() const {
        return std::make_pair(m_error, m_message);
    }

    std::pair<int32_t, std::string> move_to_pair() {
        return std::make_pair(m_error, std::move(m_message));
    }
};

enum flamestore_status_code {
    FLAMESTORE_OK         = 0,
    FLAMESTORE_EEXISTS    = 1,
    FLAMESTORE_ENOEXISTS  = 2,
    FLAMESTORE_ESIGNATURE = 3,
    FLAMESTORE_EMKDIR     = 4,
    FLAMESTORE_EIO        = 5,
    FLAMESTORE_EOTHER
};

#endif
