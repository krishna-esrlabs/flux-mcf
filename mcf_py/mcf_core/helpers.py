"""
Copyright (c) 2024 Accenture
"""

from datetime import datetime
import random


def now_micros():
    now = datetime.now()
    now_micros = int(now.timestamp() * 1e6) + now.microsecond
    return now_micros


def random_value_id():
    max_value_id = int(2**64)
    return random.randint(1, max_value_id)


def deep_update_dict(parent, child):
    """
    Update parent dictionary in place from content of child dictionary
    :param parent: the dictionary to update
    :param child:  the dictionary to update from
    """
    # both inputs must be dictionaries
    assert isinstance(parent, dict) and isinstance(child, dict), "Values must be dictionaries"

    # loop over all members of child
    for name, child_val in child.items():
        # get corresponding value of parent or None
        parent_val = parent.get(name, None)
        # recursively update parent, if parent and child dictionaries, otherwise replace or create parent entry
        if isinstance(parent_val, dict) and isinstance(child_val, dict):
            deep_update_dict(parent_val, child_val)
        else:
            parent[name] = child_val


def deep_merge_dicts(parent, children):
    """
    Update parent dictionary in place from the given child dictionaries in the given sequence
    :param parent:      the dictionary to update
    :param children:    list of dictionaries to update from
    """
    for child in children:
        deep_update_dict(parent, child)