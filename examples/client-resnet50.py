import sys
import random
from tensorflow.keras import backend as K
from flamestore.client import Client
from models import resnet50


def __load_dataset():
    return resnet50.load_dataset(
        train_file='train_signs.h5',
        test_file='test_signs.h5',
        train_set_x='train_set_x',
        train_set_y='train_set_y',
        test_set_x='test_set_x',
        test_set_y='test_set_y',
        list_classes='list_classes')


def create_and_train_new_model(workspace, dataset):
    print('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    print('===> Creating Keras model')
    model = resnet50.create_model()
    print('===> Building model')
    resnet50.build_model(model)
    print('===> Registering model')
    client.register_model('my_model', model, include_optimizer=True)
    print('===> Training model')
    resnet50.train_model(model, dataset, batch_size=32, epochs=1)
    print('===> Saving model data')
    client.save_weights('my_model', model, include_optimizer=True)
    print('===> Evaluating the model')
    score = resnet50.evaluate_model(model, dataset, verbose=0)
    print('===> Scores: '+str(score))
    del model
    K.clear_session()


def reload_and_eval_existing_model(workspace, dataset):
    print('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    print('===> Reloading model config')
    model = client.reload_model('my_model', include_optimizer=True)
    print('===> Rebuilding model')
    resnet50.build_model(model)
    print('===> Reloading model data')
    client.load_weights('my_model', model, include_optimizer=True)
    print('===> Evaluating the stored model')
    score = resnet50.evaluate_model(model, dataset, verbose=0)
    print('===> Scores: '+str(score))
    del model
    K.clear_session()


def duplicate_and_eval_existing_model(workspace, dataset):
    print('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    print('===> Duplicating model')
    client.duplicate_model('my_model', 'my_duplicated_model')
    print('===> Reloading duplicate')
    model = client.reload_model('my_duplicated_model', include_optimizer=True)
    print('===> Rebuilding model')
    resnet50.build_model(model)
    print('===> Reloading model data')
    client.load_weights('my_duplicated_model', model, include_optimizer=True)
    print('===> Evaluating the stored model')
    score = resnet50.evaluate_model(model, dataset, verbose=0)
    print('===> Scores: '+str(score))
    del model
    K.clear_session()


if __name__ == '__main__':
    random.seed(1234)
    if(len(sys.argv) < 2):
        print("Usage: python client-lenet5.py <workspace>")
        sys.exit(-1)
    print('=> Loading MNIST dataset')
    dataset = __load_dataset()
    workspace = sys.argv[1]
    print('=> Workspace is '+workspace)
    print('=> Creating and training a new model')
    create_and_train_new_model(workspace, dataset)
    print('=> Reloading and evaluating existing model')
    reload_and_eval_existing_model(workspace, dataset)
    print('=> Duplicating and evaluating a model')
    duplicate_and_eval_existing_model(workspace, dataset)
