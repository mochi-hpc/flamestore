#ifndef __FLAMESTORE_COMMON_H
#define __FLAMESTORE_COMMON_H

#include <pybind11/pybind11.h>

namespace py11 = pybind11;
namespace tl = thallium;

struct margo_instance;
typedef struct margo_instance* margo_instance_id;
typedef py11::capsule pymargo_instance_id;

#define MID2CAPSULE(__mid)   py11::capsule((void*)(__mid), "margo_instance_id", nullptr)
#define CAPSULE2MID(__caps)  (margo_instance_id)(__caps)

#endif
