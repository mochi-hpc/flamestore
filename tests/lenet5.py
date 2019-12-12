from __future__ import print_function
import sys
import os
os.environ['KERAS_BACKEND'] = 'tensorflow'
import shutil
import random
from tensorflow import keras
from tensorflow.keras.datasets import mnist
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout, Flatten
from tensorflow.keras.layers import Conv2D, MaxPooling2D
from tensorflow.keras.models import load_model
from tensorflow.keras import backend as K
import tensorflow as tf

def create_model(input_shape, num_classes):
    """
    This function is used to create the Keras model.
    input_shape should be a tuple representing the shape of the input.
    num_classes should be an int representing the number of possible
    classes output by the model.
    In the context of the MNIST dataset, input_shape should be
    (1, 28, 28) (or (28, 28, 1), depending on whether the channel is first
    or last) and num_classes should be 10.
    """
    model = Sequential()
    model.add(Conv2D(32, kernel_size=(3, 3),
                            activation='relu',
                            input_shape=input_shape,
                            name='conv2d_1'))
    model.add(Conv2D(64, (3, 3), activation='relu', name='conv2d_2'))
    model.add(MaxPooling2D(pool_size=(2, 2), name='maxpooling2d_1'))
    model.add(Dropout(0.25, name='dropout_1'))
    model.add(Flatten(name='flatten_1'))
    model.add(Dense(128, activation='relu', name='dense_1'))
    model.add(Dropout(0.5, name='dropout_2'))
    model.add(Dense(num_classes, activation='softmax', name='dense_2'))
    return model

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
    return { 'x_train' : x_train,
             'y_train' : y_train,
             'x_test'  : x_test,
             'y_test'  : y_test,
             'input_shape' : input_shape,
             'num_classes' : num_classes }

def build_model(model, optimizer=None):
    """
    This function compiles the model.
    """
    if optimizer is None:
        optimizer = 'Adam'
    model.compile(loss=keras.losses.categorical_crossentropy,
            optimizer=optimizer,
             metrics=['accuracy'])

def train_model(model, dataset, batch_size, epochs, output_name=None, verbose=1, callbacks=[]):
    """
    This function trains the provided model on the given dataset with
    a given batch size and number of epochs. If output_name is provided,
    a ModelCheckpointEveryBatch object will be used as callback to
    checkpoint the model at every batch.
    """
    model.fit(dataset['x_train'], dataset['y_train'],
            batch_size=batch_size,
            epochs=epochs,
            verbose=verbose,
            callbacks=callbacks)

def evaluate_model(model, dataset, verbose=1):
    """
    This function loads a list of models at a given path and evaluates them
    with the provided dataset. The dataset should be a dictionary containing
    at list the following entries: x_test, y_test, input_shape, num_classes.
    The function will return a dictionary mapping the model's name to
    a score composed of [ loss, accuracy ].
    """
    score = model.evaluate(dataset['x_test'], dataset['y_test'], verbose=verbose)
    return score

if __name__ == '__main__':
    random.seed(1234)
    print('===> Loading MNIST dataset')
    dataset = load_dataset()
    print('===> Creating Keras model')
    model = create_model(dataset['input_shape'], dataset['num_classes'])
    print('===> Building model')
    build_model(model)
    print('===> Training the model')
    train_model(model, dataset, batch_size=128, epochs=1)
    print('===> Evaluating the model')
    score = evaluate_model(model, dataset)
    print('===> Scores: '+str(score))
