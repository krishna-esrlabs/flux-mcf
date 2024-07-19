"""
Copyright (c) 2024 Accenture
"""

import inspect
import os
import sys

# path of python mcf module, relative to location of this script
_MCF_PY_RELATIVE_PATH = "../../"

# directory of this script and relative path of mcf python tools
_SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))

sys.path.append(f"{_SCRIPT_DIRECTORY}/{_MCF_PY_RELATIVE_PATH}")

from mcf_core.events import Event
from mcf_core.value_store import ValueStore
from mcf_core.value_store import ValueQueue


def test_value_store_set_get():

    value_store = ValueStore()

    # set value on topic 1 and read it back
    value_store.set_value("topic1", "string1")
    value1 = value_store.get_value("topic1")
    assert value1 == "string1", "Wrong value stored in value store"

    # set value on topic 2 and read it back
    value_store.set_value("topic2", "string2")
    value2 = value_store.get_value("topic2")
    assert value2 == "string2", "Wrong value stored in value store"

    # read values again
    value1 = value_store.get_value("topic1")
    value2 = value_store.get_value("topic2")
    assert value1 == "string1", "Wrong value stored in value store"
    assert value2 == "string2", "Wrong value stored in value store"

    # set new value on topic 1 and read it back
    value_store.set_value("topic1", "string3")
    value1 = value_store.get_value("topic1")
    assert value1 == "string3", "Wrong value stored in value store"

    # set new value on topic 2 and read it back
    value_store.set_value("topic2", "string4")
    value2 = value_store.get_value("topic2")
    assert value2 == "string4", "Wrong value stored in value store"

    # read new values again
    value1 = value_store.get_value("topic1")
    value2 = value_store.get_value("topic2")
    assert value1 == "string3", "Wrong value stored in value store"
    assert value2 == "string4", "Wrong value stored in value store"


def test_value_queue_receive_peek_pop():

    value_queue = ValueQueue()

    # initially, the queue is empty, maxlen should be 0 by default
    assert value_queue.empty, "Value queue not initially empty"
    assert value_queue.maxlen == 0, "Value queue has wrong maxlen"

    # receive values, check size, peek values
    num_values = 0
    topics_values = [("topic1", "s1"), ("topic2", "s2"), ("topic1", "s3"), ("topic2", "s4")]
    for topic, value in topics_values:
        value_queue.receive(topic, value)
        num_values += 1
        assert value_queue.size == num_values, "Wrong size of value queue"

        # peek() and peek_with_topic() should always return the first entry
        peek_value = value_queue.peek()
        assert peek_value == topics_values[0][1], "Peek returned wrong value"
        peek_value, peek_topic = value_queue.peek_with_topic()
        assert peek_value == topics_values[0][1], "Peek with topic returned wrong value"
        assert peek_topic == topics_values[0][0], "Peek with topic returned wrong topic"

    # pop values from the queue, check correct values
    for _, value in topics_values:
        pop_value = value_queue.pop()
        assert pop_value == value, "Wrong value popped from value queue"

        num_values -= 1
        assert value_queue.size == num_values, "Wrong size of value queue"

    # check that queue is empty
    assert value_queue.size == 0, "Queue not empty after popping all values"
    assert value_queue.empty, "Queue not empty after popping all values"

    # receive new values, check size, peek values
    topics_values = [("topic3", "s5"), ("topic2", "s6"), ("topic1", "s7"), ("topic2", "s8")]
    for topic, value in topics_values:
        value_queue.receive(topic, value)
        num_values += 1
        assert value_queue.size == num_values, "Wrong size of value queue"

        # peek() and peek_with_topic() should always return the first entry
        peek_value = value_queue.peek()
        assert peek_value == topics_values[0][1], "Peek returned wrong value"
        peek_value, peek_topic = value_queue.peek_with_topic()
        assert peek_value == topics_values[0][1], "Peek with topic returned wrong value"
        assert peek_topic == topics_values[0][0], "Peek with topic returned wrong topic"

    # pop values with topics from the queue, check correct values and topics
    for topic, value in topics_values:
        pop_value, pop_topic = value_queue.pop_with_topic()
        assert pop_value == value, "Wrong value popped from value queue"
        assert pop_topic == topic, "Wrong topic popped from value queue"

        num_values -= 1
        assert value_queue.size == num_values, "Wrong size of value queue"

    # check that queue is empty
    assert value_queue.size == 0, "Queue not empty after popping all values"
    assert value_queue.empty, "Queue not empty after popping all values"


def test_value_queue_limited_maxlen():

    # test different values of maxlen
    for maxlen in range(1, 4):

        value_queue = ValueQueue(maxlen=maxlen)

        # initially, the queue is empty, maxlen should be 0 by default
        assert value_queue.empty, "Value queue not initially empty"
        assert value_queue.maxlen == maxlen, "Value queue has wrong maxlen"

        # receive values, check size, peek values
        num_received = 0
        num_in_queue = 0
        topics_values = [("topic1", "s1"), ("topic2", "s2"), ("topic1", "s3"), ("topic2", "s4"), ("topic2", "s5")]
        for topic, value in topics_values:
            value_queue.receive(topic, value)
            num_received += 1
            num_in_queue = min(num_received, maxlen)
            assert value_queue.size == num_in_queue, "Wrong size of value queue"

            # peek() and peek_with_topic() should always return the first entry in the queue,
            # which is the entry with index num_received - num_in_queue
            peek_value = value_queue.peek()
            assert peek_value == topics_values[num_received - num_in_queue][1], "Peek returned wrong value"
            peek_value, peek_topic = value_queue.peek_with_topic()
            assert peek_value == topics_values[num_received - num_in_queue][1], "Peek with topic returned wrong value"
            assert peek_topic == topics_values[num_received - num_in_queue][0], "Peek with topic returned wrong topic"

        # pop values from the queue, check expected values
        for _, value in topics_values[-maxlen:]:
            pop_value = value_queue.pop()
            num_in_queue -= 1

            assert pop_value == value, "Wrong value popped from value queue"
            assert value_queue.size == num_in_queue, "Wrong size of value queue"

        # check that queue is empty
        assert value_queue.size == 0, "Queue not empty after popping all values"
        assert value_queue.empty, "Queue not empty after popping all values"

        # receive new values, check size, peek values
        num_received = num_in_queue
        topics_values = [("topic3", "s5"), ("topic2", "s6"), ("topic1", "s7"), ("topic2", "s8"), ("topic1", "s9")]
        for topic, value in topics_values:
            value_queue.receive(topic, value)
            num_received += 1
            num_in_queue = min(num_received, maxlen)
            assert value_queue.size == num_in_queue, "Wrong size of value queue"

            # peek() and peek_with_topic() should always return the first entry in the queue,
            # which is the entry with index num_received - num_in_queue
            peek_value = value_queue.peek()
            assert peek_value == topics_values[num_received - num_in_queue][1], "Peek returned wrong value"
            peek_value, peek_topic = value_queue.peek_with_topic()
            assert peek_value == topics_values[num_received - num_in_queue][1], "Peek with topic returned wrong value"
            assert peek_topic == topics_values[num_received - num_in_queue][0], "Peek with topic returned wrong topic"

        # pop values with topics from the queue, check correct values and topics
        for topic, value in topics_values[-maxlen:]:
            pop_value, pop_topic = value_queue.pop_with_topic()
            num_in_queue -= 1

            assert pop_value == value, "Wrong value popped from value queue"
            assert pop_topic == topic, "Wrong topic popped from value queue"
            assert value_queue.size == num_in_queue, "Wrong size of value queue"

        # check that queue is empty
        assert value_queue.size == 0, "Queue not empty after popping all values"
        assert value_queue.empty, "Queue not empty after popping all values"


def test_value_queue_event_source():
    queue = ValueQueue()
    event1 = Event()
    event2 = Event()
    event3 = Event()

    # events must be inactive initially
    assert not event1.active, "Event not inactive initially"
    assert not event2.active, "Event not inactive initially"
    assert not event3.active, "Event not inactive initially"

    # register events at value queue
    queue.add_event(event1)
    queue.add_event(event2)
    queue.add_event(event3)

    # events must still be inactive
    assert not event1.active, "Event not inactive after registration at value queue"
    assert not event2.active, "Event not inactive after registration at value queue"
    assert not event3.active, "Event not inactive after registration at value queue"

    queue.receive("topic", "value")

    # events must be active
    assert event1.active, "Event not active after value queue received a value"
    assert event2.active, "Event not active after value queue received a value"
    assert event3.active, "Event not active after value queue received a value"


def test_value_store_with_value_queues():
    value_store = ValueStore()
    queue1 = ValueQueue()
    queue2 = ValueQueue()
    queue3 = ValueQueue()

    value_store.add_receiver("topic1", queue1)
    value_store.add_receiver("topic2", queue2)
    value_store.add_all_topic_receiver(queue3)

    topics_values = [("topic1", "s1"), ("topic2", "s2"), ("topic1", "s3"),
                     ("topic2", "s4"), ("topic2", "s5"), ("topic3", "s6")]

    # send values to given topics in the value store
    for topic, value in topics_values:
        value_store.set_value(topic, value)

    # check expected queue sizes
    assert queue1.size == 2, "Wrong number of values in queue for topic1"
    assert queue2.size == 3, "Wrong number of values in queue for topic2"
    assert queue3.size == 6, "Wrong number of values in queue for all topics"

    # check expected content of all-topic receiver (queue3)
    for topic, value in topics_values:
        pop_value, pop_topic = queue3.pop_with_topic()
        assert pop_value == value, "Wrong value popped from value queue"
        assert pop_topic == topic, "Wrong topic popped from value queue"

    # check expected content of topic1 receiver:
    for topic, value in topics_values:
        if topic != "topic1":
            continue
        pop_value, pop_topic = queue1.pop_with_topic()
        assert pop_value == value, "Wrong value popped from value queue"
        assert pop_topic == topic, "Wrong topic popped from value queue"

    # check expected content of topic1 receiver:
    for topic, value in topics_values:
        if topic != "topic2":
            continue
        pop_value, pop_topic = queue2.pop_with_topic()
        assert pop_value == value, "Wrong value popped from value queue"
        assert pop_topic == topic, "Wrong topic popped from value queue"
