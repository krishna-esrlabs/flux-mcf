/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_CUDAEXTMEMVALUE_H_
#define MCF_CUDAEXTMEMVALUE_H_

#include "mcf_core/Mcf.h"
#include <array>

// forward declarations
namespace mcf { namespace cuda {
template<typename T>
class unique_array;
} }

namespace mcf {
template<typename T>
class gen_array;

template<typename T>
class CudaExtMemValue : public mcf::IExtMemValue {

public:

    /**
     * return type for extMemPtr()
     *
     * these values can be assigned to T* as well as uint8_t*
     */
    class Ptr {
    public:
        Ptr(T* ptr) : fPtr(ptr) {}

        template <typename U>
        explicit operator U*() {
            return reinterpret_cast<U*>(fPtr);
        }

        template <typename U>
        explicit operator const U*() const {
            return reinterpret_cast<const U*>(fPtr);
        }

    private:
        T* fPtr;
    };

    /**
     * device IDs for use with extMemPtr()
     */
    enum {
        CPU = -1
    };

    /**
     * default constructor
     */
    CudaExtMemValue();

    /**
     * destructor
     */
    ~CudaExtMemValue() override;

    /**
     * move constructor
     */
    CudaExtMemValue(CudaExtMemValue&& o) noexcept;

    /**
     * initialize empty ext mem value of size len in bytes
     *
     * re-initialization discards previous contents
     *
     * shall not be called after sharing on value store (no thread protection)
     */
    void extMemInit(uint64_t len) override;

    /**
     * initialize ext mem value with preallocated cuda memory
     *
     * re-initialization discards previous contents
     *
     * shall not be called after sharing on value store (no thread protection)
     */
    void extMemInit(mcf::cuda::unique_array<T>&& cudaArray);

    /**
     * initialize ext mem value with preallocated memory
     *
     * currently only cpu memory is supported
     *
     * re-initialization discards previous contents
     *
     * shall not be called after sharing on value store (no thread protection)
     */
    void extMemInit(std::unique_ptr<T[]>&& array, uint64_t len);

    /**
     * initialize ext mem value with preallocated memory
     *
     * currently only cpu memory is supported
     *
     * re-initialization discards previous contents
     *
     * shall not be called after sharing on value store (no thread protection)
     */
    void extMemInit(mcf::gen_array<T>&& array);

    /**
     * initialize ext mem value with pointer to data
     * re-initialization discards previous contents
     * shall not be called after sharing on value store (no thread protection)
     */
    void extMemInit(const void *src, uint64_t len, int dstDevice=-1, int srcDevice=-1);

    /**
     * query size of the ext mem value in bytes
     */
    uint64_t extMemSize() const override;

    /**
     * @return uint8_t pointer on CPU device
     */
    const uint8_t* extMemPtr() const override
    {
        return static_cast<const uint8_t*>(extMemPtr(CPU));
    }

    /**
     * @return uint8_t pointer on CPU device
     */
    uint8_t* extMemPtr() override
    {
        return static_cast<uint8_t*>(extMemPtr(CPU));
    }

    /**
     * Convert into a generic array
     *
     * Creates a gen_array view onto the CudaExtMamValue via the internal
     * shared pointers. The contained data will not be copied.
     */
    explicit operator mcf::gen_array<T>();

    /**
     * Convert into a generic array
     *
     * Creates a gen_array view onto the CudaExtMamValue via the internal
     * shared pointers. The contained data will not be copied.
     */
    explicit operator const mcf::gen_array<T>() const;

    /**
     * Get ext mem pointer for given cuda device id or cpu (-1), defaults to cpu.
     *
     * Allocates memory for the given device if it hasn't been allocated
     * before or passed in via extMemInit().
     *
     * If at the moment of allocation, memory for another device is already allocated 
     * or was passed via extMemInit(), the other device's memory will be copied into 
     * the newly allocated memory.
     *
     * Note that the copy operation happens only when memory for a specific device
     * is accessed for the first time. It is assumed that the provider of
     * an ext mem value either passes in memory with content already set or
     * accesses memory for one single device and then sets the content. Later
     * the receiver of the ext mem value may access memory for a different device
     * and at that point, the content will be copied automatically.
     *
     * It is an error to call this on an uninitialized ext mem value.
     */
    const Ptr extMemPtr(int device) const;
    Ptr extMemPtr(int device);

    /**
     * check if ext mem is initialized
     */
    bool extMemInitialized() const;

private:

    class ExtMem;

    static const int NUM_GPUS = 2;

    T* extMemPtrImpl(int device) const;

    std::unique_ptr<ExtMem> fExtMem;


};


}  // namespace mcf

#endif  // MCF_CUDAEXTMEMVALUE_H_
