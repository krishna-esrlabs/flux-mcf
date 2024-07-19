""""
Copyright (c) 2024 Accenture
"""
import subprocess

# helper class to run sub process
class ProcessRunner:

    def __init__(self, args: list):
        """
        :param args: list of executable and parameters
        """
        self._args = args
        self._process = None

    def __enter__(self):
        if self._process is None:
            self._process = subprocess.Popen(self._args)

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self._process is not None:
            self._process.terminate()
