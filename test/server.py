from __future__ import print_function
import sys
sys.path.append('build/lib.linux-x86_64-3.6')
from pymargo.core import Engine
from flamestore.server import Provider

if __name__ == '__main__':
    engine   = Engine('tcp://localhost:12345')
    provider = Provider(engine, 0, loglevel=1, backend="localfs")
    address, provider_id = provider.get_connection_info()
    engine.wait_for_finalize()
