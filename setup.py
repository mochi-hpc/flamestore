from distutils.core import setup
from distutils.extension import Extension
from distutils.sysconfig import get_config_vars
from distutils.command.build_clib import build_clib
from distutils.command.build_ext import build_ext
import json
import pybind11
import pkgconfig
import os
import os.path
import sys
import tmci

src_dir = os.path.dirname(os.path.abspath(__file__)) + '/flamestore/src'

tf_info = {
        'libraries'      : None,
        'library_dirs'   : None,
        'include_dirs'   : None,
        'extra_cxxflags' : None
        }
try:
    with open('tensorflow.json') as f:
        tf_info = json.loads(f.read())
except:
    pass
if tf_info['libraries'] is None:
    tf_info['libraries'] = [ ':libtensorflow_framework.so.2' ]
if tf_info['library_dirs'] is None:
    import tensorflow as tf
    path = os.path.dirname(tf.__file__)
    tf_info['library_dirs'] = [ path + '/../tensorflow_core' ]
if tf_info['include_dirs'] is None:
    import tensorflow as tf
    path = os.path.dirname(tf.__file__)
    tf_info['include_dirs'] = [ path + '/../tensorflow_core/include' ]

cxxflags = tf_info['extra_cxxflags']

def get_pybind11_include():
    path = os.path.dirname(pybind11.__file__)
    return '/'.join(path.split('/')[0:-4] + ['include'])

(opt,) = get_config_vars('OPT')
os.environ['OPT'] = " ".join(flag for flag in opt.split() if flag != '-Wstrict-prototypes')

thallium     = pkgconfig.parse('thallium')
spdlog       = pkgconfig.parse('spdlog')
bake_client  = pkgconfig.parse('bake-client')
bake_server  = pkgconfig.parse('bake-server')
sdskv_client = pkgconfig.parse('sdskv-client')
sdskv_server = pkgconfig.parse('sdskv-server')
jsoncpp      = pkgconfig.parse('jsoncpp')

flamestore_server_module_libraries    = thallium['libraries']        \
                                      + bake_client['libraries']     \
                                      + sdskv_client['libraries']    \
                                      + jsoncpp['libraries']
flamestore_server_module_library_dirs = thallium['library_dirs']     \
                                      + bake_client['library_dirs']  \
                                      + sdskv_client['library_dirs'] \
                                      + jsoncpp['library_dirs']
flamestore_server_module_include_dirs = thallium['include_dirs']     \
                                      + bake_client['include_dirs']  \
                                      + sdskv_client['include_dirs'] \
                                      + jsoncpp['include_dirs']      \
                                      + [ src_dir ]
flamestore_server_module = Extension('_flamestore_server',
        ['flamestore/src/server/backend.cpp',
         'flamestore/src/server/memory_backend.cpp',
        # 'flamestore/src/server/mochi_backend.cpp',
        # 'flamestore/src/server/mmapfs_backend.cpp',
         'flamestore/src/server/provider.cpp',
         'flamestore/src/server/server_module.cpp'
        ],
        libraries=flamestore_server_module_libraries,
        library_dirs=flamestore_server_module_library_dirs,
        include_dirs=flamestore_server_module_include_dirs,
        extra_compile_args=cxxflags,
        depends=[])

flamestore_client_module_libraries    = thallium['libraries'] + tf_info['libraries'] + [ ':'+tmci.get_library() ]
flamestore_client_module_library_dirs = thallium['library_dirs'] + tf_info['library_dirs']+ [ tmci.get_library_dir() ]
flamestore_client_module_include_dirs = thallium['include_dirs'] + [ src_dir ] + tf_info['include_dirs']
flamestore_client_module = Extension('_flamestore_client',
        ['flamestore/src/client/client.cpp',
         'flamestore/src/client/client_module.cpp',
         'flamestore/src/client/tmci_backend.cpp'],
        libraries=flamestore_client_module_libraries,
        library_dirs=flamestore_client_module_library_dirs,
        runtime_library_dirs=flamestore_client_module_library_dirs,
        include_dirs=flamestore_client_module_include_dirs,
        extra_compile_args=cxxflags,
        depends=[])

setup(name='flamestore',
      version='0.3',
      author='Matthieu Dorier',
      description='''Mochi service to store and load tensorflow models''',
      ext_modules=[ flamestore_client_module,
                    flamestore_server_module,
                  ],
      packages=['flamestore'],
      scripts=['bin/flamestore']
    )
