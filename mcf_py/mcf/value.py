""""
Copyright (c) 2024 Accenture
"""
from abc import ABC, abstractmethod
from typing import Sequence


class Value(ABC, object):
    """ the equivalent of cpp mcf::Value """
    __slots__ = ("_id")

    def __init__(self):
        self._id = 0

    @abstractmethod
    def serialize(self) -> Sequence:
        pass

    @staticmethod
    @abstractmethod
    def deserialize(packed_value: Sequence) -> "Value":
        pass

    def inject_id(self, id):
        self._id = id

    @property
    def id(self):
        return self._id