/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/CountingIdGenerator.h"
#include "mcf_core/Value.h"

#include <unistd.h>
#include <chrono>

namespace mcf
{

CountingIdGenerator::CountingIdGenerator()
{
    uint64_t pid = getpid();              // int stored in a a uint64_t variable
    CountingIdGenerator::cnt = pid << 32; // shifted to the higher 32 bits, the lower 32 bits can be filled by a counter
}

void CountingIdGenerator::injectId(Value& value) const
{
    setId(value, ++CountingIdGenerator::cnt);
}

std::atomic<uint64_t> CountingIdGenerator::cnt(0);

} // namespace mcf

