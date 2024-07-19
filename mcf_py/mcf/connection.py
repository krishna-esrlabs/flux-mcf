""""
Copyright (c) 2024 Accenture
"""
from mcf.remote_control import RemoteControl


class Connection:
    def __init__(self, ip, port, timeout=20000, own_thread=False):
        self.ip = ip
        self.port = port

        self.rc = RemoteControl(timeout, own_thread=own_thread)
        self.connect()

    def connect(self):
        self.rc.connect(self.ip, self.port)

    def __del__(self):
        self.close_connection()

    def close_connection(self):
        self.rc.disconnect()
