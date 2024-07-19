""""
Copyright (c) 2024 Accenture
"""
from type_generator.common import TypeNameParser, Scalar, Type, ConfigurationError
from collections import OrderedDict
import json
import os
from typing import List
from pathlib import Path
import pickle


def parse_kinds(project_types: dict):
    # Parse type name
    for group in project_types.values():
        group["Kind"] = TypeNameParser.parse(group["Kind"])


def parse_types(project_types: dict):
    # Parse type name
    for group in project_types.values():
        if group["Kind"].type_name_no_ns != "Enum":
            for value_type in group["Attributes"].values():
                value_type["Type"] = TypeNameParser.parse(value_type["Type"])


def fill_attribute_namespaces(project_types: dict, system_types: dict):
    """
    Type generator assumes that all types have fully specified project and group
    namespaces. For each defined value type, this functions fills out the namespaces of
    the type name for each attribute according to the following rules:
        - If one namespace is specified, the attribute is assumed to belong to the same
        project namespace as the parent value type.
        - If no namespace is specified, the attribute is assumed to belong to the same
        project and group namespace as the parent value type.

    E.g. value type project_ns::group_ns::MyValue contains 3 attributes:
        - MyAttribute1
        - other_group_ns::MyAttribute2
        - other_project_ns::other_group_ns::MyAttribute3

    The attribute type names will be updated to the following:
        - project_ns::group_ns::MyAttribute1             (Added group / project namespace)
        - project_ns::other_group_ns::MyAttribute2       (Added project namespace)
        - other_project_ns::other_group_ns::MyAttribute3 (Unchanged)
    """
    def recurse(type_: 'Type'):
        if type(type_) == Scalar:
            type_name = type_.type_name_no_ns
            if type_name not in system_types["SystemTypes"]:
                if len(type_.type_name) == 2:
                    type_.type_name = [project_namespace, *type_.type_name]
                if len(type_.type_name) == 1:
                    type_.type_name = [project_namespace, group_namespace, *type_.type_name]
            return
        for arg in type_.args:
            recurse(arg)

    for value_type in project_types.values():
        if value_type["Kind"].type_name_no_ns != "Enum":
            for attribute in value_type["Attributes"].values():
                project_namespace = value_type["PackageNamespace"]
                group_namespace = value_type["Directory"]
                recurse(attribute["Type"])


def parse_project_definitions(project_definitions: dict, system_types: dict, value_types_definition_directory: 'Path'):
    project_types = {}
    project_names = []
    group_names = {}
    for key, value in project_definitions["Definitions"].items():
        path = value_types_definition_directory / value["Directory"]
        type_files = [file for file in os.listdir(path) if file.endswith(".json")]

        indiv_namespace_types = []
        for type_file in type_files:

            file_path = path / type_file
            print(file_path)

            with open(file_path, "r") as f:
                try:
                    indiv_json = json.load(f, object_pairs_hook=OrderedDict)
                except json.decoder.JSONDecodeError as e:
                    print(f"ERROR: {f.name} could not be correctly parsed.")
                    raise e

            # Get value type name from file name
            indiv_json["Name"] = os.path.splitext(type_file)[0]

            indiv_namespace_type = "::".join([project_definitions["PackageNamespace"], value["Directory"], indiv_json["Name"]])

            if indiv_json["Name"] in system_types['SystemTypes']:
                raise NameError(f"{indiv_namespace_type} cannot have the same name as a system type: {indiv_json['Name']}.")

            project_types[indiv_namespace_type] = indiv_json
            project_types[indiv_namespace_type]["Include"] = f"{project_definitions['PackageNamespace']}/{value['Directory']}/{indiv_json['Name']}.h"
            project_types[indiv_namespace_type]["Directory"] = value["Directory"]
            project_types[indiv_namespace_type]["GroupName"] = key
            project_types[indiv_namespace_type]["PackageNamespace"] = project_definitions["PackageNamespace"]

            indiv_namespace_types.append(indiv_namespace_type)

        project_names += indiv_namespace_types
        group_names[key] = indiv_namespace_types

    parse_kinds(project_types)
    parse_types(project_types)

    fill_attribute_namespaces(project_types, system_types)

    return project_types, project_names, group_names


def convert_snake_case_to_camel_case(variable_name: str) -> str:
    return ''.join(part.capitalize() for part in variable_name.split('_'))


def extract_definitions_from_dir_structure(value_types_project_dir: 'Path') -> dict:
    """
    Parse project definitions from value type directory using directory structure and
    ProjectDefinitions.json.

    ProjectDefinitions.json should define "ProjectName" and "PackageNamespace". All
    directories within value_types_project_dir which contain .json files are
    assumed to contain value type definitions. A configuration will be thrown if there is
    a child directory which does not contain at least one .json file.
    """
    value_type_dirs = (dir for dir in value_types_project_dir.iterdir() if dir.is_dir())
    definitions = {}
    for value_type_dir in value_type_dirs:
        value_type_name = f"{convert_snake_case_to_camel_case(value_type_dir.name)}Types"

        if not list(value_type_dir.glob("*.json")):
            raise RuntimeError("Value types directory must only contain directories "
                               "which contain JSON value type definitions.")
        definitions[value_type_name] = {"Directory": value_type_dir.name}
    return definitions


def get_project_name_from_namespace(project_definitions: dict) -> str:
    return convert_snake_case_to_camel_case(project_definitions['PackageNamespace'])


def validate_project_definitions(project_definitions: dict, project_definitions_file: 'Path'):
    allowed_key = 'PackageNamespace'
    if len(project_definitions) == 1 and allowed_key in project_definitions:
        return
    raise ConfigurationError(f"{project_definitions_file.resolve()} should contain a single key: {allowed_key}")


def load_project_files(
        value_types_definition_directory: 'Path',
        allowed_types_file: 'Path'):
    project_definitions_file = value_types_definition_directory / "ProjectDefinitions.json"
    with open(project_definitions_file, "r") as f:
        try:
            project_definitions = json.load(f)
        except json.decoder.JSONDecodeError as e:
            print(f"ERROR: ProjectDefinitions.json in directory "
                  f"{value_types_definition_directory} could not be correctly parsed")
            raise e
    validate_project_definitions(project_definitions, project_definitions_file)
    project_definitions["Definitions"] = extract_definitions_from_dir_structure(
        value_types_definition_directory)
    project_definitions["ProjectName"] = get_project_name_from_namespace(project_definitions)

    with open(allowed_types_file, "r") as f:
        try:
            system_types = json.load(f)
        except json.decoder.JSONDecodeError as e:
            print(f"ERROR: {allowed_types_file} in directory"
                  f" {value_types_definition_directory} could not be correctly parsed")
            raise e

    return project_definitions, system_types


def find_value_type_definition_dirs(search_dirs: List['Path']) -> List['Path']:
    """
    Recurseively search each search_dir for all directories containing a
    ProjectDefinition.json file.
    """
    value_type_dirs = []
    for search_dir in search_dirs:
        for value_type_file in search_dir.rglob('ProjectDefinitions.json'):
            value_type_dir = value_type_file.parent
            value_type_dirs.append(value_type_dir)
    return value_type_dirs


def is_cache_valid(cache_filename: 'Path', project_types: dict, linked_types: dict) -> bool:
    # Check whether value type definitions have changed since last compilation
    print(f"value_types_definition_directory: {cache_filename}\n")

    # Check whether cache exists
    if not os.path.exists(cache_filename):
        return False

    with open(cache_filename, "rb") as f:
        try:
            cache_project_types, cache_linked_types = pickle.load(f)
        except ValueError:
            return False

    cache_valid = (cache_project_types == project_types) and (cache_linked_types == linked_types)

    if not cache_valid:
        try:
            cache_filename.unlink()
        except FileNotFoundError:
            pass

    return cache_valid


def dump_cache(cache_filename: 'Path', project_types: dict, linked_types: dict):
    with open(cache_filename, "wb") as f:
        pickle.dump([project_types, linked_types], f)
