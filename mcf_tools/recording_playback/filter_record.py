"""
Filters values from a value recording which are on selected topics and were published in
the original recording within a certain time range.

Copyright (c) 2024 Accenture
"""
import mcf_python_path.mcf_paths
from mcf import RecordReader

import argparse
import typing
import math
import pickle


class FilteredValueData(typing.NamedTuple):
    value_id: int
    topic: str


class TraceFilterFunctor(object):
    def __init__(self,
                 lower_time_range: typing.Optional[int],
                 upper_time_range: typing.Optional[int],
                 include_topics: typing.Optional[typing.List[str]]):
        self.lower_time_range = lower_time_range
        self.upper_time_range = upper_time_range
        self.include_topics = include_topics

    def __call__(self, record: 'RecordReader.Record') -> bool:
        if record.typeid != 'mcf::ComponentTracePortWrite' or record.topic != '/mcf/trace_events':
            return False

        topic = record.value[3][1]
        trace_timestamp = record.value[1]

        lower_time = self.lower_time_range if self.lower_time_range is not None else -1
        upper_time = self.upper_time_range if self.upper_time_range is not None else math.inf
        if self.include_topics is None:
            is_in_include_topics = True
        else:
            is_in_include_topics = topic in self.include_topics

        is_in_time_range = lower_time <= trace_timestamp <= upper_time

        return is_in_include_topics and is_in_time_range


class ValueFilterFunctor(object):
    def __init__(self, include_value_data: typing.List['FilteredValueData']):
        self.include_value_data = include_value_data

    def __call__(self, record: 'RecordReader.Record') -> bool:
        record_value_data = FilteredValueData(value_id=record.valueid, topic=record.topic)

        return record_value_data in self.include_value_data


def get_records(trace_files: typing.List[str],
                recording_reader: 'RecordReader',
                record_filter: typing.Callable) \
        -> typing.List['RecordReader.Record']:
    records_full = []
    for inpath in trace_files:
        recording_reader.open(inpath)

        record_generator = recording_reader.records_generator(record_filter)

        for record in record_generator:
            records_full.append(record)

    return records_full


def save_filtered_value_record(
        output_file_path: str,
        records: typing.List['RecordReader.Record']):
    # Write value records to file
    with open(output_file_path, 'wb') as f:
        pickle.dump(records, f)


def arg_sort_records(array: typing.List['RecordReader.Record']) -> typing.List[int]:
    return sorted(range(len(array)), key=lambda i: array[i].timestamp)


def filter_records(
        trace_record_paths: typing.List[str],
        value_record_paths: typing.List[str],
        trace_record_filter: 'TraceFilterFunctor') \
            -> typing.Tuple[typing.List['RecordReader.Record'],
                            typing.List['RecordReader.Record']]:
    # Get filtered trace records.
    trace_recording_reader = RecordReader()
    filtered_trace_records_unsorted = get_records(trace_record_paths,
                                                  trace_recording_reader,
                                                  trace_record_filter)
    filtered_trace_records = sorted(filtered_trace_records_unsorted,
                                    key=lambda record: record.value[1])

    # Get data from each record in the filtered trace records
    filtered_value_data = [FilteredValueData(value_id=record.value[4],
                                             topic=record.value[3][1])
                           for record in filtered_trace_records]

    # Get the values from the record.bin corresponding to the records from the filtered
    # trace records.
    value_record_filter = ValueFilterFunctor(filtered_value_data)

    value_recording_reader = RecordReader()
    filtered_value_records_unsorted = get_records(value_record_paths,
                                                  value_recording_reader,
                                                  value_record_filter)
    filtered_value_records = sorted(filtered_value_records_unsorted,
                                    key=lambda record: record.timestamp)

    matching_filtered_trace_records = []
    for value_record in filtered_value_records:
        value_data = FilteredValueData(value_id=value_record.valueid,
                                       topic=value_record.topic)
        i = filtered_value_data.index(value_data)

        matching_filtered_trace_record = filtered_trace_records[i]
        matching_filtered_trace_records.append(matching_filtered_trace_record)

        value_record.extmem_value = value_recording_reader.get_extmem(value_record)

    sorted_idxs = arg_sort_records(filtered_value_records)
    sorted_filtered_value_records = [filtered_value_records[i] for i in sorted_idxs]
    sorted_filtered_trace_records = [matching_filtered_trace_records[i] for i in sorted_idxs]

    return sorted_filtered_value_records, sorted_filtered_trace_records


if __name__ == '__main__':
    def mainlet():
        parser = argparse.ArgumentParser()
        parser.add_argument('-t', '--t', action='append', required=True,
                            default=None, dest='traces', type=str,
                            help='File(s) from which to load component trace recordings')
        parser.add_argument('-r', '--r', action='append', required=True,
                            default=None, dest='records', type=str,
                            help='File(s) from which to load component value recording.')
        parser.add_argument('-l', '--lower_time_range',
                            action='store', required=False, default=None,
                            dest='lower_time_range', type=int,
                            help='Lower timestamp of trace value timestamp range. If not '
                                 'specified, will be equal to the first trace value '
                                 'timestamp.')
        parser.add_argument('-u', '--upper_time_range',
                            action='store', required=False, default=None,
                            dest='upper_time_range', type=int,
                            help='Upper timestamp of trace value timestamp range. If not '
                                 'specified, will be equal to the last trace value '
                                 'timestamp.')
        parser.add_argument('-f', '--include_topics',
                            action='append', required=False, default=None,
                            dest='include_topics', type=str,
                            help='List of topics which will remain in filtered .bin file.'
                                 ' If not set, will include all topics.')
        parser.add_argument('-o', '--o',
                            action='store', required=False, default=None,
                            dest='output_file_path', type=str,
                            help='If set, will save a pickle of the filtered records to '
                                 'the specified path.')

        args = parser.parse_args()

        trace_filter = TraceFilterFunctor(args.lower_time_range,
                                          args.upper_time_range,
                                          args.include_topics)
        filtered_value_records, _ = filter_records(args.traces, args.records, trace_filter)

        if args.output_file_path is not None:
            save_filtered_value_record(args.output_file_path, filtered_value_records)

    mainlet()
