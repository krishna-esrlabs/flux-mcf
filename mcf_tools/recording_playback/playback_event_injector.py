"""
Publishes values from a value recording which are on selected topics and were published
in the original recording within a certain time range. Filtered values will be injected
using the remote control.

Copyright (c) 2024 Accenture
"""
import mcf_python_path.mcf_paths
from mcf import RemoteControl
from mcf import PlaybackModifier
import recording_playback.filter_record as fr

import time
import argparse
import json
import dataclasses
import typing


@dataclasses.dataclass(frozen=True)
class EventTimeSliceParams:
    start_time: int
    end_time: int
    wait_time_ms: int
    include_topics: typing.List[str]


def main(trace_files: typing.List[str],
         record_files: typing.List[str],
         config_file_path: str,
         target_ip: str,
         target_port: str):
    event_time_slice_params_list = parse_config(config_file_path)

    # Create and open target connection
    tgt_rc = RemoteControl()
    tgt_rc.connect(target_ip, target_port)

    tgt_rc.set_playback_modifier(PlaybackModifier.PAUSE)

    # Write all events with timestamps while playback is paused. This allows all events to
    # be written, regardless of the size of the messages and network bandwidth, without
    # causing delays in playback. Playback will then be managed by the
    # EventTimingController.
    end_wait_time = 0
    for event_time_slice_params in event_time_slice_params_list:
        trace_filter = fr.TraceFilterFunctor(
            event_time_slice_params.start_time,
            event_time_slice_params.end_time,
            event_time_slice_params.include_topics)
        value_records, trace_records = fr.filter_records(
            trace_files,
            record_files,
            trace_filter)

        if not value_records:
            continue

        current_simulation_time_offset = None
        for value_record, trace_record in zip(value_records, trace_records):
            trace_timestamp = trace_record.value[1]
            component_name = trace_record.value[2]
            port_name = trace_record.value[3][0]

            if current_simulation_time_offset is None:
                current_simulation_time_offset = trace_timestamp - end_wait_time

            offset_timestamp = trace_timestamp - current_simulation_time_offset
            tgt_rc.write_value(topic=value_record.topic,
                               clazz=value_record.typeid,
                               value=value_record.value,
                               extmem=value_record.extmem_value,
                               timestamp=offset_timestamp,
                               valueId=value_record.valueid,
                               component=component_name,
                               port=port_name)

        wait_time_micro_seconds = event_time_slice_params.wait_time_ms * 1000

        end_wait_time = (trace_records[-1].value[1]
                         - current_simulation_time_offset
                         + wait_time_micro_seconds)

    tgt_rc.set_playback_modifier(PlaybackModifier.RESUME)

    queue_state = tgt_rc.get_event_queue_state()
    print('Current queue size: ', queue_state)
    while queue_state['size'] > 0:
        time.sleep(0.2)

        new_state = tgt_rc.get_event_queue_state()
        print('Current queue size:     ', queue_state)

        # ensure that queue only gets shorter
        assert new_state['size'] <= queue_state['size']
        queue_state = new_state

    print("Playback injection complete.")


def parse_config(config_file_path: str) -> typing.List['EventTimeSliceParams']:
    with open(config_file_path, 'r') as f:
        json_data = json.load(f)

    config_data = json_data['PlaybackEventInjection']

    event_time_slice_params_list = []
    for config_time_slice_params in config_data:
        start_time = config_time_slice_params['StartTime']
        end_time = config_time_slice_params['EndTime']
        wait_time_ms = config_time_slice_params['WaitTimeMs']
        include_topics = config_time_slice_params['IncludeTopics']

        event_time_slice_params = EventTimeSliceParams(
            start_time,
            end_time,
            wait_time_ms,
            include_topics)
        event_time_slice_params_list.append(event_time_slice_params)

    return event_time_slice_params_list


if __name__ == '__main__':
    def mainlet():
        parser = argparse.ArgumentParser()
        parser.add_argument('-t', '--t', action='append', required=True,
                            default=None, dest='traces', type=str,
                            help='File(s) from which to load component trace recordings')
        parser.add_argument('-r', '--r', action='append', required=True,
                            default=None, dest='records', type=str,
                            help='File(s) from which to load component value recording.')
        parser.add_argument('-c', '--c', action='store', required=True,
                            default=None, dest='config_file', type=str,
                            help='Config file path.')
        parser.add_argument('--ip', action='store', required=False,
                            default='10.11.0.20', dest='target_ip', type=str,
                            help='IP of target.')
        parser.add_argument('--port', action='store', required=False,
                            default='6666', dest='target_port', type=str,
                            help='Port of target.')
        args = parser.parse_args()

        main(args.traces, args.records, args.config_file, args.target_ip, args.target_port)

    mainlet()
