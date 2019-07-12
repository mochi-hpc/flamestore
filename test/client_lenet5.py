from __future__ import print_function
import sys
import json
sys.path.append('build/lib.linux-x86_64-3.6')
import pymargo
from pymargo.core import Engine
from flamestore.client import Client
from flamestore.callbacks import RemoteCheckpointCallback
import lenet5
import keras
from keras import backend as K
import logging
from flamestore import log

def train(engine, provider_addr, provider_id):
    print('=========== TRAINING PHASE ===========')
    print('==> Initializing client')
    client = Client(engine)
    print('==> Initializing provider handle')
    provider = client.lookup(provider_addr, provider_id)
    print('==> Loading dataset')
    dataset = lenet5.load_dataset()
    print('==> Creating LeNet5 model')
    model = lenet5.create_model(dataset['input_shape'], dataset['num_classes'])
    print('==> Compiling model')
    lenet5.build_model(model, optimizer='Adadelta')
    print('==> Creating RemoteCheckpointCallback object')
    callback = RemoteCheckpointCallback(model_name='MyModel',
                client=client, provider_addr=provider_addr, provider_id=provider_id,
                include_optimizer=True, frequency={'epoch':1})
    print('==> Starting model training')
    lenet5.train_model(model, dataset, 
            batch_size=128, epochs=1,
            output_name=None, verbose=1,
            callbacks=[callback])
    print('==> Evaluating the model')
    score = model.evaluate(dataset['x_test'], dataset['y_test'], verbose=1)
    print('==> Score is '+str(score))
    K.clear_session()

def evaluate(engine, provider_addr, provider_id):
    print('=========== EVALUATION PHASE =========')
    print('==> Initializing client')
    client = Client(engine)
    print('==> Initializing provider handle')
    provider = client.lookup(provider_addr, provider_id)
    print('==> Loading dataset')
    dataset = lenet5.load_dataset()
    print('==> Loading model from provider')
    model = client.load_model(provider, 'MyModel')
    print('==> Loading optimizer from provider')
    optimizer = client.load_optimizer(provider, 'MyModel')
    print('==> Compiling model')
    lenet5.build_model(model, optimizer=optimizer)
    print('==> Starting model evaluation')
    score = model.evaluate(dataset['x_test'], dataset['y_test'], verbose=1)
    print('==> Score is '+str(score))
    print('=========== SHUTTING DOWN ============')
    provider.shutdown()
    K.clear_session()

if __name__ == '__main__':
    log.setup_logging(level=logging.DEBUG)
    if(len(sys.argv) < 2):
        print("Usage: python client-lenet5.py <connection.txt> [train, evaluate, ...")
        sys.exit(-1)
    connection_file = sys.argv[1]
    with open(connection_file) as f:
        pr_addr = f.read()
    pr_id = 0
    commands = { 'train': train, 'evaluate': evaluate }
    with Engine('tcp', use_progress_thread=True, mode=pymargo.client) as engine:
        for cmd in sys.argv[2:]:
            commands[cmd](engine, pr_addr, pr_id)
