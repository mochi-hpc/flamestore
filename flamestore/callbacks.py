from keras.callbacks import Callback
from keras import backend as K
import json

class RemoteCheckpointCallback(Callback):
    """Callback class that periodically checkpoints
    a model to a server during training.
    """

    def __init__(self,
                 model_name,
                 workspace,
                 frequency={'epoch': 1},
                 include_optimizer=True,
                 restart=False,
                 client=None,
                 engine=None):
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
        self._model_name        = model_name
        self._client            = client
        self._include_optimizer = include_optimizer
        self._frequency         = frequency
        self._restart           = restart
        self._engine            = engine
        self._workspace         = workspace
        self._owns_engine       = False
        self._owns_client       = False
        if(self._engine is None and self._client is None):
            self._owns_engine = True
            import pymargo
            import pymargo.core
            with open(self._workspace+'/.flamestore/config.json') as f:
                config = json.loads(f.read())
                protocol = config['protocol']
                self._engine = pymargo.core.Engine(protocol, 
                        use_progress_thread=True, mode=pymargo.server)
        if(self._client is None):
            self._owns_client = True
            from .client import Client
            self._client = Client(self._engine, self._workspace)

    def on_train_begin(self, logs={}):
        """Callback method called when training begins.

        This function registers the model with the server and
        initializes the Tensorflow operations required to send
        the model's and optimizer's tensors periodically.
        """
        if(not self._restart):
            self._client.register_model(self._model_name, self.model, include_optimizer=self._include_optimizer)
        else:
            self._client.load_weights(self._model_name, self.model, self._include_optimizer)

    def on_train_end(self, logs={}):
        """Callback method called when training finishes.

        This method destroyes the Tensorflow operations
        used to checkpoint tensors.
        """
        if(self._owns_client):
            del self._client
            self._client = None
        if(self._owns_engine):
            del self._engine
            self._engine = None

    def on_epoch_begin(self, epoch, logs={}):
        """Callback method called when an epoch starts."""

    def on_epoch_end(self, epoch, logs={}):
        """Callback method called when an epoch ends.

        This method may checkpoint the model and optimizer.
        """
        if 'epoch' in self.frequency:
            if epoch % self.frequency['epoch'] == 0:
                self._client.save_weights(self._model_name,
                        self.model,
                        include_optimizer=self._include_optimizer)

    def on_batch_begin(self, batch, logs={}):
        """Callback method called when a batch begins."""

    def on_batch_end(self, batch, logs={}):
        """Callback method called when a batch ends.

        This method may checkpoint the model and optimizer."""
        if 'batch' in self.frequency:
            if batch % self.frequency['batch'] == 0:
                self._client.save_weights(self._model_name,
                        self.model,
                        include_optimizer=self._include_optimizer)
