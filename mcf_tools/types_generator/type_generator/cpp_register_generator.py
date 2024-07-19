""""
Copyright (c) 2024 Accenture
"""
import os
from typing import TextIO, TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


def add_register_types(file: TextIO, project_definitions: dict) -> None:
    project_namespace = project_definitions["PackageNamespace"]
    file.write("namespace values {\n\n")
    file.write(f"namespace {project_namespace} {{\n\n")
    file.write("template<typename T>\n")
    file.write("inline void register" + project_definitions["ProjectName"] + "(T& r) {\n")

    for key, value in project_definitions["Definitions"].items():
        file.write("    " + value["Directory"] + "::register" + key + "(r);\n")
    file.write("}\n\n")
    file.write(f"}}  // namespace {project_namespace}\n\n")
    file.write("}  // namespace values\n\n")


def add_includes(file: TextIO, project_definitions: dict) -> None:
    for key, value in project_definitions["Definitions"].items():
        file.write(f"#include \"{project_definitions['PackageNamespace']}/{value['Directory']}/{key}.h\"\n")
    file.write("\n")


def add_include_guard(file: TextIO, project_definitions: dict) -> None:
    guard_name = project_definitions["PackageNamespace"].upper() + "_" + project_definitions["ProjectName"].upper() + "_H_"
    file.write("#ifndef " + guard_name + "\n")
    file.write("#define " + guard_name + "\n\n")

    add_includes(file, project_definitions)
    add_register_types(file, project_definitions)

    file.write("#endif   // " + guard_name + "\n")


def write_register_header_file(filename: 'Path', project_definitions: dict) -> None:
    execution_dir = os.getcwd()
    with open(filename / (project_definitions["ProjectName"] + ".h"), "w") as output_file:
        output_file.write("// WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/value_type_generator.py.\n")
        output_file.write("// Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write("// To make changes either edit mcf_tools/types_generator/type_generator/cpp_register_generator.py or disable the generation in: \n"
                          f"// {execution_dir}\n\n\n")

        add_include_guard(output_file, project_definitions)
