/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/ExtMemValue.h"
#include "mcf_core/ErrorMacros.h"

#include <cstring>
#include <stdexcept>

namespace mcf {

template<typename T>
class ExtMemValue<T>::ExtMem {
public:
    ExtMem(){};

    std::unique_ptr<T[]> ptr;
    uint64_t len{0};
};

template<typename T>
ExtMemValue<T>::ExtMemValue()
{
    fExtMem = std::make_unique<ExtMem>();
}

template<typename T>
ExtMemValue<T>::~ExtMemValue() = default;

template<typename T>
ExtMemValue<T>::ExtMemValue(ExtMemValue&& o) noexcept
        : fExtMem(std::move(o.fExtMem))
{
    Value::operator=(std::move(o));
}

template<typename T>
void ExtMemValue<T>::extMemInit(uint64_t len)
{
    // in case of re-init, free the previously allocated memory
    fExtMem = std::make_unique<ExtMem>();
    fExtMem->len = len;
    MCF_ASSERT(0 == len%sizeof(T), "Invalid size for selected datatype");
}

template<typename T>
void ExtMemValue<T>::extMemInit(std::unique_ptr<T[]>&& array, uint64_t len)
{
    extMemInit(len);
    fExtMem->ptr = std::move(array);
}

template<typename T>
void ExtMemValue<T>::extMemInit(const void *src, uint64_t len, int dstDevice, int srcDevice)
{
    if(dstDevice != -1 || srcDevice != -1)  // cpu->cpu copy
    {
        MCF_THROW_RUNTIME("ExtMemValue: Invalid device ID, cpu is always device -1");
    }
    this->extMemInit(len);
    memcpy(this->extMemPtrImpl(), src, len);
}

template<typename T>
uint64_t ExtMemValue<T>::extMemSize() const
{
    return fExtMem->len;
}

template<typename T>
const uint8_t* ExtMemValue<T>::extMemPtr() const
{
    return static_cast<const uint8_t*>(extMemPtr(CPU));
}

template<typename T>
uint8_t* ExtMemValue<T>::extMemPtr()
{
    return static_cast<uint8_t*>(extMemPtr(CPU));
}

template<typename T>
const typename ExtMemValue<T>::Ptr ExtMemValue<T>::extMemPtr(int device) const
{
    if (device != -1)
    {
        MCF_THROW_RUNTIME("Device id should be -1 for CPU ext mem value");
    }
    return extMemPtrImpl();
}

template<typename T>
typename ExtMemValue<T>::Ptr ExtMemValue<T>::extMemPtr(int device)
{
    if (device != -1)
    {
        MCF_THROW_RUNTIME("Device id should be -1 for CPU ext mem value");
    }
    return extMemPtrImpl();
}

template<typename T>
T* ExtMemValue<T>::extMemPtrImpl() const {
    if (!extMemInitialized())
    {
        return nullptr;
    }
    if (fExtMem->ptr == nullptr) {
        fExtMem->ptr = std::make_unique<T[]>(fExtMem->len/sizeof(T));
    }
    return const_cast<T*>(fExtMem->ptr.get());
}

template<typename T>
bool ExtMemValue<T>::extMemInitialized() const {
    return fExtMem->len > 0;
}

/**
 * We only support a very limited number of ext mem types.
 * The reason for this is that ext mem is by definition not fed through the message pack
 * serializer and thus the internal byte representation is forwarded to a receiver.
 *
 * Strictly speaking this is only correct for byte arrays.
 * In all other cases the platform specific representation of types at the sender side
 * must be considered by the receiver side.
 *
 * We still allow a few simple types other then uint8_t for performance and convenience
 * reasons and assume that a potential receiver will know how to handle this.
 */
template class ExtMemValue<uint8_t>;
template class ExtMemValue<uint16_t>;
template class ExtMemValue<int32_t>;
template class ExtMemValue<float>;


}  // namespace mcf