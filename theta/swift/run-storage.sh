#!/bin/bash

WORKSPACE=$1
STORAGEPATH=$2
SIZE=${3:-0}

if [ "$SIZE" -ne "0" ]; then
	echo "Formating storage space on workers"
	flamestore format --path=${STORAGEPATH} --size=${SIZE} --debug
fi

echo "Start FlameStore workers"
flamestore run --storage=${STORAGEPATH}/flamestore.pmem --debug --workspace=${WORKSPACE}
