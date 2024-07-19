/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_CUDA_CUDAMEMORY_H_
#define MCF_CUDA_CUDAMEMORY_H_

#include "mcf_cuda/CudaCachingAllocator.h"
#include "mcf_cuda/CudaErrorHelper.h"

#include <iostream>
#include <memory>
#include <utility>

namespace mcf
{
namespace cuda
{
/**
 * A compile-time flag to control which on-device allocation method should be used.
 *
 * Setting it to `1` will use `cub::CachingDeviceAllocator` from
 * <https://github.com/NVIDIA/cub/blob/main/cub/util_allocator.cuh>. Empirically this is the faster
 * allocator.
 *
 * `0` will switch to "raw" `cudaMalloc`/`cudaFree` which seems to be slower on some target
 * architectures.
 */
#define USE_CUB_ALLOCATOR 1

/*
 * forward declarations
 */
template <typename T>
class unique_array;

/**
 * Return an array with given number of elements
 *
 * @param num_elems  the requested size of the array
 *
 * @return  unique pointer to the array
 */
template <typename T>
unique_array<T> make_array(size_t size);

/**
 * Unique array
 */
template <typename T>
class unique_array : public std::unique_ptr<T, void (*)(void*)>
{
    friend unique_array<T> make_array<T>(size_t size);

    typedef std::unique_ptr<T, void (*)(void*)> ptr_type;

public:
    /**
     * Default constructor, creates null pointer
     */
    unique_array();

    /**
     * Move constructor
     */
    unique_array(unique_array<T>&& arg) noexcept = default;

    /**
     * Move assignment
     */
    unique_array<T>& operator=(unique_array<T>&& arg) noexcept = default;

    /**
     * (Re-) size array to given number of elements
     *
     * If the array has not been allocated before, memory will be allocated
     * on the current CUDA device. If memory has been allocated before and the
     * current CUDA device is different from the one of the previous allocation,
     * an exception will be thrown.
     */
    void alloc(unsigned long size);

    /**
     * (Re-) size array to given number of elements, if its current size is smaller
     *
     * If the array has not been allocated before, memory will be allocated
     * on the current CUDA device. If memory has been allocated before and the
     * current CUDA device is different from the one of the previous allocation,
     * an exception will be thrown.
     */
    void increase(unsigned long size);

    /**
     * Swap with other array
     */
    void swap(unique_array<T>& other) noexcept;

    /**
     * Get size
     */
    size_t size() const;

    /**
     * Get cuda device
     *
     * @return cuda device ID of allocated memory or -1 if not yet allocated.
     */
    int getCudaDevice() const;

private:
    /**
     * Constructor from pointer
     *
     * Takes ownership of provided memory array on the given device
     */
    unique_array(T* p, unsigned long size, int cudaDevice);

    /**
     * The current size of the array
     */
    unsigned long mSize = 0;

    /**
     * The CUDA device on which memory has been allocated
     */
    int mCudaDevice = -1;
};

/*
 * array default constructor
 */
template <typename T>
unique_array<T>::unique_array() : ptr_type(nullptr, [](void* p) -> void {})
{
}

/*
 * Constructor from pointer, takes ownership of provided memory array
 */
template <typename T>
unique_array<T>::unique_array(T* p, unsigned long size, int cudaDevice)
//: ptr_type(p, [](void *ptr){if (ptr) cudaFree(ptr);})
: ptr_type(
    p,
    [](void* ptr) -> void {
        try
        {
            if (ptr)
            {
                cudaPointerAttributes attributes{};
                MCF_CHECK_CUDA(cudaPointerGetAttributes(&attributes, ptr));
#if USE_CUB_ALLOCATOR
                MCF_CHECK_CUDA(gpuAllocator.DeviceFree(attributes.device, ptr));
#else
                MCF_CHECK_CUDA(cudaFree(ptr));
#endif
            }
        }
        catch (const cuda_error& cudaError)
        {
            std::cerr << "CudaMemory: Deallocation failed." << std::endl
                      << cudaError.what() << std::endl;
        }
    })
, mSize(size)
, mCudaDevice(cudaDevice)
{
}

/*
 * (Re-) size array to given number of elements
 */
template <typename T>
void
unique_array<T>::alloc(unsigned long size)
{
    // determine current cuda device
    int cudaDevice = -1;
    MCF_CHECK_CUDA(cudaGetDevice(&cudaDevice));

    // if memory has been allocated previously, cuda device must be identical
    if (mCudaDevice != -1)
    {
        CUDA_ASSERT(
            cudaDevice == mCudaDevice,
            "Re-allocating a unique_array on a different CUDA device is not allowed");
    }

    // store cuda device
    mCudaDevice = cudaDevice;

    // re-allocate only, if size is not already correct
    if (size != mSize)
    {
        *this = std::move(make_array<T>(size));
    }
}

/*
 * (Re-) size array to given number of elements, if its current size is smaller
 */
template <typename T>
void
unique_array<T>::increase(unsigned long size)
{
    // determine current cuda device
    int cudaDevice = -1;
    MCF_CHECK_CUDA(cudaGetDevice(&cudaDevice));

    // if memory has been allocated previously, cuda device must be identical
    if (mCudaDevice != -1)
    {
        CUDA_ASSERT(
            cudaDevice == mCudaDevice,
            "Re-allocating a unique_array on a different CUDA device is not allowed")
    }

    // re-allocate only, if current size is smaller
    if (size > mSize)
    {
        *this = std::move(make_array<T>(size));
    }
}

/*
 * Swap array with another array object
 */
template <typename T>
void
unique_array<T>::swap(unique_array<T>& other) noexcept
{
    using std::swap;

    // swap all members
    swap(mSize, other.mSize);
    swap(mCudaDevice, other.mCudaDevice);

    ptr_type::swap(other);
}

/*
 * Get size
 */
template <typename T>
size_t
unique_array<T>::size() const
{
    return mSize;
}

/*
 * Get cuda device
 */
template <typename T>
int
unique_array<T>::getCudaDevice() const
{
    return mCudaDevice;
}

/*
 * Return an array with given number of elements
 */
// TODO: handle size 0
template <typename T>
unique_array<T>
make_array(size_t size)
{
    // determine current cuda device
    int cudaDevice = -1;
    MCF_CHECK_CUDA(cudaGetDevice(&cudaDevice));

    // allocate CUDA memory
    T* ptr = nullptr;
    if (size > 0UL)
    {
#if USE_CUB_ALLOCATOR
        MCF_CHECK_CUDA(gpuAllocator.DeviceAllocate(cudaDevice, (void**)&ptr, size * sizeof(T)));
#else
        MCF_CHECK_CUDA(cudaMalloc((void**)&ptr, size * sizeof(T)));
#endif
    }
    // assign to unique pointer and return
    return unique_array<T>(ptr, size, cudaDevice);
}

} // namespace cuda
} // namespace mcf

#endif /* MCF_CUDA_CUDAMEMORY_H_ */
