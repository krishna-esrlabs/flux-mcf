""""
Copyright (c) 2024 Accenture
"""
import os
from typing import TextIO, TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


def add_constructor(file: TextIO, group_names: list, class_name: str) -> None:
    file.write(class_name + "Test::" + class_name + "Test() :\n")
    file.write("    mcf::Component(\"" + class_name + "Test\")\n")

    for type_name in sorted(group_names):
        type_name_no_ns = type_name.split("::")[-1]
        format_dict = {"t": type_name_no_ns}
        file.write("    , f{t}InPort(*this, \"{t}Input\", 1, true)\n".format(**format_dict))
        file.write("    , f{t}OutPort(*this, \"{t}Output\")\n".format(**format_dict))

    file.write("{\n}\n\n")


def add_configure(file: TextIO, group_names: list, type_namespace: str, class_name: str) -> None:
    file.write("void " + class_name + "Test::configure(mcf::IComponentConfig& config)\n")
    file.write("{\n")

    for type_name in sorted(group_names):
        type_name_no_ns = type_name.split("::")[-1]
        inPortName = f"f{type_name_no_ns}InPort"
        outPortName = f"f{type_name_no_ns}OutPort"
        topic_prefix = f"{type_namespace.upper()}_{type_name_no_ns.upper()}_TEST_"
        file.write("    config.registerPort(" + inPortName + ", " + topic_prefix + "IN_TOPIC);\n")
        file.write("    " + inPortName + ".registerHandler(std::bind(&" + class_name + "Test::execute, this));\n")
        file.write("    config.registerPort(" + outPortName + ", " + topic_prefix + "OUT_TOPIC);\n\n")

    file.write("}\n\n")


def add_startup(file: TextIO, class_name: str) -> None:
    file.write("void " + class_name + "Test::startup()\n")
    file.write("{\n")
    file.write("    execute();\n")
    file.write("}\n\n")


def add_execute(file: TextIO, group_names: list, project_types: dict, class_name: str) -> None:
    file.write("void " + class_name + "Test::execute()\n")
    file.write("{\n")

    for type_name in sorted(group_names):
        kind = project_types[type_name]["Kind"].type_name_no_ns
        type_name_no_ns = type_name.split("::")[-1]

        if kind != "Struct" and kind != "Enum":
            file.write("    if(f" + type_name_no_ns + "InPort.hasValue())\n")
            file.write("    {\n")

            # ExtMemValues cannot be dereferenced so keep as pointer
            if kind == "ExtMemValue":
                file.write(" " * 8 + "auto value = f" + type_name_no_ns + "InPort.getValue();\n")
            else:
                file.write(" " * 8 + "auto value = *f" + type_name_no_ns + "InPort.getValue();\n")

            file.write(" " * 8 + "f" + type_name_no_ns + "OutPort.setValue(std::move(value), true);\n")

            file.write("    }\n\n")

    file.write("    return;\n")
    file.write("}\n\n")


def add_namespace(file: TextIO, group_names: list, project_types: dict, type_namespace: str, project_namespace: str, class_name: str) -> None:
    file.write("namespace values {\n\n")
    file.write(f"namespace {project_namespace} {{\n\n")
    file.write(f"namespace {type_namespace} {{\n\n")

    add_constructor(file, group_names, class_name)
    add_configure(file, group_names, type_namespace, class_name)
    add_startup(file, class_name)
    add_execute(file, group_names, project_types, class_name)

    file.write(f"}}   // namespace {type_namespace}\n\n")
    file.write(f"}}   // namespace {project_namespace}\n\n")
    file.write("}   // namespace values\n\n")


def add_includes(file: TextIO, group_names: list, project_types: dict, class_name: str, type_namespace: str, project_namespace: str) -> None:

    file.write("#include \"" + project_namespace + "_test/" + class_name + "Test.h\"\n\n")

    add_namespace(file, group_names, project_types, type_namespace, project_namespace, class_name)


def write_cpp_src_file(filename: 'Path', group_names: list, project_types: dict, class_name: str, project_namespace: str) -> None:
    execution_dir = os.getcwd()
    with open(filename, "w") as output_file:
        output_file.write("// WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/tester_generator.py.\n")
        output_file.write("// Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write("// To make changes either edit mcf_tools/types_generator/test_generator/cpp_src_generator.py or disable the generation in: \n"
                          f"// {execution_dir}\n\n\n")

        type_namespace = project_types[group_names[0]]["Directory"]   # Can use any type to extract type_namespace
        add_includes(output_file, group_names, project_types, class_name, type_namespace, project_namespace)
