/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_TRACE_H
#define MCF_TRACE_H

#include <atomic>
#include <string>

#ifndef TRACE_FS_MOUNT
#define TRACE_FS_MOUNT "/sys/kernel/debug/tracing"
#endif

namespace mcf
{
namespace tracing
{
/// The name of the trace output file
const std::string TRACE_FILE = std::string(TRACE_FS_MOUNT) + "/trace_marker";

/**
 * @brief A debug object that writes to the kernel debug trace, if it is available.
 *
 * The Linux kernel function tracer is documented at
 * https://www.kernel.org/doc/Documentation/trace/ftrace.txt
 *
 * The idea is to call a write() syscall to the trace marker at
 * /sys/kernel/debug/tracing/trace_marker
 * after which the written string is available in the kernel trace buffer and in the output of
 * trace-cmd.
 *
 * This class is meant to be instantiated once per functional module, and each call to write() will
 * produce a marker in the kernel trace, prefixed by a custom prefix.
 */
class KernelTracer
{
public:
    /**
     * @brief Construct a new Kernel Tracer object
     *
     * The constructor attempts to open `/sys/kernel/debug/tracing/trace_marker`. If this is not
     * possible, then the mechanism will fail with a warning in the log. The mechanism will fail
     * only once: After one failure to open the trace file, the kernel trace will be marked as
     * unavailable and no further attempts to open the trace file will be made.
     *
     * @param prefix The prefix of the log messages in the kernel trace
     */
    KernelTracer(std::string prefix);

    /**
     * @brief Destructor
     *
     * Closes the open file handle
     */
    ~KernelTracer();

    /**
     * @brief Write a message to the kernel trace buffer
     *
     * @param message The message to write, prefixed by the common message prefix for this tracer
     * object.
     */
    void write(const std::string& message);

    /**
     * @brief Write a message to the kernel trace buffer, allocation-less version
     * 
     * @param message A message buffer. Together with the prefix, only 255 characters will be used.
     */
    void write(const char* message);

private:
    const std::string _prefix;
    int _traceFile;

    static std::atomic<bool> _traceAvailable;
};
} // namespace tracing
} // namespace mcf

#endif // MCF_TRACE_H