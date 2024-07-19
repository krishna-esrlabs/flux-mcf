"""
Python type generator backend

Copyright (c) 2024 Accenture
"""
from type_generator.common import Template, Type, TypeNameParser, TypesData, Scalar
from type_generator.common import python_type_from_system_type, is_enum_type, is_primitive_type
from type_generator.common import is_container_type, assert_types_validity, ConfigurationError

from typing import TextIO, Set, TYPE_CHECKING
import os

if TYPE_CHECKING:
    from pathlib import Path


def gen_pair_test(file: TextIO, idx: int, parsed_type: Type, types_data: 'TypesData') -> None:
    assert len(parsed_type.args) == 2

    first_value_type = parsed_type.args[0]
    second_value_type = parsed_type.args[1]

    if (not is_primitive_type(first_value_type.type_name, types_data.system_types)
            or not is_primitive_type(second_value_type.type_name, types_data.system_types)):
        idx += 1  # to keep variable name unique

    file.write("(")
    recursive_gen_test(file, idx, first_value_type, types_data)
    file.write(", ")
    recursive_gen_test(file, idx, second_value_type, types_data)
    file.write(")")


def gen_dict_test(file: TextIO, idx: int, parsed_type: Type, types_data: 'TypesData') -> None:
    assert len(parsed_type.args) == 2

    key_type = parsed_type.args[0]
    value_type = parsed_type.args[1]

    if (not is_primitive_type(key_type.type_name, types_data.system_types)
            or not is_primitive_type(value_type.type_name, types_data.system_types)):
        idx += 1  # to keep variable name unique

    file.write("{")
    recursive_gen_test(file, idx, key_type, types_data)
    file.write(": ")
    recursive_gen_test(file, idx, value_type, types_data)
    file.write("}")


def gen_vector_test(file: TextIO, idx: int, parsed_type: Type, types_data: 'TypesData') -> None:
    assert len(parsed_type.args) == 1

    container_type = parsed_type
    inner_type = parsed_type.args[0]

    # if inner type is a primitive we can stop here
    if is_primitive_type(inner_type.type_name, types_data.system_types):
        file.write("[")
        recursive_gen_test(file, idx, inner_type, types_data)

        # Sets cannot contained multiple copies of same value so we don't iterate
        if container_type.type_name_no_ns == "set":
            file.write("]")
        else:
            file.write(" for el_" + str(idx) + " in range(test_veclength)]")
        return

    if (container_type.type_name_no_ns != "vector"
            and container_type.type_name_no_ns != "list"
            and container_type.type_name_no_ns != "set"):
        print(f"ERROR: {parsed_type} defined in {types_data.current_type['Name']}"
              f" container {container_type} is not defined in system_types "
              f"file. Unable to recursively define unpack in python_type_generator.py")
        exit(-1)

    idx += 1  # to keep 'el' variable name unique
    file.write("[")
    recursive_gen_test(file, idx, inner_type, types_data)

    # Sets cannot contained multiple copies of same value so we don't iterate
    if container_type.type_name_no_ns == "set":
        file.write("]")
    else:
        file.write(" for el_" + str(idx) + " in range(test_veclength)]")
    return


def recursive_gen_test(file: TextIO, idx: int, parsed_type: Type, types_data: 'TypesData') -> None:

    gen_test_type_fn_call = "gen_test_type(test_float, test_str, test_int, test_bool, test_bytearray, test_IntEnum, test_veclength)"
    type_name = parsed_type.type_name_str

    python_name_with_namespace = python_type_from_system_type(parsed_type.type_name, types_data.system_types)

    class_type = types_data.current_type["Name"]
    if type_name in types_data.project_names:

        if types_data.project_types[type_name]["Kind"].type_name_no_ns == "Enum":
            file.write(python_name_with_namespace + "(test_IntEnum)")
        else:
            if not class_type in type_name:
                file.write(python_name_with_namespace + "." + gen_test_type_fn_call)
            else:
                file.write(python_name_with_namespace + "()")

    # Add linked types
    elif type_name in types_data.linked_names:

        # Enums do not have a deserialize method
        if types_data.linked_types[type_name]["Kind"].type_name_no_ns == "Enum":
            file.write(python_name_with_namespace + "(test_IntEnum)")
        else:
            if not class_type in type_name:
                file.write(python_name_with_namespace + "." + gen_test_type_fn_call)
            else:
                file.write(python_name_with_namespace + "()")

    elif type_name in types_data.system_types["SystemTypes"] and types_data.system_types["SystemTypes"][type_name]["Type"] == "Primitive":
        python_type = types_data.system_types["SystemTypes"][type_name]["PyName"]
        if python_type == "float":
            file.write("test_float")
        elif python_type == "str":
            file.write("test_str")
        elif python_type == "int":
            file.write("test_int")
        elif python_type == "bool":
            file.write("test_bool")

    elif type_name in types_data.system_types["SystemTypes"] and types_data.system_types["SystemTypes"][type_name]["Type"] == "Container":
        python_type = types_data.system_types["SystemTypes"][type_name]["PyName"]
        if python_type == "List":
            gen_vector_test(file, idx, parsed_type, types_data)
        elif python_type == "Dict":
            gen_dict_test(file, idx, parsed_type, types_data)
        elif python_type == "Tuple":
            gen_pair_test(file, idx, parsed_type, types_data)


def add_gen_test_type(file: TextIO, types_data: 'TypesData') -> None:
    gen_test_type_fn_dec = "gen_test_type(test_float: float, test_str: str, test_int: int, test_bool: bool, test_bytearray: bytearray, test_IntEnum: IntEnum, test_veclength: int)"
    file.write("    @staticmethod\n")
    file.write("    def " + gen_test_type_fn_dec + " -> \"" + types_data.current_type["Name"] + "\":\n\n")

    file.write(" " * 8 + "return " + types_data.current_type["Name"] + "(\n")
    for key, value in types_data.current_type["Attributes"].items():

        file.write(" " * 12 + key + "=")

        parsed_type = value["Type"]
        recursive_gen_test(file, 0, parsed_type, types_data)

        file.write(",\n")

    parsed_kind = types_data.current_type["Kind"].type_name_no_ns
    if parsed_kind == "ExtMemValue":
        file.write(" " * 12 + "data=test_bytearray\n")

    file.write(" " * 8 + ")\n\n")


def add_serialize(file: TextIO, types_data: 'TypesData') -> None:
    file.write("    def serialize(self) -> Sequence:\n")
    file.write(" " * 8 + "return [[\n")

    current_type = types_data.current_type
    for key, value in current_type["Attributes"].items():

        file.write(" " * 12)

        parsed_type = value["Type"]

        # Message pack treats vector<uint8_t> and vector<char> as binary so special
        # serialize required.
        if (parsed_type == TypeNameParser.parse("vector<uint8_t>")
                or parsed_type == TypeNameParser.parse("vector<char>")):
            file.write("bytearray(")

        file.write("self." + key)

        # Closing bracket for bytearray, yes I know it's ugly...
        if parsed_type == TypeNameParser.parse("vector<uint8_t>"):
            file.write(")")

        # Nested serialization. Split template types and Strip out namespace
        parsed_type_name = parsed_type.type_name_no_ns
        if parsed_type_name in types_data.project_names:
            # enums don't have a serialize()
            if types_data.project_types[parsed_type_name]["Kind"].type_name_no_ns == "Enum":
                file.write(".value")
            else:
                file.write(".serialize()[0]")

        file.write(",\n")

    file.write(" " * 12 + "],\n")

    # add type information
    file.write(f"{' ' * 12}\"{current_type[f'PackageNamespace']}::{current_type['Directory']}::{current_type['Name']}\",\n")

    # add extra data member if extmemvalue
    kind = current_type["Kind"].type_name_no_ns
    if kind == "ExtMemValue":
        file.write(" " * 12 + "self.data\n")
    file.write(" " * 8 + "]\n\n")


def add_deserialize(file: TextIO, current_type: dict) -> None:
    file.write("    @staticmethod\n")
    file.write("    def deserialize(array: Sequence) -> \"" + current_type["Name"] + "\":\n\n")
    file.write(" " * 8 + "if array is None:\n")
    file.write(" " * 12 + "raise ValueError(\"Received 'None' when trying to deserialize " + current_type["Name"] + " object\")\n\n")
    file.write(" " * 8 + "if len(array) < 2:\n")
    file.write(" " * 12 + "raise ValueError(\"Received value of incompatible length when trying to deserialize " + current_type["Name"] + " object\")\n\n")
    file.write(f"{' ' * 8}if array[1] != \"{current_type[f'PackageNamespace']}::{current_type['Directory']}::{current_type['Name']}\":\n")
    file.write(" " * 12 + "raise ValueError(\"Received value_type: \" + str(array[1]) + \", when trying to deserialize " + current_type["Name"] + " object\")\n\n")

    # add extra data member if extmemvalue
    file.write(" " * 8 + "_" + current_type["Name"].lower() + " = " + current_type["Name"] + "._" + current_type["Name"] + "__unpack(array[0])\n")
    parsed_kind = current_type["Kind"].type_name_no_ns
    if parsed_kind == "ExtMemValue":
        file.write(" " * 8 + "_" + current_type["Name"].lower() + ".data = array[2]\n")

    file.write(" " * 8 + "return _" + current_type["Name"].lower())
    file.write("\n\n")


def unpack_pair(file: TextIO, idx: int, unpack_name: str, parsed_type: Type, types_data: 'TypesData'):
    assert len(parsed_type.args) == 2

    first_value_type = parsed_type.args[0]
    second_value_type = parsed_type.args[1]

    # if pair types are primitives we can stop here
    if is_primitive_type(first_value_type.type_name, types_data.system_types) and is_primitive_type(second_value_type.type_name, types_data.system_types):
        file.write("(" + unpack_name + "[0], " + unpack_name + "[1])")

    # process case where first value is primitive
    elif is_primitive_type(first_value_type.type_name, types_data.system_types):
        idx += 1  # to keep variable name unique
        file.write("(" + unpack_name + "[0], ")
        recursive_unpack(file, idx, unpack_name + "[1]", second_value_type, types_data)
        file.write(")")

    # process case where second value is primitive
    elif is_primitive_type(second_value_type.type_name, types_data.system_types):
        idx += 1  # to keep variable name unique
        file.write("(")
        recursive_unpack(file, idx, unpack_name + "[0]", first_value_type, types_data)
        file.write(", " + unpack_name + "[1])")

    # process case where both are complex types
    else:
        idx += 1  # to keep variable name unique
        file.write("(")
        recursive_unpack(file, idx, unpack_name + "[0]", first_value_type, types_data)
        file.write(", ")
        recursive_unpack(file, idx, unpack_name + "[1]", second_value_type, types_data)
        file.write(")")


def unpack_map(file: TextIO, idx: int, unpack_name: str, parsed_type: Type, types_data: 'TypesData'):
    assert len(parsed_type.args) == 2
    key_type = parsed_type.args[0]
    value_type = parsed_type.args[1]

    # if mapped types are primitives we can stop here
    if is_primitive_type(key_type.type_name, types_data.system_types) and is_primitive_type(value_type.type_name, types_data.system_types):
        file.write(unpack_name)

    # process case where keys are primitives
    elif is_primitive_type(key_type.type_name, types_data.system_types):
        idx += 1  # to keep variable name unique
        file.write("{key_" + str(idx) + ": ")
        recursive_unpack(file, idx, "val_" + str(idx), value_type, types_data)
        file.write(" for key_" + str(idx) + ", val_" + str(idx) + " in " + unpack_name + ".items()}")

    # process case where values are primitives
    elif is_primitive_type(value_type.type_name, types_data.system_types):
        idx += 1  # to keep variable name unique
        # cleaned_type = value_type[4:]  # remove 'map<' from input value type
        file.write("{")
        recursive_unpack(file, idx, "key_" + str(idx), key_type, types_data)
        file.write(": val_" + str(idx) + " for key_" + str(idx) + ", val_" + str(idx) + " in " + unpack_name + ".items()}")

    # process case where both are complex types
    else:
        idx += 1  # to keep variable name unique
        file.write("{")
        recursive_unpack(file, idx, "key_" + str(idx), key_type, types_data)
        file.write(": ")
        recursive_unpack(file, idx, "val_" + str(idx), value_type, types_data)
        file.write(" for key_" + str(idx) + ", val_" + str(idx) + " in " + unpack_name + ".items()}")


def unpack_vector(file: TextIO, idx: int, unpack_name: str, parsed_type: Template, types_data: 'TypesData'):
    assert len(parsed_type.args) == 1

    container_type = parsed_type
    inner_type = parsed_type.args[0]

    # if inner type is a primitive we can stop here
    if is_primitive_type(inner_type.type_name, types_data.system_types):
        file.write(unpack_name)
        return

    if (container_type.type_name_no_ns != "vector"
            and container_type.type_name_no_ns != "list"
            and container_type.type_name_no_ns != "set"):
        print("ERROR: " + parsed_type + " defined in " + types_data.current_type["Name"]
              + " container " + container_type.type_name_no_ns
              + " is not defined in AllowedTypes.json. Unable to recursively define "
                "unpack in python_type_generator.py")
        exit(-1)

    idx += 1  # to keep 'el' variable name unique
    file.write("[")
    recursive_unpack(file, idx, "el_" + str(idx), inner_type, types_data)
    file.write(" for el_" + str(idx) + " in " + unpack_name + "]")


def recursive_unpack(file: TextIO, idx: int, unpack_name: str, parsed_type: Type, types_data: 'TypesData'):
    # Message pack treats vector<uint8_t> and vector<char> as binary so special unpacking
    # required.
    if (parsed_type == TypeNameParser.parse("vector<uint8_t>")
            or parsed_type == TypeNameParser.parse("vector<char>")):
        file.write("list(" + unpack_name + ")")
        return

    type_name = parsed_type.type_name_str
    type_name_no_ns = parsed_type.type_name_no_ns

    python_name_with_namespace = python_type_from_system_type(parsed_type.type_name, types_data.system_types)

    # Add project types
    if type_name in types_data.project_names:

        # Enums do not have a deserialize method
        if types_data.project_types[type_name]["Kind"].type_name_no_ns == "Enum":
            file.write(python_name_with_namespace + "(" + unpack_name + ")")
        else:
            file.write(python_name_with_namespace + "._" + type_name_no_ns + "__unpack(" + unpack_name + ")")

    # Add linked types
    elif type_name in types_data.linked_names:

        # Enums do not have a deserialize method
        if types_data.linked_types[type_name]["Kind"].type_name_no_ns == "Enum":
            file.write(python_name_with_namespace + "(" + unpack_name + ")")
        else:
            file.write(python_name_with_namespace + "._" + type_name_no_ns + "__unpack(" + unpack_name + ")")

    # # C++ primitive types
    elif type_name in types_data.system_types["SystemTypes"] and types_data.system_types["SystemTypes"][type_name]["Type"] == "Primitive":
        file.write(unpack_name)

    # # C++ containers
    elif type_name in types_data.system_types["SystemTypes"] and types_data.system_types["SystemTypes"][type_name]["Type"] == "Container":
        if type_name == "vector" or type_name == "list" or type_name == "set":
            unpack_vector(file, idx, unpack_name, parsed_type, types_data)
        elif type_name == "map":
            unpack_map(file, idx, unpack_name, parsed_type, types_data)
        elif type_name == "pair":
            unpack_pair(file, idx, unpack_name, parsed_type, types_data)
        else:
            print("ERROR: " + type_name + " defined in " + types_data.current_type["Name"] + " type is not defined in AllowedTypes.json or in a project json file. Unable to recursively define unpack in python_type_generator.py")

    # Return error if type is not found
    else:
        print("ERROR: " + type_name + " defined in " + types_data.current_type["Name"] + " is not defined in AllowedTypes.json or in a project json file. Unable to recursively define unpack in python_type_generator.py")
        exit(-1)


def add_unpack(file: TextIO, types_data: 'TypesData') -> None:
    file.write("    @staticmethod\n")
    file.write("    def __unpack(array: Sequence) -> \"" + types_data.current_type["Name"] + "\":\n\n")
    file.write(" " * 8 + "return " + types_data.current_type["Name"] + "(\n")

    for idx, (key, value) in enumerate(types_data.current_type["Attributes"].items()):

        file.write(" " * 12 + key + "=")
        unpack_name = "array[" + str(idx) + "]"

        recursive_unpack(file, 0, unpack_name, value["Type"], types_data)
        file.write(",\n")

    file.write(" " * 8 + ")\n\n")


def add_repr(file: TextIO, current_type: dict) -> None:
    file.write("\n")
    file.write("    def __repr__(self):\n")

    file.write(" " * 8 + "ret_repr = " + "\"" + current_type["Name"] + "[")
    file.write("={}, ".join(current_type["Attributes"]))

    # add extra data member if extmemvalue
    kind = current_type["Kind"].type_name_no_ns
    if kind == "ExtMemValue":
        file.write("={}, data")

    file.write("={}]\"")
    file.write(".format(self.")
    file.write(", self.".join(current_type["Attributes"]))
    if kind == "ExtMemValue":
        file.write(", self.data")
    file.write(")\n")
    file.write(" " * 8 + "return ret_repr\n\n")


def get_python_init_value(parsed_type: Type, types_data: 'TypesData'):
    def recurse(current_type: Type):
        stripped_type = current_type.type_name_str
        python_type = python_type_from_system_type(current_type.type_name, types_data.system_types)

        is_project_type = (current_type.type_name_str in types_data.project_names
                           and types_data.project_types[stripped_type]["Kind"].type_name_no_ns == "Enum")
        is_linked_type = (current_type.type_name_str in types_data.linked_names
                          and types_data.linked_types[stripped_type]["Kind"].type_name_no_ns == "Enum")
        if is_project_type or is_linked_type:
            return python_type + "(0)"

        # List[...]() or Dict[...]() are not accepted constructors
        if "List" == python_type:
            return "[]"
        elif "Dict" == python_type:
            return "{}"
        elif "Tuple" == python_type:
            output_str = f"({', '.join(map(recurse, current_type.args))})"
            return output_str
        else:
            return python_type + "()"

    return recurse(parsed_type)


def add_init_optional_args(file: TextIO, types_data: 'TypesData') -> None:
    """
    Add optional arguments. The following types can have customised default values:
        - Primitives
        - Enums
        - Immutable containers of primitives.
    Otherwise, the default value is None.

    Note. Some mutable containers can have default values, however, they are set in the
    body of the constructor, as mutable default arguments should be avoided in python.
    """
    num_variables = len(types_data.current_type["Attributes"])
    group_name = types_data.current_type["Name"]
    kind = types_data.current_type["Kind"].type_name_no_ns
    for index, (key, value) in enumerate(types_data.current_type["Attributes"].items()):
        file.write(" " * 17 + key + ": ")

        python_type = value['Type'].as_python_type(types_data.system_types)
        file.write(python_type)

        default_value_string = " = None"
        if "DefaultValue" in value:
            stripped_type = value["Type"].type_name_no_ns

            if is_enum_type(value["Type"], types_data):
                stripped_init = str(value["DefaultValue"]).split("::")[-1]
                default_value_string = f" = {python_type}.{stripped_init}"
            elif is_primitive_type(stripped_type, types_data.system_types):
                if stripped_type == "string":
                    default_value_string = f" = \"{value['DefaultValue']}\""
                else:
                    default_value_string = f" = {value['DefaultValue']}"
            elif is_container_type(stripped_type, types_data.system_types):
                if stripped_type == 'pair':
                    if not len(value["DefaultValue"]) == 2:
                        raise ConfigurationError(
                            f"Error in {group_name}::{key}. \"DefaultValue\" of a "
                            f"pair should have exactly 2 values.")
                    default_tuple_string = str(value["DefaultValue"]).replace("[", "(").replace("]", ")")
                    default_value_string = f" = {default_tuple_string}"
        file.write(default_value_string)

        if index != num_variables - 1:
            file.write(",\n")
        else:
            # Add extra data member if extmemvalue
            if kind == "ExtMemValue":
                file.write(",\n" + " " * 17 + "data: bytearray = None):\n")
            else:
                file.write("):\n")


def add_instance_member_inits(file: TextIO, types_data: 'TypesData') -> None:
    """
        Initialise values using custom value from the "DefaultValue" json field or type
        specific defaults. The following types can custom values:
            - Mutable containers of primitives.

        Note. Primitives, enums and immutable containers can have custom values, however,
        they are set using constructor optional arguments.
        """
    allowed_mutable_containers = ['map', 'set', 'vector']
    for key, value in types_data.current_type["Attributes"].items():
        stripped_type = value["Type"].type_name_no_ns
        python_type = value["Type"].as_python_type(types_data.system_types)
        file.write(" " * 8 + "super().__init__()\n")
        if "DefaultValue" in value and stripped_type in allowed_mutable_containers:

            file.write(" " * 8 + "if " + key + " is None:\n")

            if stripped_type == "map":
                default_value = dict(value["DefaultValue"])
            else:
                default_value = value["DefaultValue"]
            file.write(f"{' ' * 12}self.{key}: {python_type} = {default_value}")

            if value["Doc"]:
                file.write("  # " + value["Doc"].replace("//", "#"))
            file.write("\n")
            file.write(f"{' ' * 8}else:\n")
            file.write(f"{' ' * 12}self.{key}: {python_type} = {key}\n")
        else:
            # add default constructors. Otherwise it stays at None. Which causes issues for mcf.
            python_str = get_python_init_value(value["Type"], types_data)

            file.write(f"{' ' * 8}self.{key}: {python_type} = {key} if {key} is not None else {python_str}")
            if value["Doc"]:
                file.write(f"  # {value['Doc'].replace('//', '#')}")
            file.write("\n")


def add_init(file: TextIO, types_data: 'TypesData') -> None:
    # add doc string for the class
    file.write(" " * 4 + '""" ' + types_data.current_type["Doc"] + ' """\n\n')
    kind = types_data.current_type["Kind"].type_name_no_ns
    slots = list(types_data.current_type["Attributes"].keys())
    if kind == "ExtMemValue":  # ExtMemValues have an additional attribute
        slots.append("data")

    slots_string = "    __slots__ = ({},)\n\n".format(
        ", ".join(map("\"{}\"".format, slots))
    )
    file.write(slots_string)
    file.write("    def __init__(self,\n")

    add_init_optional_args(file, types_data)
    add_instance_member_inits(file, types_data)
    if kind == "ExtMemValue":
        file.write(" " * 8 + "self.data = data\n")


def add_enum(file: TextIO, current_type: dict) -> None:

    file.write("class " + current_type["Name"] + "(IntEnum):\n")

    for (idx, attrib) in enumerate(current_type["Attributes"]):
        file.write("    " + attrib + " = " + str(idx) + "\n")


def add_imports(file: TextIO, types_data: 'TypesData') -> None:
    file.write("from __future__ import annotations\n")
    file.write("from typing import Sequence\n")
    file.write("from enum import IntEnum\n")
    file.write("from mcf import Value\n")

    class_type_name = types_data.current_type["Name"]

    system_imports = set()
    project_imports = set()
    linked_imports = set()
    for value in types_data.current_type["Attributes"].values():
        type_list = value["Type"].as_generic_type_list()

        for t in type_list:
            type_name = "::".join(t)
            type_name_no_ns = t[-1]

            if type_name not in set.union({"enum"}, types_data.system_types["SystemTypes"], types_data.project_names, types_data.linked_names):
                print("python")
                raise KeyError("{} is not defined in AllowedTypes.json or in a project json file. Unable to add import in python_type_generator.py".format(type_name))

            # do not recursively import the same type
            if type_name == class_type_name:
                continue

            # add imports for project-types. e.g. for `Distr` that comes from `base::Distr`.
            import_pattern = "import value_types.{project_ns}.{dir}.{type}"
            if type_name in types_data.project_names:
                project_imports.add(
                    import_pattern.format(
                        project_ns=types_data.project_types[type_name]["PackageNamespace"],
                        dir=types_data.project_types[type_name]["Directory"],
                        type=type_name_no_ns)
                )

            # add imports for linked project types
            if type_name in types_data.linked_names:
                linked_imports.add(
                    import_pattern.format(
                        project_ns=types_data.linked_types[type_name]["PackageNamespace"],
                        dir=types_data.linked_types[type_name]["Directory"],
                        type=type_name_no_ns)
                )

            # add includes for cpp-system-types if needed, e.g. <vector>
            if type_name in types_data.system_types["SystemTypes"]:
                if "PyInclude" in types_data.system_types["SystemTypes"][type_name]:
                    system_imports.add(f"from typing import {types_data.system_types['SystemTypes'][type_name]['PyInclude']}")

    def generate_import_string(imports: Set) -> str:
        return "".join([f"{el}\n" for el in sorted(imports)])

    if len(system_imports) > 0:
        file.write(generate_import_string(system_imports))

    if len(linked_imports) > 0:
        file.write(generate_import_string(linked_imports))

    if len(project_imports) > 0:
        file.write(generate_import_string(project_imports))

    file.write("\n\n")


def add_class(file: TextIO, types_data: 'TypesData') -> None:
    if types_data.current_type["Kind"].type_name_no_ns == "Enum":
        file.write("from enum import IntEnum\n")
        add_enum(file, types_data.current_type)
    else:
        add_imports(file, types_data)
        file.write("class " + types_data.current_type["Name"] + "(Value):\n")
        add_init(file, types_data)
        add_repr(file, types_data.current_type)
        add_deserialize(file, types_data.current_type)
        add_unpack(file, types_data)
        add_serialize(file, types_data)
        add_gen_test_type(file, types_data)


def write_python_type_file(py_dir: 'Path', types_data: 'TypesData') -> None:
    assert_types_validity(types_data)
    current_type = types_data.current_type
    filename = py_dir / f"{types_data.current_type['Name']}.py"
    execution_dir = os.getcwd()
    with open(filename, "w") as output_file:
        # data is a protected keyword for extmemvalues
        kind = current_type["Kind"].type_name_no_ns
        if kind == "ExtMemValue" and "data" in current_type["Attributes"]:
            raise ConfigurationError(
                f"ERROR: \"data\" cannot be used as an attribute name in an ExtMemValue:"
                f" {current_type['Name']}.json")

        output_file.write("# WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/value_type_generator.py.\n")
        output_file.write("# Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write(f"# To make changes either edit mcf_tools/types_generator/type_generator/python_type_generator.py or disable the generation in: \n"
                          f"# {execution_dir}\n\n\n")

        add_class(output_file, types_data)
