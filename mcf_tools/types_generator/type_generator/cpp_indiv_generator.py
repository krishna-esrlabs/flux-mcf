""""
Copyright (c) 2024 Accenture
"""
from type_generator.common import TypesData, ConfigurationError
from type_generator.common import is_enum_type, is_primitive_type, is_container_type
from type_generator.common import assert_types_validity

import os
from typing import TextIO, TYPE_CHECKING
if TYPE_CHECKING:
    from pathlib import Path


def add_msg_pack_define(file: TextIO, current_type: dict) -> None:
    msgpack_define_string = "    MSGPACK_DEFINE({})\n".format(
        ", ".join(current_type["Attributes"].keys())
    )
    file.write(msgpack_define_string)


def add_operators(file: TextIO, current_type: dict) -> None:
    attribute_names = current_type["Attributes"].keys()
    lhs_string = ", ".join(attribute_names)
    rhs_string = ", ".join([f"other.{name}" for name in attribute_names])

    value_type_name = current_type["Name"]
    main_string = f"    bool operator<(const {value_type_name}& other) const\n" \
                   "    {\n" \
                  f"        return std::tie({lhs_string}) < std::tie({rhs_string});\n" \
                   "    }\n\n"
    file.write(main_string)


def add_attributes(file: TextIO, current_type: dict, system_types: dict) -> None:
    for key, value in current_type["Attributes"].items():
        file.write(f"    {value['Type'].as_cpp_type(system_types)} {key};  ///< {value['Doc']}\n")


def add_value_init_constructor(file: TextIO, current_type: dict, system_types: dict) -> None:

    num_variables = len(current_type["Attributes"])

    # if constructor takes a single argument, make it exlpicit
    if num_variables == 1:
        file.write("    explicit " + current_type["Name"] + "(")
    else:
        file.write("    " + current_type["Name"] + "(")

    for index, (key, value) in enumerate(current_type["Attributes"].items()):
        if index != num_variables - 1:
            file.write(value["Type"].as_cpp_type(system_types) + " " + key + ", ")
        else:
            file.write(value["Type"].as_cpp_type(system_types) + " " + key + ")\n")  # Need to remove trailing "," for last item

    file.write(" " * len(current_type["Name"]) + "     : ")  # Calculate correct indentation

    init_variables = []
    for index, (key, value) in enumerate(current_type["Attributes"].items()):
        id_type = value["Type"].type_name_no_ns

        # No move semantics for primitive types (exception added for strings)
        if id_type in system_types["SystemTypes"] and system_types["SystemTypes"][id_type]["Type"] == "Primitive" and id_type != "string":
            init_variables.append(key + "{" + key + "}")
        else:
            init_variables.append(key + "{std::move(" + key + ")}")

    file.write(", ".join(init_variables))
    file.write("{};\n\n")


def add_empty_constructor(file: TextIO, types_data: 'TypesData') -> None:
    file.write("    " + types_data.current_type["Name"] + "()")

    default_init = []
    for (key, value) in types_data.current_type["Attributes"].items():
        if "DefaultValue" in value:
            stripped_type = value["Type"].type_name_no_ns
            default_value = value["DefaultValue"]

            if is_enum_type(value["Type"], types_data):
                enum_namespace = value["Type"].as_cpp_type(types_data.system_types)
                enum_value = value["DefaultValue"]
                enum_full_value = "::".join([enum_namespace, enum_value])
                default_init.append((key, enum_full_value))
            elif is_primitive_type(stripped_type, types_data.system_types):
                if stripped_type == "string":
                    default_init.append((key, f"\"{str(default_value).lower()}\""))
                else:
                    default_init.append((key, str(default_value).lower()))
            elif is_container_type(stripped_type, types_data.system_types):
                if stripped_type == "map":
                    default_init.append((key, ", ".join(f"{{\"{k}\", {v}}}" for k, v in default_value.items())))
                elif stripped_type in ["vector", "set"]:
                    default_init.append((key, ", ".join(str(x) for x in default_value)))

    if default_init:
        file.write(" : {}".format(
            ", ".join(
                ["{}{}".format(variable, "{" + value + "}") for (variable, value) in default_init]
            )
        ))

    file.write(" {};\n\n")


def add_description(file: TextIO, current_type: dict) -> None:
    file.write("/**\n")
    file.write(" * " + current_type["Doc"] + "\n")
    file.write(" */\n")


def write_class(file: TextIO, types_data: 'TypesData') -> None:
    add_empty_constructor(file, types_data)
    add_value_init_constructor(file, types_data.current_type, types_data.system_types)
    add_operators(file, types_data.current_type)
    add_attributes(file, types_data.current_type, types_data.system_types)
    add_msg_pack_define(file, types_data.current_type)
    file.write("};\n\n")


def add_class(file: TextIO, types_data: 'TypesData') -> None:
    kind = types_data.current_type["Kind"].type_name_no_ns
    if kind == "Struct":
        add_description(file, types_data.current_type)
        file.write("struct " + types_data.current_type["Name"])
        file.write("\n{\n")
        write_class(file, types_data)

    if kind == "Value":
        add_description(file, types_data.current_type)
        file.write("struct " + types_data.current_type["Name"])
        file.write(" : public mcf::Value\n{\n")
        write_class(file, types_data)

    elif kind == "ExtMemValue":

        file.write("#if HAVE_CUDA\n")
        cpp_kind = types_data.current_type["Kind"].as_cpp_type(types_data.system_types)
        macro_name = cpp_kind.replace("<", "_").replace(">", "")
        file.write("#define " + macro_name + " mcf::Cuda" + cpp_kind + "\n")
        file.write("#else\n")
        file.write("#define " + macro_name + " mcf::" + cpp_kind + "\n")
        file.write("#endif // HAVE_CUDA\n\n")

        add_description(file, types_data.current_type)
        file.write("struct " + types_data.current_type["Name"])
        file.write(" : public " + macro_name + "\n{\n")
        write_class(file, types_data)


def add_enum(file: TextIO, current_type: dict) -> None:
    add_description(file, current_type)

    file.write("enum " + current_type["Name"])
    first_variable_flag = True
    for el in current_type["Attributes"]:

        # Return error if enum is defined in lowercase
        lowercase_letters = [c for c in el if c.islower()]
        if lowercase_letters:
            print("ERROR: Enum attributes must be uppercase, " + el + " in " +
                  current_type["Name"] + ".json contains lowercase letters.")
            exit(-1)

        if first_variable_flag:
            file.write(" {" + el)
            first_variable_flag = False
        else:
            file.write(", " + el)
    file.write("};\n\n")


def add_namespace(file: TextIO, types_data: 'TypesData') -> None:
    project_namespace = types_data.current_type['PackageNamespace']
    type_namespace = types_data.current_type['Directory']

    file.write("namespace values {\n\n")

    file.write(f"namespace {project_namespace} {{\n\n")

    file.write(f"namespace {type_namespace} {{\n\n")

    if types_data.current_type["Kind"].type_name_no_ns == "Enum":
        add_enum(file, types_data.current_type)
    else:
        add_class(file, types_data)

    file.write(f"}}   // namespace {type_namespace}\n\n")
    file.write(f"}}   // namespace {project_namespace}\n\n")
    file.write("}   // namespace values\n\n")


def add_includes(file: TextIO, types_data: 'TypesData') -> None:
    system_includes = set()
    project_includes = set()
    linked_includes = set()
    main_string = ""
    for value in types_data.current_type["Attributes"].values():
        type_list = value["Type"].as_generic_type_list()

        for t in type_list:
            type_name = "::".join(t)

            if type_name not in set.union({"enum"},
                                          types_data.system_types["SystemTypes"],
                                          types_data.project_names,
                                          types_data.linked_names):
                raise KeyError(f"{type_name} is not defined in system_types or in a project json "
                               f"file. Unable to add include in cpp_indiv_generator.py")

            # add includes for project-types. e.g. for `Distr` that comes from `base::Distr`.
            if type_name in types_data.project_names:
                project_includes.add(f"#include \"{types_data.project_types[type_name]['Include']}\"")

            # add includes for linked project types
            if type_name in types_data.linked_names:
                linked_includes.add(f"#include \"{types_data.linked_types[type_name]['Include']}\"")

            # add includes for cpp-system-types if needed, e.g. <vector>. And also replace names with the cpp names.
            if type_name in types_data.system_types["SystemTypes"]:
                if "CppInclude" in types_data.system_types["SystemTypes"][type_name]:
                    system_includes.add(f"#include <{types_data.system_types['SystemTypes'][type_name]['CppInclude']}>")

    if len(system_includes) > 0:
        for el in sorted(system_includes):
            main_string += el + "\n"
    main_string += "\n"

    if len(linked_includes) > 0:
        for el in sorted(linked_includes):
            file.write(el + "\n")

    if len(project_includes) > 0:
        for el in sorted(project_includes):
            main_string += el + "\n"

    kind = types_data.current_type["Kind"].type_name_no_ns
    if kind == "Value":
        main_string += "#include \"mcf_core/Value.h\"\n\n"
    elif kind == "ExtMemValue":
        main_string += "#include \"mcf_core/ValueStore.h\"\n\n"
        main_string += "#if HAVE_CUDA\n"
        main_string += "#include \"mcf_cuda/CudaExtMemValue.h\"\n"
        main_string += "#else\n"
        main_string += "#include \"mcf_core/ExtMemValue.h\"\n"
        main_string += "#endif // HAVE_CUDA\n\n"
    else:
        main_string += "\n"
    file.write(main_string)
    add_namespace(file, types_data)


def add_include_guard(file: TextIO, types_data: 'TypesData') -> None:
    project_namespace = types_data.current_type['PackageNamespace']
    type_namespace = types_data.current_type['Directory']
    type_name = types_data.current_type['Name']
    guard_string = f"{project_namespace.upper()}_{type_namespace.upper()}_{type_name.upper()}_H_"

    file.write(f"#ifndef {guard_string}\n")
    file.write(f"#define {guard_string}\n\n")

    if types_data.current_type["Kind"].type_name_no_ns == "Enum":
        add_namespace(file, types_data)
        file.write(f"MSGPACK_ADD_ENUM(values::{project_namespace}::{type_namespace}::{type_name})\n\n")
    else:
        add_includes(file, types_data)

    file.write("#endif   // " + guard_string)


def write_individual_header_file(header_dir: 'Path', types_data: 'TypesData') -> None:
    assert_types_validity(types_data)
    filename = header_dir / (types_data.current_type["Name"] + ".h")
    execution_dir = os.getcwd()
    with open(filename, "w") as output_file:
        output_file.write("// WARNING: This file is generated automatically in the build process by mcf_tools/types_generator/value_type_generator.py.\n")
        output_file.write("// Any changes that you make will be overwritten whenever the project is built.\n")
        output_file.write("// To make changes either edit mcf_tools/types_generator/type_generator/cpp_indiv_generator.py or disable the generation in: \n"
                          f"// {execution_dir}\n\n\n")

        add_include_guard(output_file, types_data)
