import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3' 
import sys
import random
from tensorflow.keras import backend as K
from flamestore.client import Client
import benchmark
from mpi4py import MPI
import spdlog


logger = spdlog.ConsoleLogger("Benchmark")
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v")


def create_and_store_model(workspace, model_name, num_layers, layer_size):
    logger.info('=> Creating model {} with {} layers or {} neurons'.format(model_name, num_layers, layer_size))
    logger.info('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    logger.info('===> Creating Keras model')
    model = benchmark.create_model(num_layers, layer_size)
    logger.info('===> Building model')
    benchmark.build_model(model)
    logger.info('===> Registering model')
    client.register_model(model_name, model, include_optimizer=True)
    logger.info('===> Saving model data')
    client.save_weights(model_name, model, include_optimizer=True)
    del model
    K.clear_session()


def reload_model(workspace, model_name):
    logger.info('=> Reloading model {}'.format(model_name))
    logger.info('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    logger.info('===> Reloading model config')
    model = client.reload_model(model_name, include_optimizer=True)
    logger.info('===> Rebuilding model')
    benchmark.build_model(model)
    logger.info('===> Reloading model data')
    client.load_weights(model_name, model, include_optimizer=True)
    del model
    K.clear_session()


def duplicate_model(workspace, model_name, new_model_name):
    logger.info('=> Duplicating model {} into {}'.format(model_name, new_model_name))
    logger.info('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    logger.info('===> Duplicating model')
    client.duplicate_model(model_name, new_model_name)


if __name__ == '__main__':
    random.seed(1234)
    if(len(sys.argv) < 3):
        logger.info("Usage: python client-lenet5.py <workspace> <operationfile>")
        sys.exit(-1)
    workspace = sys.argv[1]
    opfile = sys.argv[2]
    logger.info('=> Workspace is '+workspace)
    comm = MPI.COMM_WORLD
    for line in open(opfile):
        # remove comments
        line = line.split('#')[0]
        # decompose command in words
        words = line.split()
        comm.barrier()
        if words[0] == 'CREATE':
            if(len(words) != 4):
                logger.info('#> Error: wrong number of arguments for CREATE command (expected 3)')
            else:
                model_name = words[1].replace('$', str(comm.Get_rank()))
                num_layers = int(words[2])
                layer_size = int(words[3].strip())
                create_and_store_model(workspace, model_name, num_layers, layer_size)
        elif words[0] == 'DUPLICATE':
            if(len(words) != 3):
                logger.info('#> Error: wrong number of arguments for DUPLICATE command (expected 2)')
            else:
                model_name = words[1].replace('$', str(comm.Get_rank()))
                new_model_name = words[2].strip().replace('$', str(comm.Get_rank()))
                duplicate_model(workspace, model_name, new_model_name)
        elif words[0] == 'LOAD':
            if(len(words) != 2):
                logger.info('#> Error: wrong number of arguments for LOAD command (expected 1)')
            else:
                model_name = words[1].strip().replace('$', str(comm.Get_rank()))
                reload_model(workspace, model_name)
