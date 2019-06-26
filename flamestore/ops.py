import tensorflow as tf
import os
import glob

__flamestore_directory = os.path.dirname(__file__) + '/..'
__flamestore_library   = glob.glob(
    __flamestore_directory + '/_flamestore_operations.*.so')
if len(__flamestore_library) == 0:
    raise ImportError(
        'Could not find lib with TensorFlow operations for Tensorchestra')
if len(__flamestore_library) > 1:
    raise ImportError(
        'Found multiple candidate libs for Tensorchestra operations')
__flamestore_library = __flamestore_library[0]

__flamestore_operations  = tf.load_op_library(__flamestore_library)

_write_model     = __flamestore_operations.write_model
_write_optimizer = __flamestore_operations.write_optimizer
_read_model      = __flamestore_operations.read_model
_read_optimizer  = __flamestore_operations.read_optimizer
