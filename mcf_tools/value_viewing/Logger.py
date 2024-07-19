""""
Copyright (c) 2024 Accenture
"""
import time
import sys
import logging

from enum import IntEnum


class SeverityLevel(IntEnum):
    DEBUG = 0
    # Detailed information, typically of interest only when diagnosing problems
    INFO = 1
    # Confirmation that things are working as expected
    WARN = 2
    # Issue a warning regarding a particular runtime event
    # An indication that something unexpected happened / indicative of some problem in the
    # near future (e.g. ‘disk space low’). The software is still working as expected
    ERROR = 3
    # Report suppression of an error without raising exception (e.g. error handler in a long-running server process)
    # Due to a more serious problem, the software hasn't been able to perform some function
    FATAL = 4
    # A serious error, indicating that the program itself may be unable to continue running


class Logger:
    """class to apply the python logging tool to mcf components"""

    def __init__(self, rc, component_name, logfile_name, log_level, queue_length):

        self.rc = rc

        self.message_topic = '/mcf/log/' + component_name + '/message'
        self.control_topic = '/mcf/log/' + component_name + '/control'
        self.component_name = component_name

        connection_info = self.rc.get_info()
        self.component_id = next( ( i for i, r in enumerate(connection_info) if r["name"] == component_name ), None)
        if self.component_id is None:
            raise LookupError("no id found for component [{}] - check the name is correct".format(component_name))

        self.logfile_name = logfile_name

        self.log_level = log_level

        # logging configuration
        self.logger = logging.getLogger(self.component_name)  # instantiation
        self.logger.setLevel(log_level.name)  # lowest-severity log message a logger will handle
        # specify the layout of log records in the final output - ToDo: add milliseconds as well?
        self.formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                                           datefmt="%Y-%m-%d %H:%M:%S")
        self.file_handler = logging.FileHandler(self.logfile_name)  # Handlers send the log records
        self.file_handler.setFormatter(self.formatter)
        self.logger.addHandler(self.file_handler)

        self.apply_severity_level_to_remote()

        self.queue_length = queue_length
        self.start_time = 0
        self.counter = 0

    def apply_severity_level_to_remote(self):
        """
        Sets the logging level in the remote MCF process to the one configured in the instance
        """
        self.rc.write_value(self.control_topic, 'mcf::LogControl', [self.log_level.value])

    def log(self, message, severity_level):
        assert isinstance(severity_level, SeverityLevel), "provided severity level is not of type SeverityLevel"
        if severity_level == SeverityLevel.DEBUG:
            self.logger.debug(message)
        if severity_level == SeverityLevel.INFO:
            self.logger.info(message)
        if severity_level == SeverityLevel.WARN:
            self.logger.warning(message)
        if severity_level == SeverityLevel.ERROR:
            self.logger.error(message)
        if severity_level == SeverityLevel.FATAL:
            self.logger.fatal(message)

    def start_logger(self):
        self.start_time = time.time()
        self.rc.set_queue(self.message_topic, self.queue_length)

    def get_new_messages(self):
        log_queue = self.rc.read_all_values(self.message_topic, self.queue_length)
        for new_log in log_queue:
            # print("new_log = ".format(new_log))
            if new_log:
                new_log = new_log[0]
                message, severity_level = new_log
                severity_level = SeverityLevel(severity_level)

                self.counter += 1

                if self.counter % 10 == 0:
                    intermediate_time = time.time()
                    frequency_from_start = self.counter / (intermediate_time - self.start_time)
                    print(
                        "\rMessage {} ({} in queue -> freq={}) - level={} - message={}"
                        .format(
                            self.counter,
                            len(log_queue),
                            round(frequency_from_start, 2),
                            severity_level.name,
                            message.replace("\r", "").replace("\n", "")
                        ),
                        end="")
                    sys.stdout.flush()
                self.log(message, severity_level)

    def __del__(self):
        self.rc.disconnect()
