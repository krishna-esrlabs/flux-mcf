/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_LOGGINGMACROS_H_
#define MCF_LOGGINGMACROS_H_

#include "mcf_core/ComponentLogger.h"
#include "spdlog/spdlog.h"

namespace mcf
{
// Convenience macros for logging with __FILE__ and __LINE__
#define MCF_LOG(level, ...) \
    (mcf::ComponentLogger::getLocalLogger()->log( \
        spdlog::source_loc{" at " __FILE__, __LINE__, SPDLOG_FUNCTION}, level, __VA_ARGS__))
// #define MCF_LOG(level, ...)
#define MCF_TRACE(...) MCF_LOG(mcf::LogSeverity::trace, __VA_ARGS__)
#define MCF_DEBUG(...) MCF_LOG(mcf::LogSeverity::debug, __VA_ARGS__)
#define MCF_INFO(...) MCF_LOG(mcf::LogSeverity::info, __VA_ARGS__)
#define MCF_WARN(...) MCF_LOG(mcf::LogSeverity::warn, __VA_ARGS__)
#define MCF_ERROR(...) MCF_LOG(mcf::LogSeverity::err, __VA_ARGS__)
#define MCF_FATAL(...) MCF_LOG(mcf::LogSeverity::critical, __VA_ARGS__)

// Convenience macros for logging without __FILE__ and __LINE__
#define MCF_LOG_NOFILELINE(level, ...) \
    (mcf::ComponentLogger::getLocalLogger()->log(level, __VA_ARGS__))
// #define MCF_LOG_NOFILELINE(level, ...)
#define MCF_TRACE_NOFILELINE(...) MCF_LOG_NOFILELINE(mcf::LogSeverity::trace, __VA_ARGS__)
#define MCF_DEBUG_NOFILELINE(...) MCF_LOG_NOFILELINE(mcf::LogSeverity::debug, __VA_ARGS__)
#define MCF_INFO_NOFILELINE(...) MCF_LOG_NOFILELINE(mcf::LogSeverity::info, __VA_ARGS__)
#define MCF_WARN_NOFILELINE(...) MCF_LOG_NOFILELINE(mcf::LogSeverity::warn, __VA_ARGS__)
#define MCF_ERROR_NOFILELINE(...) MCF_LOG_NOFILELINE(mcf::LogSeverity::err, __VA_ARGS__)
#define MCF_FATAL_NOFILELINE(...) MCF_LOG_NOFILELINE(mcf::LogSeverity::critical, __VA_ARGS__)
} // namespace mcf

#endif // MCF_LOGGINGMACROS_H_