#include <pybind11/stl.h>
#include "server.hpp"

namespace py11 = pybind11;

PYBIND11_MODULE(_flamestore_server, m) {
    m.doc() = "FlameStore server C++ extension";
    py11::class_<flamestore::Server>(m, "Server")
        .def(py11::init<pymargo_instance_id, 
                        const std::string&,
                        const std::string&,
                        int,
                        const flamestore::Server::backend_config_t&>(),
                py11::arg("mid"),
                py11::arg("backend")=std::string("memory"),
                py11::arg("logfile")=std::string(""),
                py11::arg("loglevel")=2,
                py11::arg("config")=py11::dict())
        .def("get_connection_info", &flamestore::Server::get_connection_info);
}
