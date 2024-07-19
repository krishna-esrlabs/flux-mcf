import threading
from concurrent.futures import ThreadPoolExecutor

from mcf.exception import RCConnectionTimeout


class ReusableThread(threading.Thread):
    """
    This class provides code for a restartale / reusable thread

    join() will only wait for one (target)functioncall to finish
    finish() will finish the whole thread (after that, it's not restartable anymore)

    Taken from https://www.codeproject.com/Tips/1271787/Python-Reusable-Thread-Class
    Licensed under The Code Project Open License (CPOL) 1.02
    """

    def __init__(self, target):
        self._start_signal = threading.Event()
        self._one_run_finished = threading.Event()
        self._finish_indicator = False
        self._callable = target
        self._connection_timed_out = False

        threading.Thread.__init__(self)

    def restart(self):
        """make sure to always call join() before restarting"""
        self._start_signal.set()

    def run(self):
        """ This class will reprocess the object "processObject" forever.
        Through the change of data inside processObject and start signals
        we can reuse the thread's resources"""

        try:
            while True:
                # wait until we should process
                self._start_signal.wait()

                self._start_signal.clear()

                if self._finish_indicator:  # check if we want to stop
                    self._one_run_finished.set()
                    return

                # call the threaded function
                self._callable()

                # notify about the run's end
                self._one_run_finished.set()

        except RCConnectionTimeout:
            self._connection_timed_out = True
            self._one_run_finished.set()
            self._finish_indicator = True

    def join(self):
        """ This join will only wait for one single run (target functioncall) to be finished"""
        if not self._connection_timed_out:
            self._one_run_finished.wait()

        self._one_run_finished.clear()

        if self._connection_timed_out:
            raise RCConnectionTimeout(self.ident)

    def finish(self):
        if self._finish_indicator is False:
            self._finish_indicator = True
            self.restart()
            self.join()


class OwnThreadWorker():
    """
    Class to which work, i.e. callables can be forwarded using the submit function.
    Callables passed to the submit function will be executed by a separate thread,
    constructed and maintained by this class
    """
    def __init__(self):
        self.result = None
        self.args = None
        self.kwargs = None
        self.lock = threading.Lock()
        self.worker = ReusableThread(target=self._run)
        self.worker.start()

    def _run(self):
        if self.args is None:
            return

        args = self.args
        kwargs = self.kwargs

        if not args or len(args) < 1:
            raise TypeError('submit expected at least 1 positional argument.')

        fn, *args = args

        self.result = fn(*args, **kwargs)

    def submit(self, *args, **kwargs):
        try:
            self.lock.acquire()
            self.args = args
            self.kwargs = kwargs

            self.worker.restart()
            self.worker.join()

            self.args = None
            self.kwargs = None
            result = self.result
        finally:
            self.lock.release()

        return result

    def shutdown(self):
        self.worker.finish()


class CurrThreadWorker():
    """
    Class to which work, i.e. callables can be forwarded using the submit function.
    Callables passed to the submit function will be executed by the calling thread.
    This class is used as drop in replacement for OwnThreadWorker, for when users dont
    want additional threads to be used.
    """
    def __init__(self):
        self.thread_id = threading.get_ident()

    def submit(self, *args, **kwargs):
        if self.thread_id != threading.get_ident():
            raise RuntimeError(
                "Remote control called from different threads. "
                "This is not supported in the current setting. "
                "Build RemoteControl with flag 'own_thread' set to True to avoid this error.")

        if not args or len(args) < 1:
            raise TypeError('submit expected at least 1 positional argument.')

        fn, *args = args

        return fn(*args, **kwargs)

    def shutdown(self):
        pass
