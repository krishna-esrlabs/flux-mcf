/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_SHMEMCLIENT_H
#define MCF_SHMEMCLIENT_H

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
 * Accesses shared memory allocated by another entity, typically a ShmemKeeper.
 * This class is used on the receiving side of inter-process communication using shared memory.
 */
class ShmemClient {
public:
    /**
     * Constructor, default implementation
     */
    ShmemClient() = default;

    ~ShmemClient() = default;

    /**
     *  Disallow copy constructors
     */
    ShmemClient(const ShmemClient&) = delete;
    ShmemClient(ShmemClient&&) = delete;

    /**
     *  Disallow assignment
     */
    ShmemClient operator=(const ShmemClient&) = delete;
    ShmemClient operator=(ShmemClient&&) = delete;

    /**
     * Returns a pointer to a partition in a shared memory file identified by the shmemFileName and
     * partitionHandle
     * @warning While the returned pointer to the partition is used, it must not be destroyed by
     *          threads/processes accessing the same shared memory file
     * @param shmemFileName   The name of the shared memory file in which to find the partition
     * @param partitionHandle The handle to the partition that shall be returned
     */
    void* partitionPtr(const std::string& shmemFileName, bip::managed_shared_memory::handle_t partitionHandle);

private:
    /**
     * Checks if the shared memory segment of the passed file name is already opened. If not it
     * tries to open it.
     * @param shmemFileName   The name of the shared memory that shall be accessed
     */
    void openSegment(const std::string& shmemFileName);

    bip::managed_shared_memory _segment;
    std::string _segmentName;
};


} // end namespace remote

} // end namespace mcf

#endif