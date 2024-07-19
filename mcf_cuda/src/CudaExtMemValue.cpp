/**
 * Copyright (c) 2024 Accenture
 */

#if HAVE_CUDA
#include "mcf_cuda/CudaExtMemValue.h"
#include "mcf_core/ErrorMacros.h"
#include "mcf_cuda/GenArray.h"
#include "mcf_cuda/CudaMemory.h"

namespace mcf {

// TODO: use instance of gen_array directly instead of ExtMem
template<typename T>
class CudaExtMemValue<T>::ExtMem {
public:
    ExtMem() : len(0) {};

    mcf::gen_array<T> genArray;
    uint64_t len;

};

template<typename T>
CudaExtMemValue<T>::CudaExtMemValue()
{
    fExtMem = std::make_unique<ExtMem>();
}

template<typename T>
CudaExtMemValue<T>::CudaExtMemValue(CudaExtMemValue&& o) noexcept
: fExtMem(std::move(o.fExtMem))
{
    Value::operator=(std::move(o));
}

/*
 * destructor
 */
template<typename T>
CudaExtMemValue<T>::~CudaExtMemValue() = default;

template<typename T>
void CudaExtMemValue<T>::extMemInit(mcf::cuda::unique_array<T>&& cudaArray) {
    extMemInit(cudaArray.size() * sizeof(T));
    fExtMem->genArray = std::move(mcf::gen_array<T>(std::move(cudaArray)));
}

template<typename T>
void CudaExtMemValue<T>::extMemInit(std::unique_ptr<T[]>&& array, uint64_t len) {
    extMemInit(len);
    fExtMem->genArray = std::move(mcf::gen_array<T>(std::move(array), len/sizeof(T)));
}

template<typename T>
void CudaExtMemValue<T>::extMemInit(mcf::gen_array<T>&& array) {
    extMemInit(array.size() * sizeof(T));
    fExtMem->genArray = std::move(array);
}

template<typename T>
void CudaExtMemValue<T>::extMemInit(uint64_t len) {
    // in case of re-init, free the previously allocated memory
    fExtMem = std::make_unique<ExtMem>();
    fExtMem->len = len;
    MCF_ASSERT(0 == len%sizeof(T), "Invalid size for selected datatype");
}

template<typename T>
void CudaExtMemValue<T>::extMemInit(const void *src, uint64_t len, int dstDevice, int srcDevice)
{
    if(dstDevice == -1 && srcDevice == -1)  // cpu->cpu copy
    {
        this->extMemInit(len);
        memcpy(this->extMemPtrImpl(dstDevice), src, len);
        return;
    }
    else if(dstDevice == -1)                // gpu->cpu copy
    {
        this->extMemInit(len);
        cudaMemcpy(this->extMemPtrImpl(dstDevice), src, len, cudaMemcpyDeviceToHost);
        return;
    }
    else if(srcDevice == -1)                // cpu->gpu copy
    {
        // determine current cuda device in order to restore later
        int currentDevice;
        MCF_CHECK_CUDA(cudaGetDevice(&currentDevice));

        MCF_CHECK_CUDA(cudaSetDevice(dstDevice));
        MCF_CHECK_CUDA_ERROR;
        this->extMemInit(len);
        cudaMemcpy(this->extMemPtrImpl(dstDevice), src, len, cudaMemcpyHostToDevice);

        // restore original cuda device
        MCF_CHECK_CUDA(cudaSetDevice(currentDevice));
        return;
    }
    else                                    // gpu->gpu copy
    {
        // determine current cuda device in order to restore later
        int currentDevice;
        MCF_CHECK_CUDA(cudaGetDevice(&currentDevice));

        MCF_CHECK_CUDA(cudaSetDevice(dstDevice));
        MCF_CHECK_CUDA_ERROR;
        this->extMemInit(len);
        cudaMemcpy(this->extMemPtrImpl(dstDevice), src, len, cudaMemcpyDeviceToDevice);

        // restore original cuda device
        MCF_CHECK_CUDA(cudaSetDevice(currentDevice));
        return;
    }
};

template<typename T>
uint64_t CudaExtMemValue<T>::extMemSize() const {
    return fExtMem->len;
}

template<typename T>
const typename CudaExtMemValue<T>::Ptr CudaExtMemValue<T>::extMemPtr(int device) const {
    return extMemPtrImpl(device);
}

template<typename T>
typename CudaExtMemValue<T>::Ptr CudaExtMemValue<T>::extMemPtr(int device) {
    return extMemPtrImpl(device);
}


/*
 * Convert into a generic array (creates different view on same shared data)
 */
template<typename T>
CudaExtMemValue<T>::operator mcf::gen_array<T>()
{
    return fExtMem->genArray;
}

/*
 * Convert into a generic array (creates different view on same shared data)
 */
template<typename T>
CudaExtMemValue<T>::operator const mcf::gen_array<T>() const
{
    return fExtMem->genArray;
}


template<typename T>
T* CudaExtMemValue<T>::extMemPtrImpl(int device) const {

    if (!extMemInitialized())
    {
        return nullptr;
    }

    // determine gen_array device ID
    // TODO: Use same device IDs for CudaExtMemValue and gen_array
    typename mcf::gen_array<T>::Device deviceId(mcf::gen_array<T>::Device::CPU);
    if (device == -1)
    {
        deviceId = mcf::gen_array<T>::Device::CPU;
    }
    else if (device == 0)
    {
        deviceId = mcf::gen_array<T>::Device::CUDA_0;
    }
    else if (device == 1)
    {
        deviceId = mcf::gen_array<T>::Device::CUDA_1;
    }
    else
    {
        MCF_THROW_RUNTIME("Invalid device ID");
    }

    // get pointer to memory on requested device
    const T* memPtr = fExtMem->genArray.get(deviceId);

    // if already allocated, return pointer
    if (memPtr != nullptr)
    {
        // TODO: this const_cast is unsafe. Ideally, CudaExtMemValue should
        //       not allow to get a non-const pointer to previously
        //       allocated memory.
        return const_cast<T*>(memPtr);
    }

    // otherwise allocate and return pointer to new memory
    return fExtMem->genArray.init(deviceId, (fExtMem->len)/sizeof(T));
}


template<typename T>
bool CudaExtMemValue<T>::extMemInitialized() const {
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
template class CudaExtMemValue<uint8_t>;
template class CudaExtMemValue<uint16_t>;
template class CudaExtMemValue<int32_t>;
template class CudaExtMemValue<float>;

} // mcf

#endif // HAVE_CUDA

