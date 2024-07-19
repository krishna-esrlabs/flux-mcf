"""
Copyright (c) 2024 Accenture
"""

from mcf_core.helpers import deep_update_dict, deep_merge_dicts


def test_deep_update_dict():
    parent = {"a": 1,
              "b": "ooo",
              "d": {"hello": 17,
                    "world": {"x": [42, 43, 44],
                              "y": "oh"},
                    "test": 4200},
              "c": "m"}
    child = {"a": "2",
             "b": 0,
             "e": "e",
             "d": {"test": 41,
                   "world": {"x": 1,
                             "z": [6, 7, 8]},
                   "t": "t"}}

    deep_update_dict(parent, child)

    expected = {"a": "2",
                "b": 0,
                "c": "m",
                "d": {"hello": 17,
                      "world": {"x": 1,
                                "y": "oh",
                                "z": [6, 7, 8]},
                      "test": 41,
                      "t": "t"},
                "e": "e"}

    assert parent == expected, "Unexpected result from deep dictionary update"


def test_deep_merge_dicts():
    parent = {"a": 1,
              "b": "ooo",
              "d": {"hello": 17,
                    "world": {"x": [42, 43, 44],
                              "y": "oh"},
                    "test": 4200},
              "c": "m"}
    child_1= {"a": "2",
              "b": 0,
              "e": "e",
              "d": {"test": 41,
                    "world": {"x": 1,
                              "z": [6, 7, 8]},
                    "t": "t"}}
    child_2= {"a": "3",
              "e": "f",
              "g": "gggg",
              "d": {"world": 5,
                    "t": {"foo": 10, "bar": "20"}}}

    deep_merge_dicts(parent, [child_1, child_2])

    expected = {"a": "3",
                "b": 0,
                "c": "m",
                "d": {"hello": 17,
                      "world": 5,
                      "test": 41,
                      "t": {"foo": 10, "bar": "20"}},
                "e": "f",
                "g": "gggg"}

    assert parent == expected, "Unexpected result from deep dictionary update"
