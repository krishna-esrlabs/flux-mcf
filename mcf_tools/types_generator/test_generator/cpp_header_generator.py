""""
Copyright (c) 2024 Accenture
"""
import os
from typing import TextIO, TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


def add_class(file: TextIO, group_names: list, group_type: dict, class_name: str) -> None:
    class_name_test = class_name + "Test"
    file.write("class " + class_name_test + " : public mcf::Component\n{\n\n")
    file.write("    public:\n\n")
    file.write(" " * 8 + class_name_test + "();\n\n")
    file.write(" " * 8 + "void configure(mcf::IComponentConfig & config) override;\n\n")
    file.write(" " * 8 + "void startup() override;\n\n")

    constexpr_str = f"{' ' * 8}constexpr static const char* {group_type['Directory'].upper()}_"
    for type_name in sorted(group_names):
        type_name_no_ns = type_name.split("::")[-1]
        file.write(f"{constexpr_str}{type_name_no_ns.upper()}_TEST_IN_TOPIC = \"/{group_type['Directory']}/{type_name_no_ns.lower()}_pysend\";\n")
        file.write(f"{constexpr_str}{type_name_no_ns.upper()}_TEST_OUT_TOPIC = \"/{group_type['Directory']}/{type_name_no_ns.lower()}_cppsend\";\n")
    file.write("\n")

    file.write("    private:\n\n")
    file.write(" " * 8 + "void execute();\n\n")

    for type_name in sorted(group_names):
        type_name_no_ns = type_name.split('::')[-1]
        file.write(" " * 8 + "mcf::QueuedReceiverPort<" + type_name + "> f" + type_name_no_ns + "InPort;\n")
        file.write(" " * 8 + "mcf::SenderPort<" + type_name + "> f" + type_name_no_ns + "OutPort;\n")

    file.write("\n")

    file.write("};\n\n")


def add_namespace(file: TextIO, group_names: list, group_type: dict, class_name: str) -> None:
    project_namespace = group_type["PackageNamespace"]
    type_namespace = group_type["Directory"]

    file.write("namespace values {\n\n")
    file.write(f"namespace {project_namespace} {{\n\n")
    file.write(f"namespace {type_namespace} {{\n\n")

    add_class(file, group_names, group_type, class_name)

    file.write(f"}}   // namespace {type_namespace}\n\n")
    file.write(f"}}   // namespace {project_namespace}\n\n")
    file.write("}   // namespace values\n\n")


def add_includes(file: TextIO, group_names: list, group_type: dict, class_name: str) -> None:

    file.write("#include \"mcf_core/Mcf.h\"\n")
    file.write(f"#include \"{group_type['PackageNamespace']}/{group_type['Directory']}/{class_name}.h\"\n\n")

    add_namespace(file, group_names, group_type, class_name)


def add_include_guard(file: TextIO, group_names: list, group_type: dict, class_name: str, project_namespace: str) -> None:

    guard_string = project_namespace.upper() + "_TEST_" + class_name.upper() + "TEST_H_"

    file.write("#ifndef " + guard_string + "\n")
    file.write("#define " + guard_string + "\n\n")

    add_includes(file, group_names, group_type, class_name)

    file.write("#endif   // " + guard_string)


def write_cpp_header_file(filename: 'Path', group_names: list, group_type: dict, class_name: str, project_namespace: str) -> None:
    execution_dir = os.getcwd()
    with open(filename, "w") as output_file:
        output_file.write("// WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/tester_generator.py.\n")
        output_file.write("// Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write("// To make changes either edit mcf_tools/types_generator/test_generator/cpp_header_generator.py or disable the generation in: \n"
                          f"// {execution_dir}\n\n\n")

        add_include_guard(output_file, group_names, group_type, class_name, project_namespace)
