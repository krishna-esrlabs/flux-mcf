"""
Common definitions of abstract syntax objects and useful classes

Copyright (c) 2024 Accenture
"""
from typing import List, NamedTuple, Union, Any
import re
import abc
import builtins


class ConfigurationError(Exception):
    pass


def python_type_from_system_type(full_type_name: List[str], system_types: dict) -> Union[str, List[str]]:
    name = full_type_name[-1]
    for k, v in system_types["SystemTypes"].items():
        if name == k:
            return v["PyName"]
    return f"value_types.{python_type_with_namespace(full_type_name)}.{name}"


def cpp_type_from_system_type(name: str, system_types: dict) -> str:
    for k, v in system_types["SystemTypes"].items():
        if name == k:
            return v["CppName"]
    return name


def cpp_type_with_namespace(type_name: List[str]):
    return "::".join(type_name)


def python_type_with_namespace(type_name: List[str]):
    return ".".join(type_name)


class Type(abc.ABC):
    """
    An abstract representation of a type
    """
    def __init__(self, type_name: List[str]):
        self.type_name: List[str] = type_name
        assert type(type_name) == list

    @property
    def type_name_no_ns(self) -> str:
        """
        Return type name without namespace.
        """
        return self.type_name[-1]

    @property
    def type_name_str(self) -> str:
        """
        Return type name as string with namespaces delimited by ::
        """
        return cpp_type_with_namespace(self.type_name)

    @abc.abstractmethod
    def as_generic_type_list(self) -> List[List[str]]:
        """
        Flatten nested types and return list of lists. Inner lists contain namespace and type.
        E.g. Type string:  map<pair<int, float>, vector<my_ns::ExampleClass>>
             Output: [['map'], ['pair'], ['int'], ['float'], ['vector'], [my_ns, 'ExampleClass']]
        """
        pass

    @abc.abstractmethod
    def as_python_type(self, system_types: dict) -> str:
        """
        Flatten nested types and return list of python types. Ignores namespaces.
        E.g. Type string:  map<pair<int, float>, vector<project_ns::group_ns::ExampleClass>>
             Output: Dict[Tuple[int, float], List[value_types.project_ns.group_ns.ExampleClass]
        """
        pass

    @abc.abstractmethod
    def as_cpp_type(self, system_types: dict) -> str:
        """
        Flatten nested types and return list of cpp types. Ignores namespaces.
        E.g. Type string:  map<pair<int, float>, vector<my_ns::ExampleClass>>
             Output: std::map<std::pair<int, float>, std::vector<ExampleClass>>
        """
        pass


class Scalar(Type):
    def __init__(self, type_name: List[str]):
        super().__init__(type_name)

    def as_generic_type_list(self) -> List[List[str]]:
        return [self.type_name]

    def as_python_type(self, system_types: dict) -> str:
        return python_type_from_system_type(self.type_name, system_types)

    def as_cpp_type(self, system_types: dict) -> str:
        return cpp_type_from_system_type(cpp_type_with_namespace(self.type_name), system_types)

    def __eq__(self, other: 'Scalar'):
        return self.type_name == other.type_name


class Template(Type):
    def __init__(self, type_name: List[str], args: List[Type]):
        self.args = args
        super().__init__(type_name)

    def as_generic_type_list(self) -> List[List[str]]:
        def flatten(current_type: Type, current_type_list: List[List[str]]):
            current_type_list.append(current_type.type_name)

            if isinstance(current_type, Scalar):
                return

            for child_type in current_type.args:
                flatten(child_type, current_type_list)

        type_list = []
        flatten(self, type_list)
        return type_list

    def as_python_type(self, system_types: dict) -> str:
        def construct_python_type(t: Type) -> str:
            py_name = python_type_from_system_type(t.type_name, system_types)
            if type(t) is Template:
                return "{}[{}]".format(
                    py_name, ", ".join(construct_python_type(x) for x in t.args),
                )
            else:
                return py_name

        return construct_python_type(self)

    def as_cpp_type(self, system_types: dict) -> str:
        def construct_cpp_type(t: Type) -> str:
            type_name = cpp_type_with_namespace(t.type_name)
            py_name = cpp_type_from_system_type(type_name, system_types)
            if type(t) is Template:
                return "{}<{}>".format(
                    py_name, ", ".join(construct_cpp_type(x) for x in t.args),
                )
            else:
                return py_name

        return construct_cpp_type(self)

    def __eq__(self, other: 'Template'):
        if isinstance(other, Scalar) or len(self.args) != len(other.args):
            return False

        for self_arg, other_arg in zip(self.args, other.args):
            if self_arg != other_arg:
                return False

        return True


class ConversionException(BaseException):
    """
    A custom exception type
    """

    def __init__(self, explanation):
        self.explanation = explanation


class TypeNameParser:
    """
    A helper class that parses a single C++ type name

    Since we try to keep the dependency footprint low, we do not use pyparsing and write
    a parser by hand. Luckily, the grammar is simple enough.
    """
    @staticmethod
    def parse(string: str) -> Type:
        tokens = TypeNameParser._tokenize(string)
        return TypeNameParser._parse_scalar_or_template(tokens)

    @staticmethod
    def _tokenize(string: str) -> List[str]:
        """
        Splits a type declaration into tokens, removing whitespace. Tokens can be one of
        - literals such as name::name::...
        - comma
        - <
        - >
        :param string: The string to tokenize
        :return: A list of tokens
        """
        identifier = "[a-zA-Z_][a-zA-Z0-9_:]*"
        lbrace = "<"
        rbrace = ">"
        comma = ","

        tokens = []

        rest_string = string
        while len(rest_string) > 0:
            consumed = False
            for tok_re in [identifier, lbrace, rbrace, comma]:
                match = re.match(r"(\s*)({})(\s*)(.*)".format(tok_re), rest_string)
                if match:
                    groups = match.groups()
                    token = groups[1]
                    tokens.append(token)
                    rest_string = rest_string[len(groups[0])+len(token)+len(groups[2]):]
                    consumed = True
            if not consumed:
                raise ConversionException("Error in type name {}".format(string))
        return tokens

    @staticmethod
    def _parse_scalar_or_template(tokens: List[str]) -> Type:
        # basically, two alternatives:
        # either something<something, ...> or plain something
        typename = tokens[0]
        splitted_name = typename.split("::")
        if len(tokens) == 1 or tokens[1] != "<":
            return Scalar(splitted_name)
        else:
            # find matching brace and commas
            if tokens[-1] != ">":
                raise ConversionException(
                    "Mismatched braces in {}".format(" ".join(tokens))
                )
            brace_level = 0
            splits = [1]
            for i, token in enumerate(tokens[1:]):
                if token == "<":
                    brace_level += 1
                elif token == ">":
                    brace_level -= 1
                elif token == "," and brace_level == 1:
                    splits.append(i + 1)
            if brace_level != 0:
                raise ConversionException("Mismatched braces in " + " ".join(tokens))
            splits.append(-1)
            substrings = [tokens[splits[i]+1:splits[i+1]] for i in range(len(splits) - 1)]
            subtypes = [TypeNameParser._parse_scalar_or_template(s) for s in substrings]
            return Template(splitted_name, subtypes)


class TypesData(NamedTuple):
    current_type: dict
    project_types: dict
    system_types: dict
    project_names: list
    linked_types: dict
    linked_names: list


def is_enum_type(value_type: Type, types_data: TypesData) -> bool:
    """
    Returns if a type is an enumeration
    :param type_name: A value type object
    :return: True, of the type is an enum type
    """
    def helper(names, types) -> bool:
        return value_type.type_name_str in names and types[value_type.type_name_str]["Kind"].type_name_no_ns == "Enum"

    project_enum = helper(types_data.project_names, types_data.project_types)
    linked_enum = helper(types_data.linked_names, types_data.linked_types)

    return project_enum or linked_enum


def is_primitive_type(type_name: Union[str, List[str]], system_types: dict) -> bool:
    if isinstance(type_name, list):
        type_name = type_name[-1]
    return (type_name in system_types["SystemTypes"]
            and system_types["SystemTypes"][type_name]["Type"] == "Primitive")


def is_container_type(type_name: Union[str, List[str]], system_types: dict) -> bool:
    if isinstance(type_name, list):
        type_name = type_name[-1]
    return (type_name in system_types["SystemTypes"]
            and system_types["SystemTypes"][type_name]["Type"] == "Container")


def assert_map_key_string(value_type: dict, error_prefix_str: str) -> bool:
    parsed_type = value_type["Type"]
    if (parsed_type.type_name_no_ns == "map"
            and parsed_type.args[0].type_name_no_ns != "string"):
        raise ConfigurationError(f"{error_prefix_str}. Map key must be a string.")
    return True


def assert_container_type_valid(value_type: dict, system_types: dict, error_prefix_str: str) -> None:
    """
    Returns if all container types have the correct number of arguments.
    Otherwise, raises an exception.
    """
    parsed_type = value_type["Type"]
    if not is_container_type(parsed_type.type_name_no_ns, system_types):
        return

    if parsed_type.type_name_no_ns == "map":
        if len(parsed_type.args) != 2:
            raise ConfigurationError(f"{error_prefix_str}. Map should have 2 arguments.")
    elif parsed_type.type_name_no_ns == "pair":
        if len(parsed_type.args) != 2:
            raise ConfigurationError(f"{error_prefix_str}. Pair should have 2 argument.")
    elif parsed_type.type_name_no_ns == "vector" or parsed_type.type_name_no_ns == "set":
        if len(parsed_type.args) != 1:
            raise ConfigurationError(
                f"{error_prefix_str}. Vectors and sets should have 1 argument.")
    else:
        raise ConfigurationError(f"{error_prefix_str}. Unknown container type.")


def assert_type_permits_default_value(value_type: dict, types_data: 'TypesData', error_prefix_str: str) -> None:
    """
    Returns if:
        - there is no DefaultValue value or,
        - there is a DefaultValue value and the value type is a primitive, Enum or
          container of primitives.
    Otherwise, raises an exception.
    """
    if "DefaultValue" not in value_type:
        return

    parsed_type = value_type["Type"]
    if is_enum_type(value_type["Type"], types_data):
        return

    if is_primitive_type(parsed_type.type_name_no_ns, types_data.system_types):
        return

    if is_container_type(parsed_type.type_name_no_ns, types_data.system_types):
        for arg in parsed_type.args:
            if not is_primitive_type(arg.type_name_no_ns, types_data.system_types):
                raise ConfigurationError(
                    f"{error_prefix_str}. Default values can only be set for containers"
                    f" of primitive values.")
        return

    raise ConfigurationError(
        f"{error_prefix_str}. Default values can only be set for primitives, enums or"
        f" containers of primitive values.")


def assert_default_init_value_types_valid(value_type: dict, types_data: 'TypesData', error_prefix_str: str) -> None:
    """
    Returns if the type of the default value matches the type of the value.
    Otherwise, raises an exception.

    Note. assumes that the specified value type allows default values. Should call
    assert_type_permits_default_value() before this function.
    """
    def get_builtin(type_string: str) -> builtins.type:
        """
        Get python class type from type string.
        E.g. get_builtin('int') returns <class 'int'>   i.e. same return type as type(int)
        """
        return getattr(builtins, type_string)

    def is_primitive_correct_type(value: Any, target_type: str) -> bool:
        python_type = python_type_from_system_type([target_type], types_data.system_types)
        return type(value) == get_builtin(python_type)

    if "DefaultValue" not in value_type:
        return

    parsed_type = value_type["Type"]
    default_init = value_type["DefaultValue"]
    if is_enum_type(parsed_type, types_data):
        if type(default_init) != str:
            raise ConfigurationError(
                f"{error_prefix_str}. DefaultValue of enum should be a string.")

    if (is_primitive_type(parsed_type.type_name_no_ns, types_data.system_types) and
            not is_primitive_correct_type(default_init, parsed_type.type_name_no_ns)):
        raise ConfigurationError(
            f"{error_prefix_str}. DefaultValue of primitive doesn't match specified"
            f" type.")

    if is_container_type(parsed_type.type_name_no_ns, types_data.system_types):
        if parsed_type.type_name_no_ns == "map":
            # Use isinstance to check against subclasses of dict such as OrderedDict
            if not isinstance(default_init, dict):
                raise ConfigurationError(
                    f"{error_prefix_str}. Default value is not a map.")

            key_type = parsed_type.args[0].type_name_no_ns
            value_type = parsed_type.args[1].type_name_no_ns
            for key, value in default_init.items():
                if (not is_primitive_correct_type(key, key_type)
                        or not is_primitive_correct_type(value, value_type)):
                    raise ConfigurationError(
                        f"{error_prefix_str}. DefaultValue of map doesn't match specified "
                        f"type.")

        elif (parsed_type.type_name_no_ns == "vector"
              or parsed_type.type_name_no_ns == "set"):
            if type(default_init) != list:
                raise ConfigurationError(
                    f"{error_prefix_str}. Default value is not a list.")
            value_type = parsed_type.args[0].type_name_no_ns
            for value in default_init:
                if not is_primitive_correct_type(value, value_type):
                    raise ConfigurationError(
                        f"{error_prefix_str}. DefaultValue of list doesn't match specified"
                        f" type.")

        elif parsed_type.type_name_no_ns == "pair":
            if type(default_init) != list:
                raise ConfigurationError(
                    f"{error_prefix_str}. Default value is not a list.")

            if len(default_init) != 2:
                raise ConfigurationError(
                    f"{error_prefix_str}. DefaultValue for a pair should contain 2"
                    f" values.")
            value_type_0 = parsed_type.args[0].type_name_no_ns
            value_type_1 = parsed_type.args[1].type_name_no_ns
            if (not is_primitive_correct_type(default_init[0], value_type_0)
                    or not is_primitive_correct_type(default_init[1], value_type_1)):
                raise ConfigurationError(
                    f"{error_prefix_str}. DefaultValue of pair doesn't match specified "
                    f"type.")

        else:
            raise ConfigurationError(
                f"{error_prefix_str}. Unknown container type.")


def assert_default_values_valid(value_type: dict, types_data: 'TypesData', error_prefix_str: str) -> None:
    assert_type_permits_default_value(value_type, types_data, error_prefix_str)
    assert_default_init_value_types_valid(value_type, types_data, error_prefix_str)


def assert_types_validity(types_data: 'TypesData') -> None:
    for group in types_data.project_types.values():
        if group["Kind"].type_name_no_ns != "Enum":
            for value_name, value_type in group["Attributes"].items():
                error_prefix_str = f"Error in {group['Name']}::{value_name}"

                assert_map_key_string(value_type, error_prefix_str)
                assert_container_type_valid(value_type, types_data.system_types, error_prefix_str)
                assert_default_values_valid(value_type, types_data, error_prefix_str)
