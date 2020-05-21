import _flamestore_admin
import json
import os.path
from . import util
import spdlog

logger = spdlog.ConsoleLogger("flamestore.admin")
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v")


class Admin(_flamestore_admin.Admin):
    """Admin class allowing access to FlameStore providers."""

    def __init__(self, engine=None, workspace='.'):
        """Constructor.

        Args:
            engine (pymargo.core.Engine): Py-Margo engine.
            workspace (str): path to a workspace.
        """
        path = os.path.abspath(workspace)
        if(not os.path.isdir(path+'/.flamestore')):
            logger.critical('Directory is not a FlameStore workspace')
            raise RuntimeError('Directory is not a FlameStore workspace')
        connectionfile = path + '/.flamestore/master'
        if(engine is None):
            import pymargo
            import pymargo.core
            with open(path+'/.flamestore/config.json') as f:
                config = json.loads(f.read())
            protocol = config['protocol']
            self._engine = pymargo.core.Engine(
                protocol,
                use_progress_thread=True,
                mode=pymargo.server)
        else:
            self._engine = engine
        super().__init__(self._engine._mid, connectionfile)
        logger.debug('Creating a Admin for workspace '+path)

    def __del__(self):
        self._cleanup_hg_resources()
        del self._engine
