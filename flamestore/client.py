import _flamestore_client
import json
import os.path
from functools import reduce
from tensorflow.keras import backend as K
from tensorflow.keras.models import model_from_json
import tensorflow.keras.optimizers as optimizers
from . import util
import spdlog

logger = spdlog.ConsoleLogger("FlameStore")
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v")

class Client(_flamestore_client.Client):
    """Client class allowing access to FlameStore providers."""

    def __init__(self, engine, workspace='.'):
        """Constructor.

        Args:
            engine (pymargo.core.Engine): Py-Margo engine.
            workspace (str): path to a workspace.
        """
        path = os.path.abspath(workspace)
        if(not os.path.isdir(path+'/.flamestore')):
            logger.critical('Directory is not a FlameStore workspace')
            raise RuntimeError('Directory is not a FlameStore workspace')
        configfile = path + '/.flamestore/config.json'
        super().__init__(engine._mid, configfile)
        logger.debug('Creating a Client for workspace '+path)

    def register_model(self,
                       model_name,
                       model,
                       include_optimizer=True):
        """
        Registers a model in a provider.

        Args:
            provider (flamestore.ProviderHandle): provider handle.
            model_name (string): model name.
            model (keras Model): model.
            include_optimizer (bool): whether to register the optimizer.
        """
        model_config = { 'model' : model.to_json() }
        if(include_optimizer):
            model_config['optimizer'] = json.dumps({
                'name': type(model.optimizer).__name__,
                'config': model.optimizer.get_config()})
        else:
            model_config['optimizer'] = None
        model_size = 0
        for l in model.layers:
            for w in l.weights:
                s = w.dtype.size * reduce(lambda x, y: x * y, w.shape)
                model_size += s
        if(include_optimizer):
            model._make_train_function()
            for w in model.optimizer.weights:
                if(len(w.shape) == 0):
                    model_size += w.dtype.size
                else:
                    s = w.dtype.size * reduce(lambda x, y: x * y, w.shape)
                    model_size += s
        if(include_optimizer):
            model_signature = util._compute_signature(model, model.optimizer)
        else:
            model_signature = util._compute_signature(model)
        logger.debug('Issuing register_model RPC')
        model_config = json.dumps(model_config)
        status, message = self._register_model(model_name,
                                               model_config,
                                               model_size,
                                               model_signature)
        if(status != 0):
            logger.error(message)
            raise RuntimeError(message)

    def reload_model(self, model_name, include_optimizer=True):
        """Loads a model given its name from FlameStore. This method
        will only reload the model's architecture, not the model's data.
        The load_weights method in TMCI should be used to load the model's
        weights.

        Args:
            model_name (string): name of the model.
            include_optimizer (bool): whether to also reload the optimizer.
        Returns:
            a keras Model instance.
        """
        logger.debug('Issuing _reload_model RPC')
        status, message = self._reload_model(model_name, include_optimizer)
        if(status != 0):
            logger.error(message)
            raise RuntimeError(message)
        config = json.loads(message)
        model_config = config['model']
        logger.debug('Rebuilding model')
        model = model_from_json(model_config)
        if(include_optimizer):
            optimizer_config = json.loads(config['optimizer'])
            if(optimizer_config is None):
                raise RuntimeError('Requested model wasn\'t stored with its optimizer')
            optimizer_name = optimizer_config['name']
            optimizer_config = optimizer_config['config']
            logger.debug('Rebuilding optimizer')
            cls = getattr(optimizers, optimizer_name)
            model.optimizer = cls(**optimizer_config)
        return model

