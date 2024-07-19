""""
Copyright (c) 2024 Accenture
"""
import argparse
import os
from pathlib import Path


# For testing run with:
# python3 tester_generator.py \
#   -i /path/to/value_types/value_types_json/ \
#   -o /path/to/value_types/value_types_test/ \
#   -m ../../../../modules/


from test_generator.cpp_header_generator import write_cpp_header_file
from test_generator.cpp_src_generator import write_cpp_src_file
from test_generator.cpp_main_generator import write_main_src_file
from test_generator.python_test_generator import write_python_test_file
from test_generator.python_main_generator import write_main_python_file
from type_generator.parse_definitions import parse_project_definitions, load_project_files


def create_output_directories(
        output_directory,
        cpp_main_src_directory,
        group_names,
        project_types,
        project_definitions,
        system_types):
    # make individual cpp include directories
    cpp_include_directory = output_directory / 'include' / (
                project_definitions["PackageNamespace"] + "_test")
    os.makedirs(cpp_include_directory, exist_ok=True)

    # make individual cpp include directories
    cpp_src_directory = output_directory / "src"
    os.makedirs(cpp_src_directory, exist_ok=True)

    # make individual python directories
    py_types_test_directory = output_directory / "python" / "types_test"
    os.makedirs(py_types_test_directory, exist_ok=True)

    for key, value in group_names.items():
        cpp_include_filename = cpp_include_directory / (key + "Test.h")
        cpp_src_filename = cpp_src_directory / (key + "Test.cpp")
        python_filename = py_types_test_directory / (key + "Test.py")

        write_cpp_header_file(cpp_include_filename, value, project_types[value[0]], key,
                              project_definitions["PackageNamespace"])
        write_cpp_src_file(cpp_src_filename, value, project_types, key,
                           project_definitions["PackageNamespace"])
        write_python_test_file(python_filename, value, project_types, system_types, key,
                               project_definitions["PackageNamespace"])

    # Generate main cpp project
    os.makedirs(cpp_main_src_directory, exist_ok=True)


def generate_value_type_tests(output_directory: Path,
                              value_types_definition_directory: Path,
                              allowed_types_file: Path,
                              mcf_location_absolute: Path,
                              executable_path_absolute: Path):
    project_definitions, system_types = load_project_files(
        value_types_definition_directory,
        allowed_types_file)

    # Parse project definition files
    project_types, project_names, group_names = parse_project_definitions(
        project_definitions,
        system_types,
        value_types_definition_directory)

    cpp_main_src_directory = output_directory / "main_test" / "src"
    create_output_directories(output_directory,
                              cpp_main_src_directory,
                              group_names,
                              project_types,
                              project_definitions,
                              system_types)

    cpp_main_filename = cpp_main_src_directory / "main.cpp"
    python_main_filename = output_directory / "python" / ("test_" + project_definitions['PackageNamespace'] + ".py")

    write_main_src_file(cpp_main_filename, project_types, group_names, project_definitions)
    write_main_python_file(
        python_main_filename,
        group_names,
        project_definitions,
        mcf_location_absolute,
        executable_path_absolute)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate cpp headers from json class'
                                                 ' description files')
    parser.add_argument(
        '-o', '--output', required=True,
        help='base output directory where generated test code will be stored')
    parser.add_argument(
        '-i', '--input_dir', required=True,
        help='input directory containing json description files for classes that should'
             ' be generated')
    parser.add_argument(
        '-m', '--mcf_location', required=True,
        help='relative location of mcf to generated code')
    parser.add_argument(
        '-x', '--executable_path', required=True,
        help='relative location of generated value type test executable.')

    args = parser.parse_args()
    output_dir = Path(args.output)
    value_types_def_dir = Path(args.input_dir)
    mcf_location_relative = args.mcf_location
    mcf_location_absolute = (Path.cwd() / mcf_location_relative).resolve()
    executable_path_relative = args.executable_path
    executable_path_absolute = (Path.cwd() / executable_path_relative).resolve()

    script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
    allowed_types_file = script_dir / 'AllowedTypes.json'

    print(args.executable_path)
    print(executable_path_absolute)
    generate_value_type_tests(output_dir,
                              value_types_def_dir,
                              allowed_types_file,
                              mcf_location_absolute,
                              executable_path_absolute)
