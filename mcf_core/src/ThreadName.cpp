/**
 * A thin wrapper around thread naming interfaces by both NVIDIA (via nvToolsExt) and pthread.
 *
 * Copyright (c) 2024 Accenture
 *
 */

#include "mcf_core/ThreadName.h"

#include "mcf_core/ErrorMacros.h"
#include "spdlog/fmt/fmt.h"

#include <pthread.h>
#include <stdexcept>

// declaration here since nvToolsExt header is not available on the AGX docker image
#ifdef HAVE_CUDA
extern "C" void nvtxNameOsThreadA(uint32_t threadId, const char* name);
#endif /* HAVE_CUDA */

namespace mcf
{
void
setThreadName(const std::string& name)
{
    const auto truncatedName = name.substr(0, 15);
    const auto self          = pthread_self();
    const int result         = pthread_setname_np(self, truncatedName.c_str());
    MCF_ASSERT(
        result == 0,
        fmt::format("Cannot set thread name '{}'. Error: {}", truncatedName, strerror(result)));

#ifdef HAVE_CUDA
    nvtxNameOsThreadA(self, truncatedName.c_str());
#endif /* HAVE_CUDA */
}

} // namespace mcf