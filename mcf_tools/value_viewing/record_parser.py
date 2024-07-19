""""
Script to parse record.bin into a human readable text format.

Copyright (c) 2024 Accenture
"""

import argparse
import pathlib
import datetime

import mcf_python_path.mcf_paths
from mcf.record_reader import RecordReader


class McfRecordParser:

    def __init__(self, f_in, f_out, topic):
        self.f_in = f_in
        self.file_out = f_out

        self.topic = topic

        self.reader = RecordReader()

    def get_info(self):
        self.reader.open(self.f_in)

        topics = self.topic.split('|')

        def filter_topic(rec):
            return any(rec.topic.startswith(t) for t in topics)


        records_generator = self.reader.records_generator(lambda rec: filter_topic(rec))

        last_timestamp = None
        with self.file_out.open(mode='w') as f_out:
            for record in records_generator:
                dt = '{:.3f}'.format((record.timestamp-last_timestamp)/1000) if last_timestamp is not None else '-'
                last_timestamp = record.timestamp
                hr_time = datetime.datetime.fromtimestamp(int(record.timestamp/1000))
                f_out.write("{}.{:03d} dT {} - {} - {}\n".format(hr_time, int((record.timestamp/1000-int(record.timestamp/1000))*1000), dt, record.topic, str(record.value)))

        self.reader.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="tool to parse record.bin for one topic",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('-fin', '--file_in',
                        help='.txt file name containing the list of positions respecting the specific format',
                        required=True)

    parser.add_argument('-t', '--topic',
                        help='topic, multiple topics may be given using | as a separator',
                        type=str,
                        required=True)

    parser.add_argument('-fo', '--file_out',
                        help='output file',
                        required=True,
                        type=str)

    args = parser.parse_args()

    file_in = pathlib.Path(args.file_in)
    if not file_in.exists():
        print("ERROR: {} cannot be found".format(file_in))
        exit()
    file_out = pathlib.Path(args.file_out)
    channel_topic = args.topic

    parser = McfRecordParser(file_in, file_out, channel_topic)
    parser.get_info()
