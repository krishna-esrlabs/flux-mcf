""""
Copyright (c) 2024 Accenture
"""
import copy
import inspect
import os
from pathlib import Path
import subprocess
import sys
import time

# path of mcf python tools, relative to location of this script
_MCF_TOOLS_RELATIVE_PATH = "../../../mcf_py"

# path to the mcf_remote test value types
_MCF_REMOTE_VALUE_TYPES = "../mcf_remote_value_types/python/value_types"

# directory of this script and relative path of mcf python tools
_SCRIPT_DIRECTORY = Path(os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe()))))

# path to executable to run on target
_CMAKE_EXECUTABLE_PATH = (_SCRIPT_DIRECTORY / "../../../build/mcf_remote/test/rcTestEngine").resolve()
_BAKE_EXECUTABLE_PATH = (_SCRIPT_DIRECTORY / "../../build/RcTestEngine_mcf_remote_UnitTestBase/rcTestEngine").resolve()

# ip address and port of remote target
_TARGET_IP = "127.0.0.1"  # localhost
_TARGET_PORT = "6666"

_NUM_EPS = 1.e-5

sys.path.append(str(_SCRIPT_DIRECTORY / _MCF_TOOLS_RELATIVE_PATH))
from mcf import RemoteControl
sys.path.append(str(_SCRIPT_DIRECTORY / _MCF_REMOTE_VALUE_TYPES))
from mcf_remote_test_value_types.mcf_remote_test.TestInt import TestInt


# helper class to run sub process
class ProcessRunner:

    def __init__(self, executable):
        """
        :param executable: list of executable and parameters
        """
        self._executable = executable
        self._process = None
        pass

    def __enter__(self):
        if self._process is None:
            self._process = subprocess.Popen(self._executable)

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self._process is not None:
            self._process.terminate()
            self._process.communicate()


# absolute path of executable to run on target
if _CMAKE_EXECUTABLE_PATH.is_file():
    _EXECUTABLE_ABS_PATH = _CMAKE_EXECUTABLE_PATH
elif _BAKE_EXECUTABLE_PATH.is_file():
    _EXECUTABLE_ABS_PATH = _BAKE_EXECUTABLE_PATH
else:
    raise FileNotFoundError("Could not find executable.")


def queue_mcf_pointxyz(rc, topic, value, timestamp):
    rc.write_value(topic, "mcf_remote_test_value_types::mcf_remote_test::TestPointXYZ", [value, value, value], None, timestamp)


def read_mcf_pointxyz(rc, topic):
    return rc.read_value(topic)[0]


def test_mcf_rc_event_queue_single_topic():

    # value store topic
    test_topic = "test/test1"

    # values to be queued in the given sequence: timestamp (in microsecs), topic, float value
    # values in last column must be unique or the test will fail
    queue_values = [
        [100, test_topic, 0.],
        [500000, test_topic, 1.],
        [1000, test_topic, 2.],
        [2300000, test_topic, 3.],
        [10, test_topic, 4.],
        [2000000, test_topic, 5.],
        [1500000, test_topic, 6.],
    ]

    # helper to find a float value entry in a list of queued values
    def find_value(x, values):
        indices = [i for i, entry in enumerate(values)
                   if abs(entry[2] - x) < _NUM_EPS]  # indices of entries having that value
        return indices[0]  # return first index (should be the only one)

        # create a copy of values, sorted according to time stamp
    sorted_values = copy.deepcopy(queue_values)
    sorted_values = sorted(sorted_values, key=lambda v: v[0])

    # start mcf executable
    with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

        # create and open target connection
        tgt_rc = RemoteControl()
        tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

        # disable event queue
        tgt_rc.disable_event_queue()

        # queue all values (in unsorted order)
        for tstamp, topic, value in queue_values:
            queue_mcf_pointxyz(tgt_rc, topic, value, timestamp=tstamp)

        # queue must be empty, because it was disabled while queueing
        queue_state = tgt_rc.get_event_queue_state()
        print(queue_state)
        assert queue_state['size'] == 0

        # enable event queue, then check that it is still empty
        tgt_rc.enable_event_queue()
        queue_state = tgt_rc.get_event_queue_state()
        print(queue_state)
        assert queue_state['size'] == 0

        # queue all values again (in unsorted order)
        for tstamp, topic, value in queue_values:
            queue_mcf_pointxyz(tgt_rc, topic, value, timestamp=tstamp)

        # loop until all events have been played back on the remote target
        queue_state = tgt_rc.get_event_queue_state()
        print(queue_state)
        value = read_mcf_pointxyz(tgt_rc, test_topic)[0]  # take first entry of received TestPointXYZ
        list_index = find_value(value, sorted_values)
        while queue_state['size'] > 0:
            time.sleep(0.2)
            new_state = tgt_rc.get_event_queue_state()
            print("queue state:", new_state)

            # ensure that queue only gets shorter
            assert new_state['size'] <= queue_state['size']

            # ensure that new value on value store is later than previous value in sorted queue
            value = read_mcf_pointxyz(tgt_rc, test_topic)[0]  # take first entry of TestPointXYZ
            new_index = find_value(value, sorted_values)
            assert new_index >= list_index

            queue_state = new_state
            list_index = new_index


def test_mcf_rc_event_queue_multiple_topics():

    # values to be queued in the given sequence: timestamp (in microsecs), topic, float value
    # topics must be unique or the test will fail
    queue_values = [
        [100, "test1", 0.],
        [500000, "test2", 1.],
        [1000, "test3", 2.],
        [1000000, "test4", 3.],
        [10, "test5", 4.],
        [1500000, "test7", 5.],
    ]

    # start mcf executable
    with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

        # create and open target connection
        tgt_rc = RemoteControl()
        tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

        # enable event queue
        tgt_rc.enable_event_queue()

        # queue all values (in unsorted order)
        for tstamp, topic, value in queue_values:
            queue_mcf_pointxyz(tgt_rc, topic, value, timestamp=tstamp)

        # loop until all events have been played back on the remote target
        queue_state = tgt_rc.get_event_queue_state()
        print(queue_state)
        while queue_state['size'] > 0:
            time.sleep(0.2)
            new_state = tgt_rc.get_event_queue_state()
            print("queue state:", new_state)

            # ensure that queue only gets shorter
            assert new_state['size'] <= queue_state['size']
            queue_state = new_state

        # check values on all topics
        for _, topic, queued_value in queue_values:
            value = read_mcf_pointxyz(tgt_rc, topic)[0]
            assert abs(value - queued_value) < _NUM_EPS


def port_indices(info, component_name, topic):
    comp_idx = [i for i, c in enumerate(info) if c['name'] == component_name][0]
    port_idx = [i for i, p in enumerate(info[comp_idx]['ports']) if p['topic'] == topic][0]
    return comp_idx, port_idx


def test_mcf_rc_info():

    # start mcf executable
    with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

        # create and open target connection
        tgt_rc = RemoteControl()
        tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

        info = tgt_rc.get_info()

        assert(any([c for c in info if c['name'] == 'TestComponent1']))
        assert(any([c for c in info if c['name'] == 'TestComponent2']))


def assert_value(rc, topic, expected):
    value, _, _ = rc.read_value(topic)
    assert(value == expected)


def test_mcf_rc_disconnect():

    # test disconnecting both a sender and a receiver port

    for disconnect_port_topic in [
            '/points_connected', # sender port
            '/points_tc1_in' # receiver port
            ]:

        # start mcf executable
        with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

            # create and open target connection
            tgt_rc = RemoteControl()
            tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

            info = tgt_rc.get_info()

            # test point forwarding works
            tgt_rc.write_value('/points_tc1_in', 'mcf_remote_test_value_types::mcf_remote_test::TestPointXY', [ 10, 10 ])
            time.sleep(0.1)
            assert_value(tgt_rc, '/points_connected', [ 10, 10 ])

            # disconnect
            tgt_rc.disconnect_port(*port_indices(info, 'TestComponent1', disconnect_port_topic))

            # write a new value
            tgt_rc.write_value('/points_tc1_in', 'mcf_remote_test_value_types::mcf_remote_test::TestPointXY', [ 20, 20 ])
            time.sleep(0.1)

            # still the old value
            assert_value(tgt_rc, '/points_connected', [ 10, 10 ])

            # connect
            tgt_rc.connect_port(*port_indices(info, 'TestComponent1', disconnect_port_topic))

            # still the old value, new value was sent while port was disabled
            assert_value(tgt_rc, '/points_connected', [ 10, 10 ])

            # send new value again
            tgt_rc.write_value('/points_tc1_in', 'mcf_remote_test_value_types::mcf_remote_test::TestPointXY', [ 20, 20 ])
            time.sleep(0.1)
            # now it shows up
            assert_value(tgt_rc, '/points_connected', [ 20, 20 ])


def test_mcf_rc_port_blocking_flag():

    # start mcf executable
    with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

        # create and open target connection
        tgt_rc = RemoteControl()
        tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

        info = tgt_rc.get_info()

        assert(None == tgt_rc.get_port_blocking(*port_indices(info, 'TestComponent1', '/points_tc1_in')))
        assert(None == tgt_rc.get_port_blocking(*port_indices(info, 'TestComponent1', '/points_connected')))
        assert(True == tgt_rc.get_port_blocking(*port_indices(info, 'TestComponent2', '/points_connected')))
        assert(None == tgt_rc.get_port_blocking(*port_indices(info, 'TestComponent2', '/points_tc2_out')))

        tgt_rc.set_port_blocking(*port_indices(info, 'TestComponent2', '/points_connected'), False)
        assert(False == tgt_rc.get_port_blocking(*port_indices(info, 'TestComponent2', '/points_connected')))

        tgt_rc.set_port_blocking(*port_indices(info, 'TestComponent2', '/points_connected'), True)
        assert(True == tgt_rc.get_port_blocking(*port_indices(info, 'TestComponent2', '/points_connected')))


def test_mcf_rc_port_max_queue_length():

    # start mcf executable
    with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

        # create and open target connection
        tgt_rc = RemoteControl()
        tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

        info = tgt_rc.get_info()

        assert(None == tgt_rc.get_port_max_queue_length(*port_indices(info, 'TestComponent1', '/points_tc1_in')))
        assert(None == tgt_rc.get_port_max_queue_length(*port_indices(info, 'TestComponent1', '/points_connected')))
        assert(3 == tgt_rc.get_port_max_queue_length(*port_indices(info, 'TestComponent2', '/points_connected')))
        assert(None == tgt_rc.get_port_max_queue_length(*port_indices(info, 'TestComponent2', '/points_tc2_out')))

        tgt_rc.set_port_max_queue_length(*port_indices(info, 'TestComponent2', '/points_connected'), 2)
        assert(2 == tgt_rc.get_port_max_queue_length(*port_indices(info, 'TestComponent2', '/points_connected')))

        tgt_rc.set_port_max_queue_length(*port_indices(info, 'TestComponent2', '/points_connected'), 0)
        assert(0 == tgt_rc.get_port_max_queue_length(*port_indices(info, 'TestComponent2', '/points_connected')))


def test_mcf_rc_port_blocking():

    # test different methods of how to resolve a blocking case
    for block_resolution in ['consume_value', 'disable_blocking', 'larger_max_length', 'disconnect_port', 'smaller_max_length', 'enable_blocking']:

        with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

            # create and open target connection
            tgt_rc = RemoteControl()
            tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

            info = tgt_rc.get_info()

            # disable value consuming in receiver
            tgt_rc.write_value('/tc2_control', 'TCControlValue', [ 'stopForwarding' ])
            time.sleep(0.1)

            # we can send 3 values until the receiver queue is full
            for i in range(3):
                tgt_rc.write_value('/points_tc1_in', 'mcf_remote_test_value_types::mcf_remote_test::TestPointXY', [ i, 0 ])
                time.sleep(0.1)
                assert_value(tgt_rc, '/points_connected', [ i, 0 ])

            # on the 4th value the sender is blocked and won't forward the value to /points_connected
            tgt_rc.write_value('/points_tc1_in', 'mcf_remote_test_value_types::mcf_remote_test::TestPointXY', [ 3, 0 ])
            assert_value(tgt_rc, '/points_connected', [ 2, 0 ])

            # the sender will send the value as soon as the queue gets unblocked

            if block_resolution == 'consume_value':
                tgt_rc.write_value('/tc2_control', 'TCControlValue', [ 'consumePoint' ])
                time.sleep(0.1)
                assert_value(tgt_rc, '/points_connected', [ 3, 0 ])

            elif block_resolution == 'disable_blocking':
                tgt_rc.set_port_blocking(*port_indices(info, 'TestComponent2', '/points_connected'), False)
                time.sleep(0.1)
                assert_value(tgt_rc, '/points_connected', [ 3, 0 ])

            elif block_resolution == 'larger_max_length':
                tgt_rc.set_port_max_queue_length(*port_indices(info, 'TestComponent2', '/points_connected'), 4)
                time.sleep(0.1)
                assert_value(tgt_rc, '/points_connected', [ 3, 0 ])

            elif block_resolution == 'smaller_max_length':
                tgt_rc.set_port_max_queue_length(*port_indices(info, 'TestComponent2', '/points_connected'), 2)
                time.sleep(0.1)

                # NEGATIVE TEST
                # smaller max length should not unblock
                assert_value(tgt_rc, '/points_connected', [ 2, 0 ])

            elif block_resolution == 'disconnect_port':
                tgt_rc.disconnect_port(*port_indices(info, 'TestComponent2', '/points_connected'))
                time.sleep(0.1)

                # NEGATIVE TEST
                # disconnecting the port does not unblock
                assert_value(tgt_rc, '/points_connected', [ 2, 0 ])

            elif block_resolution == 'enable_blocking':
                tgt_rc.set_port_blocking(*port_indices(info, 'TestComponent2', '/points_connected'), True)
                time.sleep(0.1)

                # NEGATIVE TEST
                # enabling blockin while it is already block should not unblock
                assert_value(tgt_rc, '/points_connected', [ 2, 0 ])

            else:
                raise RuntimeError('unknown block_resolution')


def test_mcf_rc_read_queued():

    with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

        # create and open target connection
        tgt_rc = RemoteControl()
        tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

        info = tgt_rc.get_info()

        tgt_rc.write_value('/tc1_control', 'TCControlValue', [ 'sendBurst' ])
        time.sleep(0.1)
        # without a queue we read the last value of the burst
        assert_value(tgt_rc, '/points_connected', [ 3, 3 ])

        # queue of size 2 is too short for burst of size 3
        tgt_rc.set_queue('/points_connected', 2)

        tgt_rc.write_value('/tc1_control', 'TCControlValue', [ 'sendBurst' ])
        time.sleep(0.1)
        # the first element of the burst gets lost
        assert_value(tgt_rc, '/points_connected', [ 2, 2 ])
        assert_value(tgt_rc, '/points_connected', [ 3, 3 ])

        # remove queue
        tgt_rc.set_queue('/points_connected', 0)

        tgt_rc.write_value('/tc1_control', 'TCControlValue', [ 'sendBurst' ])
        time.sleep(0.1)
        # back to no queue
        assert_value(tgt_rc, '/points_connected', [ 3, 3 ])

        # queue of size 2 but with blocking feature enabled
        tgt_rc.set_queue('/points_connected', 2, blocking=True)

        tgt_rc.write_value('/tc1_control', 'TCControlValue', [ 'sendBurst' ])
        time.sleep(0.1)

        # due to the blocking, the queue of size 2 doesn't overflow
        assert_value(tgt_rc, '/points_connected', [ 1, 1 ])
        assert_value(tgt_rc, '/points_connected', [ 2, 2 ])
        assert_value(tgt_rc, '/points_connected', [ 3, 3 ])

def test_mcf_value_accessor():
    with ProcessRunner([str(_EXECUTABLE_ABS_PATH)]):

        # create and open target connection
        tgt_rc = RemoteControl()
        tgt_rc.connect(_TARGET_IP, _TARGET_PORT)

        value = TestInt(7)
        value.inject_id(1234)
        sender = tgt_rc.create_value_accessor(TestInt, "/test/int", 2)
        receiver = tgt_rc.create_value_accessor(TestInt, "/test/int", 1)

        sender.set(value)
        rec_value = receiver.get()
        assert(value.mode == rec_value.mode)
        assert(value.id == rec_value.id)

