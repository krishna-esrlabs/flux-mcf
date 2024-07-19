""""
Copyright (c) 2024 Accenture
"""
from enum import IntEnum
import math
import time
from typing import TypeVar

import mcf_python_path.mcf_paths
from mcf import RCValueAccessor
from mcf import Value

ValueT = TypeVar("ValueT", bound=Value)  # for instance `CameraProps` or `BpState`

_EPS_FLOAT_ACC = 1e-6


def iterate_values(value1: ValueT, value2: ValueT) -> bool:

    if value1 is None and value2 is None:
        return True
    if type(value1) is float:
        if math.isclose(value1, value2, rel_tol=_EPS_FLOAT_ACC):
            return True
        else:
            print("float test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
    elif type(value1) is str:
        if value1 == value2:
            return True
        else:
            print("str test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
    elif type(value1) is int:
        if value1 == value2:
            return True
        else:
            print("int test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
    elif type(value1) is bool:
        if value1 == value2:
            return True
        else:
            print("bool test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
    elif type(value1) is bytearray:
        if value1 == value2:
            return True
        else:
            print("bytearray test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
    elif isinstance(value1, IntEnum):
        if value1 == value2:
            return True
        else:
            print("IntEnum test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
    elif type(value1) is list:
        if len(value1) != len(value2):
            print("list length test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
        for val1, val2 in zip(value1, value2):
            if not iterate_values(val1, val2):
                return False
        return True   # return True if empty
    elif type(value1) is tuple:
        if len(value1) != len(value2):
            print("tuple length test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
        for val1, val2 in zip(value1, value2):
            if not iterate_values(val1, val2):
                return False
        return True   # return True if empty
    elif type(value1) is dict:
        if len(value1) != len(value2):
            print("dict length test failed, value1: " + str(value1) + ", value2: " + str(value2))
            return False
        if value1.keys() != value2.keys():
            return False
        for key, val1 in value1.items():
            val2 = value2[key]
            if not iterate_values(val1, val2):
                return False
        return True   # return True if empty

    # Expand custom type
    # Must check if the type has a __dict__
    if "__dict__" in value1.__dir__():
        for (key1, val1), (key2, val2) in zip(value1.__dict__.items(), value2.__dict__.items()):
            if not iterate_values(val1, val2):
                return False
    # Check if there are __slots__
    if "__slots__" in value1.__dir__():
        return all(
            iterate_values(getattr(value1, key), getattr(value2, key))
            for key in value1.__slots__
        )
    return True


def test_type_equality(send_value: ValueT, sender: RCValueAccessor, receiver: RCValueAccessor) -> bool:

    print("Testing: " + send_value.__class__.__name__)
    sender.set(send_value)

    # Wait to receive value
    count = 0
    while True:
        received_value = receiver.get()
        if received_value:

            break

        time.sleep(0.01)
        count += 1

        # Timeout if we don't receive reply
        if count > 200:
            return False

    # Check that values sent and received values are equal
    if not iterate_values(send_value, received_value):
        print(send_value.__class__.__name__ + " failed mcf test")
        print("Test values: ")
        print(send_value)
        return False

    print(send_value.__class__.__name__ + " passed mcf test")
    return True
