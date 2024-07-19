""""
Copyright (c) 2024 Accenture
"""
from typing import TextIO
from pathlib import Path
import os

def add_sys_imports(file: TextIO) -> None:
    file.write("import inspect\n")
    file.write("import os\n")
    file.write("import sys\n\n")


def add_import_paths(file: TextIO, mcf_location: 'Path', executable_path: 'Path') -> None:
    file.write("# path of mcf python tools and utils, relative to location of this script\n")

    mcf_tools_location = mcf_location / "mcf_py"
    file.write(f"_MCF_TOOLS_PATH = \"{mcf_tools_location}\"\n")

    mcf_remote_test_python_location = mcf_location / "mcf_remote" / "test" / "python"
    file.write(f"_UTILS_TEST_PATH = \"{mcf_remote_test_python_location}\"\n\n")

    file.write("# path to executable to run on target\n")
    file.write(f"_EXECUTABLE_RELATIVE_PATH = \"{executable_path}\"\n\n")

    file.write("# path to test values\n")
    file.write("_TEST_VALUES_RELATIVE_PATH = \"../TestValues.json\"\n\n")

    file.write("sys.path.append(_UTILS_TEST_PATH)\n")
    file.write("sys.path.append(_MCF_TOOLS_PATH)\n\n")

    file.write("# directory of this script and relative path of mcf python tools\n")
    file.write("_SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))\n\n")

    file.write("# absolute path of executable to run on target\n")
    file.write("_EXECUTABLE_ABS_PATH = os.path.join(_SCRIPT_DIRECTORY, _EXECUTABLE_RELATIVE_PATH)\n\n")

    file.write("# path to test values used in value store objects\n")
    file.write("_TEST_VALUES_ABS_PATH = os.path.join(_SCRIPT_DIRECTORY, _TEST_VALUES_RELATIVE_PATH)\n")

    file.write("# ip address and port of remote target\n")
    file.write("_TARGET_IP = \"127.0.0.1\"  # localhost\n")
    file.write("_TARGET_PORT = \"6666\"\n\n")


def add_project_imports(file: TextIO, group_names: dict) -> None:
    file.write("from mcf import Connection\n")
    file.write("from utils.ProcessRunner import ProcessRunner\n\n")

    for key in sorted(group_names):
        file.write("from types_test." + key + "Test import Run" + key + "Test\n")
    file.write("\n\n")


def add_main_function(file: TextIO, group_names: dict, project_definitions: dict) -> None:
    file.write("def test_" + project_definitions["PackageNamespace"] + "() -> None:\n\n")
    file.write("    # start mcf executable\n")
    file.write("    with ProcessRunner([_EXECUTABLE_ABS_PATH]):\n")
    file.write("        connection = Connection(_TARGET_IP, port=_TARGET_PORT)\n")
    file.write("        remote = connection.rc\n")

    for key in sorted(group_names):
        file.write("        Run" + key + "Test(remote, _TEST_VALUES_ABS_PATH)\n")
    file.write("\n\n")

    file.write("if __name__ == \"__main__\":\n")
    file.write("    test_" + project_definitions["PackageNamespace"] + "()\n")


def write_main_python_file(filename: 'Path', group_names: dict, project_definitions: dict, mcf_location: 'Path', executable_path: 'Path') -> None:
    execution_dir = os.getcwd()
    with open(filename, "w") as output_file:
        output_file.write("# WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/tester_generator.py.\n")
        output_file.write("# Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write("# To make changes either edit mcf_tools/types_generator/test_generator/python_main_generator.py or disable the generation in: \n"
                          f"# {execution_dir}\n\n\n")

        add_sys_imports(output_file)
        add_import_paths(output_file, mcf_location, executable_path)
        add_project_imports(output_file, group_names)
        add_main_function(output_file, group_names, project_definitions)
