#!/bin/bash

WORKSPACE=$1
STORAGEPATH=$2
SIZE=$3

echo "Formating storage space on workers"
flamestore format --path=${STORAGEPATH} --size=${SIZE} --debug

echo "Start FlameStore workers"
flamestore run --storage=${STORAGEPATH}/flamestore.pmem --debug --workspace=${WORKSPACE}
