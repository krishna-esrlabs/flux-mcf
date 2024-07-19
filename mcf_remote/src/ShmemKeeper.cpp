/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/ShmemKeeper.h"
#include "mcf_remote/Remote.h"
#include "mcf_core/ErrorMacros.h"

#include <iostream>

namespace mcf {

namespace remote {

SingleFileShmem::SingleFileShmem(const size_t filesize)
{
    // Create a managed shared memory segment
    try
    {
        _segment = bip::managed_shared_memory(bip::open_or_create, "McfSharedMemory", filesize);
    }
    catch(const bip::interprocess_exception& ipe)
    {
        throw std::runtime_error(fmt::format("Failed to create shared memory file {}:\n    {}",
                "McfSharedMemory",
                ipe.what()));
    }
    // Allocate one byte to indicate this shared memory file is currently in use
    flag = _segment.allocate(1);
}

SingleFileShmem::~SingleFileShmem()
{
    // free owned memory
    for(auto& mem : _partitionPtrs) _segment.deallocate(mem.second.ptr);

    // release flag to indicate we are not using this shared memory file any more
    _segment.deallocate(flag);

    // clean up if file no memory is allocated in the segment
    if(_segment.all_memory_deallocated())
        bip::shared_memory_object::remove("McfSharedMemory");
}

void* SingleFileShmem::partitionPtr(const std::string& partitionid)
{
    return _partitionPtrs[partitionid].ptr;
}

void* SingleFileShmem::createOrGetPartitionPtr(const std::string& partitionid, size_t size)
{
    PartitionPtr& mem = _partitionPtrs[partitionid];
    increasePartitionIfNecessary(size, mem);
    return mem.ptr;
}

bip::managed_shared_memory::handle_t SingleFileShmem::partitionHandle(const std::string& partitionid)
{
    return _segment.get_handle_from_address(_partitionPtrs[partitionid].ptr);
}

std::string SingleFileShmem::shmemFileName(const std::string& partitionid)
{
    return std::string("McfSharedMemory");
}


void SingleFileShmem::increasePartitionIfNecessary(size_t sizeNeeded, PartitionPtr& mem)
{
    if(mem.size < sizeNeeded)
    {
        try
        {
            if(mem.size > 0) _segment.deallocate(mem.ptr);
            mem.ptr = _segment.allocate(sizeNeeded + 512); // allocate extra 512 bytes to avoid frequent re-allocations
            mem.size = sizeNeeded + 512;
        }
        catch(const std::exception& e)
        {
            MCF_ERROR("Shared memory resize failed: {}", e.what());
            mem.size = 0ul;
            mem.ptr = nullptr;
        }
    }
}

} // end namespace remote

} // end namespace mcf