""""
Copyright (c) 2024 Accenture
"""
import pytest
from collections import OrderedDict
from pathlib import Path
from typing import Any, List, Tuple, Union
import os

import mcf_python_path.type_generator_paths
from type_generator.cpp_indiv_generator import write_individual_header_file
from type_generator.python_type_generator import write_python_type_file
from type_generator.common import TypesData, Scalar, Type, Template, ConfigurationError


system_types = {'SystemTypes': {'bool': {'CppName': 'bool', 'PyName': 'bool', 'FlatBufferName': 'bool', 'Type': 'Primitive'}, 'double': {'CppName': 'double', 'PyName': 'float', 'FlatBufferName': 'double', 'Type': 'Primitive'}, 'float': {'CppName': 'float', 'PyName': 'float', 'FlatBufferName': 'float', 'Type': 'Primitive'}, 'int': {'CppName': 'int', 'PyName': 'int', 'FlatBufferName': 'int32', 'Type': 'Primitive'}, 'map': {'CppName': 'std::map', 'CppInclude': 'map', 'PyName': 'Dict', 'PyInclude': 'Dict', 'FlatBufferName': 'Map', 'Type': 'Container'}, 'pair': {'CppName': 'std::pair', 'CppInclude': 'utility', 'PyName': 'Tuple', 'PyInclude': 'Tuple', 'FlatBufferName': 'Tuple', 'Type': 'Container'}, 'set': {'CppName': 'std::set', 'CppInclude': 'set', 'PyName': 'List', 'PyInclude': 'List', 'FlatBufferName': 'List', 'Type': 'Container'}, 'string': {'CppName': 'std::string', 'CppInclude': 'string', 'FlatBufferName': 'string', 'PyName': 'str', 'Type': 'Primitive'}, 'size_t': {'CppName': 'std::size_t', 'CppInclude': 'cstddef', 'FlatBufferName': 'uint64', 'PyName': 'int', 'Type': 'Primitive'}, 'int8_t': {'CppName': 'int8_t', 'PyName': 'int', 'FlatBufferName': 'int8', 'Type': 'Primitive'}, 'uint8_t': {'CppName': 'uint8_t', 'PyName': 'int', 'FlatBufferName': 'uint8', 'Type': 'Primitive'}, 'int16_t': {'CppName': 'int16_t', 'PyName': 'int', 'FlatBufferName': 'int16', 'Type': 'Primitive'}, 'uint16_t': {'CppName': 'uint16_t', 'PyName': 'int', 'FlatBufferName': 'uint16', 'Type': 'Primitive'}, 'int32_t': {'CppName': 'int32_t', 'PyName': 'int', 'FlatBufferName': 'int32', 'Type': 'Primitive'}, 'uint32_t': {'CppName': 'uint32_t', 'PyName': 'int', 'FlatBufferName': 'uint32', 'Type': 'Primitive'}, 'int64_t': {'CppName': 'int64_t', 'PyName': 'int', 'FlatBufferName': 'int64', 'Type': 'Primitive'}, 'uint64_t': {'CppName': 'uint64_t', 'PyName': 'int', 'FlatBufferName': 'uint64', 'Type': 'Primitive'}, 'vector': {'CppName': 'std::vector', 'CppInclude': 'vector', 'PyName': 'List', 'PyInclude': 'List', 'FlatBufferName': 'List', 'Type': 'Container'}}}

# Primitives
scalar_int = Scalar(["int"])
scalar_bool = Scalar(["bool"])
scalar_float = Scalar(["float"])
scalar_double = Scalar(["double"])
scalar_string = Scalar(["string"])
scalar_size_t = Scalar(["size_t"])
scalar_int8_t = Scalar(["int8_t"])
scalar_uint8_t = Scalar(["uint8_t"])
scalar_int16_t = Scalar(["int16_t"])
scalar_uint16_t = Scalar(["uint16_t"])
scalar_int32_t = Scalar(["int32_t"])
scalar_uint32_t = Scalar(["uint32_t"])
scalar_int64_t = Scalar(["int64_t"])
scalar_uint64_t = Scalar(["uint64_t"])

# Kinds
value_kind = Scalar(["Value"])
enum_kind = Scalar(["Enum"])

# Custom types
custom_type = Scalar(["CustomType"])
enum_type = Scalar(["EnumType"])


# Containers
def create_map(key: Type, value: Type):
    return Template(["map"], [key, value])


def create_vector(value: Type):
    return Template(["vector"], [value])


def create_pair(value1: Type, value2: Type):
    return Template(["pair"], [value1, value2])


def create_set(value: Type):
    return Template(["set"], [value])


@pytest.fixture(scope="module")
def output_dir_path():
    script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
    output_dir = script_dir / "output_tmp"
    return output_dir


@pytest.fixture(autouse=True, scope="module")
def setup_output_dir(output_dir_path):
    output_dir_path.mkdir(exist_ok=True)


def create_type_attribute(name: str, type: Type, default_value: Any = None) -> OrderedDict:
    """
    Creates the attributes field of a value type containing a single attribute.
    """
    type_attribute = OrderedDict([
        ("Type", type),
        ("Doc", "Type doc")
    ])
    if default_value is not None:
        type_attribute["DefaultValue"] = default_value
    return OrderedDict([(name, type_attribute)])


def create_value_type(name: str, types: OrderedDict) -> OrderedDict:
    return OrderedDict([
        ("Name", name),
        ("Kind", value_kind),
        ("Attributes", types),
        ("Directory", "."),
        ("Include", "."),
        ("PackageNamespace", "custom_ns"),
        ("Doc", "Main type doc")
    ])


def create_base_enum_type(name: str) -> OrderedDict:
    enum_type = OrderedDict([
        ("Name", name),
        ("Kind", enum_kind),
        ("Attributes", ["VALUE_0", "VALUE_1", "VALUE_2"]),
        ("Directory", "."),
        ("Include", "."),
        ("PackageNamespace", "custom_ns"),
        ("Doc", "Main type doc")
    ])
    return enum_type


def create_project_types(value_types):
    enum_value_type = create_base_enum_type(enum_type.type_name_no_ns)
    custom_value_type = create_value_type(custom_type.type_name_no_ns,
                                          create_type_attribute('int_basic', scalar_int))

    # We add the enum and custom value types so that they can be used as attributes in
    # other test types.
    value_types = [*value_types, enum_value_type, custom_value_type]

    # Create separate project_types for each test type.
    return [{main_type["Name"]: main_type,
             enum_value_type["Name"]: enum_value_type,
             custom_value_type["Name"]: custom_value_type} for main_type in value_types]


def create_type_dicts(types: List[Union[Tuple[str, Type], Tuple[str, Type, Any]]]) -> List[Tuple]:
    return [create_type_dict(*args) for args in types]


def create_type_dict(type_name: str, value_type: Type, default_value: Any = None) -> Tuple:
    attribute_name = f"{type_name}_attr"
    return type_name, create_type_attribute(attribute_name, value_type, default_value)


def primitives_types_no_default_value_pass():
    types = [
        ('int_basic', scalar_int),
        ('bool_basic', scalar_bool),
        ('float_basic', scalar_float),
        ('double_basic', scalar_double),
        ('string_basic', scalar_string),
        ('size_t_basic', scalar_size_t),
        ('int8_t_basic', scalar_int8_t),
        ('uint8_t_basic', scalar_uint8_t),
        ('int16_t_basic', scalar_int16_t),
        ('uint16_t_basic', scalar_uint16_t),
        ('int32_t_basic', scalar_int32_t),
        ('uint32_t_basic', scalar_uint32_t),
        ('int64_t_basic', scalar_int64_t),
        ('uint64_t_basic', scalar_uint64_t)
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def primitives_types_good_default_value_pass():
    types = [
        ('int_with_good_init', scalar_int, 1),
        ('bool_with_good_init', scalar_bool, False),
        ('float_with_good_init', scalar_float, 1.123),
        ('double_with_good_init', scalar_double, 1.123),
        ('string_with_good_init', scalar_string, "test"),
        ('size_t_with_good_init', scalar_size_t, 1),
        ('int8_t_with_good_init', scalar_int8_t, 1),
        ('uint8_t_with_good_init', scalar_uint8_t, 1),
        ('int16_t_with_good_init', scalar_int16_t, 1),
        ('uint16_t_with_good_init', scalar_uint16_t, 1),
        ('int32_t_with_good_init', scalar_int32_t, 1),
        ('uint32_t_with_good_init', scalar_uint32_t, 1),
        ('int64_t_with_good_init', scalar_int64_t, 1),
        ('uint64_t_with_good_init', scalar_uint64_t, 1)
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def primitives_types_bad_init_config_error():
    types = [
        ('int_with_bool_init', scalar_int, True),
        ('int_with_float_init', scalar_int, 1.123),
        ('int_with_string_init', scalar_int, "test"),

        ('bool_with_int_init', scalar_bool, 1),
        ('bool_with_float_init', scalar_bool, 1.123),
        ('bool_with_string_init', scalar_bool, "test"),

        ('float_with_int_init', scalar_float, 1),
        ('float_with_bool_init', scalar_float, True),
        ('float_with_string_init', scalar_float, "test"),
        ('double_with_int_init', scalar_double, 1),
        ('double_with_bool_init', scalar_double, True),
        ('double_with_string_init', scalar_double, "test"),

        ('string_with_bool_init', scalar_string, True),
        ('string_with_int_init', scalar_string, 1),
        ('string_with_float_init', scalar_string, 1.123),

        ('size_t_with_bool_int', scalar_size_t, True),
        ('size_t_with_float_init', scalar_size_t, 1.123),
        ('size_t_with_string_init', scalar_size_t, "test"),
        ('int8_t_with_bool_int', scalar_int8_t, True),
        ('int8_t_with_float_init', scalar_int8_t, 1.123),
        ('int8_t_with_string_init', scalar_int8_t, "test"),
        ('uint8_t_with_bool_int', scalar_uint8_t, True),
        ('uint8_t_with_float_init', scalar_uint8_t, 1.123),
        ('uint8_t_with_string_init', scalar_uint8_t, "test"),
        ('int16_t_with_bool_int', scalar_int16_t, True),
        ('int16_t_with_float_init', scalar_int16_t, 1.123),
        ('int16_t_with_string_init', scalar_int16_t, "test"),
        ('uint16_t_with_bool_int', scalar_uint16_t, True),
        ('uint16_t_with_float_init', scalar_uint16_t, 1.123),
        ('uint16_t_with_string_init', scalar_uint16_t, "test"),
        ('int32_t_with_bool_int', scalar_int32_t, True),
        ('int32_t_with_float_init', scalar_int32_t, 1.123),
        ('int32_t_with_string_init', scalar_int32_t, "test"),
        ('uint32_t_with_bool_int', scalar_uint32_t, True),
        ('uint32_t_with_float_init', scalar_uint32_t, 1.123),
        ('uint32_t_with_string_init', scalar_uint32_t, "test"),
        ('int64_t_with_bool_int', scalar_int64_t, True),
        ('int64_t_with_float_init', scalar_int64_t, 1.123),
        ('int64_t_with_string_init', scalar_int64_t, "test"),
        ('uint64_t_with_bool_int', scalar_uint64_t, True),
        ('uint64_t_with_float_init', scalar_uint64_t, 1.123),
        ('uint64_t_with_string_init', scalar_uint64_t, "test")
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def container_types_no_default_value_pass():
    types = [
        ('vector_int', create_vector(scalar_int)),
        ('vector_bool', create_vector(scalar_bool)),
        ('vector_float', create_vector(scalar_float)),
        ('vector_string', create_vector(scalar_string)),
        ('vector_custom_type', create_vector(custom_type)),
        ('vector_enum_type', create_vector(enum_type)),

        ('set_int', create_set(scalar_int)),
        ('set_bool', create_set(scalar_bool)),
        ('set_float', create_set(scalar_float)),
        ('set_string', create_set(scalar_string)),
        ('set_custom_type', create_set(custom_type)),
        ('set_enum_type', create_set(enum_type)),

        ('map_int', create_map(scalar_string, scalar_int)),
        ('map_bool', create_map(scalar_string, scalar_bool)),
        ('map_float', create_map(scalar_string, scalar_float)),
        ('map_string', create_map(scalar_string, scalar_string)),
        ('map_custom_type', create_map(scalar_string, custom_type)),
        ('map_enum_type', create_map(scalar_string, enum_type)),

        ('pair_int', create_pair(scalar_string, scalar_int)),
        ('pair_bool', create_pair(scalar_string, scalar_bool)),
        ('pair_float', create_pair(scalar_string, scalar_float)),
        ('pair_string', create_pair(scalar_string, scalar_string)),
        ('pair_custom_type', create_pair(custom_type, custom_type)),
        ('pair_enum_type', create_pair(enum_type, enum_type)),

        ('vector_custom_enum_type', create_vector(enum_type)),
        ('set_custom_enum_type', create_set(enum_type)),
        ('map_custom_enum_type', create_map(scalar_string, enum_type)),
        ('pair_custom_enum_type', create_pair(enum_type, enum_type))
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def container_types_good_default_value_pass():
    types = [
        ('vector_int', create_vector(scalar_int), [0, 1, 2]),
        ('vector_bool', create_vector(scalar_bool), [True, False, True]),
        ('vector_float', create_vector(scalar_float), [1.123, 3.456, 7.891]),
        ('vector_string', create_vector(scalar_string), ["test1", "test2", "test3"]),

        ('set_int', create_set(scalar_int), [0, 1, 2]),
        ('set_bool', create_set(scalar_bool), [True, False, True]),
        ('set_float', create_set(scalar_float), [1.123, 3.456, 7.891]),
        ('set_string', create_set(scalar_string), ["test1", "test2", "test3"]),

        ('map_int', create_map(scalar_string, scalar_int), {"a": 1, "b": 2}),
        ('map_bool', create_map(scalar_string, scalar_bool), {"a": True, "b": False}),
        ('map_float', create_map(scalar_string, scalar_float), {"a": 1.123, "b": 3.456}),
        ('map_string', create_map(scalar_string, scalar_string), {"a": "test1", "b": "test2"}),

        ('pair_int', create_pair(scalar_int, scalar_int), [0, 1]),
        ('pair_bool', create_pair(scalar_bool, scalar_bool), [True, False]),
        ('pair_float', create_pair(scalar_float, scalar_float), [1.123, 3.456]),
        ('pair_string', create_pair(scalar_string, scalar_string), ["test1", "test2"]),

        ('pair_int_bool', create_pair(scalar_int, scalar_bool), [0, True]),
        ('pair_bool_float', create_pair(scalar_bool, scalar_float), [False, 1.123]),
        ('pair_float_string', create_pair(scalar_float, scalar_string), [1.123, "test"]),
        ('pair_string_int', create_pair(scalar_string, scalar_int), ["test", 1]),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def container_types_wrong_args_config_error():
    types = [
        ('vector_extra_args', Template(["vector"], [scalar_int, scalar_int])),
        ('map_extra_args', Template(["map"], [scalar_string, scalar_int, scalar_int])),
        ('set_extra_args', Template(["set"], [scalar_int, scalar_int])),
        ('pair_extra_args', Template(["pair"], [scalar_int, scalar_int, scalar_int])),
        ('map_less_args', Template(["map"], [scalar_string])),
        ('pair_less_args', Template(["pair"], [scalar_int])),
        ('map_non_string_key', create_map(scalar_int, scalar_string))
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def container_types_mismatched_default_value_types_config_error():
    types = [
        ('vector_int', create_vector(scalar_int), [0.12, 1, 2]),
        ('vector_bool', create_vector(scalar_bool), [False, 1]),
        ('vector_float', create_vector(scalar_float), [1.123, 3, 7.891]),
        ('vector_string', create_vector(scalar_string), ["test1", True, "test3"]),

        ('set_int', create_set(scalar_int), [0, True, 2]),
        ('set_bool', create_set(scalar_bool), [1.123, False, True]),
        ('set_float', create_set(scalar_float), ["test", 3.456, 7.891]),
        ('set_string', create_set(scalar_string), ["test1", "test2", 1.123]),

        ('map_int', create_map(scalar_string, scalar_int), {"a": 1, "b": "2"}),
        ('map_bool', create_map(scalar_string, scalar_bool), {"a": 1, "b": False}),
        ('map_float', create_map(scalar_string, scalar_float), {"a": 1, "b": 3.456}),
        ('map_string', create_map(scalar_string, scalar_string), {"a": "test1", "b": True}),

        ('pair_int', create_pair(scalar_int, scalar_int), [0.123, 1]),
        ('pair_bool', create_pair(scalar_bool, scalar_bool), ["test", False]),
        ('pair_float', create_pair(scalar_float, scalar_float), [1.123, True]),
        ('pair_string', create_pair(scalar_string, scalar_string), [1, "test2"]),

        ('pair_int_bool', create_pair(scalar_int, scalar_bool), [0, 1]),
        ('pair_bool_float', create_pair(scalar_bool, scalar_float), [1.123, 1.123]),
        ('pair_float_string', create_pair(scalar_float, scalar_string), ["test", "test"]),
        ('pair_string_int', create_pair(scalar_string, scalar_int), [0, 1]),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def container_types_wrong_number_default_value_values_config_error():
    types = [
        ('pair_int_less', create_pair(scalar_int, scalar_int), [1]),
        ('pair_int_more', create_pair(scalar_int, scalar_int), [0, 1, 2]),
        ('pair_bool_less', create_pair(scalar_bool, scalar_bool), [False]),
        ('pair_bool_more', create_pair(scalar_bool, scalar_bool), [True, False, False]),
        ('pair_float_less', create_pair(scalar_float, scalar_float), [1.123]),
        ('pair_float_more', create_pair(scalar_float, scalar_float), [0.123, 1.123, 2.456]),
        ('pair_string_less', create_pair(scalar_string, scalar_string), ["test"]),
        ('pair_string_more', create_pair(scalar_string, scalar_string), ["test1", "test2", "test3"]),

        ('pair_int_bool_less', create_pair(scalar_int, scalar_bool), [0]),
        ('pair_int_bool_more', create_pair(scalar_int, scalar_bool), [0, True, False]),
        ('pair_bool_float_less', create_pair(scalar_bool, scalar_float), [1.123]),
        ('pair_bool_float_more', create_pair(scalar_bool, scalar_float), [True, False, 1.123]),
        ('pair_float_string_less', create_pair(scalar_float, scalar_string), [1.123]),
        ('pair_float_string_more', create_pair(scalar_float, scalar_string), [1.123, "test", "test2"]),
        ('pair_string_int_less', create_pair(scalar_string, scalar_int), ["test"]),
        ('pair_string_int_more', create_pair(scalar_string, scalar_int), ["test", "test2", 1]),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def container_types_custom_values_default_value_config_error():
    types = [
        ('vector_custom_type', create_vector(custom_type), []),
        ('set_custom_type', create_set(custom_type), []),
        ('map_custom_type', create_map(scalar_string, custom_type), []),
        ('pair_custom_type', create_pair(custom_type, custom_type), []),

        ('vector_enum_type', create_vector(enum_type), []),
        ('set_enum_type', create_set(enum_type), []),
        ('map_enum_type', create_map(scalar_string, enum_type), []),
        ('pair_enum_type', create_pair(enum_type, enum_type), []),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def nested_container_types_no_default_value_pass():
    types = [
        ('vector_nested_int', create_vector(create_vector(scalar_int))),
        ('vector_nested_bool', create_vector(create_vector(scalar_bool))),
        ('vector_nested_float', create_vector(create_vector(scalar_float))),
        ('vector_nested_string', create_vector(create_vector(scalar_string))),
        ('vector_nested_custom', create_vector(create_vector(custom_type))),
        ('vector_nested_enum', create_vector(create_vector(enum_type))),

        ('set_nested_int', create_set(create_set(scalar_int))),
        ('set_nested_bool', create_set(create_set(scalar_bool))),
        ('set_nested_float', create_set(create_set(scalar_float))),
        ('set_nested_string', create_set(create_set(scalar_string))),
        ('set_nested_custom', create_set(create_set(custom_type))),
        ('set_nested_enum', create_set(create_set(enum_type))),

        ('map_nested_int', create_map(scalar_string, create_map(scalar_string, scalar_int))),
        ('map_nested_bool', create_map(scalar_string, create_map(scalar_string, scalar_bool))),
        ('map_nested_float', create_map(scalar_string, create_map(scalar_string, scalar_float))),
        ('map_nested_string', create_map(scalar_string, create_map(scalar_string, scalar_string))),
        ('map_nested_custom', create_map(scalar_string, create_map(scalar_string, custom_type))),
        ('map_nested_enum', create_map(scalar_string, create_map(scalar_string, enum_type))),

        ('pair_nested_int', create_pair(scalar_int, create_pair(scalar_int, scalar_int))),
        ('pair_nested_bool', create_pair(scalar_bool, create_pair(scalar_bool, scalar_bool))),
        ('pair_nested_float', create_pair(scalar_float, create_pair(scalar_float, scalar_float))),
        ('pair_nested_string', create_pair(scalar_string, create_pair(scalar_string, scalar_string))),
        ('pair_nested_custom', create_pair(custom_type, create_pair(custom_type, custom_type))),
        ('pair_nested_enum', create_pair(enum_type, create_pair(enum_type, enum_type))),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def nested_container_types_forbidden_default_value_types_config_error():
    types = [
        ('vector_nested_int', create_vector(create_vector(scalar_int)), []),
        ('vector_nested_bool', create_vector(create_vector(scalar_bool)), []),
        ('vector_nested_float', create_vector(create_vector(scalar_float)), []),
        ('vector_nested_string', create_vector(create_vector(scalar_string)), []),
        ('vector_nested_custom', create_vector(create_vector(custom_type)), []),
        ('vector_nested_enum', create_vector(create_vector(enum_type)), []),

        ('set_nested_int', create_set(create_set(scalar_int)), []),
        ('set_nested_bool', create_set(create_set(scalar_bool)), []),
        ('set_nested_float', create_set(create_set(scalar_float)), []),
        ('set_nested_string', create_set(create_set(scalar_string)), []),
        ('set_nested_custom', create_set(create_set(custom_type)), []),
        ('set_nested_enum', create_set(create_set(enum_type)), []),

        ('map_nested_int', create_map(scalar_string, create_map(scalar_string, scalar_int)), []),
        ('map_nested_bool', create_map(scalar_string, create_map(scalar_string, scalar_bool)), []),
        ('map_nested_float', create_map(scalar_string, create_map(scalar_string, scalar_float)), []),
        ('map_nested_string', create_map(scalar_string, create_map(scalar_string, scalar_string)), []),
        ('map_nested_custom', create_map(scalar_string, create_map(scalar_string, custom_type)), []),
        ('map_nested_enum', create_map(scalar_string, create_map(scalar_string, enum_type)), []),

        ('pair_nested_int', create_pair(scalar_int, create_pair(scalar_int, scalar_int)), []),
        ('pair_nested_bool', create_pair(scalar_bool, create_pair(scalar_bool, scalar_bool)), []),
        ('pair_nested_float', create_pair(scalar_float, create_pair(scalar_float, scalar_float)), []),
        ('pair_nested_string', create_pair(scalar_string, create_pair(scalar_string, scalar_string)), []),
        ('pair_nested_custom', create_pair(custom_type, create_pair(custom_type, custom_type)), []),
        ('pair_nested_enum', create_pair(enum_type, create_pair(enum_type, enum_type)), []),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def custom_types_no_default_value_pass():
    types = [
        ('custom_basic', custom_type),
        ('enum_basic', enum_type),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def enum_type_default_value_pass():
    types = [
        ('enum_good_init', enum_type, "VALUE_1"),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def custom_types_forbidden_default_value_types_config_error():
    types = [
        ('custom_list_init', custom_type, []),
        ('custom_int_init', custom_type, 123),
        ('custom_string_init', custom_type, "bad_init_value"),
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


def enum_types_bad_init_config_error():
    types = [
        ('enum_list_init', enum_type, []),
        ('enum_int_init', enum_type, 123)
    ]
    type_attributes = create_type_dicts(types)
    value_types = [create_value_type(name, type_attribute)
                   for name, type_attribute in type_attributes]
    return create_project_types(value_types)


project_types_list_pass = [
    primitives_types_no_default_value_pass,
    primitives_types_good_default_value_pass,
    container_types_no_default_value_pass,
    container_types_good_default_value_pass,
    nested_container_types_no_default_value_pass,
    custom_types_no_default_value_pass,
    enum_type_default_value_pass
]


@pytest.mark.parametrize('project_types_list', project_types_list_pass,
                         ids=[i.__name__ for i in project_types_list_pass])
def test_types_pass(output_dir_path, project_types_list):
    for project_types in project_types_list():
        project_names = list(project_types.keys())
        for type_name, indiv_class in project_types.items():
            # We include the enum and custom types in the project types so they can be
            # used as attributes. However, we don't test them here.
            if type_name not in [enum_type.type_name_no_ns,
                                 custom_type.type_name_no_ns]:
                types_data = TypesData(
                    indiv_class,
                    project_types,
                    system_types,
                    project_names,
                    {},
                    [])
                write_individual_header_file(output_dir_path, types_data)
                write_python_type_file(output_dir_path, types_data)


project_types_list_errors = [
    primitives_types_bad_init_config_error,
    container_types_wrong_args_config_error,
    container_types_mismatched_default_value_types_config_error,
    container_types_wrong_number_default_value_values_config_error,
    container_types_custom_values_default_value_config_error,
    nested_container_types_forbidden_default_value_types_config_error,
    custom_types_forbidden_default_value_types_config_error,
    enum_types_bad_init_config_error
]


@pytest.mark.parametrize('project_types_list', project_types_list_errors,
                         ids=[i.__name__ for i in project_types_list_errors])
def test_types_config_error(output_dir_path, project_types_list):
    for project_types in project_types_list():
        project_names = list(project_types.keys())
        for type_name, indiv_class in project_types.items():
            # We include the enum and custom types in the project types so they can be
            # used as attributes. However, we don't test them here.
            if type_name not in [enum_type.type_name_no_ns, custom_type.type_name_no_ns]:
                types_data = TypesData(
                    indiv_class,
                    project_types,
                    system_types,
                    project_names,
                    {},
                    [])
                with pytest.raises(ConfigurationError):
                    write_individual_header_file(output_dir_path, types_data)

                with pytest.raises(ConfigurationError):
                    write_python_type_file(output_dir_path, types_data)
