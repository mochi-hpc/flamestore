#include <pybind11/pybind11.h>
#include "client.hpp"

namespace py11 = pybind11;

PYBIND11_MODULE(_flamestore_client, m) {
    m.doc() = "FlameStore client C++ extension";
    py11::class_<flamestore::Client>(m, "Client")
        .def(py11::init<pymargo_instance_id, const std::string&>())
        .def("_get_id", &flamestore::Client::get_id,
                "Gets and identifier to the client that "
                "can be used to retrieve the client back in C++.")
        .def("shutdown", &flamestore::Client::shutdown,
                "Shuts down the FlameStore service.")
        .def("_register_model", &flamestore::Client::register_model,
                "Registers a model.")
        .def("_reload_model", &flamestore::Client::reload_model,
                "Reloads a model.")
        .def("_duplicate_model", &flamestore::Client::duplicate_model,
                "Duplicates a model.")
        .def("_cleanup_hg_resources", &flamestore::Client::cleanup_hg_resources,
                "Cleanup internal HG resources")
        ;
}
