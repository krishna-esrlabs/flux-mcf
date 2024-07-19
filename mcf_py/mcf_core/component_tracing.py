"""
Copyright (c) 2024 Accenture
"""

import random
import sys
import threading
from datetime import datetime

from mcf_core.component_trace_messages import (PortDescriptor, ComponentTracePortWrite,
                                               ComponentTracePortRead, ComponentTraceExecTime,
                                               TriggerDescriptor, ComponentTracePortTriggerActivation,
                                               ComponentTracePortTriggerExec, ComponentTracePortPeek)

DEFAULT_TRACE_EVENTS_TOPIC = "/mcf/trace_events"


# determine thread id, if Python version is >= 3.8, otherwise use default thread id -1
if sys.version_info.major == 3 and sys.version_info.minor >= 8:
    def _get_thread_id():
        return threading.get_native_id()

else:
    def _get_thread_id():
        return -1


# CPU id is unused for now to avoid additional package dependencies just for this purpose
# => using dummy value -1
def _get_cpu_id():
    return -1


def _now_micros():
    now = datetime.now()
    now_micros = int(int(now.timestamp()) * 1e6 + now.microsecond)
    return now_micros


def _random_id():
    max_value_id = int(2**64)
    return random.randint(1, max_value_id)


def _fill_port_write_event(trace_id, comp_name, topic, input_ids, is_connected, value_written, event):
    """
    Fill the given event object in place
    """

    time = _now_micros()

    port_desc = PortDescriptor()
    port_desc.name = "unnamed"
    port_desc.topic = topic
    port_desc.connected = is_connected

    event.traceId = trace_id
    event.time = time
    event.componentName = comp_name
    event.portDescriptor = port_desc
    event.threadId = _get_thread_id()
    event.cpuId = _get_cpu_id()

    if value_written is not None:
        event.valueId = value_written.id
        if input_ids is not None:
            event.inputValueIds = input_ids

    else:
        event.valueId = 0


def _fill_port_read_event(trace_id, comp_name, topic, is_connected, value_read, event):
    """
    Fill the given event object in place
    """

    time = _now_micros()

    port_desc = PortDescriptor()
    port_desc.name = "unnamed"
    port_desc.topic = topic
    port_desc.connected = is_connected

    event.traceId = trace_id
    event.time = time
    event.componentName = comp_name
    event.portDescriptor = port_desc
    event.threadId = _get_thread_id()
    event.cpuId = _get_cpu_id()

    if value_read is not None:
        event.valueId = value_read.id

    else:
        event.valueId = 0


class ComponentTraceEventController:

    def __init__(self, trace_id, value_store, topic=None):
        """
         :param trace_id:        Trace id to set in the events
         :param value_store:      The value store to write trace events to (should be a separate one for tracing)
         :param topic:           The topic to be used on the value store (uses default value, if not specified)
         """
        self._trace_id = trace_id
        self._value_store = value_store

        if topic is None:
            topic = DEFAULT_TRACE_EVENTS_TOPIC

        self._topic = topic

        self._lock = threading.Lock()   # lock for synchronizing access to variables below
        self._trace_enabled = True

    def enable_trace(self, on_off):
        with self._lock:
            self._trace_enabled = on_off

    @property
    def trace_enabled(self):
        with self._lock:
            return self._trace_enabled

    @property
    def value_store(self):
        return self._value_store

    def create_event_generator(self, comp_name):
        return ComponentTraceEventGenerator(self._trace_id, comp_name, self, self._value_store, self._topic)


class ComponentTraceEventGenerator:

    def __init__(self, trace_id, comp_name, trace_controller, value_store, topic=None):
        """
        :param trace_id:        Trace id to set in the events
        :param comp_name:       Name of the component the generator belongs to
        :param trace_controller The trace controller globally controlling this event generator
        :param value_store:      The value store to write trace events to (should be a separate one for tracing)
        :param topic:           The topic to be used on the value store (uses default value, if not specified)
        """
        self._trace_id = trace_id
        self._name = comp_name
        self._trace_controller = trace_controller
        self._value_store = value_store

        if topic is None:
            topic = DEFAULT_TRACE_EVENTS_TOPIC

        self._topic = topic

        self._lock = threading.Lock()  # lock for synchronizing access to variables below
        self._enabled = True

    def _write_trace_event(self, event):
        # if event does not yet have a valid value id, set one here
        if event.id == 0:
            event.inject_id(_random_id())

        self._value_store.set_value(self._topic, event)

    def enable(self, on_off):
        with self._lock:
            self._enabled = on_off

    @property
    def enabled(self):
        with self._lock:
            return self._enabled

    def globally_enabled(self):
        return self._trace_controller.trace_enabled

    def trace_set_port_value(self, topic, is_connected, input_ids, value):
        if (not self.enabled) or not (self.globally_enabled()):
            return

        event = ComponentTracePortWrite()
        _fill_port_write_event(self._trace_id, self._name, topic, input_ids, is_connected, value, event)
        self._write_trace_event(event)

    def trace_peek_port_value(self, topic, is_connected, value):
        if (not self.enabled) or not (self.globally_enabled()):
            return

        event = ComponentTracePortPeek()
        _fill_port_read_event(self._trace_id, self._name, topic, is_connected, value, event)
        self._write_trace_event(event)

    def trace_get_port_value(self, topic, is_connected, value):
        if (not self.enabled) or not (self.globally_enabled()):
            return

        event = ComponentTracePortRead()
        _fill_port_read_event(self._trace_id, self._name, topic, is_connected, value, event)
        self._write_trace_event(event)

    def trace_execution_time(self, end_time, duration, name):
        if (not self.enabled) or not (self.globally_enabled()):
            return

        event = ComponentTraceExecTime()
        event.traceId = self._trace_id
        event.time = int(end_time)
        event.componentName = self._name
        event.executionTime = duration
        event.description = name
        event.threadId = _get_thread_id()
        event.cpuId = _get_cpu_id()
        self._write_trace_event(event)

    def trace_port_trigger_activation(self, time, topic):
        if (not self.enabled) or not (self.globally_enabled()):
            return

        trigger_desc = TriggerDescriptor()
        trigger_desc.topic = topic
        trigger_desc.time = int(time)

        event = ComponentTracePortTriggerActivation()
        event.traceId = self._trace_id
        event.time = trigger_desc.time
        event.componentName = self._name
        event.triggerDescriptor = trigger_desc
        event.threadId = _get_thread_id()
        event.cpuId = _get_cpu_id()
        self._write_trace_event(event)

    def trace_port_trigger_exec(self, time_end, duration, handler_name, trigger_topic, trigger_time):
        if (not self.enabled) or not (self.globally_enabled()):
            return

        trigger_desc = TriggerDescriptor()
        trigger_desc.topic = trigger_topic
        trigger_desc.time = int(trigger_time)

        event = ComponentTracePortTriggerExec()
        event.traceId = self._trace_id
        event.time = int(time_end)
        event.componentName = self._name
        event.executionTime = duration
        event.handlerName = handler_name
        event.triggerDescriptor = trigger_desc
        event.threadId = _get_thread_id()
        event.cpuId = _get_cpu_id()
        self._write_trace_event(event)
