import sys
import random
from tensorflow.keras import backend as K
from flamestore.client import Client
import lenet5


def create_and_train_new_model(workspace, dataset):
    print('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    print('===> Creating Keras model')
    model = lenet5.create_model(input_shape=dataset['input_shape'],
                                num_classes=dataset['num_classes'])
    print('===> Building model')
    lenet5.build_model(model)
    print('===> Registering model')
    client.register_model('my_model', model, include_optimizer=True)
    print('===> Training model')
    lenet5.train_model(model, dataset, batch_size=128, epochs=1)
    print('===> Saving model data')
    client.save_weights('my_model', model, include_optimizer=True)
    print('===> Evaluating the model')
    score = lenet5.evaluate_model(model, dataset, verbose=0)
    print('===> Scores: '+str(score))
    del model
    K.clear_session()


def reload_and_eval_existing_model(workspace, dataset):
    print('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    print('===> Reloading model config')
    model = client.reload_model('my_model', include_optimizer=True)
    print('===> Rebuilding model')
    lenet5.build_model(model)
    print('===> Reloading model data')
    client.load_weights('my_model', model, include_optimizer=True)
    print('===> Evaluating the stored model')
    score = lenet5.evaluate_model(model, dataset, verbose=0)
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
    lenet5.build_model(model)
    print('===> Reloading model data')
    client.load_weights('my_duplicated_model', model, include_optimizer=True)
    print('===> Evaluating the stored model')
    score = lenet5.evaluate_model(model, dataset, verbose=0)
    print('===> Scores: '+str(score))
    del model
    K.clear_session()


if __name__ == '__main__':
    random.seed(1234)
    if(len(sys.argv) < 2):
        print("Usage: python client-lenet5.py <workspace>")
        sys.exit(-1)
    print('=> Loading MNIST dataset')
    dataset = lenet5.load_dataset()
    workspace = sys.argv[1]
    print('=> Workspace is '+workspace)
    print('=> Creating and training a new model')
    create_and_train_new_model(workspace, dataset)
    print('=> Reloading and evaluating existing model')
    reload_and_eval_existing_model(workspace, dataset)
    print('=> Duplicating and evaluating a model')
    duplicate_and_eval_existing_model(workspace, dataset)
