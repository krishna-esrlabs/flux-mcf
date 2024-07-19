/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_SHMEMKEEPER_H
#define MCF_SHMEMKEEPER_H

#include "mcf_core/Mcf.h"
#include "zmq.hpp"
#include <mutex>

#define BOOST_DATE_TIME_NO_LIB
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
namespace bip = boost::interprocess;

namespace mcf {

namespace remote {

/**
 * Manages shared memory used for inter-process communication. A ShmemKeeper is typically used
 * on the sending side of the communication to allocate/keep track of data in shared memory.
 */
class ShmemKeeper {
public:
    /**
     * Constructor, default implementation
     */
    ShmemKeeper() = default;

    virtual ~ShmemKeeper() = default;

    /**
     *  Disallow copy constructors
     */
    ShmemKeeper(const ShmemKeeper&) = delete;
    ShmemKeeper(ShmemKeeper&&) = delete;

    /**
     * Returns a pointer to a partition in a shared memory file identified by the partitionId
     * @warning This function shall not be called and the returned pointer not be used concurrently
     *          with a call to createOrGetPartitionPtr
     * @param partitionId  The id of the partition that shall be returned
     */
    virtual void* partitionPtr(const std::string& partitionId) = 0;

    /**
     * Allocates/resizes a memory partition inside a shared memory file managed by this ShmemKeeper
     * @warning This function shall not be called concurrently from multiple threads and the returned
     *          pointer not be used concurrently with additional calls to this function
     * @param partitionId  The id of the partition that shall be returned
     * @param size         The minimal allocated size in bytes of the partition to be returned.
     *                     If the allocated size is too small, the partition will be re-allocated
     *                     with a bigger size
     */
    virtual void* createOrGetPartitionPtr(const std::string& partitionId, size_t size) = 0;

    /**
     * Returns a pointer to a partition in a shared memory file identified by the partitionId
     * The handle can be passed to other processes so they can access the correct pointer within a
     * shared memory file
     * @warning This function shall not be called and the returned handle not be used concurrently
     *          with a call to createOrGetPartitionPtr
     * @param partitionId  The id of the partition whose handler shall be returned
     */
    virtual bip::managed_shared_memory::handle_t partitionHandle(const std::string& partitionId) = 0;

    /**
     * Returns the name of the shared memory file in which the partition associated with the passed
     * partitionId is located
     * @param partitionId  The id of the partition whose file name shall be returned
     */
    virtual std::string shmemFileName(const std::string& partitionId) = 0;
};

/**
 * All instances of this ShmemKeeper variation use the shared memory file called
 * 'McfSharedMemory'. If the file does not exist when the constructor is called, it will
 * create the file with the size (in bytes) passed as an argument. If the file already
 * exists, it is unchanged.
*/
class SingleFileShmem: public ShmemKeeper {
    struct PartitionPtr
    {
        void* ptr = nullptr;
        size_t size = 0ul;
    };

public:
    /**
     * Constructor. Allocates one byte in this file whose sole purpose is to
     * indicate that this instance is currently accessing this file
     *
     * @param filesize Determines the size of the file 'McfSharedMemory' if it was not
     *                 allocated before this call
     */
    SingleFileShmem(const size_t filesize = 1024*1024*256); // allocate a 256 MB shared memory file per default

    /**
     * Deallocates all partitions created by this instance in the shared memory file
     * 'McfSharedMemory', including the one allocated in the constructor. If no more memory is
     * allocated in 'McfSharedMemory' after that, it removes the file.
     */
    virtual ~SingleFileShmem();

    /**
     * See base class
     */
    virtual void* partitionPtr(const std::string& partitionId);

    /**
     * See base class
     */
    virtual void* createOrGetPartitionPtr(const std::string& partitionId, size_t size);

    /**
     * See base class
     */
    virtual bip::managed_shared_memory::handle_t partitionHandle(const std::string& partitionId);

    /**
     * See base class
     */
    virtual std::string shmemFileName(const std::string& partitionId);

private:
    /**
     * Checks if the current size of mem is bigger or equal to sizeNeeded and increases its
     * size if it is not. If the current size is not sufficient, the partition will be
     * destroyed and re-allocated with an appropriate size
     * @param sizeNeeded  The minimal number of bytes that must be available in the partition
     * @param mem         A pointer to a shared memory partition
     */
    void increasePartitionIfNecessary(size_t sizeNeeded, PartitionPtr& mem);

    bip::managed_shared_memory _segment;
    std::map<std::string, PartitionPtr> _partitionPtrs;
    void* flag;
};


} // end namespace remote

} // end namespace mcf

#endif