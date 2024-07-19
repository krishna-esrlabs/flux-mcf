"""
Copyright (c) 2024 Accenture
"""

import threading


class Event:
    """
    An Event object ("Trigger" in C++ mcf) can be
    - queried to check whether it has been set by an event source
    - used to unblock threads waiting on it

    It is mainly a wrapper of a python threading.Event object.

    An Event can be registered to get triggered by one or several EventSources.
    """
    def __init__(self):
        self._flag = False
        self._cond = threading.Condition(threading.Lock())
        self._expired = threading.Event()   # initialized to False

    def wait_and_clear(self):
        """
        Wait until trigger is set, then clear it
        """
        with self._cond:
            if not self._flag:
                self._cond.wait()
            self._flag = False

    def trigger(self):
        """
        Set the trigger if not expired
        @return whether trigger is expired
        """
        if not self._expired.is_set():
            with self._cond:
                self._flag = True
                self._cond.notify_all()

        return self._expired.is_set()

    @property
    def active(self):
        with self._cond:
            return self._flag

    def clear(self):
        with self._cond:
            self._flag = False

    def expire(self):
        """
        "Destructor": Invalidate the object, i.e. disable it and remove it from any event source
        """
        self._expired.set()

    @property
    def expired(self):
        return self._expired.is_set()


class EventSource:
    """
    An event source triggers registered events
    """
    def __init__(self):
        self._lock = threading.RLock()  # recursive lock to allow events to query the event source when triggered
        self._events = []
        pass

    def add_event(self, event):
        """
        Add the given event to be notified
        """
        with self._lock:
            self._events.append(event)

    def _notify_events(self):
        """
        Notify all events that are not expired
        Note: the lock self._lock must be acquired by the caller before calling this method
        """
        found_expired = False
        for e in self._events:
            expired = e.trigger()
            if expired:
                found_expired = True

        # remove expired triggers, if any
        if found_expired:
            self._events = [t for t in self._events if not t.expired]
