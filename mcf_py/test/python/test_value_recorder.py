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

from mcf.value import Value
from mcf_core.logger import get_logger
from mcf_core.value_store import ValueStore
from mcf_core.value_recorder import ValueRecorder, RECORDER_STATUS_TOPIC, RECORDER_STATUS_TYPEID

from mcf.record_reader import RecordReader

from value_types.value_types.base.Matrix4x4F import Matrix4x4F
from value_types.value_types.base.MultiExtrinsics import MultiExtrinsics
from value_types.value_types.base.PointXYZ import PointXYZ
from value_types.value_types.camera.ImageUint8 import ImageUint8
from value_types.value_types.camera.Format import Format


_RECORDING_DIR = os.path.join(_SCRIPT_DIRECTORY, "../tmp/")
os.makedirs(_RECORDING_DIR, exist_ok=True)

_RECORDING_FILE = os.path.join(_RECORDING_DIR, "record.bin")


cur_vaue_id = 41


def _next_value_id():
    global cur_value_id
    id = cur_value_id
    cur_value_id += 1
    return id


def fully_serialize_value(value):
    if isinstance(value, list):
        return [fully_serialize_value(entry) for entry in value]

    if isinstance(value, Value):
        serialized = value.serialize()[0]
        return fully_serialize_value(serialized)

    return value


def check_record(record, recorded_extmem, orig, orig_topic, extmem_enabled=False):
    serialized_orig = orig.serialize()
    fully_serialized_orig = fully_serialize_value(orig)

    assert record.topic == orig_topic, "Value record contains wrong topic"
    assert record.valueid == orig.id, "Value record contains wrong value id"
    assert record.typeid == serialized_orig[1]

    recorded_data = record.value
    assert recorded_data == fully_serialized_orig, "Recorded value differs from original value"

    if extmem_enabled:
        orig_extmem = serialized_orig[2] if len(serialized_orig) > 2 else None
        assert recorded_extmem == orig_extmem, f"Recorded extmem differs from original extmem"
    else:
        assert recorded_extmem is None, "Recorded value contains extmem although extmem recording not enabled"


def test_start_stop():
    global cur_value_id
    cur_value_id = 41

    # remove recording file from previous test if any
    if os.path.isfile(_RECORDING_FILE):
        os.remove(_RECORDING_FILE)

    logger = get_logger("ValueRecorder")
    value_store = ValueStore()
    value_recorder = ValueRecorder(value_store, logger)

    value_recorder.start(_RECORDING_FILE)
    value_recorder.stop()

    # make sure recording file can be opened and indexed, and is empty
    rec_reader = RecordReader()
    rec_reader.open(_RECORDING_FILE)
    rec_reader.index(filter=None, unpack_value=True)

    assert rec_reader.index_size() == 0, "Value recording unexpectedly contains at least one value"


def do_test_record_single_no_extmem(extmem_enabled, extmem_compressed):
    global cur_value_id
    cur_value_id = 41

    # remove recording file from previous test if any
    if os.path.isfile(_RECORDING_FILE):
        os.remove(_RECORDING_FILE)

    logger = get_logger("ValueRecorder")
    value_store = ValueStore()
    value_recorder = ValueRecorder(value_store, logger)

    if extmem_enabled:
        value_recorder.enable_ext_mem_serialization("TOPIC_1")

    if extmem_compressed:
        value_recorder.enable_ext_mem_compression("TOPIC_1")

    value_recorder.start(_RECORDING_FILE)
    value = PointXYZ(0.1, 0.2, 0.3)
    value.inject_id(_next_value_id())
    value_store.set_value("TOPIC_1", value)

    time.sleep(0.1)
    value_recorder.stop()

    # make sure recording file can be read and contains the expected value
    rec_reader = RecordReader()
    rec_reader.open(_RECORDING_FILE)
    rec_reader.index(filter=None, unpack_value=True)

    num_records = rec_reader.index_size()
    assert num_records == 1, "Value recording contains unexpected number of entries"

    records = rec_reader.records(0, num_records)
    entry = records[0]
    extmem = rec_reader.get_extmem(entry)
    check_record(entry, extmem, value, "TOPIC_1", extmem_enabled)


def test_record_single_no_extmem_disabled():
    do_test_record_single_no_extmem(extmem_enabled=False, extmem_compressed=False)


def test_record_single_no_extmem_enabled():
    do_test_record_single_no_extmem(extmem_enabled=True, extmem_compressed=False)


def test_record_single_no_extmem_compressed():
    do_test_record_single_no_extmem(extmem_enabled=True, extmem_compressed=True)


def test_record_recorder_status():
    global cur_value_id
    cur_value_id = 41

    # remove recording file from previous test if any
    if os.path.isfile(_RECORDING_FILE):
        os.remove(_RECORDING_FILE)

    logger = get_logger("ValueRecorder")
    value_store = ValueStore()
    value_recorder = ValueRecorder(value_store, logger)
    value_recorder.start(_RECORDING_FILE)

    # sleep at least 1 sec to trigger creation of recorder status
    time.sleep(1.2)

    value = PointXYZ(0.1, 0.2, 0.3)
    value.inject_id(_next_value_id())
    value_store.set_value("TOPIC_1", value)

    time.sleep(0.1)
    value_recorder.stop()

    # make sure recording file can be opened and indexed
    rec_reader = RecordReader()
    rec_reader.open(_RECORDING_FILE)
    rec_reader.index(filter=None, unpack_value=True)

    num_records = rec_reader.index_size()  # should be 2: one recorded value followed by one recorder status
    assert num_records == 2, "Value recording contains unexpected number of entries"

    records = rec_reader.records(0, num_records)

    entry1 = records[0]
    extmem1 = rec_reader.get_extmem(entry1)
    check_record(entry1, extmem1, value, "TOPIC_1")

    entry2 = records[1]
    extmem2 = rec_reader.get_extmem(entry2)
    assert entry2.typeid == RECORDER_STATUS_TYPEID, "Unexpected typeid of RecorderStatus record"
    assert entry2.topic == RECORDER_STATUS_TOPIC, "Unexpected topic of recorder status"
    assert extmem2 is None, "Recorder status unexpectedly contains extmem"


def do_test_record_single_extmem(extmem_enabled, extmem_compressed):
    global cur_value_id
    cur_value_id = 41

    # remove recording file from previous test if any
    if os.path.isfile(_RECORDING_FILE):
        os.remove(_RECORDING_FILE)

    logger = get_logger("ValueRecorder")
    value_store = ValueStore()
    value_recorder = ValueRecorder(value_store, logger)

    if extmem_enabled:
        value_recorder.enable_ext_mem_serialization("TOPIC_1")

    if extmem_compressed:
        value_recorder.enable_ext_mem_compression("TOPIC_1")

    value_recorder.start(_RECORDING_FILE)

    img_data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9] * (10 * 200)
    value = ImageUint8(width=200, height=100, pitch=200, format=Format.GRAY, data=bytearray(img_data))
    value.inject_id(_next_value_id())
    value_store.set_value("TOPIC_1", value)

    time.sleep(0.1)
    value_recorder.stop()

    # make sure recording file can be read and contains the expected value
    rec_reader = RecordReader()
    rec_reader.open(_RECORDING_FILE)
    rec_reader.index(filter=None, unpack_value=True)

    num_records = rec_reader.index_size()
    assert num_records == 1, "Value recording contains unexpected number of entries"

    records = rec_reader.records(0, num_records)
    entry = records[0]
    extmem = rec_reader.get_extmem(entry)
    check_record(entry, extmem, value, "TOPIC_1", extmem_enabled)


def test_record_single_extmem_disabled():
    do_test_record_single_extmem(extmem_enabled=False, extmem_compressed=False)


def test_record_single_extmem_enabled():
    do_test_record_single_extmem(extmem_enabled=True, extmem_compressed=False)


def test_record_single_extmem_compressed():
    do_test_record_single_extmem(extmem_enabled=True, extmem_compressed=True)


def do_test_record_multiple_composed(extmem_enabled, extmem_compressed):
    global cur_value_id
    cur_value_id = 41

    # remove recording file from previous test if any
    if os.path.isfile(_RECORDING_FILE):
        os.remove(_RECORDING_FILE)

    logger = get_logger("ValueRecorder")
    value_store = ValueStore()
    value_recorder = ValueRecorder(value_store, logger)

    if extmem_enabled:
        value_recorder.enable_ext_mem_serialization("TOPIC_1")
        value_recorder.enable_ext_mem_serialization("TOPIC_2")
        value_recorder.enable_ext_mem_serialization("TOPIC_3")

    if extmem_compressed:
        value_recorder.enable_ext_mem_compression("TOPIC_1")
        value_recorder.enable_ext_mem_compression("TOPIC_2")
        value_recorder.enable_ext_mem_compression("TOPIC_3")

    value_recorder.start(_RECORDING_FILE)

    value1_0 = PointXYZ(0.1, 0.2, 0.3)
    value1_0.inject_id(_next_value_id())
    value_store.set_value("TOPIC_1", value1_0)

    matrix1 = Matrix4x4F(list(range(0, 16)))
    matrix2 = Matrix4x4F(list(range(16, 32)))
    matrix3 = Matrix4x4F(list(range(32, 48)))
    value2_0 = MultiExtrinsics([matrix1, matrix2, matrix3])
    value2_0.inject_id(_next_value_id())
    value_store.set_value("TOPIC_2", value2_0)

    img_data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9] * (10 * 200)
    value3_0 = ImageUint8(width=200, height=100, pitch=200, format=Format.GRAY, data=bytearray(img_data))
    value3_0.inject_id(_next_value_id())
    value_store.set_value("TOPIC_3", value3_0)

    value1_1 = PointXYZ(0.5, 0.1, -0.3)
    value1_1.inject_id(_next_value_id())
    value_store.set_value("TOPIC_1", value1_1)

    matrix1 = Matrix4x4F(list(range(0, 16)))
    matrix2 = Matrix4x4F(list(range(16, 32)))
    matrix3 = Matrix4x4F(list(range(32, 48)))
    value2_1 = MultiExtrinsics([matrix2, matrix3, matrix1])
    value2_1.inject_id(_next_value_id())
    value_store.set_value("TOPIC_2", value2_1)

    img_data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14] * (10 * 200)
    value3_1 = ImageUint8(width=200, height=150, pitch=200, format=Format.GRAY, data=bytearray(img_data))
    value3_1.inject_id(_next_value_id())
    value_store.set_value("TOPIC_3", value3_1)

    time.sleep(0.1)
    value_recorder.stop()

    # make sure recording file can be read and contains the expected value
    rec_reader = RecordReader()
    rec_reader.open(_RECORDING_FILE)
    rec_reader.index(filter=None, unpack_value=True)

    num_records = rec_reader.index_size()
    assert num_records == 6, "Value recording contains unexpected number of entries"

    records = rec_reader.records(0, num_records)
    entry = records[0]
    extmem = rec_reader.get_extmem(entry)
    check_record(entry, extmem, value1_0, "TOPIC_1", extmem_enabled)
    entry = records[1]
    extmem = rec_reader.get_extmem(entry)
    check_record(entry, extmem, value2_0, "TOPIC_2", extmem_enabled)
    entry = records[2]
    extmem = rec_reader.get_extmem(entry)
    check_record(entry, extmem, value3_0, "TOPIC_3", extmem_enabled)
    entry = records[3]
    extmem = rec_reader.get_extmem(entry)
    check_record(entry, extmem, value1_1, "TOPIC_1", extmem_enabled)
    entry = records[4]
    extmem = rec_reader.get_extmem(entry)
    check_record(entry, extmem, value2_1, "TOPIC_2", extmem_enabled)
    entry = records[5]
    extmem = rec_reader.get_extmem(entry)
    check_record(entry, extmem, value3_1, "TOPIC_3", extmem_enabled)


def test_record_multiple_composed_disabled():
    do_test_record_multiple_composed(extmem_enabled=False, extmem_compressed=False)


def test_record_multiple_composed_enabled():
    do_test_record_multiple_composed(extmem_enabled=True, extmem_compressed=False)


def test_record_multiple_composed_compressed():
    do_test_record_multiple_composed(extmem_enabled=True, extmem_compressed=True)
