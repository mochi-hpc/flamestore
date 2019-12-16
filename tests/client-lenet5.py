import sys
import random
import logging
import pymargo
from pymargo.core import Engine
import tensorflow
from tensorflow.keras import backend as K
import json
#import tmci.checkpoint
from flamestore.client import Client
from flamestore import log
import lenet5

def test(engine, connection_file):
    print('===> Creating FlameStore client')
    client = Client(engine, workspace)
    print('===> Loading MNIST dataset')
    dataset = lenet5.load_dataset()
    print('===> Creating Keras model')
    opt = True
    model = lenet5.create_model(dataset['input_shape'], dataset['num_classes'])
    print('===> Building model')
    lenet5.build_model(model)
    print('===> Registering model')
    client.register_model('my_model', model, include_optimizer=opt)
    print('===> Training model')
    lenet5.train_model(model, dataset, batch_size=128, epochs=1)
    print('===> Saving model data')
#    tmci_params = { 'model_name' : 'my_model', 'flamestore_client' : str(client) }
#    tmci_params = json.dumps(tmci_params)
#    tmci.checkpoint.save_weights(model, backend='flamestore', config=tmci_params, include_optimizer=opt)
    client.save_weights('my_model', model, include_optimizer=opt)
    print('===> Evaluating the model')
    score = lenet5.evaluate_model(model, dataset, verbose=0)
    print('===> Scores: '+str(score))
    del model
    K.clear_session()
    print('===> Reloading model config')
    model = client.reload_model('my_model', include_optimizer=opt)
    print('===> Rebuilding model')
    lenet5.build_model(model)
    print('===> Reloading model data')
#    tmci.checkpoint.load_weights(model, backend='flamestore', config=tmci_params, include_optimizer=opt)
    client.load_weights('my_model', model, include_optimizer=opt)
    print('===> Evaluating the stored model')
    score = lenet5.evaluate_model(model, dataset, verbose=0)
    print('===> Scores: '+str(score))

if __name__ == '__main__':
    log.setup_logging(level=logging.DEBUG)
    random.seed(1234)
    if(len(sys.argv) < 2):
        print("Usage: python client-lenet5.py <workspace>")
        sys.exit(-1)
    workspace = sys.argv[1]
    print('===> Creating Margo engine')
    with Engine('ofi+tcp', use_progress_thread=True, mode=pymargo.client) as engine:
        test(engine, workspace)
