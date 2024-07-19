/**
 * Copyright (c) 2024 Accenture
 */

#if HAVE_CUDA

#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "mcf_cuda/CudaCachingAllocator.h"
#include "mcf_core/ErrorMacros.h"
#include "mcf_cuda/CudaErrorHelper.h"
#include "mcf_cuda/CudaMemory.h"
#include "mcf_cuda/GenArray.h"

namespace mcf {

namespace
{

/**
 * Returns an ID, representing the device which holds the memory this pointer is pointing to
 * @return -1 or -2 if the memory resides on the CPU
 *         A positive integer representing the ID of the CUDA device which holds the corresponding memory
 */
inline int getPtrType(const void* ptr)
{
    cudaPointerAttributes attributes;
    cudaPointerGetAttributes(&attributes , ptr);

    // TODO: error is no more thrown in Cuda 11.3 and attributes.device
    //  returns -2 for CPU, remove the error checking in future
    cudaError err = cudaGetLastError();
    if(err != cudaSuccess) {
        // check if the only says we are not dealing with CUDA allocated memory
        if(err == cudaErrorInvalidValue) {
            // memory resides on CPU, return -1
            return -1;
        }
        // throw exception in case of other errors
        MCF_CHECK_CUDA_ERROR(err);
    }

    // return the device ID in case of no error
    return attributes.device;
}

/**
 * Helper function converting device ID to device index
 */
inline size_t getDeviceIndex(gen_array_base::Device device)
{
    // check device
    if (device < gen_array_base::Device::FIRST_ID || device >= gen_array_base::Device::LAST_ID)
    {
        MCF_THROW_RUNTIME("Invalid device ID.");
    }

    // determine and check target device ID
    return static_cast<ssize_t>(device) - static_cast<ssize_t>(gen_array_base::Device::FIRST_ID);
}

/**
 * Create CPU array of requested size
 *
 * @param numElems      the number of elements to allocate
 *
 * @return shared pointer with correct custom deleter
 */
template<typename T>
inline std::shared_ptr<T> createCpuArray(size_t numElems)
{
    // create unique array and convert to shared pointer with custom deleter
    std::unique_ptr<T[]> alloced = std::make_unique<T[]>(numElems);
    return std::shared_ptr<T>(alloced.release(), std::default_delete<T[]>());
}

/**
 * Create Cuda array of requested size on requested device
 *
 * @param cuda_device   CUDA device ID
 * @param numElems      the number of elements to allocate
 *
 * @return shared pointer with correct custom deleter to free CUDA memory
 */
template<typename T>
std::shared_ptr<T> createCudaArray(int cudaDevice, size_t numElems)
{
    // allocate CUDA memory
    T* ptr = nullptr;
#if USE_CUB_ALLOCATOR
    MCF_CHECK_CUDA(
        mcf::cuda::gpuAllocator.DeviceAllocate(cudaDevice, (void**)&ptr, numElems * sizeof(T)));
#else
    MCF_CHECK_CUDA(cudaMalloc((void**)&ptr, numElems * sizeof(T)));
#endif
    // create a shared pointer with matching deleter
    return std::shared_ptr<T>(ptr, [cudaDevice](T* ptr) {
        try
        {
            if (ptr)
            {
#if USE_CUB_ALLOCATOR
                MCF_CHECK_CUDA(mcf::cuda::gpuAllocator.DeviceFree(cudaDevice, ptr));
#else
                MCF_CHECK_CUDA(cudaFree(ptr));
#endif // USE_CUB_ALLOCATOR
            }
        }
        catch (mcf::cuda::cuda_error cudaError)
        {
            std::cerr << "GenArray: Deallocation failed." << std::endl;
            std::cerr << cudaError.what() << std::endl;
            return;
        }
    });
}

/**
 * Create array of requested size on requested device
 */
template<typename T>
std::shared_ptr<T> makeArrayForDevId(gen_array_base::Device device, size_t numElems)
{
    // shared pointer object to be returned
    std::shared_ptr<T> array;

    // allocate with custom deleter depending on device
    switch(device)
    {
    // array in CPU memory
    case gen_array<T>::Device::CPU:
    {
        array = createCpuArray<T>(numElems);
        break;
    }

    // array in CUDA memory, cuda device 0
    case gen_array<T>::Device::CUDA_0:
    {
        array = createCudaArray<T>(0, numElems);
        break;
    }

    // array in CUDA memory, cuda device 1
    case gen_array<T>::Device::CUDA_1:
    {
        array = createCudaArray<T>(1, numElems);
        break;
    }

    default:
        MCF_THROW_RUNTIME("Cannot allocate memory: unsupported device ID");
    }

    return array;
}

/**
 * Create array of requested size on requested device
 */
template<typename T>
inline std::shared_ptr<T> makeArrayForDevIndex(size_t deviceIndex, size_t numElems)
{
    // determine device ID from device index
    gen_array_base::Device device(
            static_cast<gen_array_base::Device>(
                    static_cast<ssize_t>(deviceIndex) +
                    static_cast<ssize_t>(gen_array_base::Device::FIRST_ID)));

    // create array from device ID
    return makeArrayForDevId<T>(device, numElems);
}

/**
 * Copy the given number of bytes from CPU memory to CUDA memory
 *
 * @param tgt       target address in CUDA memory
 * @param src       source address in CPU memory
 * @param numBytes  the number of bytes to copy
 */
void memcpyCpuToCuda(void* tgt, const void* src, size_t numBytes)
{
    MCF_CHECK_CUDA(cudaMemcpy(tgt, src, numBytes, cudaMemcpyHostToDevice));
}

/**
 * Copy the given number of bytes from CUDA memory to CPU memory
 *
 * @param tgt       target address in CPU memory
 * @param src       source address in CUDA memory
 * @param numBytes  the number of bytes to copy
 */
void memcpyCudaToCpu(void* tgt, const void* src, size_t numBytes)
{
    MCF_CHECK_CUDA(cudaMemcpy(tgt, src, numBytes, cudaMemcpyDeviceToHost));
}

/**
 * Copy the given number of bytes from CUDA memory to CUDA memory
 *
 * @param tgt       target address in CUDA memory
 * @param src       source address in CUDA memory
 * @param numBytes  the number of bytes to copy
 */
void memcpyCudaToCuda(void* tgt, const void* src, size_t numBytes)
{
    MCF_CHECK_CUDA(cudaMemcpy(tgt, src, numBytes, cudaMemcpyDeviceToDevice));
}

/**
 * Copy the given number of bytes from CPU memory to CPU memory
 *
 * @param tgt       target address in CUDA memory
 * @param src       source address in CUDA memory
 * @param numBytes  the number of bytes to copy
 */
void memcpyCpuToCpu(void* tgt, const void* src, size_t numBytes)
{
    memcpy(tgt, src, numBytes);
}

/**
 * Memcopy function type
 */
typedef void (*CopyFct)(void* tgt, const void* src, size_t numBytes);

/**
 * Table of copy functions
 *
 * copyFct[tgt][src] yields
 */
const CopyFct COPY_FCT[gen_array_base::NUM_DEVICES][gen_array_base::NUM_DEVICES] =
{ // source
  // Device::CPU  | Device::CUDA_0  | Device::CUDA_1
  {memcpyCpuToCpu, memcpyCudaToCpu, memcpyCudaToCpu},    // target Device::CPU
  {memcpyCpuToCuda, memcpyCudaToCuda, memcpyCudaToCuda}, // target Device::CUDA_0
  {memcpyCpuToCuda, memcpyCudaToCuda, memcpyCudaToCuda}, // target Device::CUDA_1
};

/**
 * Memcopy between devices
 *
 * @param tgtPtr        pointer to target memory on target device
 * @param tgtDevIndex   target device index (i.e. device ID - FIRST_ID)
 * @param srcPtr        pointer to source memory on source device
 * @param srcDevIndex   source device index (i.e. device ID - FIRST_ID)
 * @param size          number of bytes to copy
 */
inline void deviceMemcopy(void* tgtPtr, size_t tgtDevIndex,
        const void* srcPtr, size_t srcDevIndex, size_t size)
{
    // call copy function
    COPY_FCT[tgtDevIndex][srcDevIndex](tgtPtr, srcPtr, size);
}

} // anonymous namespace


/**
 * Pointer Container helper class
 */
template<typename T>
class gen_array_ptrs
{
public:

    /**
     * Shared pointers to the allocated memory locations per device.
     * Null, if not yet allocated.
     *
     * (Each shared pointer will be created with a deleter function or
     * deleter functional object to take care of resource cleanup
     * according to device and number of elements.)
     */
    std::shared_ptr<const T> fPtrs[gen_array_base::NUM_DEVICES];

    /**
     * For thread safety of internal data copying.
     *
     * Note: Accessing the same gen_array instance from multiple threads
     *       is *not* thread safe. However, usage of memory that is shared
     *       between different instances *is* thread safe. => Different
     *       threads can share data by using separate copies of a gen_array.
     *
     * Needs to be locked also in const objects, because const objects do
     * create internal copies of data => must be mutable
     */
    mutable std::mutex fPtrMutex;

};



/*
 * Default constructor
 */
template<typename T>
gen_array<T>::gen_array()
: fNumElems(0U)
, fPtrContainer(std::make_shared<gen_array_ptrs<T>>())
{
}

/*
 * Constructor consuming a unique pointer array of objects in CPU memory
 */
template<typename T>
gen_array<T>::gen_array(std::unique_ptr<T[]>&& source, size_t numElems)
: fNumElems(numElems)
, fPtrContainer(std::make_shared<gen_array_ptrs<T>>())
{
    // determine device on which memory is allocated, make sure it is CPU
    // TODO: this should probably be moved to another entity handling
    //       the relation between hardware features and gen_array device IDs.
    int memDevice = getPtrType(source.get());
    if (memDevice > -1)
    {
        MCF_THROW_RUNTIME("Can only handle CPU memory in std::unique_ptr<T[]>");
    }

    // convert to shared pointer with custom deleter
    // TODO: the next line is to ensure compatibility with gcc <= 5 and gcc >= 6
    //       Once support for gcc 5 is not needed any more (after full
    //       transition to Drive10 or higher) the type of array and the
    //       following line should be changed to
    //       std::shared_ptr<T[]> array(std::move(source));
    std::shared_ptr<T> array(source.release(), source.get_deleter());

    // store in array of pointers
    fPtrContainer->fPtrs[getDeviceIndex(Device::CPU)] = array;
}

/*
 * Constructor consuming a cuda unique array
 */
template<typename T>
gen_array<T>::gen_array(mcf::cuda::unique_array<T>&& source)
: fNumElems(source.size())
, fPtrContainer(std::make_shared<gen_array_ptrs<T>>())
{
    // determine cuda device on which memory is allocated
    int cudaDevice = getPtrType(source.get());

    // determine gen_array device ID from CUDA device ID
    // TODO: this should probably be moved to another entity handling
    //       the relation between hardware features and gen_array device IDs.
    Device deviceId(Device::CUDA_0);
    switch(cudaDevice)
    {
    case 0:
    {
        deviceId = Device::CUDA_0;
        break;
    }
    case 1:
    {
        deviceId = Device::CUDA_1;
        break;
    }
    default:
    {
        MCF_THROW_RUNTIME("Found invalid CUDA device in cuda unique_array: device " + std::to_string(cudaDevice));
    }
    }

    // convert cuda unique_array to shared pointer with custom deleter
    std::shared_ptr<T> array(source.release(), cudaFree);

    // convert device ID to device index and store pointer
    fPtrContainer->fPtrs[getDeviceIndex(deviceId)] = array;
}

/*
 * Destructor
 */
template<typename T>
gen_array<T>::~gen_array()
{
}

/*
 * Swap with another array of same type
 */
template<typename T>
void gen_array<T>::swap(gen_array<T>& other)
{
    using std::swap;

    // swap sizes
    swap(fNumElems, other.fNumElems);

    // swap pointer container
    swap(fPtrContainer, other.fPtrContainer);
}

/*
 * Create a new array with given number of elements on the specified device.
 * Resets the object, frees any previously allocated memory.
 * The returned pointer can be used to fill data into the array.
 */
template<typename T>
T* gen_array<T>::init(Device device, size_t numElems)
{
    // create a new generic array
    gen_array<T> genArray;

    // create an array on the specified device
    std::shared_ptr<T> array = makeArrayForDevId<T>(device, numElems);

    // store array and size
    genArray.fPtrContainer->fPtrs[getDeviceIndex(device)] = array;
    genArray.fNumElems = numElems;

    // swap new array with this array
    this->swap(genArray);

    // return shared pointer to new array on target device
    return array.get();
}

/**
 * Return a shared pointer to a const array on the desired device.
 *
 * If the array contents is not yet available on the specified device,
 * the required memory on that device will be allocated and the contents
 * will be copied into it.
 *
 * @param device    the device
 *
 * @return  shared pointer with correct deleter
 */
template<typename T>
const T* gen_array<T>::get(Device device) const
{
    size_t tgtIndex = getDeviceIndex(device);

    // prevent concurrent access to pointers
    const std::lock_guard<std::mutex> lock(fPtrContainer->fPtrMutex);

    // copy data to desired device
    copyToTarget(tgtIndex);

    // return resulting pointer
    return fPtrContainer->fPtrs[tgtIndex].get();
}


/*
 * Copy memory content to the specified target device
 *
 * Note: mutex of fPtrContainer must be locked before calling
 */
template<typename T>
void gen_array<T>::copyToTarget(size_t tgtIndex) const
{
    // if data already on target device, do nothing
    if (fPtrContainer->fPtrs[tgtIndex])
    {
        return;
    }

    // otherwise, copy from first device already having the data
    // TODO: could possibly be optimized so that same device types (e.g CUDA)
    //       are checked first
    for (size_t srcIndex = 0; srcIndex < NUM_DEVICES; ++srcIndex)
    {
        if (fPtrContainer->fPtrs[srcIndex])
        {
            // create array on target device
            std::shared_ptr<T> array = makeArrayForDevIndex<T>(tgtIndex, fNumElems);

            // copy data to target array
            deviceMemcopy(array.get(), tgtIndex,
                    fPtrContainer->fPtrs[srcIndex].get(), srcIndex, fNumElems * sizeof(T));

            // store pointer to copied array
            fPtrContainer->fPtrs[tgtIndex] = array;

            // exit loop
            break;
        }
    }

    // at this point, the data have been copied to the target device remains null
}


/*
 * Check if a copy for the specified device is already available
 * (without creating one, in case it is not)
 */
template<typename T>
bool gen_array<T>::hasCopyOnDevice(Device device) const
{
    size_t devIndex = getDeviceIndex(device);

    // prevent concurrent access to pointers
    const std::lock_guard<std::mutex> lock(fPtrContainer->fPtrMutex);

    return (nullptr != fPtrContainer->fPtrs[devIndex].get());
}


/*
 * Check if pointer is null on all devices, i.e. does not carry any content
 */
template<typename T>
bool gen_array<T>::isNull() const
{
    // prevent concurrent access to pointers
    const std::lock_guard<std::mutex> lock(fPtrContainer->fPtrMutex);

    for (auto ptr: fPtrContainer->fPtrs)
    {
        if(ptr != nullptr)
            return false;
    }

    return true;
}

/*
 * Helper function converting a CUDA device to a gen_array Device ID
 */
gen_array_base::Device deviceIdFromCuda(int cudaDevice)
{
    gen_array_base::Device device(gen_array_base::Device::CPU);
    if (0 == cudaDevice)
    {
        device = gen_array_base::Device::CUDA_0;
    }
    else if (1 == cudaDevice)
    {
        device = gen_array_base::Device::CUDA_1;
    }

    return device;
}

/*
 * Explicit instantiations of gen_array
 */
template class gen_array<uint8_t>;
template class gen_array<uint16_t>;
template class gen_array<int32_t>;
template class gen_array<float>;

} /* namespace mcf */

#endif // HAVE_CUDA

