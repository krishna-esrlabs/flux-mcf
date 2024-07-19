"""
Utilities for reading and writing component event traces

Copyright (c) 2024 Accenture
"""
import datetime
import struct
import binascii

import mcf_python_path.mcf_paths
import component_tracing.ctf_metadata as ctf_metadata


def parse_port_read(value, event_type):
    """
    Parse port peek or port read event from mcf recording
    """
    trace_id = value[0]

    return {
        'recording_id': trace_id,
        'timestamp': value[1],
        'type': event_type,
        'component': {'name': f'{trace_id}:{value[2]}'},
        'port': {
            'name': f'{trace_id}:{value[3][0]}',
            'topic': f'{trace_id}:{value[3][1]}',
            'connected': value[3][2],
        },
        'value_id': value[4],
        'thread_id': value[5],
        'cpu_id': value[6]
    }


def parse_port_write(value, event_type):
    """
    Parse port write event from mcf recording
    """
    trace_id = value[0]

    return {
        'recording_id': trace_id,
        'timestamp': value[1],
        'type': event_type,
        'component': {'name': f'{trace_id}:{value[2]}'},
        'port': {
            'name': f'{trace_id}:{value[3][0]}',
            'topic': f'{trace_id}:{value[3][1]}',
            'connected': value[3][2],
        },
        'value_id': value[4],
        'input_value_ids': value[5],
        'thread_id': value[6],
        'cpu_id': value[7]
    }


def parse_exec_time(value, event_type):
    """
    Parse exec time event from mcf recording
    """
    trace_id = value[0]
    return {
        'recording_id': trace_id,
        'timestamp': value[1],
        'type': event_type,
        'component': {'name': f'{trace_id}:{value[2]}'},
        'description': value[3],
        'exec_time': value[4],
        'thread_id': value[5],
        'cpu_id': value[6]
    }


def parse_remote_transfer_time(value, event_type):
    """
    Parse remote transfer time event from mcf recording
    """
    trace_id = value[0]
    return {
        'recording_id': trace_id,
        'timestamp': value[1],
        'type': event_type,
        'component': {'name': f'{trace_id}:{value[2]}'},
        'description': value[3],
        'remote_transfer_time': value[4],
        'thread_id': value[5],
        'cpu_id': value[6]
    }


def parse_trigger_act(value, event_type):
    """
    Parse trigger act event from mcf recording
    """
    trace_id = value[0]
    return {
        'recording_id': trace_id,
        'timestamp': value[1],
        'type': event_type,
        'component': {'name': f'{trace_id}:{value[2]}'},
        'trigger': {
            'topic': f'{trace_id}:{value[3][0]}',
            'tstamp': value[3][1]
        },
        'thread_id': value[4],
        'cpu_id': value[5]
    }


def parse_trigger_exec(value, event_type):
    """
    Parse trigger exec event from mcf recording
    """
    trace_id = value[0]
    return {
        'recording_id': trace_id,
        'timestamp': value[1],
        'type': event_type,
        'component': {'name': f'{trace_id}:{value[2]}'},
        'trigger': {
            'topic': f'{trace_id}:{value[3][0]}',
            'tstamp': value[3][1]
        },
        'description': value[4],
        'exec_time': value[5],
        'thread_id': value[6],
        'cpu_id': value[7]
    }


def parse_program_flow(value, event_type):
    """
    Parse program flow event from mcf recording
    """
    trace_id = value[0]
    return {
        'recording_id': trace_id,
        'timestamp': value[1],
        'type': event_type,
        'component': {'name': f'{trace_id}:{value[2]}'},
        'event_name': f'{trace_id}:{value[3]}',
        'input_value_ids': value[4],
        'thread_id': value[5],
        'cpu_id': value[6]
    }


def null_term_string(string):
    """
    Convert input string to null-terminated binary utf-8 representation
    """
    return bytes(string, 'utf-8') + b'\0'


def port_write_to_ctf(event):
    """
    Serialize port write event to common trace format
    """
    event_id = 10
    timestamp = event['timestamp']
    trace_id = event['recording_id']
    component_name = event['component']['name']
    port_name = event['port']['name']
    port_topic = event['port']['topic']
    port_connected = event['port']['connected']
    value_id = event['value_id']
    thread_id = event['thread_id']
    cpu_id = event['cpu_id']

    bdata = (struct.pack('<HQ', event_id, timestamp) +
             null_term_string(trace_id) +
             null_term_string(component_name) +
             null_term_string(port_name) +
             null_term_string(port_topic) +
             struct.pack('<B', port_connected) +
             struct.pack('<Q', value_id) +
             struct.pack('<l', thread_id) +
             struct.pack('<l', cpu_id))

    return [[timestamp, bdata]]


def port_read_to_ctf(event):
    """
    Serialize port read event to common trace format
    """
    event_id = 20
    timestamp = event['timestamp']
    trace_id = event['recording_id']
    component_name = event['component']['name']
    port_name = event['port']['name']
    port_topic = event['port']['topic']
    port_connected = event['port']['connected']
    value_id = event['value_id']
    thread_id = event['thread_id']
    cpu_id = event['cpu_id']

    bdata = (struct.pack('<HQ', event_id, timestamp) +
             null_term_string(trace_id) +
             null_term_string(component_name) +
             null_term_string(port_name) +
             null_term_string(port_topic) +
             struct.pack('<B', port_connected) +
             struct.pack('<Q', value_id) +
             struct.pack('<l', thread_id) +
             struct.pack('<l', cpu_id))

    return [[timestamp, bdata]]


def port_peek_to_ctf(event):
    """
    Serialize port peek event to common trace format
    """
    event_id = 25
    timestamp = event['timestamp']
    trace_id = event['recording_id']
    component_name = event['component']['name']
    port_name = event['port']['name']
    port_topic = event['port']['topic']
    port_connected = event['port']['connected']
    value_id = event['value_id']
    thread_id = event['thread_id']
    cpu_id = event['cpu_id']

    bdata = (struct.pack('<HQ', event_id, timestamp) +
             null_term_string(trace_id) +
             null_term_string(component_name) +
             null_term_string(port_name) +
             null_term_string(port_topic) +
             struct.pack('<B', port_connected) +
             struct.pack('<Q', value_id) +
             struct.pack('<l', thread_id) +
             struct.pack('<l', cpu_id))

    return [[timestamp, bdata]]


def exec_time_to_ctf(event):
    """
    Serialize exec time event to common trace format
    """
    event_id_start = 30
    event_id_end = 35
    timestamp_end = event['timestamp']
    trace_id = event['recording_id']
    component_name = event['component']['name']
    description = event['description']
    exec_time = event['exec_time']
    thread_id = event['thread_id']
    cpu_id = event['cpu_id']
    timestamp_start = int(round(timestamp_end - exec_time * 1.e6))

    exec_start = (struct.pack('<HQ', event_id_start, timestamp_start) +
                  null_term_string(trace_id) +
                  null_term_string(component_name) +
                  null_term_string(description) +
                  struct.pack('<f', exec_time) +
                  struct.pack('<l', thread_id) +
                  struct.pack('<l', cpu_id))

    exec_end = (struct.pack('<HQ', event_id_end, timestamp_end) +
                null_term_string(trace_id) +
                null_term_string(component_name) +
                null_term_string(description) +
                struct.pack('<f', exec_time) +
                struct.pack('<l', thread_id) +
                struct.pack('<l', cpu_id))

    return [[timestamp_start, exec_start], [timestamp_end, exec_end]]


def remote_transfer_time_to_ctf(event):
    """
    Serialize exec time event to common trace format
    """
    event_id_start = 60
    event_id_end = 65
    timestamp_end = event['timestamp']
    trace_id = event['recording_id']
    component_name = event['component']['name']
    description = event['description']
    transfer_time = event['remote_transfer_time']
    timestamp_start = int(round(timestamp_end - transfer_time * 1.e6))
    thread_id = event['thread_id']
    cpu_id = event['cpu_id']

    transfer_start = (struct.pack('<HQ', event_id_start, timestamp_start) +
                  null_term_string(trace_id) +
                  null_term_string(component_name) +
                  null_term_string(description) +
                  struct.pack('<f', transfer_time) +
                  struct.pack('<l', thread_id) +
                  struct.pack('<l', cpu_id))

    transfer_end = (struct.pack('<HQ', event_id_end, timestamp_end) +
                null_term_string(trace_id) +
                null_term_string(component_name) +
                null_term_string(description) +
                struct.pack('<f', transfer_time) +
                struct.pack('<l', thread_id) +
                struct.pack('<l', cpu_id))

    return [[timestamp_start, transfer_start], [timestamp_end, transfer_end]]


def port_trigger_act_to_ctf(event):
    """
    Serialize port trigger act event to common trace format
    """
    event_id = 40
    timestamp = event['timestamp']
    trace_id = event['recording_id']
    component_name = event['component']['name']
    trigger_topic = event['trigger']['topic']
    trigger_time = event['trigger']['tstamp']
    thread_id = event['thread_id']
    cpu_id = event['cpu_id']

    bdata = (struct.pack('<HQ', event_id, timestamp) +
             null_term_string(trace_id) +
             null_term_string(component_name) +
             null_term_string(trigger_topic) +
             struct.pack('<Q', trigger_time) +
             struct.pack('<l', thread_id) +
             struct.pack('<l', cpu_id))

    return [[timestamp, bdata]]


# serialize port trigger execution event to common trace format
def port_trigger_exec_to_ctf(event):
    """
    Serialize port trigger exec event to common trace format
    """
    event_id_start = 50
    event_id_end = 55
    timestamp_end = event['timestamp']
    trace_id = event['recording_id']
    component_name = event['component']['name']
    trigger_topic = event['trigger']['topic']
    trigger_time = event['trigger']['tstamp']
    exec_time = event['exec_time']
    thread_id = event['thread_id']
    cpu_id = event['cpu_id']
    timestamp_start = int(round(timestamp_end - exec_time * 1.e6))

    handler_start = (struct.pack('<HQ', event_id_start, timestamp_start) +
                     null_term_string(trace_id) +
                     null_term_string(component_name) +
                     null_term_string(trigger_topic) +
                     struct.pack('<Q', trigger_time) +
                     struct.pack('<f', exec_time) +
                     struct.pack('<l', thread_id) +
                     struct.pack('<l', cpu_id))

    handler_end = (struct.pack('<HQ', event_id_end, timestamp_end) +
                   null_term_string(trace_id) +
                   null_term_string(component_name) +
                   null_term_string(trigger_topic) +
                   struct.pack('<Q', trigger_time) +
                   struct.pack('<f', exec_time) +
                   struct.pack('<l', thread_id) +
                   struct.pack('<l', cpu_id))

    return [[timestamp_start, handler_start], [timestamp_end, handler_end]]


def program_flow_to_ctf(event):
    """
    Serialize program flow event to common trace format
    """
    event_id = 80
    timestamp = event['timestamp']
    trace_id = event['recording_id']
    component_name = event['component']['name']
    event_name = event['event_name']
    thread_id = event['thread_id']
    cpu_id = event['cpu_id']

    bdata = (struct.pack('<HQ', event_id, timestamp) +
             null_term_string(trace_id) +
             null_term_string(component_name) +
             null_term_string(event_name) +
             struct.pack('<l', thread_id) +
             struct.pack('<l', cpu_id))

    return [[timestamp, bdata]]


# serialize time box event to common trace format
def time_box_to_ctf(event):
    """
    Serialize time box event to common trace format
    """
    event_id_start = 70
    event_id_end = 75

    box_id = event['box_id']
    box_name = event['box_name']
    trace_id = event['trace_id']
    timestamp_start = event['timestamp']
    exec_time = event['exec_time']
    completion_status = event['completion_status']
    completion_status_id = event['completion_status_id']

    timestamp_end = int(round(timestamp_start + exec_time * 1.e6))

    time_box_start = (struct.pack('<HQ', event_id_start, timestamp_start) +
                      null_term_string(trace_id) +
                      null_term_string(box_name) +
                      struct.pack('<L', box_id) +
                      struct.pack('<L', completion_status_id) +
                      null_term_string(completion_status))
    time_box_end = (struct.pack('<HQ', event_id_end, timestamp_end) +
                    null_term_string(trace_id) +
                    null_term_string(box_name) +
                    struct.pack('<L', box_id) +
                    struct.pack('<L', completion_status_id) +
                    null_term_string(completion_status))

    return [[timestamp_start, time_box_start], [timestamp_end, time_box_end]]


# MCF recorded event deserialization map
EVENT_DESERIALIZERS_MCF = {'mcf::ComponentTracePortWrite': ['port_write', parse_port_write],
                           'mcf::ComponentTracePortRead': ['port_read', parse_port_read],
                           'mcf::ComponentTracePortPeek': ['port_peek', parse_port_read],
                           'mcf::ComponentTraceExecTime': ['exec_time', parse_exec_time],
                           'mcf::ComponentTraceRemoteTransferTime': ['remote_transfer_time', parse_remote_transfer_time],
                           'mcf::ComponentTracePortTriggerActivation': ['trigger_act', parse_trigger_act],
                           'mcf::ComponentTracePortTriggerExec': ['trigger_exec', parse_trigger_exec],
                           'mcf::ComponentTraceProgramFlowEvent': ['program_flow', parse_program_flow]}

# MCF event serialization map for Common Trace Format
EVENT_SERIALIZERS_CTF = {'port_write': port_write_to_ctf,
                         'port_read': port_read_to_ctf,
                         'port_peek': port_peek_to_ctf,
                         'exec_time': exec_time_to_ctf,
                         'remote_transfer_time': remote_transfer_time_to_ctf,
                         'trigger_act': port_trigger_act_to_ctf,
                         'trigger_exec': port_trigger_exec_to_ctf,
                         'program_flow': program_flow_to_ctf,
                         'time_box': time_box_to_ctf}


def parse_mcf_event(record):
    """
    Parse event from mcf recording

    :param record: a record from an mcf recording
    :return: a dictionary containing the event fields or None, if record is not a known event
    """

    if record.typeid not in EVENT_DESERIALIZERS_MCF.keys():
        return None

    value = record.value
    event_type = EVENT_DESERIALIZERS_MCF[record.typeid][0]
    parser = EVENT_DESERIALIZERS_MCF[record.typeid][1]
    parsed_event = parser(value, event_type)  # call type-specific parser

    return parsed_event


def event_to_ctf(parsed_event):
    """
    Convert a parsed MCF event into a sequence of events in CTF format

    :param parsed_event: A parsed mcf event
    :return: A list of timestamps and a list of binary data representing the resulting CTF events
             (possibly more than one) which the the input event has been converted to
    """

    # lists of timestamps and binary ctf records
    times = []
    records = []

    event_type = parsed_event['type']
    serializer = EVENT_SERIALIZERS_CTF.get(event_type, None)
    if serializer is not None:
        tstamps_ctf_records = serializer(parsed_event)
        times = [rec[0] for rec in tstamps_ctf_records]
        records = [rec[1] for rec in tstamps_ctf_records]

    return times, records


def create_ctf_stream(file, ctf_events, stream_id=0):
    """
    Create a CTF stream containing the given list of CTF events

    :param stream_id:   The stream ID to use (must match definition in CTF metafile, currently always 0)
    :param file:        The file path to which to write
    :param ctf_events:  The list of events to write to the stream
    """

    # write stream header
    magic = 'c1 1f fc c1'

    magic = binascii.unhexlify(''.join(magic.split()))
    clock_frequency = 1_000_000
    stream_id = struct.pack(
        '<LQQ', stream_id, 0, int(datetime.datetime.now().timestamp() * clock_frequency)
    )
    file.write(magic)
    file.write(stream_id)

    # write input events to output file
    for event in ctf_events:
        file.write(event)


def create_ctf_metadata(file):
    """
    Create a CTF metafile

    :param file:        The file path to which to write
    """

    # write input events to output file
    with open(file, 'w') as metafile:
        metafile.write(ctf_metadata.CTF_METADATA)
