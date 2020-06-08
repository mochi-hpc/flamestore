#!/bin/bash

rm -rf *.log

echo "Starting Client application"
mpirun -np 2 python hdf5-benchmark.py operations.txt
