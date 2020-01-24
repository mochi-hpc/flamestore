export PROJECT=radix-io
export QUEUE=debug-cache-quad
export WALLTIME=00:10:00
export PPN=1
export PROCS=3

export TURBINE_DIRECTIVE="module load intelpython36/2019.3.075 
		   spack load -r flamestore" 

WORKFLOW=workflow.swift
swift-t -m theta \
	-p \
	-V \
	-t w \
	./$WORKFLOW --workspace=../../../myworkspace
