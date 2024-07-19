/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_TYPEREGISTRY_H
#define MCF_TYPEREGISTRY_H

#include "mcf_core/Value.h"
#include "mcf_core/IExtMemValue.h"

#include "msgpack.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>

namespace mcf {

using ValuePtr = std::shared_ptr<const Value>;

class TypeRegistry {
public:
    using UnpackFunc = std::function<Value*(msgpack::object&, const void*, size_t, bool& isExtMem)>;
    using PackFunc = std::function<void(msgpack::packer<msgpack::sbuffer>&, ValuePtr, const void*&, size_t&, bool getPtr)>;

    struct TypemapEntry {
        std::string id;
        PackFunc packFunc;
        UnpackFunc unpackFunc;
    };

    template<typename T, typename=void>
    class FuncGen {
    public:
      static PackFunc packFunc(const std::string&);
      static UnpackFunc unpackFunc(const std::string&);
    };

    template<typename T>
    class FuncGen<T, typename std::enable_if<std::is_base_of<IExtMemValue, T>::value>::type > {
    public:
        static PackFunc packFunc(const std::string&);
        static UnpackFunc unpackFunc(const std::string&);
    };

    template<typename T>
    void registerType(const std::string& str);

    std::unique_ptr<TypemapEntry> getTypeInfo(const Value& value) const;
    std::unique_ptr<TypemapEntry> getTypeInfo(const std::string& id) const;

private:
    std::map<std::type_index, TypemapEntry> fByTypeIndex;
    std::map<std::string, TypemapEntry> fByTypeId;
};



template<typename T, typename U>
TypeRegistry::PackFunc
TypeRegistry::FuncGen<T, U>::packFunc(const std::string&) {
    return [](msgpack::packer<msgpack::sbuffer>& packer, ValuePtr value, const void*& ptr, size_t& len, bool) {
        auto val = std::dynamic_pointer_cast<const T>(value);
        packer.pack(*val);
        // we never give back a pointer in this case
        ptr = nullptr;
        len = 0;
    };
}

template<typename T, typename U>
TypeRegistry::UnpackFunc
TypeRegistry::FuncGen<T, U>::unpackFunc(const std::string&) {
    return [](msgpack::object &obj, const void*, size_t, bool& isExtMem) {
        isExtMem = false;
        return new T(obj.as<T>());
    };
}

template<typename T>
TypeRegistry::PackFunc
TypeRegistry::FuncGen<T, typename std::enable_if<std::is_base_of<IExtMemValue, T>::value>::type>::packFunc(const std::string&) {
    return [](msgpack::packer<msgpack::sbuffer>& packer, ValuePtr value, const void*& ptr, size_t& len, bool getPtr) {
        auto val = std::dynamic_pointer_cast<const T>(value);
        packer.pack(*val);
        if (getPtr) {
            ptr = static_cast<const void*>(val->extMemPtr());
            len = val->extMemSize();
        }
        else {
            ptr = nullptr;
            len = 0;
        }
    };
}

template<typename T>
TypeRegistry::UnpackFunc
TypeRegistry::FuncGen<T, typename std::enable_if<std::is_base_of<IExtMemValue, T>::value>::type>::unpackFunc(const std::string&) {
    return [](msgpack::object &obj, const void* ptr, size_t len, bool& isExtMem) {
        isExtMem = true;
        T* valptr = new T(obj.as<T>());
        if (len > 0 && ptr != nullptr) {
            valptr->extMemInit(len);
            memcpy(static_cast<void*>(valptr->extMemPtr()), ptr, len);
        }
        return valptr;
    };
}


template<typename T>
void TypeRegistry::registerType(const std::string& str) {
    TypemapEntry e;
    e.id = str;
    e.packFunc = FuncGen<T>::packFunc(str);
    e.unpackFunc = FuncGen<T>::unpackFunc(str);
    // assert to prevent having twice the same type
    assert( fByTypeIndex.find(std::type_index(typeid(T))) == fByTypeIndex.end() );
    fByTypeIndex[std::type_index(typeid(T))] = e;
    fByTypeId[e.id] = e;
}

inline std::unique_ptr<TypeRegistry::TypemapEntry> TypeRegistry::getTypeInfo(const Value& value) const {
    try {
        return std::make_unique<TypemapEntry>(fByTypeIndex.at((std::type_index(typeid(value)))));
    }
    catch (std::out_of_range& e) {
        return std::unique_ptr<TypemapEntry>();
    }
}

inline std::unique_ptr<TypeRegistry::TypemapEntry> TypeRegistry::getTypeInfo(const std::string& id) const {
    try {
        return std::make_unique<TypemapEntry>(fByTypeId.at(id));
    }
    catch (std::out_of_range& e) {
        return std::unique_ptr<TypemapEntry>();
    }
}


} // namespace mcf

#endif // MCF_TYPEREGISTRY_H
