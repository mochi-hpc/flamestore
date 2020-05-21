#include <pybind11/pybind11.h>
#include "admin.hpp"

namespace py11 = pybind11;

PYBIND11_MODULE(_flamestore_admin, m) {
    m.doc() = "FlameStore admin C++ extension";
    py11::class_<flamestore::Admin>(m, "Admin")
        .def(py11::init<pymargo_instance_id, const std::string&>())
        .def("shutdown", &flamestore::Admin::shutdown,
                "Shuts down the FlameStore service.")
        .def("_cleanup_hg_resources", &flamestore::Admin::cleanup_hg_resources,
                "Cleanup internal HG resources")
        ;
}
