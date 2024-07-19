import threading
from typing import Optional


class RCConnectionTimeout(Exception):
    _ident: int

    def __init__(self, ident: Optional[int] = None):
        """
        @param ident: Ident of the thread where the timeout occurred. If None, use the current thread's ident.
        """
        if ident is None:
            ident = threading.get_ident()
        self._ident = ident
        message = f'Received remote control connection timeout in thread {ident}.'
        super(RCConnectionTimeout, self).__init__(message)

    @property
    def ident(self):
        return self._ident
