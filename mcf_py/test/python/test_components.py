"""
Copyright (c) 2024 Accenture
"""

import inspect
import os
import sys
import time

# path of python mcf module, relative to location of this script
_MCF_PY_RELATIVE_PATH = "../../"

# directory of this script and relative path of mcf python tools
_SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))

sys.path.append(f"{_SCRIPT_DIRECTORY}/{_MCF_PY_RELATIVE_PATH}")

from mcf_core.component_framework import ComponentManager
from mcf_core.component_framework import Component
from mcf_core.value_store import ValueStore


def test_empty_components_manager():
    cmgr = ComponentManager()
    cmgr.configure()
    cmgr.startup()
    cmgr.shutdown()


def test_single_component():
    component = Component("ComponentName")
    cmgr = ComponentManager()
    cid = cmgr.register_component(component, "InstanceName")

    assert cid > 0, f"Registration returned unexpected component ID {cid}"

    cmgr.configure()
    cmgr.startup()
    cmgr.shutdown()


def test_multiple_components():
    component1 = Component("ComponentName")
    component2 = Component("ComponentName")
    component3 = Component("ComponentName")
    cmgr = ComponentManager()
    cid1 = cmgr.register_component(component1, "InstanceName1")
    cid2 = cmgr.register_component(component2, "InstanceName2")
    cid3 = cmgr.register_component(component3, "InstanceName3")

    assert cid1 > 0, f"Registration returned unexpected component ID {cid1}"
    assert cid2 > 0, f"Registration returned unexpected component ID {cid2}"
    assert cid3 > 0, f"Registration returned unexpected component ID {cid3}"

    assert cid2 != cid1 and cid3 != cid1 and cid3 != cid2, \
        f"Registration returned IDs that are not unique ({cid1}, {cid2}, {cid3})"

    cmgr.configure()
    cmgr.startup()
    time.sleep(0.1)
    cmgr.shutdown()


def test_component_double_registration():
    component1 = Component("ComponentName")
    component2 = Component("ComponentName")
    cmgr = ComponentManager()
    cid1 = cmgr.register_component(component1, "InstanceName1")
    cid2 = cmgr.register_component(component1, "InstanceName2")  # try registering same component with different name
    cid3 = cmgr.register_component(component2, "InstanceName1")  # try registering different component with same name

    assert cid1 > 0, f"Registration returned unexpected component ID {cid1}"
    assert cid2 < 0, f"Same component registered twice"
    assert cid3 < 0, f"Components registered with identical names"

    cmgr.configure()
    cmgr.startup()
    time.sleep(0.1)
    cmgr.shutdown()


def test_component_trigger():

    class ComponentWithTriggerHandler(Component):

        def __init__(self, vstore, topic, svalue):
            super().__init__("ComponentWithTriggerHandler", vstore)
            self._topic = topic
            self._value = svalue

        def configure(self):
            self.register_handler(self._trigger_handler)

        def _trigger_handler(self):
            # when triggered, write the given value to the given topic of the given value store
            self.set_value(self._topic, self._value)

    # create a value store
    value_store = ValueStore()

    # create and run component system without triggering the component
    cmgr = ComponentManager()
    component = ComponentWithTriggerHandler(value_store, "topic1", "triggered")
    cmgr.register_component(component, "component1")
    cmgr.configure()
    cmgr.startup()
    cmgr.shutdown()

    # => value store should not have any entry in the given topic
    value = value_store.get_value("topic1")
    assert value is None, "Component has been triggered unexpectedly"

    # create and run component system, triggering the component once
    # => value store should not have any entry in the given topic
    cmgr = ComponentManager()
    component = ComponentWithTriggerHandler(value_store, "topic1", "triggered")
    cmgr.register_component(component, "component1")
    cmgr.configure()
    cmgr.startup()
    component.trigger()
    time.sleep(0.5)     # wait sufficiently long to allow trigger event to be handled before shutting down
    cmgr.shutdown()

    # => value store should have entry in the given topic
    value = value_store.get_value("topic1")
    assert value == "triggered", "Component has not been triggered correctly"


def test_component_interaction():

    class ComponentWithValueQueue(Component):

        def __init__(self, vstore, rtopic, stopic):
            super().__init__("ComponentWithValueQueue", vstore)
            self._sender_topic = stopic
            self._receiver_topic = rtopic
            self._value_queue = None

        def configure(self):
            # create value queue, register to given topic at given value store
            self._value_queue = self.create_and_register_value_queue(maxlen=0,
                                                                     topic=self._receiver_topic,
                                                                     handler=self._on_value)

        def _on_value(self):
            # when triggered, read available values, convert to int and, if smaller than 5, send back incremented by 1
            while not self._value_queue.empty:
                str_value = self._value_queue.pop()
                int_value = int(str_value)
                self.logger.info(f"Received {str_value}")
                if int_value < 5:
                    self.logger.info(f"Sending {str(int_value + 1)}")
                    self.set_value(self._sender_topic, str(int_value + 1))

    # create a value store
    value_store = ValueStore()

    # create and run component system without writing to the value store
    # => nothing happens
    cmgr = ComponentManager()
    component1 = ComponentWithValueQueue(value_store, "topic1", "topic2")
    component2 = ComponentWithValueQueue(value_store, "topic2", "topic1")
    cmgr.register_component(component1, "component1")
    cmgr.register_component(component2, "component2")
    cmgr.configure()
    cmgr.startup()
    time.sleep(0.5)  # allow component communication before shutting down
    cmgr.shutdown()

    # => value store should not have any entry in the given topics
    value1 = value_store.get_value("topic1")
    value2 = value_store.get_value("topic2")
    assert value1 is None and value2 is None, "Components have communicated unexpectedly"

    # create and run component system, then initiate communication by writing to the value store
    # => nothing happens
    cmgr = ComponentManager()
    component1 = ComponentWithValueQueue(value_store, "topic1", "topic2")
    component2 = ComponentWithValueQueue(value_store, "topic2", "topic1")
    cmgr.register_component(component1, "component1")
    cmgr.register_component(component2, "component2")
    cmgr.configure()
    cmgr.startup()

    value_store.set_value("topic1", "0")
    time.sleep(0.5)  # allow component communication before shutting down

    cmgr.shutdown()

    # => value store should not have any entry in the given topics
    value1 = value_store.get_value("topic1")
    value2 = value_store.get_value("topic2")
    assert value1 == "4" and value2 == "5", "Components have not communicated as expected"
