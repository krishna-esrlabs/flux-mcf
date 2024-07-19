""""
Copyright (c) 2024 Accenture
"""

import zmq
import time

from mcf.exception import RCConnectionTimeout
from mcf.worker import OwnThreadWorker, CurrThreadWorker


class ZmqCommunicator:

    def __init__(self, timeout: int, own_thread: bool) -> None:
        self.context = None
        self.socket = None
        self.timeout = timeout
        self.own_thread = own_thread
        self.worker = None

    def __del__(self) -> None:
        self.disconnect()

    def _connect(self, ip: str, port: int) -> None:
        self.context = zmq.Context()

        url = 'tcp://{}:{}'.format(ip, port)

        print('Trying to connect to {}'.format(url))
        self.socket = self.context.socket(zmq.REQ)
        self.socket.setsockopt(zmq.LINGER, self.timeout)
        self.socket.connect(url)

    def connect(self, ip: str, port: int) -> None:
        if self.worker is not None:
            if self.is_connected():
                self.disconnect()
            else:
                self.worker.shutdown()

        self.worker = OwnThreadWorker() if self.own_thread else CurrThreadWorker()
        self.worker.submit(self._connect, ip, port)

    def _set_timeout(self, timeout: int) -> None:
        self.timeout = timeout

    def set_timeout(self, timeout: int) -> None:
        """
        :param timeout: Set timeout for reception
        """
        self.worker.submit(self._set_timeout, timeout)

    def _is_connected(self) -> bool:
        return self.socket is not None and not self.socket.closed

    def is_connected(self) -> bool:
        if self.worker is None:
            return False
        return self.worker.submit(self._is_connected)

    def _disconnect(self) -> None:
        if self._is_connected():
            self.socket.close()
            self.socket = None

        if self.context is not None:
            self.context.destroy(1)
            self.context = None

    def disconnect(self) -> None:
        if self.socket is not None:
            try:
                self.worker.submit(self._disconnect)
            except RCConnectionTimeout:
                pass
            self.worker.shutdown()
            self.worker = None

    def _receive(self) -> bytes:
        result = None
        for i in range(self.timeout):
            try:
                result = self.socket.recv(flags=zmq.NOBLOCK)
                break
            except zmq.error.Again:
                time.sleep(0.001)
        if result is None:
            raise RCConnectionTimeout()
        return result

    def receive(self) -> bytes:
        return self.worker.submit(self._receive)

    def _read_value(self, cmd: bytes):
        response = self._send(cmd)
        rcvmore = self.socket.getsockopt(zmq.RCVMORE)
        if rcvmore > 0:
            packed_value = self._receive()

            rcvmore = self.socket.getsockopt(zmq.RCVMORE)

            if rcvmore > 0:
                extmem = self._receive()
            else:
                extmem = None

            return packed_value, extmem, response
        else:
            return None, None, response

    def read_value(self, topic: str):
        return self.worker.submit(self._read_value, topic)

    def _send(self, msg: bytes) -> bytes:
        if type(msg) == list:
            for part in msg:
                flags = 0 if part is msg[-1] else zmq.SNDMORE
                self.socket.send(part, flags)
        else:
            self.socket.send(msg, 0)
        return self._receive()

    def send(self, msg: bytes) -> bytes:
        return self.worker.submit(self._send, msg)
