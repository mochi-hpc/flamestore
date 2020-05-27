#!/bin/bash
#COBALT -n 3
#COBALT -t 0:10:00
#COBALT --mode script
#COBALT -q debug-flat-quad

source `dirname "$0"`/common.sh
me=`basename "$0"`

export MPICH_GNI_NDREG_ENTRIES=1024

workspace=${1:-.}
storagepath=${2:-/dev/shm}
storagesize=${3:-1G}
pdomain=${4:-flamestore}

apmgr pdomain -c -u ${pdomain} || true

source $HOME/flamestore/spack/share/spack/setup-env.sh
export MODULEPATH=$MODULEPATH:$HOME/flamestore/spack/share/spack/modules/cray-cnl6-mic_knl

# find nodes in job.  We have to do this so that we can manually specify 
# in each aprun so that server ranks consitently run on node where we
# set up storage space
declare -a nodes=($(python /home/carns/bin/run_on_all_nids.py));

spack load -r flamestore

# ########################################################################
# MASTER SERVER
# ########################################################################

echo "Starting FlameStore's master"
aprun -cc none -n 1 -N 1 -p ${pdomain} -L ${nodes[0]} \
	flamestore run --master --debug --workspace=${workspace} > master.log 2>&1 &

echo "Waiting 30sec for FlameStore master to start up"
sleep 30

# ########################################################################
# STORAGE SERVERS
# ########################################################################

echo "Formating storage space on workers"
aprun -cc none -n 1 -N 1 -L ${nodes[1]} \
	flamestore format --path=${storagepath} --size=${storagesize} --debug

echo "Start FlameStore workers"
aprun -cc none -n 1 -N 1 -p ${pdomain} -L ${nodes[1]} \
	flamestore run --storage=${storagepath}/flamestore.pmem --debug --workspace=${workspace} > workers.log 2>&1 &

echo "FlameStore has started, waiting 30sec before shutting down"
sleep 30

# ########################################################################
# SHUTDOWN 
# ########################################################################

echo "Shutting down FlameStore"
aprun -cc none -n 1 -N 1 -p ${pdomain} -L ${nodes[2]} \
	flamestore shutdown --workspace=${workspace} --debug

echo "Waiting for FlameStore to shut down"
apmgr pdomain -r -u ${pdomain}
