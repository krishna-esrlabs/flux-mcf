""""
Copyright (c) 2024 Accenture
"""
import argparse
import os
from pathlib import Path
import random
import signal
import time

from mcf import RemoteControl


run = True
def stop(signum, frame):
    global run
    print(" stopping...")
    run = False


def queue_mcf_pointxy(rc, topic, pos):
    timestamp = time.time()
    rc.write_value(
        topic, 
        "mcf_remote_test_value_types::mcf_remote_test::TestPointXY", 
        pos, 
        None, 
        timestamp)
    return time.time() - timestamp


def check_value(rc, topic, expected):
    value, _, _ = rc.read_value(topic)
    return value == expected


def test_connection():
    global run

    parser = argparse.ArgumentParser()
    parser.add_argument('--ip', action='store', required=False, default="127.0.0.1", dest='ip', 
                        type=str, help='ip address to connect to')
    parser.add_argument('--port', action='store', required=False, default=6666, type=int, 
                        help='port number')
    args = parser.parse_args()

    # create and open target connection
    try:
        tgt_rc = RemoteControl()
        tgt_rc.connect(args.ip, args.port)
    except Exception as e:
        print("Could not connect:", e)  
        exit(-1)      

#    info = tgt_rc.get_info()
#    print(info)

    reference_image = [848, 480, 848, 848649600000]

    time.sleep(0.1)
    correct_positions = 0
    total_transfers = 0
    rec_time = 0
    rec_data = 0
    latency_send = 0
    latency_send_max = 0
    latency_rec = 0
    latency_rec_max = 0
    start_time = time.time()
    runtime = 0
    last_runtime = runtime

    try:
        while run:
            # set a target
            target = [random.randint(-100, 100), random.randint(-100, 100)]
            latency = queue_mcf_pointxy(tgt_rc, '/target', target)
            latency_send_max = max(latency_send_max, latency)
            latency_send += latency

            time.sleep(0.1)

            # read the position
            now = time.time()
            correct = check_value(tgt_rc, '/position', target)
            latency = time.time() - now
            total_transfers += 1
            if correct:
                correct_positions += 1
            latency_rec_max = max(latency_rec_max, latency)
            latency_rec += latency

            # read an image
            now = time.time()
            image = tgt_rc.read_value('/image')
            rec_time += time.time() - now
            rec_data += image[0][1] * image[0][2] / 1048576 # image size in MB

            runtime = time.time() - start_time
            if runtime - last_runtime > 10:
                print(f"Test running for {runtime:.2f} seconds")
                print(f"Received {correct_positions} positions out of {total_transfers} correctly")
                print(f"Latency send: max {latency_send_max*1000:.2f}ms average {latency_send/total_transfers*1000:.2f}ms")
                print(f"Latency receive: max {latency_rec_max*1000:.2f}ms average {latency_rec/total_transfers*1000:.2f}ms")
                print(f"Achieved bandwidth: {rec_data/rec_time} MB/s")
                print()
                last_runtime = runtime

    except Exception as e:
        print("Connection failed:", e)

    runtime = time.time() - start_time
    
    print(f"Test ran for {runtime:.2f} seconds")
    print(f"Received {correct_positions} positions out of {total_transfers} correctly")
    print(f"Latency send: max {latency_send_max*1000:.2f}ms average {latency_send/total_transfers*1000:.2f}ms")
    print(f"Latency receive: max {latency_rec_max*1000:.2f}ms average {latency_rec/total_transfers*1000:.2f}ms")
    print(f"Achieved bandwidth: {rec_data/rec_time:.3f} MB/s")


if __name__ == '__main__':
    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGHUP, stop)

    test_connection()
    print("Done")
    

