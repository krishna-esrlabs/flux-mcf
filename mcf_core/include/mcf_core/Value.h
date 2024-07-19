/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_VALUE_H_
#define MCF_VALUE_H_

#include <cstdint>

namespace mcf
{

class TypeRegistry;

class Value
{
    friend class IidGenerator;
public:
    Value() = default;
    Value(const Value& v) = default;
    Value(Value&& v) = default;
    virtual ~Value()      = default;

    Value& operator=(const Value& v) = default;
    Value& operator=(Value&& v) = default;

    uint64_t id() const
    {
        return _id;
    }

private:
    uint64_t _id = 0;
};

} // namespace mcf

#endif
