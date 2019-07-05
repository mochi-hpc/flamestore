from __future__ import print_function
import sys
import os.path
sys.path.append('build/lib.linux-x86_64-3.6')
import json
from pymargo.core import Engine
from flamestore.server import Provider as BackendProvider

def create_connection_file(engine, filename):
    with open(filename,'w+') as f:
        f.write(str(engine.addr()))

def run_flamestore_provider(engine, config):
    loglevel = config.get('loglevel', 1)
    backend  = config.get('backend', 'localfs')
    backend_provider_id = config['flamestore'].get('provider_id', 0)
    backend_config = config['localfs']
    backend_provider = BackendProvider(engine, backend_provider_id, 
            config=backend_config, loglevel=loglevel, backend=backend)
    engine.wait_for_finalize()

if __name__ == '__main__':
    if(len(sys.argv) != 3):
        print('Usage: python server-mem.py <config.json> <connection.txt>')
        sys.exit(-1)
    configfile = sys.argv[1]
    with open(configfile) as f:
        config = json.loads(f.read())
    protocol = config.get('protocol', 'tcp')
    if(not 'flamestore' in config):
        print(f'{configfile} doesn\' seem to be a flamestore configuration file')
        sys.exit(-1)
    engine = Engine(protocol)
    create_connection_file(engine, sys.argv[2])
    engine.enable_remote_shutdown()
    run_flamestore_provider(engine, config)
