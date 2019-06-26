from keras.callbacks import Callback
from keras import backend as K
from . import ops
from . import util
import logging
from .decorators import trace

class RemoteCheckpointCallback(Callback):
    """Callback class that periodically checkpoints
    a model to a server during training.
    """

    def __init__(self,
                 model_name,
                 client,
                 provider_addr,
                 provider_id=0,
                 include_optimizer=True,
                 frequency={'epoch': 1}):
        """Constructor of RemoteCheckpointCallback.

        Args:
            model_name (string): name of the model.
            client (flamestore.client.Client):
                client to use to communicate with the provider.
            provider_addr (string): provider address.
            provider_id (int): provider id.
            include_optimizer (bool):
                whether to checkpoint the optimizer state.
            frequency (dict):
                when to checkpoint the model (and possibly the optimizer).

        Notes:
            * The frequency dictionary may provide the following keys:
                epoch - how many epochs to skip between checkpoints
                batch - how many batches to skip between checkpoints
            * If a model with the same name is already registered,
              a ValueError exception will be raised when training starts.
        """
        super()
        self.logger            = logging.getLogger(type(self).__name__)
        self.logger.debug('Creating a RemoteCheckpointCallback object')
        self.model_name        = model_name
        self.client            = client
        self.provider_addr     = provider_addr
        self.provider_id       = provider_id
        self.include_optimizer = include_optimizer
        self.frequency         = frequency

    @trace
    def on_train_begin(self, logs={}):
        """Callback method called when training begins.

        This function registers the model with the server and
        initializes the Tensorflow operations required to send
        the model's and optimizer's tensors periodically.
        """
        # register the model
        provider = self.client.lookup(self.provider_addr, self.provider_id)
        status, message = self.client.register_model(
            provider,
            self.model_name,
            self.model,
            self.include_optimizer)
        if(status != 0):
            self.logger.critical('register_model error: %s', message)
            raise ValueError(message)
        # compute model signature
        sig = util._compute_model_signature(self.model)
        # list model tensors
        model_tensors = []
        for l in self.model.layers:
            for w in l.weights:
                model_tensors.append(w)
        self.logger.debug('Creating write_model TensorFlow operation')
        # create a new operation to transfer the data
        self.__write_model_op = ops._write_model(
            client=self.client._get_id(),
            provider_addr=self.provider_addr,
            provider_id=self.provider_id,
            model_name=self.model_name,
            model_signature=sig,
            tensors=model_tensors)
        if self.include_optimizer:
            # compute optimizer signature
            sig = util._compute_optimizer_signature(self.model.optimizer)
            # list optimizer tensors
            optimizer_tensors = []
            for w in self.model.optimizer.weights:
                optimizer_tensors.append(w)
            # create a new operation to transfer the data
            self.logger.debug('Creating write_optimizer TensorFlow operation')
            self.__write_optimizer_op = ops._write_optimizer(
                client=self.client._get_id(),
                provider_addr=self.provider_addr,
                provider_id=self.provider_id,
                model_name=self.model_name,
                optimizer_signature=sig,
                tensors=optimizer_tensors)

    @trace
    def on_train_end(self, logs={}):
        """Callback method called when training finishes.

        This method destroyes the Tensorflow operations
        used to checkpoint tensors.
        """
        del self.__write_model_op
        del self.__write_optimizer_op

    @trace
    def on_epoch_begin(self, epoch, logs={}):
        """Callback method called when an epoch starts."""

    @trace
    def on_epoch_end(self, epoch, logs={}):
        """Callback method called when an epoch ends.

        This method may checkpoint the model and optimizer.
        """
        if 'epoch' in self.frequency:
            if epoch % self.frequency['epoch'] == 0:
                self.logger.debug('Running write_model operation')
                self.__write_model_op.run(session=K.get_session())
                if self.include_optimizer:
                    self.logger.debug('Running write_optimizer operation')
                    self.__write_optimizer_op.run(session=K.get_session())

    @trace
    def on_batch_begin(self, batch, logs={}):
        """Callback method called when a batch begins."""

    @trace
    def on_batch_end(self, batch, logs={}):
        """Callback method called when a batch ends.

        This method may checkpoint the model and optimizer."""
        if 'batch' in self.frequency:
            if batch % self.frequency['batch'] == 0:
                self.logger.debug('Running write_model operation')
                self.__write_model_op.run(session=K.get_session())
                if self.include_optimizer:
                    self.logger.debug('Running write_optimizer operation')
                    self.__write_optimizer_op.run(session=K.get_session())
