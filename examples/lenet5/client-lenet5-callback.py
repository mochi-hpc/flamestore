import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'
import sys
import random
from tensorflow.keras import backend as K
from flamestore.client import Client
from flamestore.callbacks import RemoteCheckpointCallback
import lenet5
import spdlog


logger = spdlog.ConsoleLogger("Benchmark")
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v")


def create_and_train_new_model(workspace, dataset):
    logger.info('===> Creating Keras model')
    model = lenet5.create_model(input_shape=dataset['input_shape'],
                                num_classes=dataset['num_classes'])
    logger.info('===> Building model')
    lenet5.build_model(model)
    logger.info('===> Creating FlameStore callback')
    checkpoint = RemoteCheckpointCallback('my_model', workspace)
    logger.info('===> Training model')
    lenet5.train_model(model, dataset, batch_size=128, epochs=1, callbacks=[checkpoint])
    logger.info('===> Evaluating the model')
    score = lenet5.evaluate_model(model, dataset, verbose=0)
    logger.info('===> Scores: '+str(score))
    del model
    K.clear_session()


def reload_and_continue_training_model(workspace, dataset):
    logger.info('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    logger.info('===> Creating Keras model')
    model = lenet5.create_model(input_shape=dataset['input_shape'],
                                num_classes=dataset['num_classes'])
    logger.info('===> Building model')
    lenet5.build_model(model)
    logger.info('===> Creating FlameStore callback')
    checkpoint = RemoteCheckpointCallback('my_model', workspace, restart=True)
    logger.info('===> Continue training the mode (this will reload its existing state)')
    lenet5.train_model(model, dataset, batch_size=128, epochs=1, callbacks=[checkpoint])
    logger.info('===> Evaluating the stored model')
    score = lenet5.evaluate_model(model, dataset, verbose=0)
    logger.info('===> Scores: '+str(score))
    del model
    K.clear_session()


def duplicate_and_continue_training_model(workspace, dataset):
    logger.info('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    logger.info('===> Creating Keras model')
    model = lenet5.create_model(input_shape=dataset['input_shape'],
                                num_classes=dataset['num_classes'])
    logger.info('===> Building model')
    lenet5.build_model(model)
    logger.info('===> Creating FlameStore callback')
    checkpoint = RemoteCheckpointCallback('my_model_duplicate', workspace,
                                          restart=True, duplicate_from='my_model')
    logger.info('===> Continue training the mode (this will reload its existing state)')
    lenet5.train_model(model, dataset, batch_size=128, epochs=1, callbacks=[checkpoint])
    logger.info('===> Evaluating the stored model')
    score = lenet5.evaluate_model(model, dataset, verbose=0)
    logger.info('===> Scores: '+str(score))
    del model
    K.clear_session()


if __name__ == '__main__':
    random.seed(1234)
    if(len(sys.argv) < 2):
        logger.info("Usage: python client-lenet5.py <workspace>")
        sys.exit(-1)
    logger.info('=> Loading MNIST dataset')
    dataset = lenet5.load_dataset()
    workspace = sys.argv[1]
    logger.info('=> Workspace is '+workspace)
    logger.info('=> Creating and training a new model')
    create_and_train_new_model(workspace, dataset)
    logger.info('=> Reloading and evaluating existing model')
    reload_and_continue_training_model(workspace, dataset)
    logger.info('=> Duplicating and evaluating a model')
    duplicate_and_continue_training_model(workspace, dataset)
