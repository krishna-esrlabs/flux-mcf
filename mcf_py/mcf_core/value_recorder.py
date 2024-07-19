"""
Copyright (c) 2024 Accenture
"""

import collections
import copy
import time
from dataclasses import dataclass, field
from datetime import datetime
import enum
import msgpack
import psutil
import random
import threading
import zlib

from mcf.value import Value
from mcf_core.value_store import ValueStore

MAX_VALUE_ID = int(2**64)
MAX_DROP_COUNT = int(2**32)
MAX_QUEUE_SIZE_LIMIT = int(2**32)

RECORDER_STATUS_TOPIC = "/mcf/recorder/status"
RECORDER_STATUS_TYPEID = "msg::RecorderStatus"


def _random_id():
    return random.randint(1, MAX_VALUE_ID)


@dataclass
class PacketHeader:
    time: int = 0
    topic: str = ""
    tid: str = ""
    vid: int = 0


@dataclass
class ExtMemHeader:
    extmem_size: int = 0
    extmem_present: bool = False
    extmem_size_compressed: int = 0


@dataclass
class QueueEntry:
    """
    Input data for the queue
    """
    time: datetime = None
    topic: str = ""
    value: ... = None


class Queue:

    @dataclass
    class QueueValue:
        """
        Internal representation of queue entries
        """
        time: datetime = None
        topic_id: int = 0
        value: ... = None

    def __init__(self):
        self._lock = threading.Lock()
        self._deque = collections.deque()
        self._topic_id_map = dict()
        self._expired = False

    def receive(self, topic, value):
        topic_id = hash(topic)
        now = datetime.now()
        qv = Queue.QueueValue(time=now, topic_id=topic_id, value=value)
        with self._lock:
            if topic_id not in self._topic_id_map.keys():
                self._topic_id_map[topic_id] = topic

            self._deque.append(qv)

    def pop(self):
        with self._lock:
            size = len(self._deque)
            if len(self._deque) > 0:
                qv = self._deque[0]
                topic = self._topic_id_map[qv.topic_id]
                qe = QueueEntry(time=qv.time, topic=topic, value=qv.value)
                self._deque.popleft()
                size -= 1
            else:
                qe = QueueEntry()

        return qe, size

    @property
    def empty(self):
        with self._lock:
            return len(self._deque) <= 0

    @property
    def size(self):
        with self._lock:
            return len(self._deque)

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


@dataclass
class ValueRecorderStatus:

    # Throughput into record file in bytes per second
    output_bps: int = 0

    # Average delay between value store write and record file write in milliseconds
    avg_latency_ms: int = 0

    # Maximum delay between value store write and record file write in milliseconds
    max_latency_ms: int = 0

    avg_queue_size: int = 0
    max_queue_size: int = 0

    # CPU usage of the value recorder write thread in percent of real time;
    # a value of 100 means one cpu core worked 100% of the time
    cpu_usage_user: int = 0
    cpu_usage_system: int = 0

    # Set if values where dropped since last write due to overload
    drop_flag: bool = False

    # Set if there is at least one write error since the last status
    error_flag: bool = False

    # Error descriptions of write errors
    error_descs: set = field(default_factory=set)


class RecorderStatus(Value):
    """
    Note: This definition must be in line with the corresponding C++ definition in mcf_core/Messages.h,
          otherwise data exchange over mcf bridges will not work between C++ and Python

    TODO: use value type generator to create both C++ and Python type
    """
    __slots__ = ("outputBps", "avgLatencyMs", "maxLatencyMs", "avgQueueSize", "maxQueueSize",
                 "cpuUsageUser", "cpuUsageSystem", "dropFlag", "errorFlag", "errorDescs")

    def __init__(self, outputBps, avgLatencyMs, maxLatencyMs, avgQueueSize, maxQueueSize,
                 cpuUsageUser, cpuUsageSystem, dropFlag, errorFlag, errorDescs):
        super().__init__()
        self.outputBps: int = outputBps if outputBps is not None else int()
        self.avgLatencyMs: int = avgLatencyMs if avgLatencyMs is not None else int()
        self.maxLatencyMs: int = maxLatencyMs if maxLatencyMs is not None else int()
        self.avgQueueSize: int = avgQueueSize if avgQueueSize is not None else int()
        self.maxQueueSize: int = maxQueueSize if maxQueueSize is not None else int()
        self.cpuUsageUser: int = cpuUsageUser if cpuUsageUser is not None else int()
        self.cpuUsageSystem: int = cpuUsageSystem if cpuUsageSystem is not None else int()
        self.dropFlag: bool = dropFlag if dropFlag is not None else bool()
        self.errorFlag: bool = errorFlag if errorFlag is not None else bool()
        self.errorDescs: set = errorDescs if errorDescs is not None else set()

    def __repr__(self):
        ret_repr = ("RecorderStatus[outputBps={}, avgLatencyMs={}, avgQueueSize={}, "
                    "dropFlag={}, errorFlag={}, errorDescs={}]"
                    .format(self.outputBps, self.avgLatencyMs, self.avgQueueSize,
                            self.dropFlag, self.errorFlag, self.errorDescs))
        return ret_repr

    @staticmethod
    def deserialize(array):
        if array is None:
            raise ValueError("Received 'None' when trying to deserialize RecorderStatus object")

        if len(array) < 2:
            raise ValueError("Received value of incompatible length when trying to deserialize RecorderStatus object")

        if array[1] != "msg::RecorderStatus":
            raise ValueError("Received value_type: " + str(array[1]) + ", when trying to deserialize RecorderStatus object")

        _recorder_status = RecorderStatus._RecorderStatus__unpack(array[0])
        return _recorder_status

    @staticmethod
    def __unpack(array):

        return RecorderStatus(
            outputBps=array[0],
            avgLatencyMs=array[1],
            maxLatencyMs=array[2],
            avgQueueSize=array[3],
            maxQueueSize=array[4],
            cpuUsageUser=array[5],
            cpuUsageSystem=array[6],
            dropFlag=array[7],
            errorFlag=array[8],
            errorDescs=array[9]
        )

    def serialize(self):
        return [[
            self.outputBps,
            self.avgLatencyMs,
            self.maxLatencyMs,
            self.avgQueueSize,
            self.maxQueueSize,
            self.cpuUsageUser,
            self.cpuUsageSystem,
            self.dropFlag,
            self.errorFlag,
            list(self.errorDescs),
            ],
            RECORDER_STATUS_TYPEID,
        ]


class StatusMonitor:

    def __init__(self, value_store, logger):
        self._value_store = value_store
        self._logger = logger
        self._recorder_status = ValueRecorderStatus
        self._bytes_written = 0
        self._total_latency = 0
        self._total_queue_size = 0
        self._num_writes = 0
        self._last_out_time = datetime(year=1970, month=1, day=1)
        self._cpu_user = 0
        self._cpu_system = 0
        self._drop_count = 0

    def start(self):
        self._last_out_time = datetime.now()
        self._cpu_user = psutil.cpu_percent()
        self._cpu_system = 0  # TODO: How to get distinct percentage for system and user?
        self._init_status()

    def serialize_begin(self, queue_size, record_time):
        now = datetime.now()
        latency_ms = (now - record_time).total_seconds() * 1000
        self._total_latency += latency_ms
        self._num_writes += 1
        self._total_queue_size += queue_size

        if latency_ms > self._recorder_status.max_latency_ms:
            self._recorder_status.max_latency_ms = latency_ms

        if queue_size > self._recorder_status.max_queue_size:
            self._recorder_status.max_queue_size = queue_size

    def serialize_end(self):
        now = datetime.now()
        elapsed_secs = (now - self._last_out_time).total_seconds()
        if elapsed_secs > 1.0:
            self._output_status()

    def report_write_error(self, err_msg):
        self._recorder_status.error_flag = True
        self._recorder_status.error_descs.add(err_msg)

    def report_dropped(self):
        self._recorder_status.drop_flag = True
        if self._drop_count < MAX_DROP_COUNT:  # do not increment beyond max
            self._drop_count += 1

    def inc_bytes_written(self, num):
        self._bytes_written += num

    def _init_status(self):
        self._recorder_status.drop_flag = False
        self._recorder_status.error_flag = False
        self._recorder_status.max_latency_ms = 0
        self._recorder_status.max_queue_size = 0
        self._recorder_status.error_descs = set()
        self._bytes_written = 0
        self._total_latency = 0
        self._total_queue_size = 0
        self._num_writes = 0

    def _output_status(self):
        now = datetime.now()
        dt = (now - self._last_out_time).total_seconds()

        cpu_user = psutil.cpu_percent()
        cpu_system = 0   # TODO: How to get distinct percentage for system and user?
        self._recorder_status.output_bps = self._bytes_written / dt
        self._recorder_status.avg_latency_ms = self._total_latency / self._num_writes
        self._recorder_status.avg_queue_size = self._total_queue_size / self._num_writes
        self._recorder_status.cpu_usage_user = cpu_user
        self._recorder_status.cpu_usage_system = cpu_system

        status_value = RecorderStatus(outputBps=self._recorder_status.output_bps,
                                      avgLatencyMs=self._recorder_status.avg_latency_ms,
                                      maxLatencyMs=self._recorder_status.max_latency_ms,
                                      avgQueueSize=self._recorder_status.avg_queue_size,
                                      maxQueueSize=self._recorder_status.max_queue_size,
                                      cpuUsageUser=self._recorder_status.cpu_usage_user,
                                      cpuUsageSystem=self._recorder_status.cpu_usage_system,
                                      dropFlag=self._recorder_status.drop_flag,
                                      errorFlag=self._recorder_status.error_flag,
                                      errorDescs=self._recorder_status.error_descs)
        status_value.inject_id(_random_id())
        self._value_store.set_value(RECORDER_STATUS_TOPIC, copy.deepcopy(status_value))

        if self._recorder_status.drop_flag:
            self._logger.error(f"ValueRecorder has dropped {self._drop_count} values "
                               f"as it cannot process them fast enough.")
            self._drop_count = 0

        if self._recorder_status.avg_latency_ms > 1000:
            self._logger.warning(f"ValueRecorder writes are delayed by {self._recorder_status.avg_latency_ms} ms. "
                                 f"Values are piling up in the ValueRecorder.")

        self._last_out_time = now
        self._cpu_user = cpu_user
        self._cpu_system = cpu_system
        self._init_status()


class ValueRecorder:

    def __init__(self, value_store, logger):
        self._value_store: ValueStore = value_store
        self._queue = Queue()
        self._file = None
        self._thread = None
        self._lock = threading.Lock()
        self._stop_request = threading.Event()
        self._logger = logger
        self._status_monitor = StatusMonitor(value_store, logger)
        self._queue_size_limit = MAX_QUEUE_SIZE_LIMIT
        self._disabled_topics = set()
        self._enabled_ext_mem_topics = set()
        self._compressed_ext_mem_topics = set()

    def __del__(self):
        self.stop()

    def start(self, filename):
        """
        Start recording all topics
        :param filename: the filename to use for recording
        """
        if self._file is not None:
            self._logger.error("Value recorder already started")
            return

        try:
            file = open(filename, "wb")
        except OSError as e:
            self._logger.error(f"Failed to open recording file: {e}")
            return
        except Exception as e:
            self._logger.error(f"Failed to open recording file: {e}")
            return

        self._file = file
        self._value_store.add_all_topic_receiver(self._queue)
        self._thread = threading.Thread(target=self.write_thread)
        self._stop_request.clear()
        self._thread.start()
        return

    @property
    def write_queue_empty(self):
        """
        Check if the queue holding the values to be written is empty
        :return: true if the queue is empty, false otherwise
        """
        return self._queue.empty

    def stop(self):
        if self._file is not None:
            self._queue.expire()
            self._stop_request.set()

            if self._thread is not None:
                self._thread.join()
                self._thread = None

            self._file.close()
            self._file = None

    def enable_ext_mem_serialization(self, topic):
        """
        Enable serialization of ext mem values for a specific topic
        :param topic: the topic
        """
        with self._lock:
            self._enabled_ext_mem_topics.add(topic)

    def enable_ext_mem_compression(self, topic):
        """
        Enable compression of ext mem values for a specific topic.
        Enabling compression only has an effect on topics with serialization enabled.
        :param topic: the topic
        """
        with self._lock:
            self._compressed_ext_mem_topics.add(topic)
            is_ext_mem_enabled = self._is_ext_mem_enabled(topic)

        if not is_ext_mem_enabled:
            self._logger.warn(
                f"Setting ExtMem compression for topic '{topic}' has no effect. "
                f"Its ExMem data is not recorded.")

    def set_write_queue_size_limit(self, limit):
        """
        Set a hard limit for the number of elements in the write buffer queue
        :param limit: the allowed max number of queued elements
        """
        with self._lock:
            self._queue_size_limit = limit

    def disable_serialization(self, topic):
        """
        Disable serialization for a specific topic
        :param topic: the topic
        """
        with self._lock:
            self._disabled_topics.add(topic)

    def write_thread(self):
        self._status_monitor.start()
        while not self._stop_request.is_set():
            qe, queue_size = self._queue.pop()
            while qe.value is not None:
                if queue_size < self._queue_size_limit or qe.topic == RECORDER_STATUS_TOPIC:
                    self._serialize(qe)
                else:
                    self._status_monitor.report_dropped()
                qe, queue_size = self._queue.pop()
            time.sleep(0.01)

    def _is_topic_enabled(self, topic):
        """
        Check if extMem recording is enabled for a given topic.
        Note: The caller must acquire the lock before calling this method
        """
        return topic not in self._disabled_topics

    def _is_ext_mem_enabled(self, topic):
        """
        Check if extMem recording is enabled for a given topic.
        Note: The caller must acquire the lock before calling this method
        """
        return topic in self._enabled_ext_mem_topics

    def _is_ext_mem_compression_enabled(self, topic):
        """
        Check if extMem compression is enabled for a given topic.
        Note: The caller must acquire the lock before calling this method
        """
        return topic in self._compressed_ext_mem_topics

    def _serialize(self, qe):
        """
        Serialize the given queue entry for recording.
        Note: The caller must acquire the lock before calling this method
        """
        def encode_serializable(o):
            if isinstance(o, Value):
                return o.serialize()[0]
            elif isinstance(o, enum.Enum):
                return o.value
            return o

        topic = qe.topic
        value = qe.value

        with self._lock:

            if not self._is_topic_enabled(topic):
                return

            extmem_enabled = self._is_ext_mem_enabled(topic)
            compress_extmem = self._is_ext_mem_compression_enabled(topic)
            recording_file = self._file

        if not isinstance(value, Value):
            self._logger.warning(f"Value on topic {topic} is not an MCF Value, recording rejected")
            return

        if recording_file is None:
            self._logger.error(f"Recording file not open")
            return

        self._status_monitor.serialize_begin(self._queue.size, qe.time)

        serialized = value.serialize()

        now = datetime.now()
        pheader = PacketHeader()
        pheader.time = int(now.timestamp()) * 1000 + now.microsecond // 1000  # timestamp in ms
        pheader.topic = qe.topic
        pheader.tid = serialized[1]
        pheader.vid = value.id

        pack_extmem = len(serialized) > 2 and extmem_enabled

        uncompressed_len = 0
        extmem_data = None
        if pack_extmem:
            extmem_data = serialized[2]
            uncompressed_len = len(extmem_data)

        mheader = ExtMemHeader
        mheader.extmem_size = uncompressed_len
        mheader.extmem_present = pack_extmem
        mheader.extmem_size_compressed = 0

        if (pack_extmem and compress_extmem):

            try:
                compressed = zlib.compress(extmem_data)
                mheader.extmem_size_compressed = len(compressed)
                extmem_data = compressed

            except zlib.error as e:
                warning = (f"Could not compress extmem data on {topic}. "
                           f"Falling back to non-compressed recording")
                self._status_monitor.report_write_error(warning)
                self._logger.warning(warning)

        data = [msgpack.packb([pheader.time,  # header
                               pheader.topic,
                               pheader.tid,
                               pheader.vid]),
                msgpack.packb(serialized[0], default=encode_serializable),  # value
                msgpack.packb([mheader.extmem_size,
                               mheader.extmem_present,
                               mheader.extmem_size_compressed]),
                extmem_data]

        for entry in data:
            if entry is not None:
                recording_file.write(entry)
                self._status_monitor.inc_bytes_written(len(entry))

        self._status_monitor.serialize_end()


