#!/bin/bash
CLIENT=(flamestore/callbacks.py flamestore/client.py flamestore/decorators.py flamestore/__init__.py flamestore/log.py flamestore/ops.py flamestore/util.py flamestore/src/client* flamestore/src/tensorflow/*)
SERVER=(flamestore/server.py flamestore/src/backend* flamestore/src/*backend.cpp flamestore/src/common.h flamestore/src/model.hpp flamestore/src/server* flamestore/src/status.hpp)
echo "************** CLIENT ***************"
sloccount "${CLIENT[@]}"

echo "************** SERVER ***************"
sloccount "${SERVER[@]}"
