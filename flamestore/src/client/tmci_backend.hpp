#ifndef __DUMMY_BACKEND_HPP
#define __DUMMY_BACKEND_HPP

#include <tmci/backend.hpp>

namespace flamestore {

class MochiBackend : public tmci::Backend {

    static std::vector<char> m_data; // tmp

    public:

    MochiBackend(const char* config) {
        std::cout << "[FlameStore] Initialization, config: " << config << std::endl;
    }

    ~MochiBackend() = default;

    virtual int Save(const std::vector<std::reference_wrapper<const tensorflow::Tensor>>& tensors) {
        std::cout << "[FlameStore] Saving " << tensors.size() << " tensors:" << std::endl;
        size_t total_size = 0;
        for(const tensorflow::Tensor& t : tensors) {
            std::cout << "  data=" << (void*)t.tensor_data().data() << " size=" << t.tensor_data().size() << std::endl;
            total_size += t.tensor_data().size();
        }
        m_data.resize(total_size);
        size_t offset = 0;
        for(const tensorflow::Tensor& t : tensors) {
            memcpy(m_data.data() + offset, (void*)t.tensor_data().data(), t.tensor_data().size());
            offset += t.tensor_data().size();
        }
        return 0;
    }
    virtual int Load(const std::vector<std::reference_wrapper<const tensorflow::Tensor>>& tensors) {
        std::cout << "[FlameStore] Loading " << tensors.size() << " tensors:" << std::endl;
        size_t offset = 0;
        for(const tensorflow::Tensor& t : tensors) {
            std::cout << "  data=" << (void*)t.tensor_data().data() << " size=" << t.tensor_data().size() << std::endl;
            memcpy((void*)t.tensor_data().data(), m_data.data() + offset, t.tensor_data().size());
            offset += t.tensor_data().size();
        }
        return 0;
    }
};

}

#endif
