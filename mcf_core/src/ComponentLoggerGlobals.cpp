/**
 * Copyright (c) 2024 Accenture
 */

#include <mutex>
#include <memory>

namespace spdlog {
class logger;
namespace sinks {
class stderr_color_sink_mt;
}
}

namespace mcf {

/**
 * A thread-local logger
 */
thread_local std::shared_ptr<spdlog::logger> componentLogger;

std::mutex loggerAccessMutex;
std::shared_ptr<spdlog::logger> mcfLogger = nullptr;
std::shared_ptr<spdlog::sinks::stderr_color_sink_mt> consoleSink = nullptr;
}