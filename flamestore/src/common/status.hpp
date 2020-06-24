#ifndef __FLAMESTORE_STATUS_H
#define __FLAMESTORE_STATUS_H

#include <map>
#include <thallium/serialization/stl/string.hpp>

namespace flamestore {

struct Status {

    int32_t     m_code = 0;
    std::string m_message;

    Status() = default;

    Status(int32_t code, std::string message)
    : m_code(code)
    , m_message(std::move(message)) {}

    template<typename A>
    void serialize(A& ar) {
        ar & m_code;
        ar & m_message;
    }

    static Status OK(const std::string& msg) {
        return Status{0, msg};
    }

    static const Status& OK() {
        static Status ok{ 0, "OK" };
        return ok;
    }

    std::pair<int32_t, std::string> copy_to_pair() const {
        return std::make_pair(m_code, m_message);
    }

    std::pair<int32_t, std::string> move_to_pair() {
        return std::make_pair(m_code, std::move(m_message));
    }
};

enum StatusCode {
    FLAMESTORE_OK         = 0,
    FLAMESTORE_EEXISTS    = 1,
    FLAMESTORE_ENOEXISTS  = 2,
    FLAMESTORE_ESIGNATURE = 3,
    FLAMESTORE_EMKDIR     = 4,
    FLAMESTORE_EIO        = 5,
    FLAMESTORE_EBACKEND   = 6,
    FLAMESTORE_EBAKE      = 7,
    FLAMESTORE_ENOIMPL    = 8,
    FLAMESTORE_EOTHER
};

}

#endif
