import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3' 
import sys
import shutil
import random
import tensorflow.keras
from tensorflow.keras import backend as K
import benchmark
from mpi4py import MPI
import spdlog


logger = spdlog.ConsoleLogger("Benchmark")
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v")


def create_and_store_model(model_name, num_layers, layer_size):
    logger.info('=> Creating model {} with {} layers or {} neurons'.format(model_name, num_layers, layer_size))
    logger.info('===> Creating Keras model')
    model = benchmark.create_model(num_layers, layer_size)
    logger.info('===> Building model')
    benchmark.build_model(model)
    logger.info('===> Registering model')
    model_json = model.to_json()
    with open(model_name+'.json', "w+") as json_file:
        json_file.write(model_json)
    logger.info('===> Saving model data')
    model.save_weights(model_name+'.h5')
    logger.info('=> Done')
    del model
    K.clear_session()


def reload_model(model_name):
    logger.info('=> Reloading model {}'.format(model_name))
    logger.info('===> Reloading model config')
    with open(model_name+'.json', "r") as json_file:
        model = tensorflow.keras.models.model_from_json(json_file.read())
    logger.info('===> Reloading model data')
    model.load_weights(model_name+'.h5')
    logger.info('=> Done')
    del model
    K.clear_session()


def duplicate_model(model_name, new_model_name):
    logger.info('=> Duplicating model {} into {}'.format(model_name, new_model_name))
    logger.info('===> Duplicating model')
    shutil.copyfile(model_name+'.h5', new_model_name+'.h5')
    shutil.copyfile(model_name+'.json', new_model_name+'.json')
    logger.info('=> Done')


if __name__ == '__main__':
    random.seed(1234)
    if(len(sys.argv) < 2):
        logger.info("Usage: python client-lenet5.py <operationfile>")
        sys.exit(-1)
    opfile = sys.argv[1]
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
                create_and_store_model(model_name, num_layers, layer_size)
        elif words[0] == 'DUPLICATE':
            if(len(words) != 3):
                logger.info('#> Error: wrong number of arguments for DUPLICATE command (expected 2)')
            else:
                model_name = words[1].replace('$', str(comm.Get_rank()))
                new_model_name = words[2].strip().replace('$', str(comm.Get_rank()))
                duplicate_model(model_name, new_model_name)
        elif words[0] == 'LOAD':
            if(len(words) != 2):
                logger.info('#> Error: wrong number of arguments for LOAD command (expected 1)')
            else:
                model_name = words[1].strip().replace('$', str(comm.Get_rank()))
                reload_model(model_name)
