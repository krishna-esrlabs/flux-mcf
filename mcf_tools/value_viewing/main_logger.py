#!/usr/bin/env python3
"""
script to set a listener to the logs of one mcf component
Usage:
    ./main_logger.py -c COMPONENT [-o FILE] [-a IP] [-p PORT] [-v LVL] [-q QUEUE_SIZE]

Arguments:
    -c COMPONENT          name of the component for logs recording

Options:
    -o FILE               output file                 [default: log.txt]
    -a IP, --address IP   ip-address of the target    [default: 127.0.0.1]
    -p PORT, --port PORT  port to connect to          [default: 6666]
    -v LVL                verbosity level ∈  [0...4]  [default: 1]
    -q QUEUE_SIZE         queue size for the topic    [default: 5]

Copyright (c) 2024 Accenture
"""
from docopt import docopt

import mcf_python_path.mcf_paths
from mcf import RemoteControl
from Logger import Logger, SeverityLevel

if __name__ == "__main__":
    args = docopt(__doc__)
    rc = RemoteControl()
    rc.connect ( args['--address'], args['--port'] )

    logger = Logger(
        rc             = rc,
        component_name = args['-c'],
        logfile_name   = args['-o'],
        log_level      = SeverityLevel(int(args['-v'])),
        queue_length   = int(args['-q'])
    )

    logger.start_logger()

    while True:
        logger.get_new_messages()
