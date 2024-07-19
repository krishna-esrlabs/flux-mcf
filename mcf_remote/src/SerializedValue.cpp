/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/SerializedValue.h"

#include "mcf_core/ErrorMacros.h"

#include <memory>

namespace mcf
{
namespace remote
{
SerializedValue::SerializedValue(
    std::unique_ptr<char[]>&& valueBuffer,
    std::size_t bufferSize,
    std::unique_ptr<char[]>&& extMem,
    std::size_t extMemSize)
: _valueBuffer(std::move(valueBuffer))
, _bufferSize(bufferSize)
, _extMemPresent(extMem != nullptr)
, _extMem(std::move(extMem))
, _extMemSize(extMemSize)
, _timestamp(std::chrono::steady_clock::now())
{
}

} // namespace remote

} // namespace mcf
