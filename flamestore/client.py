# =============================================== #
# (C) 2018 The University of Chicago
#
# See COPYRIGHT in top-level directory.
# =============================================== #
import tmci.checkpoint
import _flamestore_client
import json
import os.path
import numpy as np
from functools import reduce
from typing import Optional, Callable, List
import spdlog

import pymargo
from pymargo.core import Engine

from tensorflow.keras.models import model_from_json
import tensorflow.keras.optimizers as optimizers
from . import util
from .dataset import Descriptor

logger = spdlog.ConsoleLogger("flamestore.client")
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v")


class Client(_flamestore_client.Client):
    """Client class allowing access to FlameStore providers."""

    def __init__(self, engine: Optional[Engine] = None,
                 workspace: str = '.'):
        """Constructor.

        Args:
            engine (pymargo.core.Engine): Py-Margo engine.
            workspace (str): path to a workspace.
        """
        path = os.path.abspath(workspace)
        if(not os.path.isdir(path+'/.flamestore')):
            logger.critical('Directory is not a FlameStore workspace')
            raise RuntimeError('Directory is not a FlameStore workspace')
        connectionfile = path + '/.flamestore/master'
        if(engine is None):
            with open(path+'/.flamestore/config.json') as f:
                config = json.loads(f.read())
            protocol = config['protocol']
            self._engine = pymargo.core.Engine(
                protocol,
                use_progress_thread=True,
                mode=pymargo.server)
        else:
            self._engine = engine
        super().__init__(self._engine._mid, connectionfile)
        logger.debug('Creating a Client for workspace '+path)

    def __del__(self):
        self._cleanup_hg_resources()
        del self._engine

    def register_model(self,
                       model_name: str,
                       model,
                       include_optimizer: bool = True):
        """
        Registers a model in a provider.

        Args:
            provider (flamestore.ProviderHandle): provider handle.
            model_name (string): model name.
            model (keras Model): model.
            include_optimizer (bool): whether to register the optimizer.
        """
        model_config = {'model': model.to_json()}
        if(include_optimizer):
            model_config['optimizer'] = json.dumps({
                'name': type(model.optimizer).__name__,
                'config': model.optimizer.get_config()})
        else:
            model_config['optimizer'] = None
        model_size = 0
        for layer in model.layers:
            for w in layer.weights:
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
        model.__flamestore_model_signature = model_signature
        logger.debug('Issuing register_model RPC')
        model_config = json.dumps(model_config)
        status, message = self._register_model(model_name,
                                               model_config,
                                               model_size,
                                               model_signature)
        if(status != 0):
            logger.error(message)
            raise RuntimeError(message)

    def reload_model(self, model_name: str, include_optimizer: bool = True):
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
        status, message = self._reload_model(model_name)
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
                raise RuntimeError(
                    'Requested model wasn\'t stored with its optimizer')
            optimizer_name = optimizer_config['name']
            optimizer_config = optimizer_config['config']
            logger.debug('Rebuilding optimizer')
            cls = getattr(optimizers, optimizer_name)
            model.optimizer = cls(**optimizer_config)
        return model

    def duplicate_model(self, source_model_name: str, dest_model_name: str):
        """This function requests the FlameStore backend to
        duplicate a model (both architecture and data).
        The new model name must not be already in use.

        Args:
            source_model_name (str): name of the model to duplicate
            dest_model_name (str): name of the new model
        """
        status, message = self._duplicate_model(
            source_model_name,
            dest_model_name)
        if(status != 0):
            logger.error(message)
            raise RuntimeError(message)

    def __transfer_weights(self, model_name: str, model,
                           include_optimizer: bool, transfer: Callable):
        """Helper function that can save and load weights (the save and load
        functions must be passed as the "transfer" argument). Used by the
        save_weights and load_weights methods.

        Args:
            model_name (str): name of the model.
            model (keras.Model): model to transfer.
            include_optimizer (bool): whether to include the model's optimizer.
            transfer (fun): transfer function.
        """
        if(hasattr(model, '__flamestore_model_signature')):
            model_signature = model.__flamestore_model_signature
        elif(include_optimizer):
            model_signature = util._compute_signature(model, model.optimizer)
        else:
            model_signature = util._compute_signature(model)
        tmci_params = {'model_name': model_name,
                       'flamestore_client': self._get_id(),
                       'signature': model_signature}
        transfer(model, backend='flamestore',
                 config=json.dumps(tmci_params),
                 include_optimizer=include_optimizer)

    def save_weights(self, model_name: str, model,
                     include_optimizer: bool = True):
        """Saves the model's weights. The model must have been registered.

        Args:
            model_name (str): name of the model.
            model (keras.Model): model from which to save the weights.
            include_optimizer (bool): whether to include the model's optimizer.
        """
        self.__transfer_weights(model_name, model, include_optimizer,
                                tmci.checkpoint.save_weights)

    def load_weights(self, model_name: str, model,
                     include_optimizer: bool = True):
        """Loads the model's weights. The model must have been registered
        and built.

        Args:
            model_name (str): name of the model.
            model (keras.Model): model into which to load the weights.
            include_optimizer (bool): whether to include the model's optimizer.
        """
        model._make_train_function()
        self.__transfer_weights(model_name, model, include_optimizer,
                                tmci.checkpoint.load_weights)

    def register_dataset(self, dataset_name: str,
                         descriptor: Descriptor,
                         metadata: str = ''):
        """Register a dataset's informations.

        Args:
            dataset_name (str): name of the dataset.
            descriptor (Descriptor): dataset descriptor.
            metadata (str): string (e.g. JSON-formatted) to attach to the dataset.
        """
        status, message = self._register_dataset(dataset_name, str(descriptor), metadata)
        if status != 0:
            raise RuntimeError(message)

    def get_dataset_descriptor(self, dataset_name: str):
        """Get a particular dataset's descriptor.

        Args:
            dataset_name (str): name of the dataset.
        Returns:
            a Descriptor instance.
        """
        status, message = self._get_dataset_descriptor(dataset_name)
        if status != 0:
            raise RuntimeError(message)
        return Descriptor.from_string(message)

    def get_dataset_size(self, dataset_name: str):
        """Get a particular dataset's number of samples.

        Args:
            dataset_name (str): name of the dataset.
        Returns:
            the number of samples contained in the dataset.
        """
        status, message = self._get_dataset_size(dataset_name)
        if status != 0:
            raise RuntimeError(message)
        return int(message)
    
    def get_dataset_metadata(self, dataset_name: str):
        """Get a particular dataset's metadata.

        Args:
            dataset_name (str): name of the dataset.
        Returns:
            the metadata associated with the dataset at registration time.
        """
        status, message = self._get_dataset_metadata(dataset_name)
        if status != 0:
            raise RuntimeError(message)
        return message

    def add_samples(self, dataset_name: str,
                    **kwargs: List[np.array]):
        """Add a set of samples to the dataset.

        Args:
            dataset_name (str): name of the dataset.
            kwargs: association between field name and numpy array
        """
        field_names = list(kwargs.keys())
        array_lists = list(kwargs.values())
        if len(field_names) == 0:
            return
        descriptor = self._make_descriptor_from_kwargs(**kwargs)
        status, message = self._add_samples(
            dataset_name, str(descriptor), field_names, array_lists)
        if status != 0:
            raise RuntimeError(message)

    def load_samples(self, dataset_name: str, ids: List[int],
                     **kwargs: List[np.array]):
        """Load a set of samples from the dataset.

        Args:
            dataset_name (str): name of the dataset.
            kwargs: association between field name and numpy array
        """
        field_names = list(kwargs.keys())
        array_lists = list(kwargs.values())
        if len(field_names) == 0:
            return
        descriptor = self._make_descriptor_from_kwargs(**kwargs)
        status, message = self._load_samples(
            dataset_name, str(descriptor), field_names, array_lists)

    def _make_descriptor_from_kwargs(self, **kwargs: List[np.array]):
        field_names = list(kwargs.keys())
        array_lists = list(kwargs.values())
        # check that all the lists fo numpy arrays have the same size
        num_arrays = 0
        for i, lst in enumerate(array_lists):
            num_arrays = len(lst)
            if i == 0:
                continue
            if len(lst) != len(array_lists[i-1]):
                raise KeyError("Lists of numpy arrays must be the same size")
        if num_arrays == 0:
            None
        # check that all the arrays have the same dtype and shape
        for lst in array_lists:
            a0 = lst[0]
            for a in lst[1:]:
                if a0.dtype != a.dtype:
                    raise RuntimeError(
                        "Numpy arrays in the same"
                        + " field must have the same dtype")
                if a0.shape != a.shape:
                    raise RuntimeError(
                        "Numpy arrays in the same"
                        + " field must have the same shape")
        # make a descriptor
        d = Descriptor()
        for lst, field in zip(array_lists, field_names):
            a = lst[0]
            d.add_field(field, a.dtype, a.shape)
        return d
