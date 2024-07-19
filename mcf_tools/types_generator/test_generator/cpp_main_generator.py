""""
Copyright (c) 2024 Accenture
"""
import os
from typing import TextIO, TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path


def add_cpp_includes(file: TextIO) -> None:

    file.write("/*******************************************************************************\n")
    file.write("* cpp Includes\n")
    file.write("******************************************************************************/\n")
    file.write("#include <iostream>\n")
    file.write("#include <signal.h>\n")
    file.write("\n")


def add_common_includes(file: TextIO) -> None:
    file.write("/*******************************************************************************\n")
    file.write("* mcf Includes\n")
    file.write("******************************************************************************/\n")
    file.write("#include \"mcf_remote/RemoteControl.h\"\n")
    file.write("#include \"mcf_core/ErrorMacros.h\"\n")
    file.write("\n")


def add_project_includes(file: TextIO, project_definitions: dict) -> None:
    file.write("/*******************************************************************************\n")
    file.write(f"* {project_definitions['PackageNamespace']} Includes\n")
    file.write("******************************************************************************/\n")
    file.write(f"#include \"{project_definitions['PackageNamespace']}/{project_definitions['ProjectName']}.h\"\n")
    file.write("\n")


def add_test_includes(file: TextIO, group_names: dict, project_definitions: dict) -> None:
    file.write("/*******************************************************************************\n")
    file.write("* " + project_definitions["PackageNamespace"] + "_test Includes\n")
    file.write("******************************************************************************/\n")

    for key in sorted(group_names):
        file.write("#include \"" + project_definitions["PackageNamespace"] + "_test/" + key + "Test.h\"\n")

    file.write("\n")


def add_sig_handler(file: TextIO) -> None:
    file.write("namespace\n")
    file.write("{\n\n")
    file.write("// TODO: should be used with mutex and condition variable\n")
    file.write("volatile bool runFlag = true;\n\n")
    file.write("void sig_int_handler(int sig)\n")
    file.write("{\n")
    file.write("    runFlag = false;\n")
    file.write("}\n\n")
    file.write("} // anonymous namespace\n\n")


def add_main(file: TextIO, project_types: dict, group_names: dict, project_definitions: dict) -> None:
    file.write("int main(int argc, char **argv)\n")
    file.write("{\n")
    file.write("    struct sigaction action;\n")
    file.write("    memset(&action, 0, sizeof(action));\n")
    file.write("    action.sa_handler = sig_int_handler;\n")
    file.write("    sigaction(SIGINT, &action, NULL);  // Ctrl-C\n\n")
    file.write("    mcf::ValueStore valueStore;\n")

    file.write(f"    values::{project_definitions['PackageNamespace']}::register{project_definitions['ProjectName']}(valueStore);\n\n")

    file.write("    mcf::ComponentManager componentManager(valueStore, \"dummy\");\n")
    file.write("    auto remoteControl = std::make_shared<mcf::remote::RemoteControl>(6666, componentManager, valueStore);\n")
    file.write("    componentManager.registerComponent(remoteControl);\n\n")

    for key, value in sorted(group_names.items()):
        project_namespace = project_definitions["PackageNamespace"]
        namespace = project_types[value[0]]["Directory"]  # extract namespace by using first type given in group
        typename = f"values::{project_namespace}::{namespace}::{key}Test"
        objectname = "test" + key
        file.write("    auto " + objectname + " = std::make_shared<" + typename + ">();\n")
        file.write("    componentManager.registerComponent(" + objectname + ");\n\n")

    file.write("    // Start component manager\n")
    file.write("    componentManager.configure();\n")
    file.write("    componentManager.startup();\n\n")
    file.write("    std::cout << \"Running\" << std::endl;\n")
    file.write("    while(runFlag)\n")
    file.write("    {\n")
    file.write("        std::this_thread::sleep_for(std::chrono::milliseconds(100));\n")
    file.write("    }\n\n")
    file.write("    componentManager.shutdown();\n")
    file.write("    return 0;\n")

    file.write("}\n")


def write_main_src_file(filename: 'Path', project_types: dict, group_names: dict, project_definitions: dict) -> None:
    execution_dir = os.getcwd()
    with open(filename, "w") as output_file:
        output_file.write("// WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/tester_generator.py.\n")
        output_file.write("// Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write("// To make changes either edit mcf_tools/types_generator/test_generator/cpp_main_generator.py or disable the generation in: \n"
                          f"// {execution_dir}\n\n\n")

        add_cpp_includes(output_file)
        add_common_includes(output_file)
        add_project_includes(output_file, project_definitions)
        add_test_includes(output_file, group_names, project_definitions)
        add_sig_handler(output_file)
        add_main(output_file, project_types, group_names, project_definitions)
