Getting Started with MCF Value Type Generator
====================

For an introduction to MCF value types, see [README.md](README.md).

The Value Type Generator automatically generates sets of C++ and Python value types from JSON definition files and a 
specific folder structure. Each set of value types is referred to as a Value Type Package. Each project can contain 
multiple Value Type Packages. Each Value Type Package can depend on other Value Type Packages.

## Quickstart

### Running the Generator

The value type generator is run by calling a python script with arguments. Run `python value_type_generator.py --help` 
for details.

To generate an example value type package, `mcf_example_types`:

```
python3 value_type_generator.py \
  -i      path/to/mcf_example_types/value_types_json \
  -o_cpp  path/to/mcf_example_types/include \
  -o_py   path/to/mcf_example_types/python \
  -i_link path/to/other_value_types_package \
  -i_link path/to/another_value_types_package
```

If a value type from the package being generated uses a value type from a different package as an attribute, it must 
link against that external package by passing the path to that package's json description file to the generator script with the `-i_link` argument.

### Cleaning Generated Types

#### Clean Value Type Package

To delete all generated files for a single value type package: 

```
python3 clean_generated_value_types.py \
  -o_cpp path/to/mcf_example_types/include \
  -o_py  path/to/mcf_example_types/python \
  -i     path/to/mcf_example_types/value_types_json
```

Note. the values of the arguments `-i`, `-o_cpp` and `-o_py` should match the values used when calling 
`value_type_generator.py`.

#### Clean Multiple Value Type Packages

The value type generator creates a cache file, `.cache.pkl`, for each value type package that it has successfully 
generated. If the value type definitions have not changed since the last time that the package was generated, the value
types will not be re-generated. To easily remove the cache file from one or more packages, the following script can
be called. It will recursively search for and remove all cache files under the input directory. This is particularly 
useful if you have modified the type generator, and would like to re-create all value types.

```
python clean_generated_value_type_caches.py \
  -d /path/to/root_dir \
  -n cache.pkl
```

### Automating Building and Cleaning

The value type generator generation and cleaning commands can also be added to a build script. See the 
`mcf_demos/mcf_value_type_demo` project for Cmake and Bake examples.

## Understanding the Value Type Generator

The structure of the value type package is best demonstrated with an example. In this example, we create a value type 
package, `mcf_example_types`, which contains two value type groups, `camera` and `odometry`. The package can be found 
`mcf_demos/mcf_value_type_demo`.

### Creating Value Type Package
The Value Type Package, `mcf_example_types`, should have the following folder structure:

```
mcf_example_types                      # Value type package root. Name is not used by generator.
├── value_types_json      
│   ├── ProjectDefinitions.json
│   ├── camera                          # Value type group
│   │   ├── CameraInfo.json             # Value type definition 
│   │   ├── ImageFormat.json
│   │   └── ImageUint8.json              
│   ├── odometry                        # Value type group
│   │   ├── Pose.json                   # Value type definition
│   │   ├── Position.json  
│   │   └── Rotation.json                
```

* **ProjectDefinitions.json**: defines the value of `PackageNamespace` which is used as the `package_namespace` for the 
generated types. It is also used for naming the "[Package header file](#understanding-generated-files)".
  ```json
  {
      "PackageNamespace": "mcf_example_types"
  }
  ```

* **Value Type Group Directories** (e.g. `first_value_type_group`): Directories which contain individual value type 
definitions. The names of these directories are used as the `group_namespace` for the generated types. They're also used
for naming the "[Group header files](#understanding-generated-files)".


* **Value Type Files** (e.g. `value_type_a.json`): Definition files which contain value type definitions. More 
information in [Writing JSON Type Definition Files](#writing-json-type-definition-files).

### Writing JSON Type Definition Files

Json value type definition files contain the following keys: 
* **Doc**: Documentation of the value type. Will be added to the generated value type files.
* **Kind**: (See [README.md](README.md) for details about each option) 
  * `Value`: Basic value type.
  * `ExtMemValue`: Extended value type that also manages dynamic / pre-allocated memory for large objects.
  * `Struct`: Struct which has the same attributes as `Value`, but cannot be a top level value. i.e. it can only be used
  as an attribute of a `Value` or `ExtMemValue`.
  * `Enum`: Cannot be a top level value. i.e. it can only be used as an attribute of a `Value` or `ExtMemValue`.
* **Attributes**: 
  * Map of data attributes:
    * **Keys**: Attribute names.
    * **Values**: 
      * **Type**: Type of attribute which can be a primitive, container or custom type. (See [README.md](./README.md#value-type-attributes) for details)
      * **Doc**: Documentation of the attribute. Will be added to the generated value type files.
      * **DefaultValue** (Optional): The default value of the attribute. (See [Default Values](#default-values) for details).
  

  Example: `mcf_example_types/value_types_json/camera/ImageUint8.json`
  
  ```json
  {
      "Doc": "Image container",
      "Kind": "ExtMemValue<uint8_t>",
      "Attributes":
      {
          "width":
          {
              "Type": "uint16_t",
              "DefaultValue" : 1920,
              "Doc": "Width of image"
          },
          "height":
          {
              "Type": "uint16_t",
              "DefaultValue" : 1080,
              "Doc": "Height of image"
          },
          "format":
          {
              "Type": "ImageFormat",
              "DefaultValue" : "RGB",
              "Doc": "Format of image"
          }
      }
  }
  ```
### Value Type Namespaces

The generated value types are created within two namespaces, `package_namespace` and `group_namespace`. These namespaces 
are used in the JSON value type definition files and also in the generated C++ types (Note. the C++ types have an 
additional outer namespace, `values`). The namespaces in the definition files are delimited by `::`, e.g. 
`values::<package_namespace>::<group_namespace>::<ValueTypeName>`. When a custom value type is used as an attribute of another value type, 
the value type generator requires the full namespace of the child value type. If the full namespace is not provided, 
then the value type generator makes assumptions about the location of the type and will automatically fill the 
namespaces based on the current package and / or group.

**Example**: We are creating a new value type, `MyType`, in `my_package/value_types_json/my_group/MyType.json`. We want 
to add a custom value type, `OtherType`, as an attribute of `MyType`. Depending on the namespaces we provide, the value 
type generator will make the following assumptions:

* *Full namespace provided*: The value type generator will look for the type (`OtherType`) in the `group_namespace` 
(`other_group`) of the `package_namespace` (`other_package`). In this case, we also have to add a dependency on 
`other_package` when running the value type generator (See [Running the Generator](#running-the-generator)).

  ```json
  "my_value":
  {
      "Type": "other_package::other_group::OtherType",
      "Doc": ""
  }
  ```

* *Group namespace provided*: The value type generator assumes that the type is in the same package but a different 
group. It will look for the type (`OtherType`) in the `group_namespace` (`other_group`) of the `package_namespace` 
(`my_package`).

  ```json
  "my_value":
  {
      "Type": "other_group::OtherType",
      "Doc": ""
  }
  ```
* *No namespace provided*: The value type generator assumes that the type is in the same package and group. It will look 
for the type (`OtherType`) in the `group_namespace` (`my_group`) of the `package_namespace` (`my_package`).

  ```json
  "my_value":
  {
      "Type": "OtherType",
      "Doc": ""
  }
  ```

### Default Values

The following attributes can have default values:
  * **Primitives**: `int`, `float`, `uin8_t`, `string`, `size_t` etc.
    
    ```json
    "primitive_attribute":
    {
        "Type": "float",
        "DefaultValue": "1.5",
        "Doc": "Float attribute"
    }
    ```
    
  * **Containers**: `vector`, `map` etc. However, they **must** be containers of primitives. A container of
  containers or custom types is permitted, however, they cannot have default values.
    * `vector`: 
    
      ```json
      "vector_attribute":
      {
          "Type": "vector<bool>",
          "DefaultValue": [true, false, true],
          "Doc": "Vector of bools"
      }
      ```
    
    * `map`: 
    
      ```json
      "map_attribute":
      {
          "Type": "map<string, float>",
          "DefaultValue": {"a": 0.0, "b": 1.0},
          "Doc": "Map of string / float pairs"
      }
      ```
    
    * `set`: 
    
      ```json
      "vector_attribute":
      {
          "Type": "set<int>",
          "DefaultValue": [0, 1, 2, 3, 4, 5],
          "Doc": "Set of ints"
      }
      ```
    
    * `pair`: 
    
      ```json
      "vector_attribute":
      {
          "Type": "pair<float, string>",
          "DefaultValue": [10.5, "example"],
          "Doc": "Pair of int and string"
      }
      ```
  
  * **Enums**:
      
    ```json
    "enum_attribute":
    {
        "Type": "CustomEnum",
        "DefaultValue": "VALUE1",
        "Doc": "Custom enum attribute"
    }
    ```

### Understanding Generated Files

Returning to our example of the `mcf_example_types` value types package. We can generate the C++ and python value types
by calling:

```python
    python value_type_generator.py \
       -i /path/to/mcf/mcf_demos/mcf_value_type_demo/mcf_example_types/value_types_json/ \
       -o_py /path/to/mcf/mcf_demos/mcf_value_type_demo/mcf_example_types/python \
       -o_cpp /path/to/mcf/mcf_demos/mcf_value_type_demo/mcf_example_types/include
```

This will result in the following directory structure:

```
mcf_example_types
├── value_types_json                      # Value type definitions that were defined previously
├── include                               # Generated c++ files
│   ├── mcf_example_types
│   │   ├── McfExampleTypes.h            # Package header file
│   │   ├── camera
│   │   │   ├── CameraTypes.h             # Group header file
│   │   │   ├── CameraInfo.h              # Individual header file
│   │   │   ├── ImageFormat.h
│   │   │   ├── ImageUint8.h
│   │   ├── odometry
│   │   │   ├── OdometryTypes.h           # Group header file
│   │   │   ├── Pose.h                    # Individual header file
│   │   │   ├── Position.h
│   │   │   ├── Rotation.h
├── python                                # Generated python files
│   ├── value_types
│   │   ├── mcf_example_types
│   │   │   ├── camera
│   │   │   │   ├── CameraInfo.py         # Invidual python file
│   │   │   │   ├── ImageFormat.py
│   │   │   │   ├── ImageUint8.py
│   │   │   ├── odometry
│   │   │   │   ├── Pose.py
│   │   │   │   ├── Position.py
│   │   │   │   ├── Rotation.py
```

* **Package header file** (C++): 
  * Each package contains a package header file which includes all group header files in the group. Can be included to 
  easily include all value types within the package.
  * Defines a function for registering each value type within the package with the `ValueStore`.
  * *Example file*: `mcf_example_types/include/mcf_example_types/McfExampleTypes.h`


* **Group header files** (C++):
  * Each group contains a group header file which includes all header files in the group. Can be included to easily 
  include all value types within the group.
  * Defines a function for registering each value type within the group with the `ValueStore`.
  * *Example file*: `mcf_example_types/include/mcf_example_types/camera/CameraTypes.h`
 

* **Individual header files** (C++):
  * Contain generated C++ value types. 
  * Types are in the package and group namespaces: `values::package_namespace::group_namespace::ValueType`.
  * *Example type*: `values::mcf_example_types::camera::ImageUint8`
  * *Example file*: `mcf_example_types/include/mcf_example_types/camera/ImageUint8.h`
 

* **Individual Python files** (Python): 
  * Contain generated python value types.
  * *Example file*: `mcf_example_types/python/value_types/mcf_example_types/camera/ImageUint8.py`
