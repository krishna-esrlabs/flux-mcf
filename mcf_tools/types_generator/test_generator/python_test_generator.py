""""
Copyright (c) 2024 Accenture
"""
import os
from typing import TextIO, TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


def add_main(file: TextIO, class_name: str) -> None:
    file.write("if __name__ == \"__main__\":\n")
    file.write("    connection = Connection(\"127.0.0.1\", port=6666)\n")
    file.write("    remote = connection.rc\n\n")
    file.write("    Run" + class_name + "Test(remote)\n\n")


def add_set_value_tests(file: TextIO, indiv_types: list) -> None:

    file.write("    # ------------------------------------------------------------------\n")
    file.write("    # Set value tests\n")
    file.write("    # ------------------------------------------------------------------\n\n")

    file.write("    with open(test_values_file) as json_file:\n")
    file.write(" " * 8 + "test_json = json.load(json_file)\n")
    file.write(" " * 8 + "test_values = test_json[\"TestValues\"]\n\n")

    file.write("    for test_value in test_values:\n")

    for type_name in sorted(indiv_types):
        type_name_no_ns = type_name.split("::")[-1]
        file.write(" " * 8 + "# " + type_name_no_ns + " sending and receiving test\n")
        sent_value_str = "sent_value_" + type_name_no_ns
        file.write(" " * 8 + sent_value_str + " = " + type_name_no_ns + ".gen_test_type(test_value[\"test_float\"], test_value[\"test_str\"], test_value[\"test_int\"], test_value[\"test_bool\"], bytearray(test_value[\"test_bytearray\"]), test_value[\"test_IntEnum\"], test_value[\"test_veclength\"])\n")
        file.write(" " * 8 + "assert test_type_equality(" + sent_value_str + ", sender_" + type_name_no_ns + ", receiver_" + type_name_no_ns + ")\n")

        file.write("\n")

    file.write("\n")


def add_default_init_tests(file: TextIO, indiv_types: list, project_types: dict) -> None:

    file.write("    # ------------------------------------------------------------------\n")
    file.write("    # Default initialisation tests\n")
    file.write("    # ------------------------------------------------------------------\n\n")

    for type_name in sorted(indiv_types):
        kind = project_types[type_name]["Kind"].type_name_no_ns
        type_name_no_ns = type_name.split("::")[-1]

        file.write("    # " + type_name_no_ns + " sending and receiving test\n")
        sent_value_str = "sent_value_" + type_name_no_ns
        file.write("    " + sent_value_str + " = " + type_name_no_ns + "()\n")

        if kind == "ExtMemValue":
            file.write("    # ExtMemValues require data field to have entry otherwise main application returns error\n")
            file.write("    " + sent_value_str + ".data = bytearray([0, 1, 2, 3, 4, 5, 6, 7])\n")
        file.write("    assert test_type_equality(" + sent_value_str + ", sender_" + type_name_no_ns + ", receiver_" + type_name_no_ns + ")\n")

        file.write("\n")

    file.write("\n")


def add_accessors(file: TextIO, indiv_types: list, type_namespace: str, project_namespace: str) -> None:

    file.write("    # Test accessors\n")
    for type_name in sorted(indiv_types):
        type_name_no_ns = type_name.split("::")[-1]
        file.write(f"    sender_{type_name_no_ns} = remote.create_value_accessor({type_name_no_ns}, \"/{type_namespace}/{type_name_no_ns.lower()}_pysend\", ValueAccessorDirection.SENDER)\n")
        file.write(f"    receiver_{type_name_no_ns} = remote.create_value_accessor({type_name_no_ns}, \"/{type_namespace}/{type_name_no_ns.lower()}_cppsend\", ValueAccessorDirection.RECEIVER)\n")
    file.write("\n")


def add_set_queues(file: TextIO, indiv_types: list, type_namespace: str) -> None:
    file.write("    # Set blocking queues so that no values are lost.\n")
    for type_name in sorted(indiv_types):
        type_name_no_ns = type_name.split("::")[-1]
        file.write(f"    remote.set_queue(\"/{type_namespace}/{type_name_no_ns.lower()}_cppsend\", 1, True)\n")
    file.write("\n")


def add_test_function(file: TextIO, class_name: str) -> None:

    file.write("def Run" + class_name + "Test(remote: RemoteControl, test_values_file: str) -> None:\n\n")


def add_value_type_imports(file: TextIO, indiv_types: list, type_namespace: str, project_namespace: str) -> None:

    file.write("sys.path.append(os.path.join(os.path.dirname(__file__), '../../../', 'python'))\n\n")

    for type_name in sorted(indiv_types):
        type_name_no_ns = type_name.split("::")[-1]
        type_path = type_name.replace("::", ".")
        file.write(f"from value_types.{type_path} import {type_name_no_ns}\n")
    file.write("\n\n")


def add_imports(file: TextIO) -> None:

    file.write("import json\n")
    file.write("import os\n")
    file.write("import sys\n\n")

    file.write("from mcf import Connection\n")
    file.write("from mcf import RemoteControl\n")
    file.write("from mcf import ValueAccessorDirection\n")
    file.write("from utils.TestTypeEquality import test_type_equality\n\n")


def remove_enum_and_struct_types(indiv_types: list, project_types: dict) -> list:
    types_filtered = [type_ for type_ in indiv_types 
                      if project_types[type_]["Kind"].type_name_no_ns not in ["Struct", "Enum"]]
    return types_filtered


def write_python_test_file(filename: 'Path', indiv_types: list, project_types: dict, system_types: dict, class_name: str, project_namespace: str) -> None:
    execution_dir = os.getcwd()
    with open(filename, "w") as output_file:
        output_file.write("# WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/tester_generator.py.\n")
        output_file.write("# Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write("# To make changes either edit mcf_tools/types_generator/test_generator/python_test_generator.py or disable the generation in: \n"
                          f"# {execution_dir}\n\n\n")

        project_namespace = project_types[indiv_types[0]]["PackageNamespace"]
        type_namespace = project_types[indiv_types[0]]["Directory"]
        indiv_types = remove_enum_and_struct_types(indiv_types, project_types)

        add_imports(output_file)
        add_value_type_imports(output_file, indiv_types, type_namespace, project_namespace)
        add_test_function(output_file, class_name)
        add_accessors(output_file, indiv_types, type_namespace, project_namespace)
        add_set_queues(output_file, indiv_types, type_namespace)
        add_default_init_tests(output_file, indiv_types, project_types)
        add_set_value_tests(output_file, indiv_types)
        add_main(output_file, class_name)
