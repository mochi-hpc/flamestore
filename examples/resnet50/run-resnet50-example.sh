#!/bin/bash

workspace=./workspace
storagepath=/dev/shm
storagesize=1G
backend=mochi
protocol=ofi+tcp

if [ ! -f "test_signs.h5" ]; then
    echo "Downloading test_signs.h5"
    wget https://github.com/priya-dwivedi/Deep-Learning/raw/master/resnet_keras/datasets/test_signs.h5
fi
if [ ! -f "train_signs.h5" ]; then
    echo "Downloading train_signs.h5"
    wget https://github.com/priya-dwivedi/Deep-Learning/raw/master/resnet_keras/datasets/train_signs.h5
fi

rm -rf ${workspace} *.log ${storagepath}/flamestore.pmem

echo "Creating FlameStore workspace"
mkdir ${workspace}
flamestore init  --workspace ${workspace} \
                 --backend ${backend} \
                 --protocol ${protocol}

echo "Starting FlameStore master"
flamestore run --master --debug --workspace ${workspace} > master.log 2>&1 &
while [ ! -f ${workspace}/.flamestore/master.ssg.id ]; do sleep 1; done

echo "Starting FlameStore worker"
flamestore run --storage \
               --format --path ${storagepath} --size ${storagesize} \
               --debug --workspace ${workspace} > worker.log 2>&1 &

echo "Starting Client application"
python client-resnet50.py ${workspace}

echo "Shutting down FlameStore"
flamestore shutdown --workspace=${workspace} --debug

wait
