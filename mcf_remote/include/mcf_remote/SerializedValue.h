/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_REMOTE_SERIALIZEDVALUE_H
#define MCF_REMOTE_SERIALIZEDVALUE_H

#include "mcf_core/Value.h"

#include <chrono>
#include <memory>

namespace mcf
{
namespace remote
{
/**
 * @brief A structure representing a serialized message as it is transferred on the wire
 *
 * This structure owns the serialized message parts and will explicitly take care of their
 * destruction.
 */
class SerializedValue
{
public:
    /**
     * Constructor
     *
     * @param valueBuffer A byte object representing a serialized value
     * @param bufferSize  The size of the serialized byte object valueBuffer
     * @param extMem      A pointer to the extMem part of the value referenced in valueBuffer.
     *                    In case valueBuffer is not an ExtMemValue, this argument shall be a unique_ptr
     *                    pointing to nullptr.
     * @param extMemSize  The size of the data extMem is pointing to.
     *                    In case valueBuffer is not an ExtMemValue, this argument shall be 0
     */
    explicit SerializedValue(
        std::unique_ptr<char[]>&& valueBuffer,
        std::size_t bufferSize,
        std::unique_ptr<char[]>&& extMem,
        std::size_t extMemSize);

    /**
     * Disallow copy constructors
     *
     * @param other The SerializedValue to be copied
     */
    SerializedValue(const SerializedValue& other) = delete;

    /**
     * Move constructor is the default implementation
     *
     * @param other The SerializedValue to be moved
     */
    SerializedValue(SerializedValue&& other) = default;

    /**
     *  Disallow assignment
     *
     * @param v The SerializedValue to be copied
     */
    SerializedValue& operator=(const SerializedValue& v) = delete;

    /**
     * Get a pointer to valueBuffer.
     * The ownership of valueBuffer stays with this SerializedValue instance.
     *
     * @return A pointer to valueBuffer.
     */
    char* valueBuffer() const { return _valueBuffer.get(); }

    /**
     * Get the size of valueBuffer in bytes
     *
     * @return The size of valueBuffer in bytes
     */
    std::size_t valueBufferSize() const { return _bufferSize; }

    /**
     * Check whether this SerializedValue instance does have an extMem part.
     *
     * @return false if extMem contains nullptr, true otherwise
     */
    bool extMemPresent() const { return _extMemPresent; }

    /**
     * Get a pointer to extmem.
     * The ownership of extMem stays with this SerializedValue instance.
     *
     * @return A pointer to extMem.
     */
    char* extMem() const { return _extMem.get(); }

    /**
     * Get the size of extMem in bytes
     *
     * @return The size of extmem in bytes
     */
    std::size_t extMemSize() const { return _extMemSize; }

    /**
     * Get the timestamp associated with this SerializedValue instance.
     * The timestamp corresponds to the moment in time when this SerializedValue instacne was
     * created, i.e. the execution of the constructor.
     *
     * @return The timestamp
     */
    std::chrono::steady_clock::time_point timestamp() const { return _timestamp; }

private:
    std::unique_ptr<char[]> _valueBuffer;
    std::size_t _bufferSize;
    bool _extMemPresent;
    std::unique_ptr<char[]> _extMem;
    std::size_t _extMemSize;
    std::chrono::steady_clock::time_point _timestamp;
};

} // namespace remote
} // namespace mcf

#endif // MCF_REMOTE_SERIALIZEDVALUE_H