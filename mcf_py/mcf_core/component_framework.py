"""
Copyright (c) 2024 Accenture
"""
import os.path
import enum
import json
import sys
import threading
import time
import traceback

from mcf_core.events import Event
from mcf_core.helpers import now_micros, deep_merge_dicts
from mcf_core.logger import get_logger
from mcf_core.value_store import ValueQueue


class ComponentManager:

    class CompState(enum.Enum):
        REGISTERED = 0
        CONFIGURED = 1
        RUNNING = 2

    class CompMapEntry:
        def __init__(self, component, comp_id, instance_name, state):
            self.component = component
            self.id = comp_id
            self.instance_name = instance_name
            self.state = state

    def __init__(self, ctrace_controller=None):
        self._lock = threading.Lock()
        self._components = dict()
        self._last_comp_id = 0
        self._logger = get_logger("ComponentManager")
        self._ctrace_controller = ctrace_controller

    def _get_next_comp_id(self):
        self._last_comp_id += 1
        return self._last_comp_id

    def register_component(self, component, instance_name, config_name=None):

        registered = [e.component for e in self._components.values()]
        if component in registered:
            self._logger.error("Component already registered")
            return -1

        instance_names = [e.instance_name for e in self._components.values()]
        if instance_name in instance_names:
            self._logger.error("Instance name already exists")
            return -1

        comp_id = self._get_next_comp_id()
        state = ComponentManager.CompState.REGISTERED

        if config_name is None:
            config_name = f"{instance_name}.json"

        component.ctrl_set_config_name(config_name)

        if self._ctrace_controller is not None:
            ctrace_event_gen = self._ctrace_controller.create_event_generator(instance_name)
            component.ctrl_set_component_trace_event_generator(ctrace_event_gen)

        map_entry = ComponentManager.CompMapEntry(component=component,
                                                  comp_id=comp_id,
                                                  instance_name=instance_name,
                                                  state=state)

        with self._lock:
            self._components[comp_id] = map_entry

        self._logger.info(f"Registered component instance {instance_name} with id {comp_id}")
        return comp_id

    def configure(self):
        """
        Configure components.
        This calls the component's configure methods
        """
        self._logger.info("Configuring")
        with self._lock:
            for entry in self._components.values():

                # if component has already been configured, skip it
                if entry.state != ComponentManager.CompState.REGISTERED:
                    continue

                entry.component.ctrl_configure(entry.instance_name)
                entry.state = ComponentManager.CompState.CONFIGURED
                self._logger.info(f"Configured component instance {entry.instance_name} with id {entry.id}")

    def startup(self):
        """
        Starts all components
        """
        self._logger.info("Starting up")
        components_started = []
        with self._lock:
            for entry in self._components.values():

                # if component not configured or already running, skip it
                if entry.state != ComponentManager.CompState.CONFIGURED:
                    continue

                self._logger.info(f"Starting component instance {entry.instance_name} with id {entry.id}")
                # start component, remember it
                entry.component.ctrl_start()
                components_started.append(entry)

            # wait until all components started
            waiting = True
            while waiting:
                waiting = False
                for entry in components_started:
                    if not entry.component.is_running:
                        waiting = True
                time.sleep(0.01)

            # trigger each components run method once
            for entry in components_started:
                entry.component.ctrl_run()
                entry.state = ComponentManager.CompState.RUNNING

        self._logger.info("Startup finished")

    def shutdown(self):
        """
        Shuts down all components
        """
        self._logger.info("Shutting down")
        components_stopped = []
        with self._lock:
            for entry in self._components.values():

                # if component not running, skip it
                if entry.state != ComponentManager.CompState.RUNNING:
                    continue

                entry.component.ctrl_stop()
                components_stopped.append(entry)

            # wait until all components stopped
            waiting = True
            while waiting:
                waiting = False
                for entry in components_stopped:
                    if entry.component.is_running:
                        waiting = True
                time.sleep(0.01)

            # trigger each components run method once
            for entry in components_stopped:
                entry.state = ComponentManager.CompState.CONFIGURED

        self._logger.info("Shutdown finished")

    def get_component(self, comp_id):
        """
        Returns the component registered with the given ID
        :param comp_id: the component ID
        :return: the component
        """


class ValueReceivedEvent(Event):

    def __init__(self, queue, ctrace_event_gen):
        """
        :param queue:            The value queue that will be triggering this event
        :param ctrace_event_gen: An optional trace event generator (might be None)
        """
        self._queue = queue
        self._trace_generator = ctrace_event_gen
        super().__init__()

    def trigger(self):
        super().trigger()
        if self._trace_generator is not None:
            topic, time = self._queue.last_topic_and_time
            self._trace_generator.trace_port_trigger_activation(time, topic)


class ComponentThread(threading.Thread):

    def __init__(self, **kw_args):
        super().__init__(**kw_args)

    def run(self) -> None:
        super().run()


class Component:

    class ValueHandler:
        """
        A value queue with an optional handler triggered on incoming values
        """
        def __init__(self, queue, comp_trigger, handler, trace_generator, name=None):
            """
            :param queue:           the value queue object
            :param comp_trigger:    the component trigger event
            :param handler:         the handler to call when the queue receives a value (might be None)
            :param trace_generator: an optional ComponentTraceEventGenerator (might be None)
            :param name:            the name of the handler; default is an empty string
            """

            # register this object as an event
            self._queue = queue
            self._comp_trigger = comp_trigger
            self._received_value = ValueReceivedEvent(queue, trace_generator)
            self._name = "" if name is None else name

            # if we have a handler, register component trigger and received-value event at queue
            if handler is not None:
                self._queue.add_event(self._received_value)
                self._queue.add_event(self._comp_trigger)
                self._handler = handler

            # otherwise store an empty handler (normally never called)
            else:
                self._handler = lambda: None

        @property
        def name(self):
            return self._name

        @property
        def queue(self):
            return self._queue

        def handle(self):
            """
            Call the handler (if any)
            """
            self._handler()

        @property
        def active(self):
            return self._received_value.active

        def clear(self):
            self._received_value.clear()

    def __init__(self, name, value_store=None, config_dirs=None):

        # the component thread
        self._thread = None
        self._name = name
        self._instance_name = name
        self._config_name = name + ".json"
        self._trigger = Event()
        self._logger = get_logger(f"Component.{name}.unregistered")
        self._startup_request = threading.Event()
        self._shutdown_request = threading.Event()
        self._trigger_handlers = []
        self._value_handlers = []
        self._vstore = value_store
        self._trace_generator = None

        self._config_dirs = []
        if config_dirs is not None:
            self._config_dirs = list(config_dirs)

        if self._vstore is None:
            msg = (f"Component {self._name} created without a value store. "
                   f"It is recommended to provide a value store and to "
                   f"use the component methods set_value() and get_value() in order to "
                   f"enable component tracing.")
            self._logger.warning(msg)
            print(msg)

    @property
    def component_trace_event_generator(self):
        return self._trace_generator

    @property
    def config_dirs(self):
        return self._config_dirs

    def read_config(self):
        cfg_files = []
        for cfg_dir in self._config_dirs:
            cfg_file = os.path.join(cfg_dir, self.config_name)
            cfg_files.append(cfg_file)

        # read all config files (skip non-existing)
        cfg_dicts = []
        for path in cfg_files:
            if not os.path.exists(path):
                continue

            with open(path, "r") as file:
                cfg = json.load(file)
                cfg_dicts.append(cfg)

        # if no config file found, return empty config
        if len(cfg_dicts) == 0:
            return {}

        # otherwise merge configs
        parent = cfg_dicts[0]
        children = cfg_dicts[1:]
        deep_merge_dicts(parent, children)

        return parent

    def check_config(self, cfg_dict):
        """
        Helper method checking if the given config dictionary contains
        the component name as key and extracting the corresponding value
        :param cfg_dict: the config dictionary to check, usually obtained from calling 'read_config'.
        :return: The value of the config entry corresponding to the component name or None
        """
        if len(cfg_dict.keys()) == 0:
            self.logger.error("Component config file does not exist or is empty")
            return None

        config = cfg_dict.get(self.name, None)
        if config is None:
            self.logger.error(f"Missing key '{self.name}' in component config file")
            return None

        return config

    def ctrl_configure(self, instance_name):
        if self.is_running:
            self.logger.error("Cannot configure component, because it is already running")
            return

        self._instance_name = instance_name
        self._logger = get_logger(self._instance_name)
        self.configure()
        self.logger.info("configured")

    def ctrl_start(self):
        if self.is_running:
            self.logger.error("Cannot start component, because it is already running")
            return

        self._thread = ComponentThread(target=self._thread_main, name=self._name)
        self._startup_request.clear()
        self._shutdown_request.clear()
        self._thread.start()

    def ctrl_stop(self):
        if self.is_running:
            self._shutdown_request.set()
            self._trigger.trigger()
            self._thread.join()
            self._thread = None

    def ctrl_set_config_name(self, config_name):
        self._config_name = config_name

    def ctrl_run(self):
        self._startup_request.set()

    def ctrl_set_component_trace_event_generator(self, ctrace_event_gen):
        self._trace_generator = ctrace_event_gen

    def startup(self):
        pass

    def shutdown(self):
        pass

    def configure(self):
        pass

    @property
    def shutdown_request(self):
        return self._shutdown_request.is_set()

    @property
    def startup_request(self):
        return self._startup_request.is_set()

    def trigger(self):
        self._trigger.trigger()

    def abort(self):
        """
        May be called from any thread to abort component operation, e.g. in case of an error.
        """
        self.logger.error("Aborting operation")
        self._shutdown_request.set()
        self.trigger()

    def _thread_main(self):
        self.logger.info("Starting up")

        try:

            self.startup()

            # wait until requested to run or stop
            while (not self.startup_request) and (not self.shutdown_request):
                time.sleep(0.01)

            self.logger.info("Running")

            while not self.shutdown_request:

                self._trigger.wait_and_clear()
                for handler in self._trigger_handlers:
                    if not self.shutdown_request:
                        start_time = now_micros()
                        handler()
                        end_time = now_micros()
                        self._trace_trigger_handler_exec(start_time, end_time)

                for vhandler in self._value_handlers:
                    if (not self.shutdown_request) and vhandler.active:
                        vhandler.clear()
                        start_time = now_micros()
                        vhandler.handle()
                        end_time = now_micros()
                        self._trace_port_trigger_handler_exec(start_time, end_time, vhandler)

        except:
            exc_type, exc, tb = sys.exc_info()
            self.logger.error(f"Main thread exception {exc}: {traceback.format_tb(tb)}")
            self.abort()

        finally:

            self.logger.info("Shutting down")
            self.shutdown()

    @property
    def config_name(self):
        return self._config_name

    @property
    def name(self):
        return self._name

    @property
    def instance_name(self):
        return self._instance_name

    @property
    def logger(self):
        return self._logger

    @property
    def value_store(self):
        return self._vstore

    @property
    def is_running(self):
        return self._thread is not None

    def register_handler(self, trigger_handler):
        self._trigger_handlers.append(trigger_handler)
        # TODO: avoid multiple registration of same handler

    def register_value_queue(self, queue, handler=None):
        """
        Register a value queue with an optional handler triggered on incoming values
        :param queue:    the value queue object to register
        :param handler:  the handler to call when the queue receives a value
        """
        value_handler = Component.ValueHandler(queue, self._trigger, handler, self._trace_generator)
        self._value_handlers.append(value_handler)
        # TODO: avoid multiple registration of same queue

    def create_and_register_value_queue(self, maxlen, topic, handler=None):
        """
        Convenience method for creating and registering a value queue
        with an optional handler triggered on incoming values

        :param maxlen:   maximal length of the queue
        :param topic:    The topic of values the queue will receive
        :param handler:  the handler to call when the queue receives a value

        :return: the queue
        """
        queue = ValueQueue(maxlen=maxlen, ctrace_event_gen=self._trace_generator)
        self._vstore.add_receiver(topic, queue)
        self.register_value_queue(queue, handler)
        return queue

    def set_value(self, topic, value, input_ids=None):
        assert self._vstore is not None, "Value store not defined"
        if self._trace_generator is not None:
            self._trace_generator.trace_set_port_value(topic, True, input_ids, value)
        self._vstore.set_value(topic, value)

    def get_value(self, topic):
        assert self._vstore is not None, "Value store not defined"
        value = self._vstore.get_value(topic)
        if self._trace_generator is not None:
            self._trace_generator.trace_get_port_value(topic, True, value)
        return value

    def _trace_trigger_handler_exec(self, start_time_us, end_time_us):
        if self._trace_generator is None:
            return

        duration_secs = (end_time_us - start_time_us) * 1.e-6
        self._trace_generator.trace_execution_time(end_time_us, duration_secs, "triggerHandlers")

    def _trace_port_trigger_handler_exec(self, start_time_us, end_time_us, handler: ValueHandler):
        if self._trace_generator is None:
            return

        topic, trigger_time = handler.queue.last_topic_and_time
        duration_secs = (end_time_us - start_time_us) * 1.e-6
        self._trace_generator.trace_port_trigger_exec(start_time_us, duration_secs, handler.name, topic, trigger_time)
