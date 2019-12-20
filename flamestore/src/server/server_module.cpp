#include <pybind11/stl.h>
#include "server/master_server.hpp"
#include "server/storage_server.hpp"

namespace py11 = pybind11;

PYBIND11_MODULE(_flamestore_server, m) {
    m.doc() = "FlameStore server C++ extension";
    py11::class_<flamestore::MasterServer>(m, "MasterServer")
        .def(py11::init<pymargo_instance_id, 
                        const std::string&,
                        const std::string&,
                        const std::string&,
                        int,
                        const flamestore::MasterServer::backend_config_t&>(),
                py11::arg("mid"),
                py11::arg("workspace")=std::string("."),
                py11::arg("backend")=std::string("master-memory"),
                py11::arg("logfile")=std::string(""),
                py11::arg("loglevel")=2,
                py11::arg("config")=py11::dict())
        .def("get_connection_info", &flamestore::MasterServer::get_connection_info);
    py11::class_<flamestore::StorageServer>(m, "StorageServer")
        .def(py11::init<pymargo_instance_id,
                        const std::string&,
                        const std::string&,
                        const std::string&,
                        int,
                        const flamestore::StorageServer::backend_config_t&>(),
                py11::arg("mid"),
                py11::arg("workspace")=std::string("."),
                py11::arg("backend")=std::string("master-memory"),
                py11::arg("logfile")=std::string(""),
                py11::arg("loglevel")=2,
                py11::arg("config")=py11::dict());
}
