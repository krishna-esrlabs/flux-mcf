"""
Copyright (c) 2024 Accenture
"""

from abc import abstractmethod
from dataclasses import dataclass, field, fields
from typing import List

from mcf.value import Value


def _to_dict_shallow(obj):
    """
    Non-recursive conversion to dictionary
    """
    result = {}
    for f in fields(obj):
        value = getattr(obj, f.name)
        result[f.name] = value
    return result


@dataclass(repr=False)
class DataclassValue(Value):
    """
    Base class converting a data class into a value store value
    """
    def __new__(cls, *args, **kwargs):
        obj = object.__new__(cls)
        Value.__init__(obj)  # call base class constructor
        return obj

    def __repr__(self):
        flds = self.field_names()
        field_dict = _to_dict_shallow(self)
        repr_data_strs = [f"{fld}={field_dict[fld]}" for fld in flds]
        repr_str = self.__class__.__name__ + "[" + ", ".join(repr_data_strs) + "]"
        return repr_str

    @staticmethod
    def _unpack(clazz, array):

        typeid = clazz.typeid()
        flds = clazz.field_names()

        if array is None:
            raise ValueError(f"Received 'None' when trying to deserialize {typeid} object")

        if len(array) < 2:
            raise ValueError(f"Received value of incompatible length when trying to deserialize {typeid} object")

        if array[1] != typeid:
            raise ValueError(f"Received value_type: {array[1]}, when trying to deserialize {typeid} object")

        if len(array[0]) != len(flds):
            raise ValueError(f"Received {len(array[0])} instead of {len(flds)} field values, "
                             f"when trying to deserialize {typeid} object")

        field_dict = {fld: val for fld, val in zip(flds, array[0])}
        return clazz(**field_dict)

    def serialize(self):
        flds = self.field_names()
        field_dict = _to_dict_shallow(self)
        typeid = self.typeid()
        data = [field_dict[fld] for fld in flds]
        serialized = [data, typeid]
        return serialized

    @staticmethod
    @abstractmethod
    def typeid():
        pass

    @staticmethod
    @abstractmethod
    def field_names():
        """
        :return: list of names of fields to be serialized and deserialized in the desired order
        """
        pass

    @staticmethod
    @abstractmethod
    def deserialize(packed_value):
        pass


@dataclass(repr=False)
class PortDescriptor(DataclassValue):

    name: str = ""
    topic: str = ""
    connected: bool = False

    @staticmethod
    def deserialize(packed_value):
        clazz = PortDescriptor
        return clazz._unpack(clazz, packed_value)

    @staticmethod
    def typeid():
        return "mcf::PortDescriptor"

    @staticmethod
    def field_names():
        return ["name", "topic", "connected"]


@dataclass(repr=False)
class TriggerDescriptor(DataclassValue):
    topic: str = ""
    time: int = 0
    hello: PortDescriptor = PortDescriptor("a", "b", False)

    @staticmethod
    def deserialize(packed_value):
        clazz = TriggerDescriptor
        return clazz._unpack(clazz, packed_value)

    @staticmethod
    def typeid():
        return "mcf::TriggerDescriptor"

    @staticmethod
    def field_names():
        return ["topic", "time", "hello"]


@dataclass(repr=False)
class ComponentTraceEvent(DataclassValue):
    """
    Base class for component trace event messages.
    """

    traceId: str = ""
    time: int = 0
    componentName: str = ""
    threadId: int = 0
    cpuId: int = 0

    @staticmethod
    @abstractmethod
    def deserialize(packed_value):
        pass

    @staticmethod
    @abstractmethod
    def typeid():
        pass


@dataclass(repr=False)
class ComponentTracePortWrite(ComponentTraceEvent):
    """
    Value holding a port_write component trace event
    """

    portDescriptor: PortDescriptor = None
    valueId: int = 0
    inputValueIds: List[int] = field(default_factory=list)

    @staticmethod
    def deserialize(packed_value):
        clazz = ComponentTracePortWrite
        return clazz._unpack(clazz, packed_value)

    @staticmethod
    def typeid():
        return "mcf::ComponentTracePortWrite"

    @staticmethod
    def field_names():
        return ["traceId", "time", "componentName", "portDescriptor", "valueId", "inputValueIds", "threadId", "cpuId"]


@dataclass(repr=False)
class ComponentTracePortPeek(ComponentTraceEvent):
    """
    Value holding a port_write component trace event
    """

    portDescriptor: PortDescriptor = None
    valueId: int = 0

    @staticmethod
    def deserialize(packed_value):
        clazz = ComponentTracePortPeek
        return clazz._unpack(clazz, packed_value)

    @staticmethod
    def typeid():
        return "mcf::ComponentTracePortPeek"

    @staticmethod
    def field_names():
        return ["traceId", "time", "componentName", "portDescriptor", "valueId", "threadId", "cpuId"]


@dataclass(repr=False)
class ComponentTracePortRead(ComponentTraceEvent):
    """
    Value holding a port_write component trace event
    """

    portDescriptor: PortDescriptor = None
    valueId: int = 0

    @staticmethod
    def deserialize(packed_value):
        clazz = ComponentTracePortRead
        return clazz._unpack(clazz, packed_value)

    @staticmethod
    def typeid():
        return "mcf::ComponentTracePortRead"

    @staticmethod
    def field_names():
        return ["traceId", "time", "componentName", "portDescriptor", "valueId", "threadId", "cpuId"]


@dataclass(repr=False)
class ComponentTraceExecTime(ComponentTraceEvent):
    """
    Value holding a port_write component trace event
    """

    description: str = ""
    executionTime: float = 0.0

    @staticmethod
    def deserialize(packed_value):
        clazz = ComponentTraceExecTime
        return clazz._unpack(clazz, packed_value)

    @staticmethod
    def typeid():
        return "mcf::ComponentTraceExecTime"

    @staticmethod
    def field_names():
        return ["traceId", "time", "componentName", "description", "executionTime", "threadId", "cpuId"]


@dataclass(repr=False)
class ComponentTracePortTriggerActivation(ComponentTraceEvent):
    """
    Value holding a port_write component trace event
    """

    triggerDescriptor: TriggerDescriptor = None

    @staticmethod
    def deserialize(packed_value):
        clazz = ComponentTracePortTriggerActivation
        return clazz._unpack(clazz, packed_value)

    @staticmethod
    def typeid():
        return "mcf::ComponentTracePortTriggerActivation"

    @staticmethod
    def field_names():
        return ["traceId", "time", "componentName", "triggerDescriptor", "threadId", "cpuId"]


@dataclass(repr=False)
class ComponentTracePortTriggerExec(ComponentTraceEvent):
    """
    Value holding a port_write component trace event
    """

    triggerDescriptor: TriggerDescriptor = None
    handlerName: str = ""       # name of the executed trigger handler
    executionTime: float = 0.0  # duration of execution time event in seconds. (Field 'time' is end time.)

    @staticmethod
    def deserialize(packed_value):
        clazz = ComponentTracePortTriggerExec
        return clazz._unpack(clazz, packed_value)

    @staticmethod
    def typeid():
        return "mcf::ComponentTracePortTriggerExec"

    @staticmethod
    def field_names():
        return ["traceId", "time", "componentName", "triggerDescriptor",
                "handlerName", "executionTime", "threadId", "cpuId"]
