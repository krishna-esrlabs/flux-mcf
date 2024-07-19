""""
Copyright (c) 2024 Accenture
"""
from mcf.value import Value

from enum import Enum, IntEnum
from typing import Type, Optional, Tuple, List, Iterator, TYPE_CHECKING
import zmq
import msgpack
from io import BytesIO

from mcf.zmq_communicator import ZmqCommunicator


from mcf.value_accessor import RCValueAccessor, ValueAccessorDirection
from mcf.value_accessor import ReceiverAccessor, SenderAccessor

if TYPE_CHECKING:
    from mcf.value_accessor import ValueT


class RcError(Exception):
    pass


class PlaybackModifier(IntEnum):
    """
    Enum of playback modifiers which are used to control the replay playback via the
    ReplayEventController. This enum will be cast to ReplayEventController::PlaybackModifier and
    should therefore match ReplayEventController::PlaybackModifier.
    """
    PAUSE = 0
    RESUME = 1
    STEPONCE = 2
    FINISH = 3


class ReplayParams(object):
    class RunMode(IntEnum):
        CONTINUOUS = 0
        SINGLESTEP = 1
        STEPTIME = 2

    def __init__(self, run_mode, run_without_drops, speed_factor, pipeline_end_trigger_names,
                 wait_input_event_name, wait_input_event_topic,
                 step_time_microseconds):
        self.run_mode = ReplayParams.RunMode(run_mode)
        self.run_without_drops = run_without_drops
        self.speed_factor = speed_factor
        self.pipeline_end_trigger_names = pipeline_end_trigger_names
        self.wait_input_event_name = wait_input_event_name
        self.wait_input_event_topic = wait_input_event_topic
        self.step_time_microseconds = step_time_microseconds

    def __eq__(self, other):
        if not isinstance(other, ReplayParams):
            return NotImplemented

        return self.run_mode == other.run_mode and \
               self.run_without_drops == other.run_without_drops and \
               self.speed_factor == other.speed_factor and \
               self.pipeline_end_trigger_names == other.pipeline_end_trigger_names and \
               self.wait_input_event_name == other.wait_input_event_name and \
               self.wait_input_event_topic == other.wait_input_event_topic and \
               self.step_time_microseconds == other.step_time_microseconds

    def __str__(self):
        return "run_mode : {}\n" \
               "run without drops : {}\n" \
               "speed factor : {}\n" \
               "trigger names : {}\n" \
               "wait name : {}\n" \
               "wait topic : {}\n" \
               "step time : {}".format(self.run_mode, self.run_without_drops, self.speed_factor,
                                       self.pipeline_end_trigger_names, self.wait_input_event_name,
                                       self.wait_input_event_topic, self.step_time_microseconds)


class RemoteControl:
    def __init__(self, timeout: int = 20000, own_thread: bool = False) -> None:
        """
        Creates a new instance of RemoteControl to communicate to a flux process over tcp.
        :param timeout: Timeout in ms when trying to receive a message from the flux process
        :own_thread: If true, RemoteControl will internally create a new thread which will handle
                     all zmq socket operations to ensure thread safety
                     If false, all operations of RemoteControl will be executed in the thread
                     calling the respective function. In that case, the user is responsible to
                     assure that all functions operating on the zmq socket are called from the same
                     thread.
        """

        self.communicator = ZmqCommunicator(timeout, own_thread)

    def connect(self, ip: str, port: int) -> bool:
        self.communicator.connect(ip, port)

        # try a test command to check the connection
        if self.get_info():
            print('Successfully connected to tcp://{}:{}'.format(ip, port))
            return True
        else:
            print('ERROR: can not connect')
            return False

    def set_timeout(self, timeout: int) -> None:
        """
        :param timeout: Set timeout for reception
        """
        self.communicator.set_timeout(timeout)

    def is_connected(self) -> bool:
        return self.communicator.is_connected()

    def disconnect(self) -> None:
        self.communicator.disconnect()

    def get_info(self) -> bool or dict:
        cmd = msgpack.packb({'command': 'get_info'})
        response = self._send(cmd)
        if response is None:
            return False
        elif response['type'] == 'response':
            return response['content']
        else:
            print('ERROR: ' + response['content'])
            return False

    def set_queue(self, topic: str, size: int, blocking: bool=False) -> bool:
        """
        Attach a queue of length +size+ to +topic+ on the target.
        This can be used to prevent missing value updates.

        Setting a queue size of 0 removes the queue.

        If +blocking+ is set to true, senders on the specified topic
        will block if the queue is full. This can be used to prevent
        queue overflow or to halt specific components.
        """
        cmd = msgpack.packb({'command': 'set_queue', 'topic': topic, 'size': size, 'blocking': blocking})
        response = self._send(cmd)
        return RemoteControl.check_response(response)

    def write_value(
        self,
        topic: str,
        clazz: str,
        value: list,
        extmem: Optional[bytes]=None,
        timestamp: Optional[int]=None,
        valueId: int=0,
        component: str="",
        port: str="") -> bool:
        """
        Write an event to the Remote Control Event Source. Can optionally pass a component
        and port name which will be used in the trace event of the passed value.
        """
        def encode_serializable(o):
            if isinstance(o, Value):
                return o.serialize()[0]
            elif isinstance(o, Enum):
                return o.value
            return o

        messages = []
        cmd = {'command': 'write_value', 'topic': topic, 'component': component, 'port': port}
        if timestamp is not None:
            cmd['timestamp'] = timestamp

        cmd = msgpack.packb(cmd)
        messages.append(cmd)
        if valueId != 0:
            cmd = msgpack.packb(valueId) + msgpack.packb(clazz) + msgpack.packb(value, default=encode_serializable)
        else:
            cmd = msgpack.packb(clazz) + msgpack.packb(value, default=encode_serializable)
        messages.append(cmd)
        if extmem is not None:
            messages.append(extmem)
        response = self._send(messages)
        return RemoteControl.check_response(response)

    def get_event_queue_state(self) -> bool or dict:
        cmd = msgpack.packb({'command': 'event_queue_info'})
        response = self._send(cmd)
        if response is None:
            return False
        elif response['type'] == 'response':
            return response['content']
        else:
            print('ERROR: ' + response['content'])
            return False

    def enable_event_queue(self, enabled: bool=True) -> bool:
        cmd = msgpack.packb({'command': 'enable_event_queue', 'enabled': enabled})
        response = self._send(cmd)
        return RemoteControl.check_response(response)

    def disable_event_queue(self) -> None:
        self.enable_event_queue(enabled=False)

    def _unpack_msgpack(self, data: bytes) -> dict:
        if data is None: return data
        unpacked = msgpack.unpackb(data, raw=False)
        return unpacked

    def _send(self, msg: bytes) -> dict:
        packed = self.communicator.send(msg)
        return self._unpack_msgpack(packed)

    def _decode_value(self, packed_value: bytes, extmem: bytes, withId: bool, packedResponse: bytes
    ) -> Tuple[Optional[Tuple], bool]:
        response = self._unpack_msgpack(packedResponse)
        if response is not None and response['type'] == 'response':
            if packed_value is not None:
                io = BytesIO(packed_value)
                try:
                    unpacker = msgpack.Unpacker(io, raw=False)
                    valueId = unpacker.unpack()
                    typename = unpacker.unpack()
                    value = unpacker.unpack()

                    if withId:
                        retVal = (valueId, value, typename, extmem)
                    else:
                        retVal = (value, typename, extmem)
                    if 'has_more' in response['content']:
                        return retVal, response['content']['has_more']
                    else:
                        return retVal, False
                except:
                    raise RcError('read failed: Value did not have the expected format.')
            else:
                # the has_more attribute indicates that a queue is present
                if 'has_more' in response['content']:
                    # queue is not necessarily empty
                    # if the value could not be serialized we return None
                    # but there may be more values in the queue
                    return None, response['content']['has_more']
                else:
                    # no such value in value store
                    return None, False
        else:
            raise RcError('read failed: ' + response['content'])

    def read_value(self, topic: str, withId: bool=False) -> 'ValueT':
        cmd = msgpack.packb({'command': 'read_value', 'topic': topic})
        header, data, response = self.communicator.read_value(cmd)
        value, _ = self._decode_value(header, data, withId, response)
        return value

    def read_all_values(self, topic: str, max_num: int=10, withId: bool=False) -> List['ValueT']:
        values = []
        for i in range(max_num):
            header, data, response = self.communicator.read_value(topic, withId)
            value, has_more = self._decode_value(header, data, withId, response)
            values.append(value)
            if not has_more:
                break
        return values

    def listen(self, topic: str) -> Iterator['ValueT']:
        while True:
            yield self.read_value(topic)

    def disconnect_port(self, component: str, port: str) -> bool:
        cmd = msgpack.packb({'command': 'disconnect_port', 'component': component, 'port': port})
        response = self._send(cmd)
        return RemoteControl.check_response(response)

    def connect_port(self, component: str, port: str) -> bool:
        cmd = msgpack.packb({'command': 'connect_port', 'component': component, 'port': port})
        response = self._send(cmd)
        return RemoteControl.check_response(response)

    def get_port_blocking(self, component: str, port: str) -> Optional[bool]:
        cmd = msgpack.packb({'command': 'get_port_blocking', 'component': component, 'port': port})
        response = self._send(cmd)
        if response is None:
            return None
        elif response['type'] == 'response':
            params = response['content']
            return params['value']
        else:
            print('ERROR: ' + response['content'])
            return None

    def set_port_blocking(self, component: str, port: str, blocking: bool) -> bool:
        cmd = msgpack.packb({'command': 'set_port_blocking', 'component': component, 'port': port, 'value': blocking})
        response = self._send(cmd)
        return RemoteControl.check_response(response)

    def get_port_max_queue_length(self, component: str, port: str) -> Optional[int]:
        cmd = msgpack.packb({'command': 'get_port_max_queue_length', 'component': component, 'port': port})
        response = self._send(cmd)
        if response is None:
            return None
        elif response['type'] == 'response':
            params = response['content']
            return params['value']
        else:
            print('ERROR: ' + response['content'])
            return None

    def set_port_max_queue_length(self, component: str, port: str, length: int) -> bool:
        cmd = msgpack.packb({'command': 'set_port_max_queue_length', 'component': component, 'port': port, 'value': length})
        response = self._send(cmd)
        return RemoteControl.check_response(response)

    @staticmethod
    def check_response(response: dict) -> bool:
        if response is None:
            return False
        elif response['type'] == 'response':
            return True
        elif response['content'] == 'receive error':
            print(('ERROR: ' + response['content']))
            print('Is corresponding cpp type registered in main application?')
        else:
            print('ERROR: ' + response['content'])
        return False

    def create_value_accessor(
        self,
        value_type: Type['ValueT'],
        topic: str,
        direction: 'ValueAccessorDirection'
    ) -> RCValueAccessor['ValueT']:
        """
        in the future, we could generalize and make RemoteControl implement this method from some ValueAccessorFactory.
        """
        new_value_accessor = RCValueAccessor[value_type](topic, value_type, self)

        def forbidden_set(self, value: 'ValueT') -> None:
            raise PermissionError("this object cannot SET due to its ValueAccessorDirection")

        def forbidden_get(self) -> None:
            raise PermissionError("this object cannot GET due to its ValueAccessorDirection")

        if direction == ValueAccessorDirection.SENDER:
            # using __get__ on the function to bind it to the instance
            new_value_accessor.get = forbidden_get.__get__(new_value_accessor, RCValueAccessor)
        elif direction == ValueAccessorDirection.RECEIVER:
            new_value_accessor.set = forbidden_set.__get__(new_value_accessor, RCValueAccessor)

        return new_value_accessor

    def create_receiver_accessor(
        self,
        value_type: Type['ValueT'],
        topic: str
    ) -> ReceiverAccessor['ValueT']:
        return ReceiverAccessor(topic, value_type, self)

    def create_sender_accessor(
        self,
        value_type: Type['ValueT'],
        topic: str
    ) -> SenderAccessor['ValueT']:
        return SenderAccessor(topic, value_type, self)

    def set_playback_modifier(self, playback_modifier: PlaybackModifier) -> bool:
        assert isinstance(playback_modifier, PlaybackModifier)
        cmd = msgpack.packb({'command': 'set_playback_modifier',
                             'playback_modifier': playback_modifier})
        response = self._send(cmd)
        return RemoteControl.check_response(response)

    def set_replay_params(self, replay_params : ReplayParams) -> bool:
        assert isinstance(replay_params, ReplayParams)
        cmd = msgpack.packb({'command': 'set_replay_params',
                             'run_mode': replay_params.run_mode,
                             'run_without_drops' : replay_params.run_without_drops,
                             'speed_factor' : replay_params.speed_factor,
                             'pipeline_end_trigger_names': replay_params.pipeline_end_trigger_names,
                             'wait_input_event_name': replay_params.wait_input_event_name,
                             'wait_input_event_topic': replay_params.wait_input_event_topic,
                             'step_time_microseconds': replay_params.step_time_microseconds})

        response = self._send(cmd)
        return RemoteControl.check_response(response)

    def get_replay_params(self) -> ReplayParams:
        cmd = msgpack.packb({'command': 'get_replay_params'})
        response = self._send(cmd)
        if response is None:
            return False
        elif response['type'] == 'response':
            params = response['content']
            replay_params = ReplayParams(params['run_mode'],
                                         params['run_without_drops'],
                                         params['speed_factor'],
                                         params['pipeline_end_trigger_names'],
                                         params['wait_input_event_name'],
                                         params['wait_input_event_topic'],
                                         params['step_time_microseconds'])

            return replay_params
        else:
            print('ERROR: ' + response['content'])
            return False

    def get_sim_time(self) -> bool or int:
        cmd = msgpack.packb({'command': 'get_sim_time'})
        response = self._send(cmd)
        if response is None:
            return False
        elif response['type'] == 'response':
            params = response['content']
            if params['is_initialised']:
                return params['sim_time']
            else:
                return False
        else:
            print('ERROR: ' + response['content'])
            return False
