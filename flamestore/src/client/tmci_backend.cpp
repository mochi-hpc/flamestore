#include "tmci_backend.hpp"
//#include <pybind11/pybind11.h>

//namespace py11 = pybind11;

TMCI_REGISTER_BACKEND(flamestore, flamestore::MochiBackend);
/*
PYBIND11_MODULE(_flamestore_tmci_backend, m) {
    m.doc() = "FlameStore backend for TMCI";
}
*/

std::vector<char> flamestore::MochiBackend::m_data;
