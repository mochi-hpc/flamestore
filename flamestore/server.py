import _flamestore_server


class Provider(_flamestore_server.Provider):

    def __init__(self, engine, *args, **kwargs):
        self.__engine_wrapper = \
            _flamestore_server.EngineWrapper(engine._mid)
        super().__init__(self.__engine_wrapper, *args, **kwargs)
