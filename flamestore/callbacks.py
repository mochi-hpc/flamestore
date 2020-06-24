# =============================================== #
# (C) 2018 The University of Chicago
#
# See COPYRIGHT in top-level directory.
# =============================================== #
from tensorflow.keras.callbacks import Callback
from typing import Dict, Optional
import json
import spdlog
import pymargo
from pymargo.core import Engine
from .client import Client


logger = spdlog.ConsoleLogger("flamestore.callbacks")
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%F] [%n] [%^%l%$] %v")


class RemoteCheckpointCallback(Callback):
    """Callback class that periodically checkpoints
    a model to a server during training.
    """

    def __init__(self,
                 model_name: str,
                 workspace: str,
                 frequency: Dict[str, int] = {'epoch': 1},
                 include_optimizer: bool = True,
                 restart: bool = False,
                 duplicate_from: Optional[str] = None,
                 client: Optional[Client] = None,
                 engine: Optional[Engine] = None):
        """Constructor of RemoteCheckpointCallback.

        Args:
            model_name (string): name of the model.
            workspace (string): path to the workspace.
            frequency (dict):
                when to checkpoint the model (and possibly the optimizer).
            include_optimizer (bool):
                whether to checkpoint the optimizer state.
            restart (bool):
                whether to restart from an existing model.
            duplicate_from (str):
                if restart is True but the user wants to fork
                an existing model, indicates which model
                to duplicate.
            client (flamestore.client.Client):
                client to use to communicate with the provider
                (if None, this class will initialize a client).
            engine (pymargo.core.Engine):
                Margo instance (if None, this class will instantiate one).

        Notes:
            * The frequency dictionary may provide the following keys:
                epoch - how many epochs to skip between checkpoints
                batch - how many batches to skip between checkpoints
            * If a model with the same name is already registered,
              a ValueError exception will be raised when training starts.
        """
        super()
        self._model_name = model_name
        self._client = client
        self._include_optimizer = include_optimizer
        self._frequency = frequency
        self._restart = restart
        self._duplicate_from = duplicate_from
        self._engine = engine
        self._workspace = workspace
        self._owns_engine = False
        self._owns_client = False
        if(self._engine is None and self._client is None):
            self._owns_engine = True
            logger.debug("Importing pymargo")
            logger.debug("Finding out protocol to use")
            with open(self._workspace+'/.flamestore/config.json') as f:
                config = json.loads(f.read())
                protocol = config['protocol']
                logger.debug("Protocol is "+protocol)
                logger.debug("Initializing pymargo engine")
                self._engine = Engine(
                    protocol, use_progress_thread=True, mode=pymargo.server)
                logger.debug("Engine initialized")
        if(self._client is None):
            self._owns_client = True
            logger.debug("Initializing FlameStore client")
            self._client = Client(self._engine, self._workspace)
            logger.debug("Client initialized")

    def on_train_begin(self, logs={}):
        """Callback method called when training begins.

        This function registers the model with the server and
        initializes the Tensorflow operations required to send
        the model's and optimizer's tensors periodically.
        """
        logger.debug("on_train_begin called")
        if(not self._restart):
            logger.info("Registering model "+self._model_name)
            self._client.register_model(
                self._model_name, self.model,
                include_optimizer=self._include_optimizer)
            logger.info("Model registered successfully")
        else:
            if(self._duplicate_from is not None):
                logger.info("Duplicating model " + self._duplicate_from
                            + " into model " + self._model_name)
                self._client.duplicate_model(self._duplicate_from,
                                             self._model_name)
                logger.info("Duplicated successfully")
            logger.info("Loading weights into model " + self._model_name)
            self._client.load_weights(self._model_name,
                                      self.model,
                                      self._include_optimizer)
            logger.info("Weights loaded successfully")

    def on_train_end(self, logs={}):
        """Callback method called when training finishes.

        This method destroyes the Tensorflow operations
        used to checkpoint tensors.
        """
        logger.debug("on_train_end called")
        if(self._owns_client):
            del self._client
            self._client = None
        if(self._owns_engine):
            del self._engine
            self._engine = None

    def on_epoch_begin(self, epoch, logs={}):
        """Callback method called when an epoch starts."""
        logger.debug("on_epoch_begin called")

    def on_epoch_end(self, epoch, logs={}):
        """Callback method called when an epoch ends.

        This method may checkpoint the model and optimizer.
        """
        logger.debug("on_epoch_end called")
        if 'epoch' in self._frequency:
            if epoch % self._frequency['epoch'] == 0:
                logger.info("Saving weights into model "+self._model_name)
                self._client.save_weights(
                    self._model_name, self.model,
                    include_optimizer=self._include_optimizer)
                logger.info("Weights loaded successfully")

    def on_batch_begin(self, batch: int, logs={}):
        """Callback method called when a batch begins."""
        logger.debug("on_batch_begin called")

    def on_batch_end(self, batch: int, logs={}):
        """Callback method called when a batch ends.

        This method may checkpoint the model and optimizer."""
        logger.debug("on_batch_end called")
        if 'batch' in self._frequency:
            if batch % self._frequency['batch'] == 0:
                logger.info("Saving weights into model "+self._model_name)
                self._client.save_weights(
                    self._model_name, self.model,
                    include_optimizer=self._include_optimizer)
                logger.info("Weights saved successfully")
