# =============================================== #
# (C) 2018 The University of Chicago
#
# See COPYRIGHT in top-level directory.
# =============================================== #
import _flamestore_server


class MasterServer(_flamestore_server.MasterServer):

    def __init__(self, engine, *args, **kwargs):
        super().__init__(engine.get_internal_mid(), *args, **kwargs)


class StorageServer(_flamestore_server.StorageServer):

    def __init__(self, engine, *args, **kwargs):
        super().__init__(engine.get_internal_mid(), *args, **kwargs)
