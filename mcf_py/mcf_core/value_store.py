"""
Copyright (c) 2024 Accenture
"""

import collections
from datetime import datetime
import threading

from mcf_core.events import EventSource


def _now_micros():
    now = datetime.now()
    now_micros = int(now.timestamp()) * 1e6 + now.microsecond
    return now_micros


class ValueStore:
    # TODO: make stored objects immutable, otherwise they may get modified concurrently by different components

    class _MapEntry:

        def __init__(self):
            self.entry_lock = threading.Lock()     # a lock for accessing data of this map entry
            self.value = None                      # the latest value
            self.receivers = []                    # the list of receivers listening to this topic

    def __init__(self):
        self._store = dict()
        self._all_topic_receivers = []
        self._lock = threading.Lock()

    def _create_key_if_absent(self, key):
        """
        Create a new empty entry for the given key, if not yet present in the value store
        Note: the mutex must be acquired before calling this method
        """

        if key not in self._store:
            self._store[key] = ValueStore._MapEntry()

    def add_receiver(self, key, receiver):
        """
        Add a receiver for the given key
        :param key:         the key (= "topic")
        :param receiver:    the receiver (e.g. a ValueQueue)
        """
        with self._lock:
            self._create_key_if_absent(key)
            entry = self._store[key]

            with entry.entry_lock:

                # skip, if receiver already registered
                for r in entry.receivers:
                    if receiver is r:
                        return

                # otherwise append receiver to list of receivers
                entry.receivers.append(receiver)

    def add_all_topic_receiver(self, receiver):
        """
        Add a receiver listening to all topic
        :param receiver: the receiver (e.g. a ValueQueue)
        """
        with self._lock:

            # skip, if receiver already registered
            for r in self._all_topic_receivers:
                if receiver is r:
                    return

            # otherwise append receiver to list of receivers
            self._all_topic_receivers.append(receiver)

    @staticmethod
    def _notify_receivers_and_cleanup(receivers, key, value):
        """
        Helper method for notifying a set of receivers and remove expired receivers from the list.
        Note: required locks for accessing the list of receivers must be acquired prior to calling this method.

        :param receivers: the current list of receivers
        :param key:       the key
        :param value:     the value
        :return the updated list of receivers
        """
        has_expired = False
        for r in receivers:
            expired = r.receive(key, value)
            if expired:
                has_expired = True

        upd_receivers = receivers
        if has_expired:
            upd_receivers = [r for r in receivers if not r.expired]

        return upd_receivers

    def set_value(self, key, value):
        """
        Set the given value into the value store for the given key. (For now: only non-blocking access supported)
        """
        if value is None:
            raise RuntimeError("Value store does not accept setting None values")

        with self._lock:
            self._create_key_if_absent(key)  # TODO: skip this step?
            entry = self._store[key]
            all_topic_receivers_copy = self._all_topic_receivers.copy()
            with entry.entry_lock:
                entry.value = value
                entry_receivers_copy = entry.receivers.copy()

        ValueStore._notify_receivers_and_cleanup(all_topic_receivers_copy, key, value)
        ValueStore._notify_receivers_and_cleanup(entry_receivers_copy, key, value)

    def get_value(self, key: str):
        """
        Return the current value stored for the given key. May return None.
        """
        with self._lock:
            entry = self._store.get(key, None)

            # return None, if key not found
            if entry is None:
                return None

            with entry.entry_lock:
                return entry.value

    def get_keys(self):
        with self._lock:
            return self._store.keys()


class ValueQueue(EventSource):
    """
    A Value Queue can be registered at the ValueStore as a receiver so that
    value updates for a specific topic are pushed into the queue.

    The queue holds both, the actual value and the topic it came from.
    Each queue entry consists of a pair (value, topic).

    If triggers are registered to the queue, they will be activated whenever
    the queue receives a new value.
    """

    def __init__(self, maxlen=0, ctrace_event_gen=None):
        """
        @param maxlen:           Maximal number of items in the queue; 0 means infinite.
                                 When the queue is full, oldest entries will be dropped upon reception of new values.
        @param ctrace_event_gen: An optional component trace event generator for tracing value store access events.
        """
        super().__init__()
        self._maxlen = maxlen
        self._queue = collections.deque()
        self._expired = False
        self._trace_generator = ctrace_event_gen
        self._last_receive_topic = ""
        self._last_receive_time = 0

    @property
    def empty(self):
        with self._lock:
            return len(self._queue) <= 0

    @property
    def size(self):
        with self._lock:
            return len(self._queue)

    @property
    def maxlen(self):
        with self._lock:
            return self._maxlen

    @property
    def component_trace_event_generator(self):
        return self._trace_generator

    @maxlen.setter
    def maxlen(self, value):
        with self._lock:
            self._maxlen = value
            while self._maxlen > 0 and len(self._queue) > value:
                self._queue.popleft()

    def peek(self):
        """
        :return: the value (without topic) at the front (oldest entry) of the queue without removing it from the queue
        """
        return self.peek_with_topic()[0]

    def pop(self):
        """
        :return: the value (without topic) at the front (oldest entry) of the queue and remove the entry from the queue
        """
        return self.pop_with_topic()[0]

    def peek_with_topic(self):
        """
        :return: the value-topic pair at the front (oldest entry) of the queue without removing it from the queue
        """
        with self._lock:
            if len(self._queue) > 0:
                topic_value = self._queue[0]
                if self._trace_generator is not None:
                    self._trace_generator.trace_peek_port_value(topic_value[1], True, topic_value[0])
                return topic_value
            else:
                return None

    def pop_with_topic(self):
        """
        :return: the value-topic pair at the front (oldest entry) of the queue and remove the entry from the queue
        """
        with self._lock:
            if len(self._queue) > 0:
                topic_value = self._queue.popleft()
                if self._trace_generator is not None:
                    self._trace_generator.trace_get_port_value(topic_value[1], True, topic_value[0])
                return topic_value
            else:
                return None

    def receive(self, topic, value):
        """
        Receive the given topic and value, if receiver is not expired
        :param topic: the topic
        :param value: the value
        :return: whether the receiver is expired
        """
        with self._lock:
            if 0 < self._maxlen <= len(self._queue):
                self._queue.popleft()
            self._queue.append((value, topic))
            self._last_receive_topic = topic
            self._last_receive_time = _now_micros()
            self._notify_events()
            return self._expired

    def expire(self):
        """
        "Destructor": Invalidate the trigger, i.e. disable it and remove it from the value store
        """
        with self._lock:
            self._expired = True

    @property
    def expired(self):
        with self._lock:
            return self._expired

    @property
    def last_topic_and_time(self):
        with self._lock:
            return self._last_receive_topic, self._last_receive_time