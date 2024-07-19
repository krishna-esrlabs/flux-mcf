"""
Copyright (c) 2024 Accenture
"""

import argparse
import inspect
import os
import sys
import time

# path of python mcf module, relative to location of this script
_MCF_PY_RELATIVE_PATH = "../../"
_MCF_VALUE_TYPES_RELATIVE_PATH = "../../../../components/value_types/python/"

# directory of this script and relative path of mcf python tools
_SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))

sys.path.append(f"{_SCRIPT_DIRECTORY}/{_MCF_PY_RELATIVE_PATH}")
sys.path.append(f"{_SCRIPT_DIRECTORY}/{_MCF_VALUE_TYPES_RELATIVE_PATH}")

from mcf_core.component_framework import ComponentManager
from mcf_core.component_framework import Component
from mcf_core.value_store import ValueStore
from mcf_remote.remote_service import RemoteService
from mcf_remote.zmq_msgpack_comm import ZmqMsgPackReceiver, ZmqMsgPackSender

from value_types.value_types.base.PointXYZ import PointXYZ
from value_types.value_types.base.MultiExtrinsics import MultiExtrinsics
from value_types.value_types.camera.ImageUint8 import ImageUint8


DEFAULT_TARGET_SCONN = "tcp://127.0.0.1:5561"
DEFAULT_TARGET_RCONN = "tcp://127.0.0.1:5560"

SEND_TOPIC_1 = "/topic_to_py_1"
RECEIVE_TOPIC_1 = "/topic_from_py_1"
SEND_TOPIC_2 = "/topic_to_py_2"
RECEIVE_TOPIC_2 = "/topic_from_py_2"
SEND_TOPIC_3 = "/topic_to_py_3"
RECEIVE_TOPIC_3 = "/topic_from_py_3"

TIMEOUT_MS = 3000


class EchoComponent(Component):

    def __init__(self, value_store: ValueStore, in_topic, out_topic):
        super().__init__("EchoComponent", value_store)
        self._in_topic = in_topic
        self._out_topic = out_topic
        self._in_queue = None

    def configure(self):
        self._in_queue = self.create_and_register_value_queue(maxlen=0, topic=self._in_topic, handler=self._on_input)

    def _on_input(self):
        while not self._in_queue.empty:
            value = self._in_queue.pop()

            if value is None:
                self.logger.warning("Skipping empty value")
                continue

            self.logger.info("Received value, echoing back")
            self.set_value(self._out_topic, value)


def main(conn_send, conn_rec):

    cmgr = ComponentManager()

    try:
        value_store = ValueStore()

        ec1 = EchoComponent(value_store, RECEIVE_TOPIC_1, SEND_TOPIC_1)
        ec2 = EchoComponent(value_store, RECEIVE_TOPIC_2, SEND_TOPIC_2)
        ec3 = EchoComponent(value_store, RECEIVE_TOPIC_3, SEND_TOPIC_3)

        sender = ZmqMsgPackSender(conn_send, TIMEOUT_MS)
        receiver = ZmqMsgPackReceiver(conn_rec, TIMEOUT_MS, [ImageUint8, PointXYZ, MultiExtrinsics])
        remote_service = RemoteService(value_store, sender, receiver)
        remote_service.add_send_rule(SEND_TOPIC_1)
        remote_service.add_receive_rule(RECEIVE_TOPIC_1)
        remote_service.add_send_rule(SEND_TOPIC_2)
        remote_service.add_receive_rule(RECEIVE_TOPIC_2)
        remote_service.add_send_rule(SEND_TOPIC_3)
        remote_service.add_receive_rule(RECEIVE_TOPIC_3)

        cmgr.register_component(ec1, "py_echo_1")
        cmgr.register_component(ec2, "py_echo_2")
        cmgr.register_component(ec3, "py_echo_3")
        cmgr.register_component(remote_service, "bridge_from_test")

        cmgr.configure()
        cmgr.startup()

        while True:
            time.sleep(10)

    finally:

        cmgr.shutdown()


# main part
if __name__ == '__main__':

    parser = argparse.ArgumentParser()

    parser.add_argument('-sconn', '--sconn', action='store', default=DEFAULT_TARGET_SCONN,
                        dest='sconn', type=str, help='remote service sender connection to use')
    parser.add_argument('-rconn', '--rconn', action='store', default=DEFAULT_TARGET_RCONN,
                        dest='rconn', type=str, help='remote service receiver connection to use')

    args = parser.parse_args()

    main(args.sconn, args.rconn)
