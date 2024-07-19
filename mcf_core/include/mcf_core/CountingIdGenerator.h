/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_COUNTINGIDGENERATOR_H_
#define MCF_COUNTINGIDGENERATOR_H_

#include "mcf_core/IdGeneratorInterface.h"

#include <cstdint>
#include <atomic>

namespace mcf
{

class Value;

/**
 * Simple implementation of an IdGenerator. The generated ids consist of the process id in the upper 32 bit and an
 * inremental, static counter in the lower 32 bits.
 * WARNING: Thread safe but not process safe
 */
class CountingIdGenerator : public IidGenerator
{
public:
    /**
     * Constructor queries and stores the current pid
     */
    CountingIdGenerator();

    /**
     * Injects an Id based on pid and current counter int the value and increses the counter
     * @param value Rvalue reference of the value whose id shall be set
     */
    virtual void injectId(Value& value) const override;
private:
    static std::atomic<uint64_t> cnt;
};


} // namespace mcf

#endif
