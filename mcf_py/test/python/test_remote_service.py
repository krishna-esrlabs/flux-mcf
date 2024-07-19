"""
Copyright (c) 2024 Accenture
"""

import inspect
import os
import subprocess
import sys
import time

import pytest

# path of python mcf module, relative to location of this script
_MCF_PY_RELATIVE_PATH = "../../"
_MCF_VALUE_TYPES_RELATIVE_PATH = "../../../../components/value_types/python/"
# directory of this script and relative path of mcf python tools
_SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))

sys.path.append(f"{_SCRIPT_DIRECTORY}/{_MCF_PY_RELATIVE_PATH}")
sys.path.append(f"{_SCRIPT_DIRECTORY}/{_MCF_VALUE_TYPES_RELATIVE_PATH}")

# TODO: remove
sys.path.append(f"{_SCRIPT_DIRECTORY}/../../mcf_remote")

from mcf_core.component_framework import ComponentManager
from mcf_core.value_store import ValueStore
from mcf_remote.remote_service import RemoteService
from mcf_remote.zmq_msgpack_comm import ZmqMsgPackReceiver, ZmqMsgPackSender

from value_types.value_types.base.Matrix4x4F import Matrix4x4F
from value_types.value_types.base.MultiExtrinsics import MultiExtrinsics
from value_types.value_types.base.PointXYZ import PointXYZ
from value_types.value_types.camera.ImageUint8 import ImageUint8
from value_types.value_types.camera.Format import Format


REMOTE_SERVICE_TEST_CPP_EXECUTABLE = f"{_SCRIPT_DIRECTORY}/{_MCF_PY_RELATIVE_PATH}/test/bin/py_remote_service_test"
LD_LIB_PATH = f"{_SCRIPT_DIRECTORY}/{_MCF_PY_RELATIVE_PATH}/test/bin/"
REMOTE_SERVICE_TEST_PYTHON_EXECUTABLE = f"{_SCRIPT_DIRECTORY}/{_MCF_PY_RELATIVE_PATH}/test/python/bridge_runner.py"

TIMEOUT_MS = 3000


# helper class to run sub process
class ProcessRunner:

    def __init__(self, args: list, env=None):
        """
        :param args: list of executable and parameters
        """
        self._args = args
        self._env = env
        self._process = None

    def __enter__(self):
        if self._process is None:
            self._process = subprocess.Popen(self._args, env=self._env)

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self._process is not None:
            self._process.terminate()
            self._process.communicate()


def test_echo_non_extmem_with_cpp():
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = LD_LIB_PATH
    with ProcessRunner([REMOTE_SERVICE_TEST_CPP_EXECUTABLE], env=env):
        exec_test_echo_non_extmem()

def test_echo_non_extmem_with_python():
    with ProcessRunner(["python3", REMOTE_SERVICE_TEST_PYTHON_EXECUTABLE]):
        exec_test_echo_non_extmem()


def exec_test_echo_non_extmem():

    connection1 = "tcp://127.0.0.1:5560"
    connection2 = "tcp://127.0.0.1:5561"

    SEND_TOPIC_1 = "/topic_from_py_1"
    RECEIVE_TOPIC_1 = "/topic_to_py_1"

    value_store = ValueStore()

    sender = ZmqMsgPackSender(connection1, TIMEOUT_MS)
    receiver = ZmqMsgPackReceiver(connection2, TIMEOUT_MS, [ImageUint8, PointXYZ, MultiExtrinsics])
    remote_service = RemoteService(value_store, sender, receiver)
    remote_service.add_send_rule(SEND_TOPIC_1)
    remote_service.add_receive_rule(RECEIVE_TOPIC_1)

    cmgr = ComponentManager()
    cmgr.register_component(remote_service, "remote_service")
    cmgr.configure()
    cmgr.startup()

    try:
        time.sleep(3)

        value = PointXYZ(0.1, 0.2, 0.3)
        value.inject_id(42)
        value_store.set_value(SEND_TOPIC_1, value)

        time.sleep(1)

        value_back = value_store.get_value(RECEIVE_TOPIC_1)
        print("value, id", value_back, value_back.id)

        assert value_back.x == pytest.approx(value.x) and \
               value_back.y == pytest.approx(value.y) and \
               value_back.z == pytest.approx(value.z), "Received value differs from sent value"
        assert value_back.id == 42, "Received value ID differs from sent value ID"

    except Exception as e:
        raise e

    finally:
        cmgr.shutdown()


def test_echo_extmem_with_cpp():
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = LD_LIB_PATH
    with ProcessRunner([REMOTE_SERVICE_TEST_CPP_EXECUTABLE], env=env):
        exec_test_echo_extmem()


def test_echo_extmem_with_python():
    with ProcessRunner(["python3", REMOTE_SERVICE_TEST_PYTHON_EXECUTABLE]):
        exec_test_echo_extmem()


def exec_test_echo_extmem():

    connection1 = "tcp://127.0.0.1:5560"
    connection2 = "tcp://127.0.0.1:5561"

    RECEIVE_TOPIC_2 = "/topic_to_py_2"
    SEND_TOPIC_2 = "/topic_from_py_2"

    value_store = ValueStore()

    sender = ZmqMsgPackSender(connection1, TIMEOUT_MS)
    receiver = ZmqMsgPackReceiver(connection2, TIMEOUT_MS, [ImageUint8, PointXYZ, MultiExtrinsics])
    remote_service = RemoteService(value_store, sender, receiver)
    remote_service.add_send_rule(SEND_TOPIC_2)
    remote_service.add_receive_rule(RECEIVE_TOPIC_2)

    cmgr = ComponentManager()
    cmgr.register_component(remote_service, "remote_service")
    cmgr.configure()
    cmgr.startup()

    try:
        time.sleep(3)

        value = ImageUint8(width=4, height=2, pitch=4, format=Format.GRAY, data=bytearray([0, 1, 2, 3, 4, 5, 6, 7]))
        value.inject_id(43)
        value_store.set_value(SEND_TOPIC_2, value)

        time.sleep(1)

        value_back = value_store.get_value(RECEIVE_TOPIC_2)
        print("value, id", value_back, value_back.id)

        assert value_back.width == value.width and \
               value_back.height == value.height and \
               value_back.pitch == value.pitch and \
               value_back.format == value.format and \
               value_back.data == value.data, "Received value differs from sent value"
        assert value_back.id == 43, "Received value ID differs from sent value ID"

    except Exception as e:
        raise e

    finally:
        cmgr.shutdown()


def test_echo_value_of_values_with_cpp():
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = LD_LIB_PATH
    with ProcessRunner([REMOTE_SERVICE_TEST_CPP_EXECUTABLE], env=env):
        exec_test_echo_value_of_values()


def test_echo_values_with_cpp_with_python():
    with ProcessRunner(["python3", REMOTE_SERVICE_TEST_PYTHON_EXECUTABLE]):
        exec_test_echo_value_of_values()


def exec_test_echo_value_of_values():

    connection1 = "tcp://127.0.0.1:5560"
    connection2 = "tcp://127.0.0.1:5561"

    SEND_TOPIC_3 = "/topic_from_py_3"
    RECEIVE_TOPIC_3 = "/topic_to_py_3"

    value_store = ValueStore()

    sender = ZmqMsgPackSender(connection1, TIMEOUT_MS)
    receiver = ZmqMsgPackReceiver(connection2, TIMEOUT_MS, [ImageUint8, PointXYZ, MultiExtrinsics])
    remote_service = RemoteService(value_store, sender, receiver)
    remote_service.add_send_rule(SEND_TOPIC_3)
    remote_service.add_receive_rule(RECEIVE_TOPIC_3)

    cmgr = ComponentManager()
    cmgr.register_component(remote_service, "remote_service")
    cmgr.configure()
    cmgr.startup()

    try:
        time.sleep(3)

        matrix1 = Matrix4x4F(list(range(0, 16)))
        matrix2 = Matrix4x4F(list(range(16, 32)))
        matrix3 = Matrix4x4F(list(range(32, 48)))
        value = MultiExtrinsics([matrix1, matrix2, matrix3])
        value.inject_id(44)
        value_store.set_value(SEND_TOPIC_3, value)

        time.sleep(1)

        value_back = value_store.get_value(RECEIVE_TOPIC_3)
        print("value, id", value_back, value_back.id)

        assert len(value_back.matrices) == 3, "Received value differs from sent value"
        assert value_back.matrices[0].data == list(range(0, 16)), "Received value differs from sent value"
        assert value_back.matrices[1].data == list(range(16, 32)), "Received value differs from sent value"
        assert value_back.matrices[2].data == list(range(32, 48)), "Received value differs from sent value"
        assert value_back.id == 44, "Received value ID differs from sent value ID"

    except Exception as e:
        raise e

    finally:
        cmgr.shutdown()


def test_peer_process_cpp_kept_running():
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = LD_LIB_PATH
    with ProcessRunner([REMOTE_SERVICE_TEST_CPP_EXECUTABLE], env=env):
        exec_test_echo_value_of_values()
        exec_test_echo_extmem()
        exec_test_echo_non_extmem()


def test_peer_process_python_kept_running():
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = LD_LIB_PATH
    with ProcessRunner([REMOTE_SERVICE_TEST_CPP_EXECUTABLE], env=env):
        exec_test_echo_value_of_values()
        exec_test_echo_extmem()
        exec_test_echo_non_extmem()
