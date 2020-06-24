# =============================================== #
# (C) 2018 The University of Chicago
#
# See COPYRIGHT in top-level directory.
# =============================================== #
import numpy as np
import json
from typing import Tuple


class Descriptor:

    def __init__(self):
        self._fields = dict()

    def add_field(self,
                  name: str,
                  dtype: np.dtype,
                  shape: Tuple[int,...]):
        if name in self._fields:
            raise KeyError(name+" already a field of this descriptor")
        self._fields[name] = (dtype, shape)

    def get_field_type(self, name: str):
        if name not in self._fields:
            raise KeyError(name+ " not a field of this descriptor")
        self._fields[name][0]

    def get_field_shape(self, name: str):
        if name not in self._fields:
            raise KeyError(name+ " not a field of this descriptor")
        self._fields[name][1]

    def __str__(self):
        fields_str = []
        for field, (dtype, shape) in self._fields.items():
            fields_str.append('"' + field + '":("' + str(dtype) + '",' + str(shape) + ')')
        return '{' + ','.join(fields_str) + '}'

    @staticmethod
    def from_string(descriptor_str):
        fields = json.loads(descriptor_str)
        descriptor = Descriptor()
        for field, (dtype, shape) in fields.items():
            descriptor.add_field(field, np.dtype(dtype), shape)
        return descriptor
