/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_REMOTE_PERFLOGGER_H
#define MCF_REMOTE_PERFLOGGER_H

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace mcf {

#if PROFILING

class PerfLogger
{
public:
    PerfLogger(std::string tag, std::shared_ptr<spdlog::logger> logger) :
            _tag(tag),
            _logger(logger)
    {
        tick();
    }

    PerfLogger(std::string tag, uint64_t id, std::string topic, std::shared_ptr<spdlog::logger> logger) :
            _tag(tag),
            _id(id),
            _topic(topic),
            _logger(logger)
    {
        tick();
    }

    ~PerfLogger()
    {
        if(_id > 0)
        {
            _logger->trace(fmt::format("{} {} on {} at {}",
                    _tag, _id, _topic, _timestamp));
        }
    }

    void tick()
    {
        _timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void setData(uint64_t id, std::string topic)
    {
        _id = id;
        _topic = topic;
    }

private:
    std::string _tag;
    uint64_t _id = 0ul;
    std::string _topic;

    uint64_t _timestamp;
    std::shared_ptr<spdlog::logger> _logger;
};

#else

class PerfLogger
{
public:
    PerfLogger(const std::string& /*unused*/, const std::shared_ptr<spdlog::logger> /*unused*/&) {}

    PerfLogger(
        const std::string& /*unused*/,
        uint64_t /*unused*/,
        const std::string /*unused*/&,
        const std::shared_ptr<spdlog::logger> /*unused*/&)
    {}

    ~PerfLogger() = default;

    void tick()
    {}

    void setData(uint64_t /*unused*/, const std::string /*unused*/&) {}
};

#endif

} // end namespace mcf

#endif
