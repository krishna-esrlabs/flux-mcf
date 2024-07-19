"""
Copyright (c) 2024 Accenture
"""

import logging

_DEFAULT_LOGGING_LEVEL = logging.DEBUG


def init_logger(logger: logging.Logger):
    # remove all existing handlers from this logger
    for h in logger.handlers:
        logger.removeHandler(h)

    logger.setLevel(_DEFAULT_LOGGING_LEVEL)
    handler = logging.StreamHandler()
    handler.setLevel(_DEFAULT_LOGGING_LEVEL)

    # create formatter
    formatter = logging.Formatter('[PID-%(process)d] [%(asctime)s] [%(name)s] [%(levelname)s] %(message)s')

    # add formatter to ch
    handler.setFormatter(formatter)

    # add ch to logger
    logger.addHandler(handler)


def get_logger(name):
    logger = logging.getLogger(name)
    init_logger(logger)
    return logger
