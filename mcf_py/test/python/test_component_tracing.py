"""
Copyright (c) 2024 Accenture
"""

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

from mcf_core.component_framework import Component, ComponentManager
from mcf_core.component_trace_messages import ComponentTracePortPeek, ComponentTracePortRead
from mcf_core.component_trace_messages import ComponentTraceExecTime, ComponentTracePortWrite
from mcf_core.component_trace_messages import ComponentTracePortTriggerActivation, ComponentTracePortTriggerExec
from mcf_core.component_tracing import ComponentTraceEventController
from mcf_core.value_store import ValueQueue, ValueStore
from mcf_core.value_recorder import ValueRecorder

from value_types.value_types.base.PointXYZ import PointXYZ

from mcf_core.logger import get_logger

_RECORDING_DIR = os.path.join(_SCRIPT_DIRECTORY, "../tmp/")
os.makedirs(_RECORDING_DIR, exist_ok=True)

_RECORDING_FILE = os.path.join(_RECORDING_DIR, "trace.bin")


cur_value_id = 41


def _next_value_id():
    global cur_value_id
    id = cur_value_id
    cur_value_id += 1
    return id


def test_queue_trace_get_port_value():
    global cur_value_id
    cur_value_id = 41

    # create a value stores for "normal" values and another one for trace events
    value_store = ValueStore()
    event_store = ValueStore()

    # create a queue receiving events so that we can check the events being generated
    event_queue = ValueQueue()
    event_store.add_all_topic_receiver(event_queue)

    # create an event generator
    trace_controller = ComponentTraceEventController("A", event_store)
    event_generator = trace_controller.create_event_generator("test")

    # create a value queue connected to the "normal" value store
    value_queue = ValueQueue(ctrace_event_gen=event_generator)
    value_store.add_receiver("topic1", value_queue)

    # write two values to the value store
    value_1 = PointXYZ(0.1, 0.1, 0.1)
    value_id_1 = _next_value_id()
    value_1.inject_id(value_id_1)
    value_store.set_value("topic1", value_1)

    value_2 = PointXYZ(0.2, 0.2, 0.2)
    value_id_2 = _next_value_id()
    value_2.inject_id(value_id_2)
    value_store.set_value("topic1", value_2)

    # peek value => should create related trace events
    _ = value_queue.peek()
    _, _ = value_queue.peek_with_topic()

    time.sleep(0.1)

    # two events should be in the queue
    assert event_queue.size == 2, "Event trace does not contain the expected number of events"

    # both events should be of type ComponentTracePortPeek, stored value id should match the "peeked" value
    while event_queue.size > 0:
        event = event_queue.pop()
        assert type(event) is ComponentTracePortPeek, "Unexpected event type"
        assert event.valueId == value_id_1, f"Wrong valueId in event {event}"

    # pop both values => should create related trace events
    _ = value_queue.pop()
    _, _ = value_queue.pop_with_topic()

    # two events should be in the queue
    assert event_queue.size == 2, "Event trace does not contain the expected number of events"

    # both events should be of type ComponentTracePortRead, stored value id should match the raed values
    event = event_queue.pop()
    assert type(event) is ComponentTracePortRead, "Unexpected event type"
    assert event.valueId == value_id_1, f"Wrong valueId in event {event}"
    event = event_queue.pop()
    assert type(event) is ComponentTracePortRead, "Unexpected event type"
    assert event.valueId == value_id_2, f"Wrong valueId in event {event}"


class MyTestComponent(Component):

    def __init__(self, value_store):
        super().__init__("MyTestComponent", value_store)
        self._in_queue = None

    def configure(self):
        self._in_queue = self.create_and_register_value_queue(maxlen=0, topic="topic_1", handler=self._handle_in)
        self.register_handler(self._trigger_handler)

    def write_test_value(self):
        test_value = PointXYZ(0.3, 0.3, 0.3)
        value_id = _next_value_id()
        test_value.inject_id(value_id)
        self.set_value("topic_2", test_value)
        return value_id

    def _handle_in(self):
        pass

    def _trigger_handler(self):
        pass


def test_component():

    global cur_value_id
    cur_value_id = 41

    # create a value stores for "normal" values and another one for trace events
    value_store = ValueStore()
    event_store = ValueStore()

    # create a queue receiving events so that we can check the events being generated
    event_queue = ValueQueue()
    event_store.add_all_topic_receiver(event_queue)

    # remove recording file from previous test if any
    if os.path.isfile(_RECORDING_FILE):
        os.remove(_RECORDING_FILE)
    event_recorder = ValueRecorder(event_store, get_logger("event_recorder"))
    event_recorder.start(_RECORDING_FILE)

    # create an event controller
    trace_controller = ComponentTraceEventController("A", event_store)
    cmgr = ComponentManager(ctrace_controller=trace_controller)

    # create a trace generator and a component using it
    comp_inst_name_1 = "test_comp_1"
    comp_1 = MyTestComponent(value_store)

    cmgr.register_component(comp_1, comp_inst_name_1)
    cmgr.configure()

    try:
        cmgr.startup()

        # trigger the component => should create an event when handled
        comp_1.trigger()
        time.sleep(0.1)

        # let the component write a value => should create an event
        test_value_id = comp_1.write_test_value()

        # write to component's input queue => should create
        # - a port trigger activation event
        # - an execution time event from the trigger handler called
        # - a port trigger handled event from
        test_value = PointXYZ(0.4, 0.4, 0.4)
        value_id = _next_value_id()
        test_value.inject_id(value_id)
        value_store.set_value("topic_1", test_value)

        # wait until events are written
        time.sleep(0.1)

    finally:
        cmgr.shutdown()

    # event queue should contain:
    # - 1 execution time event from handling the explicitly called trigger
    # - 1 port write event from handling the explicitly called trigger
    # - 1 port trigger activation event from receiving a value on the input queue
    # - 1 execution time event from the trigger handler called after receiving a value on the input queue
    # - 1 port trigger execution event from handling the port trigger after receiving a value on the input queue

    # check expected number of events in the queue
    assert event_queue.size == 5, "Event trace does not contain the expected number of events"

    # check events
    event = event_queue.pop()
    assert type(event) is ComponentTraceExecTime, "Unexpected event type"
    assert event.componentName == comp_inst_name_1

    event = event_queue.pop()
    assert type(event) is ComponentTracePortWrite, "Unexpected event type"
    assert event.valueId == test_value_id, f"Wrong valueId in event {event}"
    assert event.componentName == comp_inst_name_1
    assert event.portDescriptor.topic == "topic_2"

    event = event_queue.pop()
    assert type(event) is ComponentTracePortTriggerActivation, "Unexpected event type"
    assert event.componentName == comp_inst_name_1
    assert event.triggerDescriptor.topic == "topic_1"

    event = event_queue.pop()
    assert type(event) is ComponentTraceExecTime, "Unexpected event type"
    assert event.componentName == comp_inst_name_1

    event = event_queue.pop()
    assert type(event) is ComponentTracePortTriggerExec, "Unexpected event type"
    assert event.componentName == comp_inst_name_1
    assert event.triggerDescriptor.topic == "topic_1"

    time.sleep(2)
    event_recorder.stop()
