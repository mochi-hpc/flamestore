#ifndef __DUMMY_BACKEND_HPP
#define __DUMMY_BACKEND_HPP

#include <tmci/backend.hpp>
#include <json/json.h>
#include "client/client.hpp"

namespace flamestore {

class MochiBackend : public tmci::Backend {

    Client*     m_client = nullptr;
    std::string m_model_name;
    std::string m_signature;

    public:

    MochiBackend(const char* config) {
        std::cout << "[FlameStore] MochiBackend constructor" << std::endl;
        std::stringstream ss(config);
        Json::Value root;
        ss >> root;
        std::cout << "model name is " << root["model_name"].asString() << std::endl;
        std::cout << "flamestore client is " << root["flamestore_client"].asString() << std::endl;
        m_client = Client::from_id(root["flamestore_client"].asString());
        m_model_name = root["model_name"].asString();
        m_signature = root["signature"].asString();
    }

    ~MochiBackend() = default;

    virtual int Save(const std::vector<std::reference_wrapper<const tensorflow::Tensor>>& tensors) {
        std::cout << "[FlameStore] Saving " << tensors.size() << " tensors:" << std::endl;
        // TODO check that m_client is valid
        std::vector<std::pair<void*,size_t>> segments;
        segments.reserve(tensors.size());
        size_t total_size = 0;
        for(const tensorflow::Tensor& t : tensors) {
            total_size += t.tensor_data().size();
            segments.emplace_back((void*)t.tensor_data().data(), (size_t)t.tensor_data().size());
        }
        Client::return_status status = m_client->write_model_data(m_model_name, m_signature, segments, total_size);
        return status.first;
    }
    virtual int Load(const std::vector<std::reference_wrapper<const tensorflow::Tensor>>& tensors) {
        std::cout << "[FlameStore] Loading " << tensors.size() << " tensors:" << std::endl;
        // TODO check that m_client is valid
        std::vector<std::pair<void*,size_t>> segments;
        segments.reserve(tensors.size());
        size_t total_size = 0;
        for(const tensorflow::Tensor& t : tensors) {
            total_size += t.tensor_data().size();
            segments.emplace_back((void*)t.tensor_data().data(), (size_t)t.tensor_data().size());
        }
        Client::return_status status = m_client->read_model_data(m_model_name, m_signature, segments, total_size);
        return status.first;
    }
};

}

#endif
