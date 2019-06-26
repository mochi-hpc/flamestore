#include <pybind11/pybind11.h>
#include "client.hpp"

namespace py11 = pybind11;

PYBIND11_MODULE(_flamestore_client, m) {
    m.doc() = "Tensorchestra client C++ extension";
    py11::class_<flamestore_provider_handle>(m, "ProviderHandle")
        .def("shutdown", &flamestore_provider_handle::shutdown,
                "Shuts down the provider pointed by this provider handle.")
        .def("get_addr", &flamestore_provider_handle::get_addr,
                "Get the address of the provider as a string.")
        .def("get_id", &flamestore_provider_handle::get_id,
                "Get the id of the provider.");
    py11::class_<flamestore_client>(m, "Client")
        .def(py11::init<pymargo_instance_id>())
        .def("_get_id",              &flamestore_client::get_id,
                "Gets and identifier to the client that can be used to retrieve the client back in C++.")
        .def("lookup",               &flamestore_client::lookup,
                "Creates a ProviderHandle from an address and a provider id.")
        .def("_register_model",       &flamestore_client::register_model,
                "Registers a model.")
        .def("get_model_config",     &flamestore_client::get_model_config,
                "Retrieves the configuration of the model.")
        .def("get_optimizer_config", &flamestore_client::get_optimizer_config,
                "Retrieves the configuration of the optimizer.")
        ;
}
