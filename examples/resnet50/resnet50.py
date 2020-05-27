# code coming from Resnet-keras tutorial
# https://github.com/priya-dwivedi/Deep-Learning/blob/master/resnet_keras/resnets_utils.py

import numpy as np
import h5py
import sys
import os
sys.path.append('build/lib.linux-x86_64-3.6')
os.environ['KERAS_BACKEND'] = 'tensorflow'
from tensorflow.keras.optimizers import SGD, Adam
from tensorflow.keras import applications
from tensorflow.keras.models import Model
from tensorflow.keras.layers import Dense, Dropout, GlobalAveragePooling2D


sys.setrecursionlimit(1500)


def __convert_to_one_hot(Y, C):
    """
    Converts a vector to one-hot.
    """
    Y = np.eye(C)[Y.reshape(-1)].T
    return Y


def load_dataset(train_file=None, test_file=None,
                 train_set_x=None, train_set_y=None,
                 test_set_x=None, test_set_y=None,
                 list_classes=None):
    """
    Loads the datasets from the HDF5 files located in the data directory.
    """
    train_dataset = None
    test_dataset = None
    X_train = None
    Y_train = None
    X_test = None
    Y_test = None
    classes = None

    if(train_file is not None):
        if(train_set_x is None or train_set_y is None):
            print('Error in load_dataset:',
                  ' train_set_x and train_set_y must be specified')
            sys.exit(-1)
        train_dataset = h5py.File(train_file, "r")
        train_set_x_orig = np.array(train_dataset[train_set_x][:])
        train_set_y_orig = np.array(train_dataset[train_set_y][:])
        if(list_classes is not None):
            classes = np.array(train_dataset[list_classes][:])
        train_set_y_orig = \
            train_set_y_orig.reshape((1, train_set_y_orig.shape[0]))
        X_train = train_set_x_orig/255.
        Y_train = __convert_to_one_hot(train_set_y_orig, 6).T

    if(test_file is not None):
        if(test_set_x is None or test_set_y is None):
            print('Error in load_dataset:',
                  'test_set_x and test_set_y must be specified')
            sys.exit(-1)
        test_dataset = h5py.File(test_file, "r")
        test_set_x_orig = np.array(test_dataset[test_set_x][:])
        test_set_y_orig = np.array(test_dataset[test_set_y][:])
        if(list_classes is not None):
            classes = np.array(test_dataset[list_classes][:])
        test_set_y_orig = \
            test_set_y_orig.reshape((1, test_set_y_orig.shape[0]))
        X_test = test_set_x_orig/255.
        Y_test = __convert_to_one_hot(test_set_y_orig, 6).T

    result = dict()
    result['x_train'] = X_train
    result['y_train'] = Y_train
    result['x_test'] = X_test
    result['y_test'] = Y_test
    result['classes'] = classes

    return result


def create_model(img_shape=(64, 64), num_classes=6, dropout=0.7):
    """
    Creates the model with either a ResNet50 base of a VGG16 base.
    If trainable is set to a number X, only the last X layers will
    be trainable.
    """
    img_height, img_width = img_shape
    base_model = applications.resnet50.ResNet50(
                    weights='imagenet',
                    include_top=False,
                    input_shape=(img_height, img_width, 3))
    x = base_model.output
    x = GlobalAveragePooling2D()(x)
    x = Dropout(dropout)(x)
    predictions = Dense(num_classes, activation='softmax')(x)
    model = Model(inputs=base_model.input, outputs=predictions)
    return model


def build_model(model, optimizer='Adam'):
    """
    Compiles the model.
    optimizer can be 'Adam' or 'SGD'.
    """
    opt = None
    if optimizer == 'Adam':
        opt = Adam(lr=0.0001)
    elif optimizer == 'SGD':
        opt = SGD(lr=0.0001, momentum=0.9, nesterov=False)
    else:
        print('Unkown optimizer '+optimizer)
        sys.exit(-1)
    model.compile(optimizer=opt,
                  loss='categorical_crossentropy',
                  metrics=['accuracy'])


def train_model(model, dataset, batch_size=32,
                epochs=1, callbacks=[], verbose=1):
    model.fit(dataset['x_train'], dataset['y_train'],
              batch_size=batch_size,
              epochs=epochs,
              verbose=verbose,
              callbacks=callbacks)


def evaluate_model(model, dataset, verbose=0):
    score = model.evaluate(dataset['x_test'],
                           dataset['y_test'],
                           verbose=verbose)
    return score
                           

if __name__ == '__main__':
    path = os.path.dirname(os.path.abspath(__file__))+'/data'
    dataset = load_dataset(
            train_file=path+'/train_signs.h5',
            test_file=path+'/test_signs.h5',
            train_set_x='train_set_x',
            train_set_y='train_set_y',
            test_set_x='test_set_x',
            test_set_y='test_set_y',
            list_classes='list_classes')
    model = create_model()
    build_model(model)
    train_model(model, dataset)
