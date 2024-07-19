/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/Trace.h"

#include "mcf_core/LoggingMacros.h"
#include "spdlog/fmt/fmt.h"

#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_CUDA
extern "C" void nvtxMarkA(const char* message);
#endif // HAVE_CUDA

namespace mcf
{
namespace tracing
{
std::atomic<bool> KernelTracer::_traceAvailable(true);

KernelTracer::KernelTracer(std::string prefix) : _prefix(std::move(prefix)), _traceFile(-1)
{
    if (_traceAvailable)
    {
        _traceFile = open(TRACE_FILE.c_str(), O_WRONLY);
        if (_traceFile < 0)
        {
            MCF_WARN_NOFILELINE("Cannot open kernel trace file");
            _traceAvailable = false;
        }
    }
}

KernelTracer::~KernelTracer()
{
    if (_traceFile >= 0)
    {
        ::close(_traceFile);
    }
}

void
KernelTracer::write(const std::string& message)
{
    if (_traceFile >= 0)
    {
        std::string messageString(fmt::format("{}: {}", _prefix, message));
        auto bytesWritten = ::write(_traceFile, messageString.c_str(), messageString.size());
#ifdef HAVE_CUDA
        nvtxMarkA(messageString.c_str());
#endif
        if (bytesWritten < 0)
        {
            MCF_WARN_NOFILELINE("Error writing to kernel trace");
        }
    }
}

void
KernelTracer::write(const char* message)
{
    if (_traceFile >= 0)
    {
        char buffer[256];
        int length = snprintf(buffer, 255, "%s: %s", _prefix.c_str(), message);
#ifdef HAVE_CUDA
        nvtxMarkA(buffer);
#endif
        auto bytesWritten = ::write(_traceFile, buffer, length > 0 ? length : 255);
        if (bytesWritten < 0)
        {
            MCF_WARN_NOFILELINE("Error writing to kernel trace");
        }
    }
}

} // namespace tracing

} // namespace mcf
