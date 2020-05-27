# 1. Setting up spack

Spack must be setup to use the GNU programming environment by default.
If you already have spack setup, you can skip the following steps.
It may however be useful to reinstall it anyway (possibly in a new
		folder), to make sure you have the latest version and to install the
FlameStore dependencies in an environment that is separated from your
normal spack installation. In the following, I'll assume all the
commands are run from a $HOME/flamestore directory.

## 1.1. Clone spack

	git clone https://github.com/spack/spack.git
		
## 1.2. Load the right environment modules

	module swap PrgEnv-intel PrgEnv-gnu
	module load cce
				
## 1.3. Load spack and export MODULEPATH

	. spack/share/spack/setup-env.sh
	export MODULEPATH=$MODULEPATH:$HOME/flamestore/spack/share/spack/modules/cray-cnl6-mic_knl
						
## 1.4. Clone and add sds-repo

	git clone https://xgitlab.cels.anl.gov/sds/sds-repo.git
	spack repo add sds-repo
								
## 1.5. Edit your packages.yaml file

Open $HOME/.spack/cray/packages.yaml. This file lists packages that
are already available in the system, and/or preferences for variants
and versions of packages to be installed. See the attached file, which
you may simply copy, or copy individual entries as appropriate.
The important entries for this work are cmake, python, py-numpy,
py-h5py, py-mpi4py, py-setuptools, py-pip, py-tensorflow, ssg,
mercury, py-tmci, flamestore, and libfabric. These entries will
instruct spack to use the Python environment that is already available
on Theta, and which options to use when building the Mochi-related
libraries.

# 2. Install FlameStore's dependencies

## 2.1. Load spack

If you have quit the terminal used for the section above, re-run the
commands in subsections 1.2 and 1.3 to make sure spack is in your
environment, as well as the correct environment modules.

## 2.2. Load python packages

	module load intelpython36/2019.3.075
	module load datascience/tensorflow-2.0

## 2.3. Install FlameStore and its dependencies

	spack install flamestore
											
If, when building flamestore, you encounter an error indicating that
tmci/backend.hpp could not be found, go to the installation directory
of py-tmci (i.e. if your spack installation is in ~/flamestore, that would be
~/flamestore/spack/opt/spack/cray-cnl6-mic_knl/gcc-7.3.0/py-tmci-develop-<some-hash>/)
and move include/python3.6m/tmci one folder back, to have include/tmci instead.
(this should be corrected in spack soon).

# 3. Preparing a FlameStore workspace

## 3.1. Load FlameStore

If you've restarted a terminal since completing section 2, re-run commands
from 1.2, 1.3, 2.2, then run:

	spack load -r flamestore

## 3.2. Creating a FlameStore workspace

Create a folder, and initialize it as a FlameStore workspace.

	mkdir myworkspace
	cd myworkspace
	flamestore init --protocol=ofi+gni --backend=bake
						
You should see a message like the following:

> [2020-01-20 14:39:55.725171816] [FlameStore] [info] Initialized FlameStore workspace (/gpfs/mira-home/mdorier/flamestore/myworkspace/.flamestore)

# 4. Running FlameStore

You may look at the theta/run.qsub file in the FlameStore repository to see
how flamestore is deployed. This script cam be used on 3 nodes. 1 node deploys
a flamestore master, 1 runs a storage server, and 1 node is left to run the
command that shuts everything down.

