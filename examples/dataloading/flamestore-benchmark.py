import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'
import sys
import random
import tensorflow.keras as keras
from tensorflow.keras import backend as K
from tensorflow.keras.datasets import mnist
from flamestore.client import Client
from flamestore.dataset import Descriptor
from mpi4py import MPI
import spdlog


logger = spdlog.ConsoleLogger("Benchmark")
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v")


def load_dataset():
    """
    This function loads the desired dataset from Keras. It returns
    a dataset dictionary with keys x_train, y_train, x_test, y_test,
    input_shape, and num_classes.
    """
    img_rows, img_cols = 28, 28
    num_classes = 10
    (x_train, y_train), (x_test, y_test) = mnist.load_data()
    if K.image_data_format() == 'channels_first':
        x_train = x_train.reshape(x_train.shape[0], 1, img_rows, img_cols)
        x_test = x_test.reshape(x_test.shape[0], 1, img_rows, img_cols)
        input_shape = (1, img_rows, img_cols)
    else:
        x_train = x_train.reshape(x_train.shape[0], img_rows, img_cols, 1)
        x_test = x_test.reshape(x_test.shape[0], img_rows, img_cols, 1)
        input_shape = (img_rows, img_cols, 1)
    x_train = x_train.astype('float32')
    x_test = x_test.astype('float32')
    x_train /= 255
    x_test /= 255
    y_train = keras.utils.to_categorical(y_train, num_classes)
    y_test = keras.utils.to_categorical(y_test, num_classes)
    return {'x_train': x_train,
            'y_train': y_train,
            'x_test': x_test,
            'y_test': y_test,
            'input_shape': input_shape,
            'num_classes': num_classes}


def run_test(comm, workspace):
    logger.info('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    dataset = load_dataset()
    logger.info('===> Creating dataset descriptor')
    train_descriptor = Descriptor()
    train_descriptor.add_field('x_train', dataset['x_train'].dtype, dataset['x_train'].shape[1:])
    train_descriptor.add_field('y_train', dataset['y_train'].dtype, dataset['y_train'].shape[1:])
    test_descriptor = Descriptor()
    test_descriptor.add_field('x_test', dataset['x_test'].dtype, dataset['x_test'].shape[1:])
    test_descriptor.add_field('y_test', dataset['y_test'].dtype, dataset['y_test'].shape[1:])
    logger.info('===> Registering training dataset')
    client.register_dataset('training', train_descriptor, metadata='this is some metadata')
    client.register_dataset('testing', test_descriptor)
    d = client.get_dataset_descriptor('training')
    logger.info('===> Getting the descriptor back: {}'.format(d))
    s = client.get_dataset_size('training')
    logger.info('===> Size of the training dataset is: {}'.format(s))
    m = client.get_dataset_metadata('training')
    logger.info('===> Training dataset metadata is: {}'.format(m))

if __name__ == '__main__':
    random.seed(1234)
    if(len(sys.argv) < 2):
        logger.info("Usage: python flamestore-benchmark.py <workspace>")
        sys.exit(-1)
    workspace = sys.argv[1]
    logger.info('=> Workspace is '+workspace)
    comm = MPI.COMM_WORLD
    run_test(comm, workspace)
