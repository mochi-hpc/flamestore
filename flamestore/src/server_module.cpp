#include "server.hpp"

namespace py11 = pybind11;

PYBIND11_MODULE(_flamestore_server, m) {
    m.doc() = "Tensorchestra server C++ extension";
    py11::class_<flamestore_engine_wrapper>(m, "EngineWrapper")
        .def(py11::init<pymargo_instance_id>());
    py11::class_<flamestore_provider>(m, "Provider")
        .def(py11::init<flamestore_engine_wrapper&, uint16_t, const std::string&, const std::string&, int>(),
                py11::arg("engine_wrapper"),
                py11::arg("provider_id"),
                py11::arg("backend")=std::string("memory"),
                py11::arg("logfile")=std::string(""),
                py11::arg("loglevel")=2)
        .def("get_connection_info", &flamestore_provider::get_connection_info);
}
