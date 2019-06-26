import functools

def trace(func):
    @functools.wraps(func)
    def wrapper(self, *args, **kwargs):
        try:
            self.logger.debug('Entering %s', func.__name__)
            xx = func(self, *args, **kwargs)
            self.logger.debug('Leaving %s', func.__name__)
            return xx
        except Exception as err:
            self.logger.error('Exception encountered in %s: %s', func.__name__, str(err))
            raise
    return wrapper
