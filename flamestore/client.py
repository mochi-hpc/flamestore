import _flamestore_client
import json
from functools import reduce
from keras import backend as K
from keras.models import model_from_json
import keras.optimizers as optimizers
from . import ops
from . import util
import logging
from .decorators import trace

class Client(_flamestore_client.Client):
    """Client class allowing access to Tensorchestra providers."""

    def __init__(self, engine):
        """Constructor.

        Args:
            engine (pymargo.core.Engine): Py-Margo engine.
        """
        super().__init__(engine._mid)
        self.logger = logging.getLogger(type(self).__name__)
        self.logger.debug('Creating a client')

    @trace
    def register_model(self,
                       provider,
                       model_name,
                       model,
                       include_optimizer=True):
        """
        Registers a model in a provider.

        Args:
            provider (tensorchestra.ProviderHandle): provider handle.
            model_name (string): model name.
            model (keras Model): model.
            include_optimizer (bool): whether to register the optimizer.
        Returns:
            a pair (status, message) where status is an int (0 for success,
            other values for other error codes detailed in the message string).
        """
        model_config = model.to_json()
        optimizer_config = '{}'
        if(include_optimizer):
            optimizer_config = json.dumps({
                'name': type(model.optimizer).__name__,
                'config': model.optimizer.get_config()})
        model_size = 0
        for l in model.layers:
            for w in l.weights:
                s = w.dtype.size * reduce(lambda x, y: x * y, w.shape)
                model_size += s
        model_signature = util._compute_model_signature(model)
        optimizer_size = 0
        optimizer_signature = ''
        if(include_optimizer):
            model._make_train_function()
            for w in model.optimizer.weights:
                if(len(w.shape) == 0):
                    optimizer_size += w.dtype.size
                else:
                    s = w.dtype.size * reduce(lambda x, y: x * y, w.shape)
                    optimizer_size += s
            optimizer_signature = util._compute_optimizer_signature(model.optimizer)
        self.logger.debug('Issuing register_model RPC')
        status, message = self._register_model(provider,
                                               model_name,
                                               model_config,
                                               optimizer_config,
                                               model_size,
                                               optimizer_size,
                                               model_signature,
                                               optimizer_signature)
        return status, message

    @trace
    def load_model(self, provider, model_name, load_data=True):
        """Loads a model from a provider given its name.

        If load_data is False, the model will only be rebuild from
        the configuration sent by the provider. If load_data is True,
        the weights data will be loaded as well.

        Args:
            provider (flamestore.ProviderHandle): provider handle.
            model_name (string): name of the model.
            load_data (bool): whether to also load the weights' data.
        Returns:
            a keras Model instance.
        """
        self.logger.debug('Issuing get_model_config RPC')
        status, message = self.get_model_config(provider, model_name)
        if(status != 0):
            self.logger.error(message)
            raise ValueError(message)
        model_config = message
        self.logger.debug('Rebuilding model')
        model = model_from_json(model_config)
        if(load_data):
            self.logger.debug('Loading model data')
            self.__load_model_data(provider, model_name, model)
        return model

    @trace
    def load_optimizer(self, provider, model_name, load_data=True):
        """Loads the optimizer from a provider given a model name.

        If load_data is False, the optimizer will only be rebuild from
        the configuration sent by the provider. If load_data is True,
        the weights data will be loaded as well.

        Args:
            provider (flamestore.ProviderHandle): provider handle.
            model_name (string): name of the model.
            load_data (bool): whether to also load the weights' data.
        Returns:
            a keras Optimizer instance.
        """
        self.logger.debug('Issuing get_optimizer_config RPC')
        status, message = self.get_optimizer_config(provider, model_name)
        if(status != 0):
            self.logger.error(message)
            raise ValueError(message)
        self.logger.debug('Loading optimizer config from JSON')
        optimizer_config = json.loads(message)
        optimizer_name = optimizer_config['name']
        optimizer_config = optimizer_config['config']
        self.logger.debug('Rebuilding optimizer')
        cls = getattr(optimizers, optimizer_name)
        optimizer = cls(**optimizer_config)
        if(load_data):
            self.logger.debug('Loading optimizer data')
            self.__load_optimizer_data(provider, model_name, optimizer)
        return optimizer

    @trace
    def save_model(self, provider, model_name, model):
        """Saves a model to a provider given its name.

        The model must have been registered first.

        Args:
            provider (flamestore.ProviderHandle): provider handle.
            model_name (string): name of the model.
            model (Model): Keras model to save.
        """
        self.logger.debug('Saving model data')
        self.__save_model_data(provider, model_name, model)

    @trace
    def save_optimizer(self, provider, model_name, optimizer):
        """Saves an optimizer to a provider given a model name.

        The model must have been registered first.

        Args:
            provider (flamestore.ProviderHandle): provider handle.
            model_name (string): name of the model.
            optimizer (Optimizer): Keras optimizer to save.
        """
        self.logger.debug('Saving optimizer data')
        self.__save_optimizer_data(provider, model_name, optimizer)
        return optimizer

    @trace
    def load_model_data(self, provider, model_name, model):
        """
        Load the model's data from a provider.

        Args:
            provider (tensorchestra.ProviderHandle): provider handle.
            model_name (string): name of the model.
            model (keras Model): keras Model.
        """
        self.__load_model_data(provider, model_name, model)

    @trace
    def load_optimizer_data(self, provider, model_name, optimizer):
        """
        Load an optimizer's data from a provider.

        Args:
            provider (tensorchestra.ProviderHandle): provider handle.
            model_name (string): name of the model.
            optimizer (keras Optimizer): keras Optimizer.
        """
        self.__load_optimizer_data(provider, model_name, optimizer)

    @trace
    def __load_model_data(self, provider, model_name, model):
        # list model tensors
        model_tensors = []
        for l in model.layers:
            for w in l.weights:
                model_tensors.append(w._ref())
        # compute signature
        sig = util._compute_model_signature(model)
        # create a read operation
        if(len(model_tensors) > 0):
            self.logger.debug('Creating read_model operation')
            read_model_op = ops._read_model(
                client=self._get_id(),
                provider_addr=provider.get_addr(),
                provider_id=provider.get_id(),
                model_name=model_name,
                model_signature=sig,
                tensors=model_tensors)
            self.logger.debug('Executing read_model operation')
            read_model_op.run(session=K.get_session())

    @trace
    def __load_optimizer_data(self, provider, model_name, optimizer):
        # list optimizer tensors
        optimizer_tensors = []
        for w in optimizer.weights:
            optimizer_tensors.append(w._ref())
        # compute signature
        sig = util._compute_optimizer_signature(optimizer)
        # create a read operation
        if(len(optimizer_tensors) > 0):
            self.logger.debug('Creating read_optimizer operation')
            read_optimizer_op = ops._read_optimizer(
                client=self._get_id(),
                provider_addr=provider.get_addr(),
                provider_id=provider.get_id(),
                model_name=model_name,
                optimizer_signature=sig,
                tensors=optimizer_tensors)
            self.logger.debug('Executing read_optimizer operation')
            read_optimizer_op.run(session=K.get_session())

    @trace
    def __save_model_data(self, provider, model_name, model):
        # list model tensors
        model_tensors = []
        for l in model.layers:
            for w in l.weights:
                model_tensors.append(w)
        # compute signature
        sig = util._compute_model_signature(model)
        # create a read operation
        if(len(model_tensors) > 0):
            self.logger.debug('Creating write_model operation')
            write_model_op = ops._write_model(
                client=self._get_id(),
                provider_addr=provider.get_addr(),
                provider_id=provider.get_id(),
                model_name=model_name,
                model_signature=sig,
                tensors=model_tensors)
            self.logger.debug('Executing write_model operation')
            write_model_op.run(session=K.get_session())

    @trace
    def __save_optimizer_data(self, provider, model_name, optimizer):
        # list optimizer tensors
        optimizer_tensors = []
        for w in optimizer.weights:
            optimizer_tensors.append(w)
        # compute signature
        sig = util._compute_optimizer_signature(optimizer)
        # create a read operation
        if(len(optimizer_tensors) > 0):
            self.logger.debug('Creating read_optimizer operation')
            write_optimizer_op = ops._write_optimizer(
                client=self._get_id(),
                provider_addr=provider.get_addr(),
                provider_id=provider.get_id(),
                model_name=model_name,
                optimizer_signature=sig,
                tensors=optimizer_tensors)
            self.logger.debug('Executing read_optimizer operation')
            write_optimizer_op.run(session=K.get_session())
