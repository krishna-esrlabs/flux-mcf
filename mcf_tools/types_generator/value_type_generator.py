""""
Copyright (c) 2024 Accenture
"""
import argparse
import os
from pathlib import Path
from typing import List, Optional

from type_generator.cpp_group_generator import write_group_header_file
from type_generator.cpp_indiv_generator import write_individual_header_file
from type_generator.cpp_register_generator import write_register_header_file
from type_generator.python_type_generator import write_python_type_file
from type_generator.parse_definitions import parse_project_definitions, load_project_files
from type_generator.parse_definitions import is_cache_valid, dump_cache
from type_generator.parse_definitions import find_value_type_definition_dirs
from type_generator.common import TypesData


def create_group_header_files(group_names, cpp_header_directory, project_types):
    for key, value in group_names.items():
        # make individual cpp include directories
        output_cpp_directory = cpp_header_directory / project_types[value[0]]["PackageNamespace"] / project_types[value[0]]["Directory"]
        os.makedirs(output_cpp_directory, exist_ok=True)
        combined_filename = output_cpp_directory / (key + ".h")

        write_group_header_file(combined_filename, value, project_types[value[0]], key,
                                project_types)


def create_register_header_file(cpp_header_directory, project_definitions):
    # make directory for header that registers all different types
    output_cpp_directory = cpp_header_directory / project_definitions["PackageNamespace"]
    os.makedirs(output_cpp_directory, exist_ok=True)

    write_register_header_file(output_cpp_directory, project_definitions)


def create_individual_type_file(types_data, cpp_header_directory, py_directory, indiv_class):
    output_py_directory = py_directory / indiv_class["PackageNamespace"] / indiv_class["Directory"]
    os.makedirs(output_py_directory, exist_ok=True)

    output_cpp_directory = cpp_header_directory / indiv_class["PackageNamespace"] / indiv_class["Directory"]

    write_individual_header_file(output_cpp_directory, types_data)
    write_python_type_file(output_py_directory, types_data)


def generate_value_types(
        cpp_header_directory: Path,
        py_directory: Path,
        value_types_definition_directory: Path,
        allowed_types_file: Path,
        linked_search_dirs: Optional[List[Path]]):
    project_definitions, system_types = load_project_files(
        value_types_definition_directory,
        allowed_types_file)
    project_namespaces = [project_definitions['PackageNamespace']]

    # Parse project definition files
    project_types, project_names, group_names = parse_project_definitions(
        project_definitions,
        system_types,
        value_types_definition_directory)

    # Parse linked definition files
    linked_types = {}
    linked_names = []
    if linked_search_dirs:
        linked_type_definition_dirs = find_value_type_definition_dirs(linked_search_dirs)

        for linked_type_definition_dir in linked_type_definition_dirs:
            linked_project_definitions, linked_system_types = load_project_files(
                linked_type_definition_dir,
                allowed_types_file)
            project_namespaces.append(linked_project_definitions['PackageNamespace'])
            linked_types_i, linked_names_i, _ = parse_project_definitions(
                linked_project_definitions,
                linked_system_types,
                linked_type_definition_dir)
            linked_types = {**linked_types, **linked_types_i}
            linked_names = [*linked_names, *linked_names_i]

    cache_filename = value_types_definition_directory / ".cache.pkl"
    print(f"Checking for value types cache in {cache_filename}:")
    if is_cache_valid(cache_filename, project_types, linked_types):
        print("\tValue type definitions are unchanged, will not regenerate value types\n")
    else:
        print("\tValue type definitions are changed, regenerating value types\n")

        # Throw an error if any project namespaces are the same across the whole project
        duplicate_namespaces = [namespace for namespace in project_namespaces
                                if project_namespaces.count(namespace) > 1]
        if duplicate_namespaces:
            raise NameError(
                str(duplicate_namespaces) + " project namespaces are defined in multiple "
                                            "json type definition files")

        create_register_header_file(cpp_header_directory, project_definitions)
        create_group_header_files(group_names, cpp_header_directory, project_types)

        for _, current_type in project_types.items():
            types_data = TypesData(current_type, project_types, system_types, project_names,
                                   linked_types, linked_names)
            create_individual_type_file(types_data, cpp_header_directory, py_directory, current_type)

        dump_cache(cache_filename, project_types, linked_types)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Generate cpp headers from json class description files')
    parser.add_argument('-o_cpp', '--output_cpp_dir', required=True,
                        help='base output directory where cpp header files will be stored')
    parser.add_argument('-o_py', '--output_py_dir', required=True,
                        help='base output directory where python files will be stored')
    parser.add_argument('-i', '--input_dir', required=True,
                        help='input directory containing json description files for classes that should be generated')
    parser.add_argument('-i_link', '--linked_search_dirs', required=False,  action='append', default=None,
                        help='input directory containing json description files for classes that should be linked against')

    args = parser.parse_args()
    cpp_dir = Path(args.output_cpp_dir)
    py_dir = Path(args.output_py_dir) / "value_types"
    value_types_dir = Path(args.input_dir)

    script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
    allowed_types_file = script_dir / 'AllowedTypes.json'

    linked_search_dirs = None
    if args.linked_search_dirs:
        linked_search_dirs = [Path(dir) for dir in args.linked_search_dirs]
    generate_value_types(cpp_dir, py_dir, value_types_dir, allowed_types_file, linked_search_dirs)
