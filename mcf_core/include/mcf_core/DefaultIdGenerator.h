/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_DEFAULTIDGENERATOR_H_
#define MCF_DEFAULTIDGENERATOR_H_

#include "mcf_core/IdGeneratorInterface.h"
#include <cstdint>

namespace mcf
{

class Value;

/**
 * Default implementation of the IdGenerator. The generated ids consist of hash of the concatenated
 * hostname and current pid, xored with the current timestamp (nanoseconds since epoch start) at
 * the time of id generation
 */
class DefaultIdGenerator : public IidGenerator
{
public:
    /**
     * Constructor queries and stores a hash of the hostname + the current pid
     */
    DefaultIdGenerator();

    /**
     * Injects an Id based on pid, hash of the host name and a timestamp
     * @param value Reference of the value whose id shall be set
     */
    virtual void injectId(Value& value) const override;

private:
    /**
     * Generates a base for the ids to be injected consisting of a hash of the concatenated
     * hostname and pocess id. This hashBase will be xored with the timestamp of the id generation
     * to form a unique id.
     */
    static uint64_t genHashBase();

    const uint64_t _hashBase;
};


} // namespace mcf

#endif
