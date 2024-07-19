"""
Script for converting an MCF component trace event recording to Common TRace Format

Copyright (c) 2024 Accenture
"""

import argparse
import os

import mcf_python_path.mcf_paths
from mcf import RecordReader
import component_tracing.event_serialization as event_serialization


def parse_input_events(evt_reader):

    # open value store recording and filter all entries
    records_generator = evt_reader.records_generator()

    # read all input events, skipping records that are not component trace events
    in_events = []
    for record in records_generator:
        event = event_serialization.parse_mcf_event(record)
        if event is not None:
            in_events.append(event)

    return in_events


def events_to_ctf(in_events):

    # lists of timestamps and binary ctf records
    timestamps = []
    ctf_records = []

    for event in in_events:
        times, records = event_serialization.event_to_ctf(event)
        timestamps += times
        ctf_records += records

    return timestamps, ctf_records


def sort_ctf_events(timestamps, ctf_events):
    print("    Sorting according to time ...")
    sort_indices = sorted(range(len(timestamps)), key=timestamps.__getitem__)
    return [ctf_events[i] for i in sort_indices]


def write_ctf_file(filename, ctf_events, out_dir):
    print("    Writing output file", filename, "...")
    with open(os.path.join(out_dir, filename), "wb") as outfile:
        event_serialization.create_ctf_stream(outfile, ctf_events)


def write_single_channel_ctf(timed_events_list, out_dir):
    timestamps = []
    ctf_events = []

    print("    Merging channels")
    for timed_events in timed_events_list:
        timestamps += timed_events[0]
        ctf_events += timed_events[1]

    # sort according to time
    ctf_events_sorted = sort_ctf_events(timestamps, ctf_events)

    # write channel file
    write_ctf_file("channel0_0", ctf_events_sorted, out_dir)


def write_multi_channel_ctf(timed_events_list, out_dir):
    for i, events in enumerate(timed_events_list):
        timestamps = events[0]
        ctf_events = events[1]

        # sort according to time
        ctf_events_sorted = sort_ctf_events(timestamps, ctf_events)

        # write channel file
        filename = "channel0_" + str(i)
        write_ctf_file(filename, ctf_events_sorted, out_dir)


def convert2ctf(traces, out_dir, single_channel):
    # create output directory if non-existent
    os.makedirs(out_dir, exist_ok=True)

    # write metafile
    print("Writing metadata file ...")
    event_serialization.create_ctf_metadata(os.path.join(out_dir, "metadata"))

    timed_events_list = []

    # process all traces passed as arguments
    for trace in traces:
        print("Processing ", trace, "...")

        # load event data from trace recording file
        print("    Reading event trace recording ...")
        recording_reader = RecordReader()
        recording_reader.open(trace)

        # parse input events from MCF event recording
        parsed_events = parse_input_events(recording_reader)

        # convert to CTF format
        print("    Converting to Common Trace Format ...")
        timestamps, ctf_events = events_to_ctf(parsed_events)

        timed_events_list.append((timestamps, ctf_events))

    print("Generating CTF output files ...")

    if single_channel:
        # merge the events of all traces into a single ctf file
        write_single_channel_ctf(timed_events_list, out_dir)
    else:
        # generate a different ctf file (i.e. channel) for each trace
        write_multi_channel_ctf(timed_events_list, out_dir)


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('-trace', '--trace', action='append', required=True, default=None,
                        dest='traces', type=str,
                        help='file(s) from which to load remote target trace recording')
    parser.add_argument('-o', '--o', action='store', required=True, default=None,
                        dest='out_dir', type=str,
                        help='directory to which to write the trace files')
    parser.add_argument('-s', '--single_channel', action='store_true',
                        required=False, dest='single_channel',
                        help='if set, multiple traces will be merged into a '
                             'single channel file')
    args = parser.parse_args()
    convert2ctf(args.traces, args.out_dir, args.single_channel)
