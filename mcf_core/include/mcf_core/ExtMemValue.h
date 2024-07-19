/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_EXTMEMVALUE_H_
#define MCF_EXTMEMVALUE_H_

#include "mcf_core/IExtMemValue.h"

#include <memory>

namespace mcf {

template<typename T>
class ExtMemValue : public mcf::IExtMemValue {

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
    ExtMemValue();

    /**
     * destructor
     */
    ~ExtMemValue() override;

    /**
     * move constructor
     */
    ExtMemValue(ExtMemValue&& o) noexcept;

    /**
     * initialize empty ext mem value of size len in bytes
     * len must be greater than 0
     * re-initialization discards previous contents
     * shall not be called after sharing on value store (no thread protection)
     */
    void extMemInit(uint64_t len) override;

    /**
     * initialize ext mem value with preallocated memory
     * re-initialization discards previous contents
     * shall not be called after sharing on value store (no thread protection)
     */
    void extMemInit(std::unique_ptr<T[]>&& array, uint64_t len);

    /**
     * initialize ext mem value with pointer to data
     * re-initialization discards previous contents
     * shall not be called after sharing on value store (no thread protection)
     * devices are specified to match interface of CudaExtMemValue but will throw an error if either not equal to -1
     */
    void extMemInit(const void *src, uint64_t len, int dstDevice=-1, int srcDevice=-1);

    /**
     * query size of the ext mem value in bytes
     */
    uint64_t extMemSize() const override;

    /**
     * @return uint8_t pointer on CPU device
     */
    const uint8_t* extMemPtr() const override;

    /**
     * @return uint8_t pointer on CPU device
     */
    uint8_t* extMemPtr() override;

    /**
     * @return uint8_t pointer for reading, device should be -1 for cpu memory
     */
    const Ptr extMemPtr(int device) const;

    /**
     * @return uint8_t pointer to data for writing, device should be -1 for cpu memory
     */
    Ptr extMemPtr(int device);

    /**
     * check if ext mem is initialized
     */
    bool extMemInitialized() const;

private:

    class ExtMem;

    T* extMemPtrImpl() const;

    std::unique_ptr<ExtMem> fExtMem;
};

}  // namespace mcf

#endif  // MCF_EXTMEMVALUE_H_
