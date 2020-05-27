import sys
import random
from tensorflow.keras import backend as K
from flamestore.client import Client
import benchmark


def create_and_store_model(workspace, model_name, num_layers, layer_size):
    print('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    print('===> Creating Keras model')
    model = benchmark.create_model(num_layers, layer_size)
    print('===> Building model')
    benchmark.build_model(model)
    print('===> Registering model')
    client.register_model(model_name, model, include_optimizer=True)
    print('===> Saving model data')
    client.save_weights(model_name, model, include_optimizer=True)
    del model
    K.clear_session()


def reload_model(workspace, model_name):
    print('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    print('===> Reloading model config')
    model = client.reload_model(model_name, include_optimizer=True)
    print('===> Rebuilding model')
    benchmark.build_model(model)
    print('===> Reloading model data')
    client.load_weights(model_name, model, include_optimizer=True)
    del model
    K.clear_session()


def duplicate_model(workspace, model_name, new_model_name):
    print('===> Creating FlameStore client')
    client = Client(workspace=workspace)
    print('===> Duplicating model')
    client.duplicate_model(model_name, new_model_name)


if __name__ == '__main__':
    random.seed(1234)
    if(len(sys.argv) < 3):
        print("Usage: python client-lenet5.py <workspace> <operationfile>")
        sys.exit(-1)
    workspace = sys.argv[1]
    opfile = sys.argv[2]
    print('=> Workspace is '+workspace)
    for line in open(opfile):
        # remove comments
        line = line.split('#')[0]
        # decompose command in words
        words = line.split()
        if words[0] == 'CREATE':
            print('=> Creating a new model')
            if(len(words) != 4):
                print('#> Error: wrong number of arguments for CREATE command (expected 3)')
            else:
                model_name = words[1]
                num_layers = int(words[2])
                layer_size = int(words[3].strip())
                create_and_store_model(workspace, model_name, num_layers, layer_size)
        elif words[0] == 'DUPLICATE':
            print('=> Duplicating a model')
            if(len(words) != 3):
                print('#> Error: wrong number of arguments for DUPLICATE command (expected 2)')
            else:
                model_name = words[1]
                new_model_name = words[2].strip()
                duplicate_model(workspace, model_name, new_model_name)
        elif words[0] == 'LOAD':
            print('=> Reloading existing model')
            if(len(words) != 2):
                print('#> Error: wrong number of arguments for LOAD command (expected 1)')
            else:
                model_name = words[1].strip()
                reload_model(workspace, model_name)
