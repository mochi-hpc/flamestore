from __future__ import print_function
import random
import os
os.environ['KERAS_BACKEND'] = 'tensorflow'
from tensorflow import keras
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense
from tensorflow.keras import backend as K


def create_model(num_layers, layer_size):
    """
    This function is used to create the Keras model.
    """
    model = Sequential()
    model.add(Dense(layer_size,
                    activation='relu',
                    input_shape=(layer_size,)))
    if num_layers > 1:
        for i in range(1, num_layers):
            if i == num_layers - 1:
                model.add(Dense(layer_size, activation='softmax'))
            else:
                model.add(Dense(layer_size, activation='relu'))
    return model


def build_model(model, optimizer=None):
    """
    This function compiles the model.
    """
    if optimizer is None:
        optimizer = 'Adam'
    model.compile(loss=keras.losses.categorical_crossentropy,
                  optimizer=optimizer,
                  metrics=['accuracy'])

