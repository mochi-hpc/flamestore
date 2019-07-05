from __future__ import print_function
import sys
import os.path
sys.path.append('build/lib.linux-x86_64-3.6')
from mpi4py import MPI
import json
from pymargo.core import Engine
from pybake.server import BakeProvider
import pybake.server
import pysdskv.server
from pysdskv.server import SDSKVProvider
from flamestore.server import Provider as BackendProvider

def make_and_share_backend_config(comm, engine, config, bake_targets=[]):
    this_addr = str(engine.addr())
    if(comm.Get_rank() == 0):
        # create sdskv config for the backend
        sdskv_config = dict()
        sdskv_config['address']  = this_addr
        sdskv_config['provider'] = config['sdskv']['provider_id']
        sdskv_config['database'] = config['sdskv']['database']['name']
        # create the bake config for the backend
        bake_config = []
        for target in bake_targets:
            bake_config.append({
                    'address'  : this_addr,
                    'provider' : config['bake']['provider_id'],
                    'target'   : str(target) })
        for i in range(1,comm.Get_size()):
            targets = comm.recv(source=i, tag=0)
            bake_config.extend(targets)
    else:
        sdskv_config = None
        bake_config = []
        for target in bake_targets:
            bake_config.append({
                    'address'  : this_addr,
                    'provider' : config['bake']['provider_id'],
                    'target'   : str(target) })
        comm.send(bake_config, dest=0, tag=0)

    backend_config = { 'config' : json.dumps({
            'sdskv' : sdskv_config,
            'bake'  : bake_config
        })
    }
    return backend_config

def init_storage_provider(comm, engine, config):
    config = config['bake']
    bake_provider_id = config.get('provider_id', 0)
    bake_provider = BakeProvider(engine, bake_provider_id)
    bake_target_path = config['target']['path'].format(rank=comm.Get_rank())
    bake_target_size = config['target']['size']
    if(not os.path.isfile(bake_target_path)):
        pybake.server.make_pool(bake_target_path, bake_target_size, 0o664)
    bake_target = bake_provider.add_storage_target(bake_target_path)
    return bake_provider, bake_target

def run_metadata_providers(comm, engine, config):
    loglevel = config.get('loglevel', 1)
    sdskv_provider_id = config['sdskv'].get('provider_id', 0)
    sdskv_provider = SDSKVProvider(engine, sdskv_provider_id)
    sdskv_db_type = config['sdskv']['database'].get('type', 'leveldb')
    sdskv_db_type = getattr(pysdskv.server, sdskv_db_type, pysdskv.server.leveldb)
    sdskv_db_path = config['sdskv']['database']['path']
    sdskv_db_name = config['sdskv']['database']['name']
    sdskv_provider.attach_database(sdskv_db_name, sdskv_db_path, sdskv_db_type)
    bake_targets = []
    if(comm.Get_size() == 1):
        print("Warning: communicator has size 1, Bake will run with metadata providers")
        bake_provider, bake_target = init_storage_provider(comm, engine, config)
        bake_targets = [bake_target]
    backend  = config.get('backend', 'localfs')
    backend_provider_id = config['flamestore'].get('provider_id', 0)
    backend_config = make_and_share_backend_config(comm, engine, config, bake_targets)
    backend_provider = BackendProvider(engine, backend_provider_id, 
            config=backend_config, loglevel=loglevel, backend=backend)
    engine.wait_for_finalize()

def run_storage_providers(comm, engine, config):
    bake_provider, bake_target = init_storage_provider(comm, engine, config)
    addr = engine.addr()
    make_and_share_backend_config(comm, engine, config, [bake_target])
    engine.wait_for_finalize()

if __name__ == '__main__':
    if(len(sys.argv) != 2):
        print('Usage: mpirun <mpiargs> python server.py <config.json> <connection.json>')
        sys.exit(-1)
    configfile = sys.argv[1]
    with open(configfile) as f:
        config = json.loads(f.read())
    protocol = config.get('protocol', 'tcp')
    if(not 'flamestore' in config):
        print(f'{configfile} doesn\' seem to be a flamestore configuration file')
        sys.exit(-1)
    engine = Engine(protocol)
    engine.enable_remote_shutdown()
    comm = MPI.COMM_WORLD
    if(comm.Get_rank() == 0):
        run_metadata_providers(comm, engine, config)
    else:
        run_storage_providers(comm, engine, config)
