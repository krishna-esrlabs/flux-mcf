"""
Copyright (c) 2024 Accenture
"""

from collections import deque
from dataclasses import dataclass
from datetime import datetime
from datetime import timedelta
import enum
import random
import threading
import time

from mcf_core.component_framework import Component
from mcf_core.value_store import ValueQueue


class RemoteState(enum.Enum):
    """
     Assumed state of the other side. Depending on whether the other side is responding to
     messages or not, the RemoteService enters one of the following stages and acts
     accordingly:
     DOWN:   Assuming the other side is not active. Don't forward any Values.
             Don't send any messages, listen to messages from the other side.
     UNSURE: State of the other side is uncertain, Don't forward any Values.
             Actively send ping messages and wait for responses and listen to messages from
             the other side
     UP:     Assuming the other side is up and ready to receive messages. Forward values to
             the other side and listen for the responses and other messages form the other side.
             Actively send ping messages at a low frequency and listen for responses
    """
    STATE_DOWN = 0
    STATE_UNSURE = 1
    STATE_UP = 2


class RemoteStatusTracker:

    """
    Class to keep track of the assumed state of the other side of a RemotePair.
    Pings are sent and the RemoteState is changed depending on if Pongs are received from the
    other side.
    In STATE_DOWN, no pings are sent out. If a ping or another message is received, the state is
    changed to STATE_UNSURE.
    In STATE_UNSURE, the ping interval will be doubled on every ping, starting with pingInterval.
    When the ping interval reaches pingIntervalMax and no matching pong was received, the state is
    changed to STATE_DOWN. If a pong is received, the state is changed to STATE_UP.
    In STATE_UP the ping frequency is decreased to 1/pingIntervalMax. If a ping is not answered by
    a pong, the state is changed to STATE_DOWN. If the function sendingTimeout is called, the state
    is changed to STATE_UNSURE.
    """

    def __init__(self, ping_sender, ping_interval_min=100, ping_interval_max=3000, pong_timeout=5000):
        """
        :param ping_sender:         function to call for sending a ping
        :param ping_interval_min:   minimal and initial ping interval [ms]
        :param ping_interval_max:   maximal ping interval [ms]
        :param pong_timeout:        time [ms] to wait for a pong until the other side is considered to be down
        """
        self._ping_sender = ping_sender
        self._ping_interval_min = timedelta(0, 0, 0, ping_interval_min)
        self._ping_interval_max = timedelta(0, 0, 0, ping_interval_max)
        self._ping_interval = self._ping_interval_min
        self._pong_timeout = timedelta(0, 0, 0, pong_timeout)
        self._remote_state = RemoteState.STATE_UNSURE
        self._condition = threading.Condition(threading.Lock())
        self._ping_freshness = random.randint(0, 18446744073709551615)  # uint64
        self._last_ping_time = datetime(1970, 1, 1)
        self._last_pong_time = datetime(1970, 1, 1)

    @property
    def remote_state(self):
        """
        :return: the current remote state
        """
        return self._remote_state

    def pong_received(self, freshness):
        """
        Function to call when a pong is received. The freshness value is compared to the ones
        sent out with the most recent ping to verify the latest ping was answered
        :param freshness: The freshness value that was received with the pong message
        """
        now = datetime.utcnow()
        with self._condition:
            if freshness != self._ping_freshness:
                return

            if self._remote_state == RemoteState.STATE_UNSURE:
                self._set_remote_state(RemoteState.STATE_UP)

            if self._remote_state == RemoteState.STATE_UP:
                self._last_pong_time = now

    def run_cyclic(self):
        """
        Function responsible for sending pings and check if pong-timeouts have occurred.
        This function should be called on regularly by an external caller.
        """
        now = datetime.utcnow()
        with self._condition:

            if self._remote_state == RemoteState.STATE_UNSURE:
                if now > self._last_ping_time + self._ping_interval:
                    self._send_ping(now)
                    self._ping_interval = 2 * self._ping_interval
                    if self._ping_interval > self._ping_interval_max:
                        self._set_remote_state(RemoteState.STATE_DOWN)

            if self._remote_state == RemoteState.STATE_UP:
                if now > self._last_ping_time + self._ping_interval:
                    self._send_ping(now)
                    if now > self._last_pong_time + self._pong_timeout:
                        self._set_remote_state(RemoteState.STATE_DOWN)

    @property
    def ping_interval(self):
        with self._condition:
            return self._ping_interval

    def message_received_in_down(self):
        """
        This function shall be called if a message is received while in STATE_DOWN. It will change
        the state to STATE_UNSURE
        """
        with self._condition:
            self._set_remote_state(RemoteState.STATE_UNSURE)

    def sending_timeout(self):
        """
        This function shall be called if a timeout on any sent message (not only ping/pong messages)
        has occurred. It will change the state to STATE_UNSURE.
        """
        with self._condition:
            self._set_remote_state(RemoteState.STATE_UNSURE)

    def wait_for_event(self):
        """
        Blocks the calling thread until an event is triggered in the RemoteStatusTracker.
        This event is a change of the remote state or a ping interval timeout
        """
        with self._condition:
            wait_timeout_secs = self._ping_interval.total_seconds()
            self._condition.wait(wait_timeout_secs)

    def _set_remote_state(self, state):
        if state == RemoteState.STATE_UNSURE:
            self._ping_interval = self._ping_interval_min
            self._last_ping_time = datetime(1970, 1, 1)

        if state == RemoteState.STATE_UP:
            self._ping_interval = self._ping_interval_max

        self._remote_state = state
        self._condition.notify_all()

    def _send_ping(self, time_now):
        self._last_ping_time = time_now
        self._ping_freshness += 1
        self._ping_sender(self._ping_freshness)


class StorageEndpoint:
    """
    Interface class (written out for clarity, not strictly needed).

    A storage endpoint is used by the remote service for locally retrieving data to be sent and writing
    data received. In a standard scenario, for example, this can be a connector to an MCF value store.
    """
    def value_received(self, topic, value):
        """
         Value reception handler
        :param topic:   the topic of the received value
        :param value:   the received value
        :return:    response string
        """
        pass

    def send_all(self):
        """
        Handler for the request for all available values
        """
        pass

    def reset_pending_values(self):
        """
        Handler that resets values pending to be written
        """
        pass

    def trigger_send_cycle(self):
        """
        Handler that wakes up the sending cycle
        """
        pass

    def blocked_value_injected_received(self, topic):
        """
        Handler for the event when a blocked value has been injected on the remote side.

        :param topic: the topic of the value.
        """
        pass

    def blocked_value_rejected_received(self, topic):
        """
        Handler for the event when a blocked value has been rejected on the remote side.

        :param topic: the topic of the value.
        """
        pass


class RemotePair:

    def __init__(self, sender, receiver, storage_endpoint, logger):
        self._sender = sender
        self._receiver = receiver
        self._remote_status_tracker = RemoteStatusTracker(self._send_ping)
        self._logger = logger
        self._pong_queue = deque()
        self._endpoint = storage_endpoint
        self._oldState = RemoteState.STATE_UNSURE

        self._lock = threading.Lock()

        self._receiver.set_event_listener(self)

    def _send_ping(self, freshness):
        with self._lock:
            self._sender.send_ping(freshness)

    def send_value(self, topic, value):

        """
        Sends a value to a receiving remote service
        :param topic: The topic
        :param value: The value
        :return: A string indicating the result of the sending operation.
                 Can be one of "INJECTED", "REJECTED", "TIMEOUT"
        """
        with self._lock:
            result = self._sender.send_value(topic, value)
            if result == "TIMEOUT":
                self._remote_status_tracker.sending_timeout()
            return result

    def send_blocked_value_injected(self, topic):
        """
        Communicates to a sending remote service that a previously blocked value has been injected.

        :param topic: The topic of the injected value
        :return: Result of the operation
        """
        with self._lock:
            result = self._sender.send_blocked_value_injected(topic)
            if result == "TIMEOUT":
                self._remote_status_tracker.sending_timeout()

    def send_blocked_value_rejected(self, topic):
        """
        Communicates to a sending remote service that a previously blocked value has been rejected.

        :param topic: The topic of the injected value
        :return: Result of the operation
        """
        with self._lock:
            result = self._sender.send_blocked_value_rejected(topic)
            if result == "TIMEOUT":
                self._remote_status_tracker.sending_timeout()
            return result

    def value_received(self, topic, value):
        """
        Function to be called by the receiver when it received a Value.

        :param topic: the topic to which the value shall be written
        :param value: the received value
        :return: A string indicating the result of the operation
                 There are 3 possible return strings
                 - REJECTED   value was rejected the value (e.g. because of a missing receive rule)
                 - RECEIVED   the value was received but not yet injected into the target value store
                 - INJECTED   the value was received and injected into the target value store
        """
        return self._endpoint.value_received(topic, value)

    def ping_received(self, freshness):
        """
        Function to be called by the receiver when it received a ping message.

        :param freshness: The freshness value that has been received with the ping
        """
        with self._lock:
            self._pong_queue.append(freshness)
            self._endpoint.trigger_send_cycle()

    def pong_received(self, freshness):
        """
        Function to be called by the receiver when it received a pong message.
        :param freshness: The freshness value that has been received with the pong
        """
        self._remote_status_tracker.pong_received(freshness)

    def request_all_received(self):
        """
        Function to be called by the receiver when it received a message with a requestAll command
        """
        self._endpoint.send_all()

    def blocked_value_injected_received(self):
        """
        Function to be called by the receiver when it received a message with a valueInjected command
        """
        self._endpoint.blocked_value_injected_received()

    def blocked_value_rejected_received(self):
        """
        Function to be called by the receiver when it received a message with a valueRejected command
        """
        self._endpoint.blocked_value_rejected_received()

    @property
    def connected(self):
        """
        Checks if a connection to the remote side is currently established

        :return: True, if the remote state is STATE_UP;
                 False otherwise
        """
        return self._remote_status_tracker.remote_state == RemoteState.STATE_UP

    def send_request_all(self):
        """
        Sends a control message to a remote service requesting one value (if any are available) on every sendRule
        """
        self._sender.send_request_all()

    @property
    def remote_state(self):
        """
        :return: The assumed status of the remote side (UP, DOWN, or UNKNOWN)
        """
        return self._remote_status_tracker.remote_state

    def connect_receiver(self):
        """
        Connects the _receiver

        """
        self._receiver.connect()

    def disconnect_receiver(self):
        """
        Disconnects the receiver
        """
        self._receiver.disconnect()

    def connect_sender(self):
        """
        Connects the _receiver

        """
        self._sender.connect()

    def disconnect_sender(self):
        """
        Disconnects the receiver
        """
        self._sender.disconnect()

    def receive(self):
        """
        Try to receive a value. The function will return either after it received a value or when
        the timeout has expired.

        :return: True if a message has been received, False if the function timed out
        """
        success = self._receiver.receive()
        if success and self._remote_status_tracker.remote_state == RemoteState.STATE_DOWN:
            self._remote_status_tracker.message_received_in_down()
        return success

    @property
    def ping_interval(self):
        """
        :return: the current interval [ms] at which ping messages are sent
        """
        return 1000 * self._remote_status_tracker.ping_interval.total_seconds()

    def wait_for_event(self):
        """
        Blocks the calling thread until an event is triggered in the RemoteStatusTracker.
        This event is a change of the remote state or a ping interval timeout
        """
        self._remote_status_tracker.wait_for_event()

    def remote_state_string(self):
        """
        Returns a string that represents the currently assumed remote state.

        :return: One of the strings "DOWN", "UNSURE", "UP"
        """
        state = self._remote_status_tracker.remote_state

        if state == RemoteState.STATE_DOWN:
            return "DOWN"
        elif state == RemoteState.STATE_UNSURE:
            return "UNSURE"
        elif state == RemoteState.STATE_UP:
            return "UP"

        return "UnknownRemoteState"

    def observe_state_change(self):
        """
        Observe if the remote state has changed since the last time this function was called.
        """
        curr_state = self._remote_status_tracker.remote_state
        if self._oldState != curr_state:

            if self._logger is not None:
                self._logger.info(f"Switching to state {self.remote_state_string()}")

            if self._oldState == RemoteState.STATE_UP:
                self._change_state_from_up()

            self._oldState = curr_state

    def cycle(self):
        """
        Triggers the runCyclic method of _remoteStatusTracker and the sending of all pongs in _pong_queue
        """
        self._remote_status_tracker.run_cyclic()
        self._send_pongs()

    def _send_pongs(self):
        with self._lock:
            while len(self._pong_queue) > 0:
                freshness = self._pong_queue.popleft()
                self._sender.send_pong(freshness)

    def _change_state_from_up(self):
        # reset connection
        self.disconnect_sender()
        self.connect_sender()

        self._endpoint.reset_pending_values()


@dataclass
class SendState:
    forced_send: bool = False
    send_pending: bool = False


@dataclass
class SendRule:
    topic: str
    queue_length: int
    state: SendState
    queue: ValueQueue = None


@dataclass
class ReceiveState:
    pending_value: ... = None


@dataclass
class ReceiveRule:
    topic: str
    state: ReceiveState


class RemoteService(Component, StorageEndpoint):

    def __init__(self, value_store, sender, receiver):
        Component.__init__(self, f"RemoteService", value_store)
        StorageEndpoint.__init__(self)

        self._send_rules = {}
        self._receive_rules = {}
        self._inserted_topics = []
        self._rejected_topics = []
        self._initialized = False
        self._sender = sender
        self._receiver = receiver
        self._transceiver = None

        self._lock_send = threading.Lock()
        self._cond_receive = threading.Condition()
        self._lock_topics = threading.Lock()

        self._trigger_cyclic_thread = None
        self._receive_thread = None
        self._handle_pending_values_thread = None

        self._sender.set_logger(self.logger)
        self._receiver.set_logger(self.logger)

    def _handle_triggers(self):

        self._transceiver.observe_state_change()

        if not self._initialized and self._transceiver.connected:
            self._initialized = True
            self._transceiver.send_request_all()

        self._handle_send()
        self._handle_injected_rejected()

        self._transceiver.cycle()

    def add_send_rule(self, topic_local, topic_remote=None, queue_len=1, blocking=False):
        """
        Add a sending rule.
        MUST NOT be called after ComponentManager configure() call

        :param topic_local:     The topic whose values shall be forwarded from the local value store to the target
        :param topic_remote:    The topic name used to forward the values from topicLocal to the target.
                                Only one send rule for a topicRemote shall be added.
                                If set to None, the value of topic_local will be used.
        :param queue_len:       Maximum number of values that are queued before dropping
        :param blocking:        If True (currently not supported) the sender will be blocked if the queue is full
                                     until at least one Value has been taken from the queue
                                If False new Values will be dropped if the queue is full
        :return:
        """
        if topic_remote is None:
            topic_remote = topic_local

        assert not blocking, "Blocking queues are not yet supported by RemoteService"
        assert topic_remote not in self._send_rules, \
               f"A send rule for remote topic {topic_remote} has already been defined for remote " \
               f"service instance '{self._instance_name}'"

        self._send_rules[topic_remote] = SendRule(topic=topic_local,
                                                  queue_length=queue_len,
                                                  state=SendState())

    def add_receive_rule(self, topic_local, topic_remote=None):
        """
        Add a sending rule.
        MUST NOT be called after ComponentManager configure() call

        :param topic_local:  The topic name under which the received values will be put into the local
                             value store
        :param topic_remote: The topic whose values shall be received from the sender. Only one
                             receive rule per topicRemote shall be added.
                             If set to None, the value of topic_local will be used.
        """
        if topic_remote is None:
            topic_remote = topic_local

        assert topic_remote not in self._receive_rules, \
               f"A reception rule for remote topic {topic_remote} has already been defined for remote " \
               f"service instance '{self._instance_name}'"

        self._receive_rules[topic_remote] = ReceiveRule(topic=topic_local, state=ReceiveState())

    def _handle_ports(self):
        #  nothing to be done, main component trigger handler will always be called
        #  and handle the port event
        pass

    def configure(self):
        self._sender.set_logger(self.logger)
        self._receiver.set_logger(self.logger)
        self._transceiver = RemotePair(self._sender, self._receiver, self, self.logger)

        # create and register value queues for send rules
        for snd_rule in self._send_rules.values():
            value_queue = self.create_and_register_value_queue(maxlen=snd_rule.queue_length,
                                                               topic=snd_rule.topic,
                                                               handler=self._handle_ports)
            snd_rule.queue = value_queue

        self.register_handler(self._handle_triggers)

    def startup(self):
        self._transceiver.connect_sender()

        # create and start threads
        self._trigger_cyclic_thread = threading.Thread(target=self._trigger_cyclic_thread_main, name="trigger_cyclic")
        self._receive_thread = threading.Thread(target=self._receive_thread_main, name="receive")
        self._handle_pending_values_thread = threading.Thread(target=self._handle_pending_values_thread_main,
                                                              name="handle_pending_values")

        self._trigger_cyclic_thread.start()
        self._receive_thread.start()
        self._handle_pending_values_thread.start()

    def shutdown(self):

        with self._cond_receive:
            self._cond_receive.notify_all()
        self._handle_pending_values_thread.join()
        self._receive_thread.join()
        self._trigger_cyclic_thread.join()

        if self._transceiver is not None:
            self._transceiver.disconnect_sender()

    def value_received(self, topic, value):
        """
        See parent class StorageEndpoint
        """
        if not self._initialized:
            return "REJECTED"

        with self._cond_receive:
            rec_rule = self._receive_rules.get(topic, None)

            if rec_rule is None:
                return "REJECTED"
            if rec_rule.state.pending_value is not None:
                return "REJECTED"

            self.set_value(topic, value)
            # TODO: handle port blocking here, once implemented

            return "INJECTED"

    def send_all(self):
        """
        See parent class StorageEndpoint
        """
        with self._lock_send:
            for snd_rule in self._send_rules.values():
                snd_rule.state.forced_send = True

        self.trigger()

    def reset_pending_values(self):
        """
        See parent class StorageEndpoint
        """
        with self._lock_send:
            for snd_rule in self._send_rules.values():
                snd_rule.state.send_pending = False

    def trigger_send_cycle(self):
        """
        See parent class StorageEndpoint
        """
        self.trigger()

    def blocked_value_injected_received(self, topic):
        """
        See parent class StorageEndpoint
        """
        with self._lock_send:
            snd_rule = self._send_rules.get(topic, None)
            if snd_rule is not None:
                snd_rule.state.send_pending = False

        self.trigger()

    @property
    def connected(self):
        """
        Checks if a connection to the remote side is currently established

        :return: True, if _initialized is true and the remote state is STATE_UP,
                 False otherwise
        """
        return self._initialized and self._transceiver.connected

    def blocked_value_rejected_received(self, topic):
        """
        See parent class StorageEndpoint
        """
        with self._lock_send:
            snd_rule = self._send_rules.get(topic, None)
            if snd_rule is not None:
                snd_rule.state.send_pending = False

        self.trigger()

    def _handle_send(self):
        if not self._initialized:
            return

        more_values_to_send = True
        while more_values_to_send:
            more_values_to_send = False
            with self._lock_send:

                for snd_rule in self._send_rules.values():
                    # check remote state and send only in state "up"
                    if not self._transceiver.connected:
                        break

                    self._handle_send_single(snd_rule)
                    if snd_rule.state.forced_send or not snd_rule.queue.empty:
                        more_values_to_send = True

    def _handle_send_single(self, send_rule):

        topic = send_rule.topic
        queue = send_rule.queue
        state = send_rule.state

        # do not send if an old send is pending, or we do not have a queue to read from
        if state.send_pending or queue is None:
            return

        # if a value is in the queue, send it
        if not queue.empty:
            value = queue.peek()

            result = self._transceiver.send_value(topic, value)

            if result == "INJECTED" or result == "RECEIVED" or result == "REJECTED":
                queue.pop()
                state.forced_send = False
                if result == "RECEIVED":
                    state.send_pending = True

        # if no value is in the queue, get latest value from value store
        elif state.forced_send:
            value = self.get_value(topic)  # Note: can be None

            # if a value has been written to the topic while we read a value from the ValueStore
            # we stop the forcedSend. The value from the queue will be sent in the next iteration.
            # This check is to make sure that the value we just read is *not* a new value just written,
            # otherwise we would send this new value twice: now and in the next iteration
            if not queue.empty:
                state.forced_send = False
                return

            if value is None:  # Nothing has been written to the value store for this topic => Nothing to transmit
                state.forced_send = False
                return

            result = self._transceiver.send_value(topic, value)

            if result == "RECEIVED":
                state.forced_send = False
                state.send_pending = True
            elif result == "INJECTED" or result == "REJECTED":
                state.forced_send = False

    def _handle_injected_rejected(self):
        with self._lock_topics:

            # report successfully inserted values back to the sender
            for topic in self._inserted_topics:
                self._transceiver.send_blocked_value_injected(topic)

            # report rejected values back to the sender
            for topic in self._rejected_topics:
                self._transceiver.send_blocked_value_rejected(topic)

            self._inserted_topics.clear()
            self._rejected_topics.clear()

    @property
    def _received_value_pending(self):
        """
        Check if a value is pending in any of the reception rules.
        The lock of _cond_receive must be locked before calling this method
        """
        return any([r.state.pending_value is not None for r in self._receive_rules.values()])

    def _trigger_cyclic_thread_main(self):
        """
        Function running in a separate thread to cyclically call the trigger function
        so that pings will be sent even if no values trigger its execution
        """
        try:
            while (not self.startup_request) and (not self.shutdown_request):
                time.sleep(0.01)

            while not self.shutdown_request:
                self.trigger()
                self._transceiver.wait_for_event()

        except Exception as e:
            self._logger.error(f"Cyclic trigger thread failed: {e}")
            self.abort()

    def _receive_thread_main(self):
        """
        Function running in a separate thread, constantly querying the _transceiver to receive values
        """
        try:
            self._transceiver.connect_receiver()

            while (not self.startup_request) and (not self.shutdown_request):
                time.sleep(0.01)

            while not self.shutdown_request:
                self._transceiver.receive()

        except Exception as e:
            self._logger.error(f"Receiver thread failed: {e}")
            self.abort()

        finally:
            self._transceiver.disconnect_receiver()

    def _handle_pending_values_thread_main(self):
        """
        Function running in a separate thread handling injection of values into temporarily blocked topics
        """
        try:
            while (not self.startup_request) and (not self.shutdown_request):
                time.sleep(0.01)

            while not self.shutdown_request:

                with self._cond_receive:

                    while (not self._received_value_pending) and (not self.shutdown_request):
                        self._cond_receive.wait()

                    # get pending values, insert them into the value store
                    inserted_topics = []
                    rejected_topics = []
                    for rec_rule in self._receive_rules.values():

                        value = rec_rule.state.pending_value

                        if value is not None:
                            # should never happen for now, as we do not have blocking value queues
                            topic = rec_rule.topic
                            self.set_value(topic, value)  # TODO: handle blocking behaviour once implem.
                            rec_rule.pending_value = None
                            self._inserted_topics.append(topic)
                            raise RuntimeError("Unexpected pending value")

                with self._lock_topics:
                    self._inserted_topics += inserted_topics
                    self._rejected_topics += rejected_topics

                # trigger main thread to handle received and rejected topics
                # Note: this has to be done by the main thread, because sockets (e.g. ZMQ) cannot
                #       easily be shared between threads.
                self.trigger()

                # sleep to avoid high-frequency polling when receiver ports are continuously blocked
                time.sleep(0.001)

        except Exception as e:
            self._logger.error(f"Pending values thread failed: {e}")
            self.abort()
