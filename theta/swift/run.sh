export PROJECT=radix-io
export QUEUE=debug-cache-quad
export WALLTIME=00:10:00
export PPN=1
export PROCS=4

export TURBINE_PRELAUNCH="module load intelpython36/2019.3.075
	source $HOME/flamestore/spack/share/spack/setup-env.sh
	export MODULEPATH=$MODULEPATH:$HOME/flamestore/spack/share/spack/modules/cray-cnl6-mic_knl
	spack load -r flamestore
	export MPICH_GNI_NDREG_ENTRIES=1024"

export TURBINE_DIRECTIVE="echo Turbine directive working"

WORKFLOW=workflow.swift
swift-t -m theta \
	-p \
	-V \
	-t w \
	./$WORKFLOW --workspace=$HOME/flamestore/myworkspace
