from distutils.core import setup
from distutils.extension import Extension
from distutils.sysconfig import get_config_vars
from distutils.command.build_clib import build_clib
from distutils.command.build_ext import build_ext
theta = False
if theta:
    tf = None
else:
    import tensorflow as tf
import pybind11
import pkgconfig
import os
import os.path
import sys

cxxflags = ['-std=c++14', '-g']
if not theta:
    cxxflags.append('-D_GLIBCXX_USE_CXX11_ABI=0')
    cxxflags.append('-D_GLIBCXX_USE_CXX14_ABI=0')

def get_pybind11_include():
    path = os.path.dirname(pybind11.__file__)
    return '/'.join(path.split('/')[0:-4] + ['include'])

def get_tensorflow_include():
    if tf is None:
        path = '/soft/datascience/tensorflow/tf1.13/tensorflow'
    else:
        path = os.path.dirname(tf.__file__)
    return path+'/include'

def get_tensorflow_lib_dir():
    if tf is None:
        path = '/soft/datascience/tensorflow/tf1.13/tensorflow'
    else:
        path = os.path.dirname(tf.__file__)
    return path

def get_tensorflow_library():
    if tf is None:
        return 'tensorflow_framework'
    if tf.__version__ < '1.14.0':
        return 'tensorflow_framework'
    else:
        return ':libtensorflow_framework.so.1'

(opt,) = get_config_vars('OPT')
os.environ['OPT'] = " ".join(flag for flag in opt.split() if flag != '-Wstrict-prototypes')

thallium     = pkgconfig.parse('thallium')
spdlog       = pkgconfig.parse('spdlog')
bake_client  = pkgconfig.parse('bake-client')
bake_server  = pkgconfig.parse('bake-server')
sdskv_client = pkgconfig.parse('sdskv-client')
sdskv_server = pkgconfig.parse('sdskv-server')
jsoncpp      = pkgconfig.parse('jsoncpp')

flamestore_op_module_libraries    = [get_tensorflow_library()]     \
                                  + thallium['libraries']
flamestore_op_module_library_dirs = [get_tensorflow_lib_dir()]     \
                                  + thallium['library_dirs']
flamestore_op_module_include_dirs = [get_tensorflow_include(),'.'] \
                                  + thallium['include_dirs']
flamestore_op_module = Extension('_flamestore_operations',
        ['flamestore/src/tensorflow/write_model.cpp',
         'flamestore/src/tensorflow/write_optimizer.cpp',
         'flamestore/src/tensorflow/read_model.cpp',
         'flamestore/src/tensorflow/read_optimizer.cpp',
         'flamestore/src/client.cpp' ],
        libraries=flamestore_op_module_libraries,
        library_dirs=flamestore_op_module_library_dirs,
        include_dirs=flamestore_op_module_include_dirs,
        extra_compile_args=cxxflags,
        depends=[])

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
                                      + ['.']
flamestore_server_module = Extension('_flamestore_server',
        ['flamestore/src/server.cpp',
         'flamestore/src/backend.cpp',
         'flamestore/src/memory_backend.cpp',
         'flamestore/src/mochi_backend.cpp',
         'flamestore/src/mmapfs_backend.cpp',
         'flamestore/src/server_module.cpp'
        ],
        libraries=flamestore_server_module_libraries,
        library_dirs=flamestore_server_module_library_dirs,
        include_dirs=flamestore_server_module_include_dirs,
        extra_compile_args=cxxflags,
        depends=[])

flamestore_client_module_libraries    = thallium['libraries']
flamestore_client_module_library_dirs = thallium['library_dirs']
flamestore_client_module_include_dirs = thallium['include_dirs'] + ['.']
flamestore_client_module = Extension('_flamestore_client',
        ['flamestore/src/client.cpp',
         'flamestore/src/client_module.cpp'],
        libraries=flamestore_client_module_libraries,
        library_dirs=flamestore_client_module_library_dirs,
        include_dirs=flamestore_client_module_include_dirs,
        extra_compile_args=cxxflags,
        depends=[])

setup(name='flamestore',
      version='0.2',
      author='Matthieu Dorier',
      description='''Python library to access TensorFlow tensors in C++''',
      ext_modules=[ flamestore_op_module, 
                    flamestore_server_module,
                    flamestore_client_module ],
      packages=['flamestore']
    )
