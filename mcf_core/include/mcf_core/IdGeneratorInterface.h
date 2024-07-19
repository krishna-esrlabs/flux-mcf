/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_IDGENERATORINTERFACE_H_
#define MCF_IDGENERATORINTERFACE_H_

#include "mcf_core/Value.h"
#include <cstdint>
#include <memory>
#include <cassert>

namespace mcf
{
/**
 * Interface class for IdGenerators. The use of an IdGenerator is the only way to set/modify
 * the id field of a mcf::Value
 */
class IidGenerator
{
public:
    /**
     * Id generating function taking a reference as argument. The id generation shall be implemented in a child
     * class which shall pass the generated id and value reference to the function setId, which injects the id into
     * a value.
     * @param value  Reference of the value whose id shall be set
     */
    virtual void injectId(Value& value) const = 0;


protected:
    /**
     * Sets the id of a value. To be called in implementations of the function injectId(Value&)
     * @param value  Reference of the value whose id shall be set
     * @param id     The id which shall be injected into value
     */
    void setId(Value& value, uint64_t id) const { value._id = id; }
};

} // namespace mcf

#endif
