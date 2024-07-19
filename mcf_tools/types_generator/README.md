Introduction to MCF Value Types
===========

## Overview
The MCF framework maintains a `ValueStore` of typed values on various topics. The framework facilitates the 
transmission of these values between Components in two ways. First, these values can be transmitted via typed ports 
within component code. Second, via the RemoteControl, which allows reading and writing values directly to the ValueStore 
using a C++ or Python interface. See [README.md](../../README.md) for more details. 

Each of these values has a custom type definition, referred to as a value type, which defines the type's attributes and 
also information on how the type can be serialized for transmission. Value types must be defined in C++ and can 
optionally be defined in Python if the MCF Python API is used. A value type can either be:

* **Value**: Regular value type which inherits from `mcf::Value`. It contains a custom type definition which defines the 
type's attributes and also information on how the type can be serialized for transmission (using `MSGPACK_DEFINE`). It 
must also implement `operator<`.
  
  ```cpp
  struct ExampleValue : public mcf::Value
  {
      bool operator<(const TestContainers& other) const
      {
          return std::tie(intValue, containerValue, customValue) < std::tie(other.intValue, other.containerValue, other.customValue);
      }
  
      int intValue;
      std::vector<float> containerValue;
      CustomValue customValue;
      MSGPACK_DEFINE(intValue, containerValue, customValue)
  };
  ```

* **ExtMemValue**: Extended value type which contains the same attributes and serialization information as a Value, but 
derives from `mcf::ExtMemValue<T>` or `mcf::CudaExtMemValue<T>`. These values manage memory for larger objects, such as 
Images, Tensors etc which can be pre-allocated or dynamically allocated on the CPU or GPU. We currently support 
`uint8_t`, `uint16_t`, `int32_t` and `float` for the type of the larger object to be managed, `T`. The interface for 
managing the memory for these larger objects can be found in 
[ExtMemValue.h](../../mcf_core/include/mcf_core/ExtMemValue.h) and
[CudaExtMemValue.h](../../mcf_cuda/include/mcf_cuda/CudaExtMemValue.h).
  
  ```cpp
  struct ExampleExtMemValue : public mcf::ExtMemValue<uint8_t>  // or inherit from mcf::CudaExtMemValue<uint8_t>
  {
      bool operator<(const TestContainers& other) const
      {
          return std::tie(intValue, containerValue, customValue) < std::tie(other.intValue, other.containerValue, other.customValue);
      }
  
      int intValue;
      std::vector<float> containerValue;
      CustomValue customValue;
      MSGPACK_DEFINE(intValue, containerValue, customValue)
  };
  ```

## Value Type Attributes
Value types contain attributes which can be of the following types:

* Standard types (A full list of allowed types can be found in [AllowedTypes.json](./AllowedTypes.json))
  * **Primitive**: `bool`, `double`, `float`, `int`, `string`, `size_t`, `int8_t`, `int16_t`, `int32_t`, `uint32_t`, `uint64_t`.
  * **Container**: `vector`, `map`, `pair`, `set`. Note. A `map` must have a `string` as its key.
* Custom types
  * **Struct**: custom struct which should contain `MSGPACK_DEFINE`. Can also contain other custom Values, Structs and Enums.
  It must also implement `operator<`.
    
    ```cpp
    struct ExampleStruct
    {
        bool operator<(const ExampleStruct& other) const
        {
            return std::tie(intValue, containerValue, customValue) < std::tie(other.intValue, other.containerValue, other.customValue);
        }
        
        int intValue;
        std::vector<float> containerValue;
        CustomValue customValue;
        MSGPACK_DEFINE(exampleValue, customValue)
    };
    ```
  * **Enum**: custom enum which should contain `MSGPACK_ADD_ENUM`.
    
    ```cpp
    enum ExampleEnum {VALUE0, VALUE1, VALUE2};
    MSGPACK_ADD_ENUM(ExampleEnum)
    ```
  
  * **Values**: Values can be nested. However, a Value or ExtMemValue cannot contain another ExtMemValue.

## Creating Value Types

Manually creating value types can be tedious, error-prone and difficult to maintain. If using Python value types,
both C++ and Python value types must be maintained together, so that their type definitions match. Additionally, value 
types must be correctly namespaced to prevent naming clashes between different value types defined throughout a project. 

To deal with these issues, we have developed a value type generator, which automatically generates sets of C++ and Python 
value types from JSON configuration files and a specific folder structure.

See [GETTING_STARTED.md](GETTING_STARTED.md) for more details on generating value types.
