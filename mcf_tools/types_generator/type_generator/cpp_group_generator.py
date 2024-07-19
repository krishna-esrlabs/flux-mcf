""""
Copyright (c) 2024 Accenture
"""
import os
from typing import TextIO, TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


def add_register_types(file: TextIO, group_names: list, class_name: str, project_types: dict) -> None:
    file.write("template<typename T>\n")
    file.write(f"inline void register{class_name}(T& r) {{\n")

    for type_name in group_names:
        # Strip template parameters from extmemvalue types
        kind = project_types[type_name]["Kind"].type_name_no_ns
        type_namespace = project_types[type_name]["Directory"]
        project_namespace = project_types[type_name]["PackageNamespace"]

        # Only register values and extmemvalues in value store
        if kind == "Value" or kind == "ExtMemValue":
            type_name_no_ns = type_name.split("::")[-1]
            file.write(f"    r.template registerType<{type_name_no_ns}>(\"{project_namespace}::{type_namespace}::{type_name_no_ns}\");\n")

    file.write("}\n\n")


def add_namespace(file: TextIO, group_names: list, group_type: dict, class_name: str, project_types: dict) -> None:
    file.write("namespace values {\n\n")
    file.write(f"namespace {group_type['PackageNamespace']} {{\n\n")
    file.write(f"namespace {group_type['Directory']} {{\n\n")

    add_register_types(file, group_names, class_name, project_types)

    file.write(f"}}   // namespace {group_type['Directory']}\n\n")
    file.write(f"}}   // namespace {group_type['PackageNamespace']}\n\n")
    file.write("}   // namespace values\n\n")


def add_includes(file: TextIO, group_names: list, group_type: dict, class_name: str, project_types: dict) -> None:
    file.write("#include \"msgpack.hpp\"\n")

    for el in sorted(group_names):
        include_path = el.replace('::', '/')
        file.write(f"#include \"{include_path}.h\"\n")
    file.write("\n")

    add_namespace(file, group_names, group_type, class_name, project_types)


def add_include_guard(file: TextIO, group_names: list, group_type: dict, class_name: str, project_types: dict) -> None:
    guard_string = f"{group_type['PackageNamespace'].upper()}_{class_name.upper()}_H_"

    file.write("#ifndef " + guard_string + "\n")
    file.write("#define " + guard_string + "\n\n")

    add_includes(file, group_names, group_type, class_name, project_types)

    file.write("#endif   // " + guard_string)


def write_group_header_file(filename: 'Path', group_names: list, group_type: dict, class_name: str, project_types: dict) -> None:
    execution_dir = os.getcwd()
    with open(filename, "w") as output_file:
        output_file.write("// WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/value_type_generator.py.\n")
        output_file.write("// Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write("// To make changes either edit mcf_tools/types_generator/type_generator/cpp_group_generator.py or disable the generation in: \n"
                          f"// {execution_dir}\n\n\n")
        add_include_guard(output_file, group_names, group_type, class_name, project_types)
