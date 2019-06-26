import logging
import time

try: # Python >= 3.7
    from time import time_ns
except: # Python <= 3.6
    from time import time as _time_
    time_ns = lambda: int(_time_() * 1e9)

class LogRecord(logging.LogRecord):
    def __init__(self, *args, **kwargs):
        self.created_ns = time_ns() # Fetch precise timestamp
        super().__init__(*args, **kwargs)

class Formatter(logging.Formatter):
    default_nsec_format = '%s.%09d'
    def formatTime(self, record, datefmt=None):
        if datefmt is not None: # Do not handle custom formats here ...
            return super().formatTime(record, datefmt) # ... leave to original implementation
        ct = self.converter(record.created_ns / 1e9)
        t = time.strftime(self.default_time_format, ct)
        s = self.default_nsec_format % (t, record.created_ns - (record.created_ns // 10**9) * 10**9)
        return s

def setup_logging(level=logging.INFO):
    logging.setLogRecordFactory(LogRecord)
    logging.basicConfig(level=level)
    log_formater = Formatter('[%(asctime)s] [flamestore_client:%(name)s] [%(levelname)s] %(message)s')
    ch = logging.StreamHandler()
    ch.setLevel(level)
    ch.setFormatter(log_formater)
    logger = logging.getLogger()
    logger.handlers = [ch]
    logging.addLevelName(logging.DEBUG, 'debug')
    logging.addLevelName(logging.INFO, 'info')
    logging.addLevelName(logging.ERROR, 'error')
    logging.addLevelName(logging.WARNING, 'warning')
    logging.addLevelName(logging.CRITICAL, 'critical')
