/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_IEXTMEMVALUE_H_
#define MCF_IEXTMEMVALUE_H_

#include "mcf_core/Value.h"

#include <cstdint>

namespace mcf {

/**
 * ExtMemValue interface
 */
class IExtMemValue : public Value {

public:

    ~IExtMemValue() override = default;

    virtual void extMemInit(uint64_t len) = 0;

    virtual uint64_t extMemSize() const = 0;

    virtual const uint8_t* extMemPtr() const = 0;

    virtual uint8_t* extMemPtr() = 0;

protected:

    IExtMemValue() = default;

};

}

#endif // MCF_IEXTMEMVALUE_H_