"""
Copyright (c) 2024 Accenture
"""

import enum
from io import BytesIO
import msgpack
import time
import zmq

from mcf.value import Value
from mcf_core.logger import get_logger


def zmq_send(zmq_socket, msg_bytes):  # TODO: align with C++ method transferData()
    if type(msg_bytes) == list:
        for part in msg_bytes:
            flags = 0 if part is msg_bytes[-1] else zmq.SNDMORE
            zmq_socket.send(part, flags)
    else:
        zmq_socket.send(msg_bytes, 0)


def zmq_receive(zmq_socket, timeout):
    result = None
    for i in range(timeout):  # TODO: better way of handling timeout? => class poller
        try:
            result = zmq_socket.recv(flags=zmq.NOBLOCK)
            break
        except zmq.error.Again:
            time.sleep(0.001)
    return result


class ZmqMsgPackSender:

    def __init__(self, connection, send_timeout):
        """
        :param connection:      A string describing the zmq connection that shall be created
                                Currently only tcp connections are supported: "tcp://[ip]:[port]"
        :param send_timeout:    Time in milliseconds the system waits for an ack after a send before
                                the send function returns TIMEOUT
        """
        self._connection = connection
        self._timeout = int(send_timeout)
        self._socket = None
        self._zmq_context = None
        self._logger = get_logger(f"ZmqMsgPackSender.{connection}.unregistered")

    def set_logger(self, logger):
        self._logger = logger

    def connect(self):
        self.disconnect()

        self._zmq_context = zmq.Context()
        self._socket = self._zmq_context.socket(zmq.REQ)

        # allow destruction of port/context even if there are still some messages in flight
        self._socket.setsockopt(zmq.LINGER, 0)
        self._socket.setsockopt(zmq.REQ_RELAXED, 1)
        # self._socket.setsockopt(zmq.REQ_CORRELATE, 1)
        self._socket.connect(self._connection)

    def disconnect(self):
        if self._socket is not None:
            self._socket.setsockopt(zmq.LINGER, 0)
            self._socket.close()
            self._socket = None

        if self._zmq_context is not None:
            self._zmq_context.destroy(linger=0)
            self._zmq_context = None

    def _send(self, msg_bytes):
        zmq_send(self._socket, msg_bytes)
        return self._check_for_response()

    def _receive(self):
        return zmq_receive(self._socket, self._timeout)

    def send_value(self, topic, value):

        def encode_serializable(o):
            if isinstance(o, Value):
                return o.serialize()[0]
            elif isinstance(o, enum.Enum):
                return o.value
            return o

        # reject values that are not an instance of Value, because they are not compatible with MCF
        if not isinstance(value, Value):
            self._logger.warning(f"Value to send on topic {topic} is not an MCF Value, transmission rejected")
            return "REJECTED"

        serialized = value.serialize()
        data = (msgpack.packb(value.id) +                                    # value ID
                msgpack.packb(serialized[1]) +                               # value type as string
                msgpack.packb(serialized[0], default=encode_serializable))   # value

        messages = [msgpack.packb("value"), msgpack.packb(topic), data]

        # if serialized data contains extmem part
        if len(serialized) > 2:
            messages.append(serialized[2])

        response = self._send(messages)
        return response

    def send_ping(self, freshness):
        assert self.connected, "trying to send a Ping before ZmqMspPackSender was connected"
        result = self._send([msgpack.packb("ping"), msgpack.packb(freshness)])
        if result == "TIMEOUT":
            self._logger.warning(f"ping {freshness} timed out")

    def send_pong(self, freshness):
        assert self.connected, "trying to send a Pong before ZmqMspPackSender was connected"
        result = self._send([msgpack.packb("pong"), msgpack.packb(freshness)])
        if result == "TIMEOUT":
            self._logger.warning(f"pong {freshness} timed out")

    def send_request_all(self):
        assert self.connected, "trying to send a command before ZmqMspPackSender was connected"
        result = self._send([msgpack.packb("command"), msgpack.packb("sendAll")])
        if result == "TIMEOUT":
            self._logger.warning(f"command sendAll timed out")

    def send_blocked_value_injected(self, topic):
        assert self.connected, "trying to send a command before ZmqMspPackSender was connected"
        result = self._send([msgpack.packb("command"), msgpack.packb("valueInjected"), msgpack.packb(topic)])
        if result == "TIMEOUT":
            self._logger.warning(f"command valueInjected timed out")

    def send_blocked_value_rejected(self, topic):
        assert self.connected, "trying to send a command before ZmqMspPackSender was connected"
        result = self._send([msgpack.packb("command"), msgpack.packb("valueRejected"), msgpack.packb(topic)])
        if result == "TIMEOUT":
            self._logger.warning(f"command valueRejected timed out")

    @property
    def connected(self):
        return (self._socket is not None) and (not self._socket.closed)

    def _check_for_response(self):
        try:
            result = self._receive()
            if result is not None:
                result = msgpack.unpackb(result)
        except zmq.error.ZMQError as e:
            self._logger.warning(f"ZMQ error: {e}")
            result = "REJECTED"

        if result is None:
            result = "TIMEOUT"

        return result


class TypeRegistry:

    def __init__(self, list_of_types):

        assert isinstance(list_of_types, (list, tuple)), "Argument must be a list or tuple"

        # create registry of types
        self._types_lookup = {}

        # for each type given in the list
        for tp in list_of_types:

            # create an instance and check for correct base type
            try:
                value = tp()
            except Exception as e:
                raise RuntimeError(f"Type {tp} cannot be instantiated: {e}")

            assert isinstance(value, Value), f"Type {tp} is not an MCF value"

            # serialize to retrieve C++ registry name
            type_name_mcf = value.serialize()[1]

            # add to registry
            self._types_lookup[type_name_mcf] = tp

    def get(self, mcf_typename):
        return self._types_lookup.get(mcf_typename, None)


class ZmqMsgPackReceiver:

    def __init__(self, connection, timeout, list_of_types):
        """
        :param connection:      A string describing the zmq connection that shall be created
                            Currently only tcp connections are supported: "tcp://[ip]:[port]"
        """
        self._connection = connection
        self._timeout = timeout
        self._socket = None
        self._zmq_context = None
        self._listener = None
        self._type_registry = TypeRegistry(list_of_types)
        self._logger = get_logger(f"ZmqMsgPackReceiver.{connection}.unregistered")
        self._socket_ctx = None

    def set_logger(self, logger):
        self._logger = logger

    def set_event_listener(self, listener):
        self._listener = listener

    def connect(self):
        self.disconnect()

        self._zmq_context = zmq.Context()
        self._socket = self._zmq_context.socket(zmq.REP)

        self._socket.bind(self._connection)

    def disconnect(self):
        if self._socket is not None:
            self._socket.setsockopt(zmq.LINGER, 0)
            self._socket.close()
            self._socket.__del__()
            self._socket = None

        if self._zmq_context is not None:
            self._zmq_context.destroy(linger=0)
            self._zmq_context = None

    def _receive(self):
        return zmq_receive(self._socket, self._timeout)

    def receive(self):
        kind = self._receive_and_unpack_data()
        if kind is None:
            return False  # timeout

        result = True
        if kind == "ping":
            self._receive_ping()

        elif kind == "pong":
            self._receive_pong()

        elif kind == "command":
            self._receive_command()

        elif kind == "value":
            self._receive_value()

        else:
            self._logger.warning(f"RemoteService received unexpected message kind: {kind}")
            return False

        return result

    def _receive_ping(self):
        freshness = self._receive_and_unpack_data()
        self._send_response()
        if self._listener is not None:
            self._listener.ping_received(freshness)

    def _receive_pong(self):
        freshness = self._receive_and_unpack_data()
        self._send_response()
        if self._listener is not None:
            self._listener.pong_received(freshness)

    def _receive_command(self):
        command = self._receive_and_unpack_data()
        if command == "sendAll":
            self._send_response()
            if self._listener is not None:
                self._listener.request_all_received()
        elif command == "valueInjected":
            topic = self._receive_and_unpack_data()
            self._send_response()
            if self._listener is not None:
                self._listener.blocked_value_injected_received(topic)
        elif command == "valueRejected":
            topic = self._receive_and_unpack_data()
            self._send_response()
            if self._listener is not None:
                self._listener.blocked_value_rejected_received(topic)
        else:
            self._logger.warning(f"ZmqMsgPackReceiver received unsupported command: {command}")

    def _receive_value(self):

        topic = self._receive_and_unpack_data()
        packed_data = self._receive()
        extmem = None

        rcvmore = self._socket.getsockopt(zmq.RCVMORE)
        if rcvmore > 0:
            extmem = self._receive()

        io = BytesIO(packed_data)
        unpacker = msgpack.Unpacker(io, raw=False)
        value_id = unpacker.unpack()
        typename = unpacker.unpack()
        value_data = unpacker.unpack()

        mcf_type = self._type_registry.get(typename)

        if mcf_type is None:
            self._logger.warning(f"Received value type {typename} is unknown, cannot deserialize")
            self._send_response("REJECTED")
            return

        value = mcf_type.deserialize((value_data, typename, extmem))
        value.inject_id(value_id)

        result = self._listener.value_received(topic, value)
        self._send_response(result)

    def _receive_and_unpack_data(self):
        result = self._receive()
        if result is None:
            return None
        result = msgpack.unpackb(result)
        return result

    def _send_response(self, response=None):
        if response is None:
            response = ""

        response = msgpack.packb(response)
        zmq_send(self._socket, response)
