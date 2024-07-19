/**
 * Copyright (c) 2024 Accenture
 */

#ifndef INCLUDE_MCF_GENARRAY_H_
#define INCLUDE_MCF_GENARRAY_H_

#include <memory>

/*
 * forward declarations
 */
namespace mcf { namespace cuda {
template<typename T> class unique_array;
} }

namespace mcf {

// forawrd declaration of pointer container
template<typename T> class gen_array_ptrs;

/**
 * Generic Array base class
 */
class gen_array_base
{
public:
    /**
     * Available device IDs
     *
     * Note: This must be kept in sync with array
     *       COPY_FCT in implementation file!
     */
    enum class Device : int32_t
    {
        FIRST_ID = -1,
                CPU = FIRST_ID,
                CUDA_0,
                CUDA_1,
                LAST_ID
    };


    /**
     * The number of available devices
     */
    static constexpr size_t NUM_DEVICES =
            static_cast<ssize_t>(Device::LAST_ID) -
            static_cast<ssize_t>(Device::FIRST_ID);
};

/**
 * Generic array with automatic inter-device view functionality
 *
 * Arrays created in the memory space of one device (e.g. CPU/Cuda) can be
 * automatically transferred to another device (e.g Cuda) when required.
 * Required copies of contents will be created and managed automatically.
 *
 * Note: Since the implementation of this template class in in a separate
 *       cpp file, users must explicitly create instantiations for new
 *       types. For examples, have a look at the bottom of gen_array.cpp
 *
 * Note: Correct operation is only possible with basic types or structures
 *       of basic types, because copying of array contents is carried out
 *       using memcpy() operations and thus requires data to be arranged
 *       in a continuous memory block.
 *
 * Note: Accessing the same gen_array instance from multiple threads
 *       is *not* thread safe. However, usage of memory that is shared
 *       between different instances *is* thread safe. => Different
 *       threads can share data by using separate copies of a gen_array.
 */
template<typename T>
class gen_array : public gen_array_base
{

public:

    /**
     * Default constructor
     */
    gen_array();

    /**
     * Constructor consuming a unique pointer array of objects in CPU memory
     *
     * @param source    unique array to be converted into a generic array
     * @param numElems  the number of elements in the array
     *
     * Note: Since the number of elements in a unique_ptr array cannot be
     *       queried, its size must be passed separately
     */
    gen_array(std::unique_ptr<T[]>&& source, size_t numElems);

    /**
     * Constructor consuming a cuda unique array
     */
    gen_array(mcf::cuda::unique_array<T>&& source);

    /**
     * Destructor
     */
    virtual ~gen_array();

    /**
     * Return the number of elements stored in the array
     */
    size_t size() const
    { return fNumElems; }

    /**
     * Swap with another array of same type
     *
     * @param other     the array to swap with
     */
    void swap(gen_array<T>& other);

    /**
     * Create a new array with given number of elements on the specified device.
     * Resets the object, frees any previously allocated memory.
     * The returned pointer can be used to fill data into the array.
     *
     * Note: Once the contents has been copied to another device, updates
     *       of the original data using the returned pointer will not be
     *       synchronized to that other device. Thus, this method should
     *       be used carefully.
     *       Moving in pre-allocated memory via one of the constructors
     *       is to be preferred.
     *
     * @param device    The device ID
     * @param numElems  The number of elements
     *
     * @return  shared pointer to the created array with appropriate deleter.
     */
    T* init(Device device, size_t numElems);

    /**
     * Return a pointer to a const array on the desired device.
     *
     * If the array contents is not yet available on the specified device,
     * the required memory on that device will be allocated and the contents
     * will be copied into it.
     *
     * If memory has not yet been allocated on any device,
     * the returned pointer will be null.
     *
     * @param device    the device
     *
     * @return  pointer with correct deleter or null-pointer
     */
    const T* get(Device device) const;

    /**
     * Check if a copy for the specified device is already available
     * (without creating one, in case it is not)
     */
    bool hasCopyOnDevice(Device device) const;

    /**
     * Check if pointer is null on all devices, i.e. does not carry any content
      */
    bool isNull() const;

private:

    /**
     * Copy memory content to the specified target device
     *
     * Note: mutex of fPtrContainer must be locked before calling
     *
     * @param tgtIndex   Index of the target device, i.e. deviceId - FIRST_ID
     */
    void copyToTarget(size_t tgtIndex) const;

    /**
     * The number of elements in the array
     */
    size_t fNumElems;

    /**
     * Shared pointer to container of pointers to the memory locations per device.
     */
    std::shared_ptr<gen_array_ptrs<T>> fPtrContainer;

};

/**
 * Non-member swap function for Generic Arrays
 */
template<typename T>
inline void swap(gen_array<T>& a, gen_array<T>& b)
{
    a.swap(b);
}

/**
 * Helper function converting a CUDA device to a gen_array Device ID
 *
 * (Returns Device::CPU, if Cuda device ID is invalid.)
 *
 */
// TODO: this should probably be moved to another entity handling
//       the relation between hardware features and gen_array device IDs.
gen_array_base::Device deviceIdFromCuda(int cudaDevice);


} /* namespace mcf */

#endif /* INCLUDE_MCF_GENARRAY_H_ */
