export PROJECT=radix-io
export QUEUE=debug-cache-quad
export WALLTIME=00:10:00
export PPN=1
export PROCS=3

export TURBINE_DIRECTIVE="module load intelpython36/2019.3.075
export MODULEPATH=\$MODULEPATH:/home/mdorier/flamestore/spack/share/spack/modules/cray-cnl6-mic_knl
spack load -r flamestore"

export TURBINE_PRELAUNCH=$TURBINE_DIRECTIVE

WORKFLOW=workflow.swift
swift-t -m theta \
	-p \
	-V \
	-t w \
	./$WORKFLOW --workspace=/home/mdorier/flamestore/myworkspace
