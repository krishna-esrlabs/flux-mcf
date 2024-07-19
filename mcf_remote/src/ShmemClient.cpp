/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/ShmemClient.h"
#include "mcf_remote/Remote.h"
#include "mcf_core/ErrorMacros.h"

namespace mcf {

namespace remote {

void* ShmemClient::partitionPtr(const std::string& segmentName, bip::managed_shared_memory::handle_t handle)
{
    openSegment(segmentName);
    return _segment.get_address_from_handle(handle);
}

void ShmemClient::openSegment(const std::string& segmentName)
{
    if(segmentName == _segmentName) return;

    try
    {
        _segment = bip::managed_shared_memory(bip::open_only, segmentName.c_str());
        _segmentName = segmentName;
    }
    catch(const bip::interprocess_exception& ipe)
    {
        throw std::runtime_error(fmt::format("Cannot open shared memory file {}\n    {}", segmentName, ipe.what()));
    }

}

} // end namespace remote

} // end namespace mcf