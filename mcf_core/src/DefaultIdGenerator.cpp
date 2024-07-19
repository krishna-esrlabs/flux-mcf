/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/DefaultIdGenerator.h"
#include <unistd.h>
#include <chrono>
#include <string>

namespace mcf
{

DefaultIdGenerator::DefaultIdGenerator() : _hashBase(genHashBase())
{ }

void DefaultIdGenerator::injectId(Value& value) const
{
    uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    setId(value, _hashBase ^ now);
}

uint64_t DefaultIdGenerator::genHashBase()
{
    char hostname[256];
    gethostname(hostname, 256);

    int pid = getpid();

    std::string hostnamePid = std::string(hostname) + std::to_string(pid);

    uint64_t hash = static_cast<uint64_t>(std::hash<std::string>{}(hostnamePid));
    return hash;
}


} // namespace mcf
