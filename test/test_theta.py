from __future__ import print_function
import sys
import os.path
sys.path.append('build/lib.linux-x86_64-3.5')
import json
from mpi4py import MPI
from pymargo.core import Engine
import pymargo
from flamestore.server import Provider as BackendProvider
import logging
from flamestore import log
import client_lenet5

def init_storage_provider(engine, config):
    import pybake.server
    from pybake.server import BakeProvider
    config = config['bake']
    bake_provider_id = config.get('provider_id', 0)
    bake_provider = BakeProvider(engine, bake_provider_id)
    bake_target_path = config['target']['path']
    bake_target_size = config['target']['size']
    if(not os.path.isfile(bake_target_path)):
        pybake.server.make_pool(bake_target_path, bake_target_size, 0o664)
    bake_target = bake_provider.add_storage_target(bake_target_path)
    bake_config = [{
            'address'  : str(engine.addr()),
            'provider' : bake_provider_id,
            'target'   : str(bake_target)
    }]
    return bake_provider, bake_config

def init_metadata_provider(engine, config):
    import pysdskv.server
    from pysdskv.server import SDSKVProvider
    config = config['sdskv']
    sdskv_provider_id = config.get('provider_id', 0)
    sdskv_provider = SDSKVProvider(engine, sdskv_provider_id)
    sdskv_db_type = config['database'].get('type', 'leveldb')
    sdskv_db_type = getattr(pysdskv.server, sdskv_db_type, pysdskv.server.leveldb)
    sdskv_db_path = config['database']['path']
    sdskv_db_name = config['database']['name']
    sdskv_provider.attach_database(sdskv_db_name, sdskv_db_path, sdskv_db_type)
    sdskv_config = {
        'address' : str(engine.addr()),
        'provider' : sdskv_provider_id,
        'database' : sdskv_db_name
    }
    return sdskv_provider, sdskv_config

def run_flamestore_provider(engine, config):
    loglevel = config.get('loglevel', 1)
    backend  = config.get('backend', 'memory')
    if(backend == 'memory'):
        backend_config = {}
    elif(backend == 'mmapfs'):
        backend_config = config['localfs']
    elif(backend == 'mochi'):
        bake_provider, bake_config = init_storage_provider(engine, config)
        sdskv_provider, sdskv_config = init_metadata_provider(engine, config)
        this_addr = str(engine.addr())
        backend_config = { 'config' : json.dumps({
                'sdskv' : sdskv_config,
                'bake'  : bake_config
                })
        }
    backend_provider_id = config['flamestore'].get('provider_id', 0)
    backend_provider = BackendProvider(engine, backend_provider_id, 
            config=backend_config, loglevel=loglevel, backend=backend)
    engine.wait_for_finalize()

def server(configfile):
    with open(configfile) as f:
        config = json.loads(f.read())
    protocol = config.get('protocol', 'tcp')
    if(not 'flamestore' in config):
        print('{} doesn\' seem to be a flamestore configuration file'.format(configfile))
        sys.exit(-1)
    engine = Engine(protocol)
    addr_str = str(engine.addr())
    MPI.COMM_WORLD.send(addr_str, dest=1, tag=0)
    engine.enable_remote_shutdown()
    run_flamestore_provider(engine, config)

def client(configfile, cmds):
    log.setup_logging(level=logging.DEBUG)
    with open(configfile) as f:
        config = json.loads(f.read())
    protocol = config.get('protocol', 'tcp')
    pr_addr = MPI.COMM_WORLD.recv(source=0, tag=0)
    pr_id = 0
    with Engine(protocol, use_progress_thread=True, mode=pymargo.client) as engine:
        for cmd in cmds:
            f = getattr(client_lenet5, cmd, None)
            if(f is not None):
                f(engine, pr_addr, pr_id)

if __name__ == '__main__':
    if(len(sys.argv) < 3 or MPI.COMM_WORLD.Get_size() != 2):
        print('Usage: mpirun -np 2 python test_theta_mem.py <config.json> [ train evaluate shutdown ...')
        sys.exit(-1)
    if(MPI.COMM_WORLD.Get_rank() == 0):
        server(sys.argv[1])
    else:
        client(sys.argv[1], sys.argv[2:])
