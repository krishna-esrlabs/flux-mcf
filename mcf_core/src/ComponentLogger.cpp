/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/Messages.h"
#include "mcf_core/ComponentLogger.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/Port.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/dist_sink.h"

#include "json/json.h"

namespace mcf {

constexpr const char* ComponentLogger::CONSOLE_LOG_LEVEL_KEY;
constexpr const char* ComponentLogger::VALUE_STORE_LOG_LEVEL_KEY;
constexpr const char* ComponentLogger::LOGGER_FORMAT;

namespace logger
{
/**
 * \brief A helper class for logging that encapsulates a logging function and acts as a proxy sink
 * for spdlog.
 */
class LambdaLoggerSink : public spdlog::sinks::base_sink<std::mutex>
{
    private:
        // The contained logging function to call
        using LoggingFunction = std::function<void(spdlog::level::level_enum, const std::string&)>;
        LoggingFunction _loggingFunction;

    public:
        explicit LambdaLoggerSink(LoggingFunction loggingFunction, const std::string& pattern)
        : _loggingFunction(std::move(loggingFunction))
        {
            this->set_pattern(pattern);
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            _loggingFunction(msg.level, fmt::to_string(msg.payload));
        }

        void flush_() override {}
};

/**
 * @brief A wrapper to express a "single sink, multiple producers" idiom
 *
 * Owns a std::shared_ptr to a "real" sink which, in turn, handles the messages.
 */
class SinkWrapper : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit SinkWrapper(std::shared_ptr<spdlog::sinks::sink> sink)
    : _sink(std::move(sink))
    {
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override { _sink->log(msg); }

    void flush_() override { _sink->flush(); }

private:
    const std::shared_ptr<spdlog::sinks::sink> _sink;
};

/*
 * Log level mappings.
 *
 * spdlog has TRACE. It will be mapped to DEBUG (=0) in the value store.
 */

inline spdlog::level::level_enum fromMcfLogLevel(int level)
{
    switch (level)
    {
    case 0: // DEBUG
        return spdlog::level::trace;
    case 1: // INFO
        return spdlog::level::info;
    case 2: // WARN
        return spdlog::level::warn;
    case 3: // ERROR
        return spdlog::level::err;
    case 4: // FATAL
        return spdlog::level::critical;
    case 5: // OFF
        return spdlog::level::off;
    default:
        return spdlog::level::trace;
    }
}

inline spdlog::level::level_enum fromStringLogLevel(const std::string &level)
{
    // Can't use switch statement with c++ strings so...
    if (level == "trace") // DEBUG
    {
        return spdlog::level::trace;
    }
    else if (level == "debug") // DEBUG
    {
        return spdlog::level::trace;
    }
    else if (level == "info") // INFO
    {
        return spdlog::level::info;
    }
    else if (level == "warn") // WARN
    {
        return spdlog::level::warn;
    }
    else if (level == "err") // ERROR
    {
        return spdlog::level::err;
    }
    else if (level == "critical") // FATAL
    {
        return spdlog::level::critical;
    }
    else if (level == "off") // OFF
    {
        return spdlog::level::off;
    }
    else  // throw error if string is not recognised
    {
        throw std::runtime_error("Log level string: " + level + ", is not a valid spd log level");
    }
}

inline int fromSpdLogLevel(spdlog::level::level_enum level)
{
    switch(level)
    {
        case spdlog::level::trace:
        case spdlog::level::debug: return 0;

        case spdlog::level::info: return 1;

        case spdlog::level::warn: return 2;

        case spdlog::level::err: return 3;

        case spdlog::level::critical: return 4;

        default: return 0;
    }
}

} // namespace logger

/**
 * A thread-local logger
 */
extern thread_local std::shared_ptr<spdlog::logger> componentLogger;

extern std::mutex loggerAccessMutex;
extern std::shared_ptr<spdlog::logger> mcfLogger;
extern std::shared_ptr<spdlog::sinks::stderr_color_sink_mt> consoleSink;

/*
 * Constructor
 */
ComponentLogger::ComponentLogger(std::string name, SenderPort<msg::LogMessage>& logMessagePort)
: fName(std::move(name)),
  fLogMessagePort(logMessagePort)
{
    {
        std::lock_guard<std::mutex> guard(loggerAccessMutex);
        spdlog::set_pattern(LOGGER_FORMAT);
        if (!consoleSink)
        {
            consoleSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            consoleSink->set_level(spdlog::level::trace);
            consoleSink->set_pattern(LOGGER_FORMAT);
        }
    }
    fConsoleSink = std::make_shared<logger::SinkWrapper>(consoleSink);
    fValueStoreSink = std::make_shared<logger::LambdaLoggerSink>(
        [this](spdlog::level::level_enum level, std::string message) {
            auto msg = std::make_unique<msg::LogMessage>();
            msg->message = std::move(message);
            msg->severity = logger::fromSpdLogLevel(level);
            this->fLogMessagePort.setValue(std::move(msg));
        },
        LOGGER_FORMAT);
    auto distSink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    distSink->add_sink(fConsoleSink);
    distSink->add_sink(fValueStoreSink);
    fLogger = std::make_shared<spdlog::logger>(fName, distSink);
    fLogger->set_level(spdlog::level::trace);
    fConsoleSink->set_level(spdlog::level::debug);
    fValueStoreSink->set_level(spdlog::level::err);
}

bool ComponentLogger::hasComponentLogger()
{
    return (componentLogger != nullptr);
}

std::shared_ptr<spdlog::logger> ComponentLogger::getLocalLogger()
{
    if (!componentLogger)
    {
        std::lock_guard<std::mutex> guard(loggerAccessMutex);
        if (!mcfLogger) {
            mcfLogger = spdlog::stderr_color_mt("MCF");
            spdlog::set_pattern(LOGGER_FORMAT);
        }
        return mcfLogger;
    }
    return componentLogger;
}

void ComponentLogger::injectLocalLogger()
{
    if (!componentLogger)
    {
        componentLogger = fLogger;
    }
    else
    {
        spdlog::warn("Thread-local logger already set");
    }
}

void ComponentLogger::setLogLevelsFromConfig(const Json::Value& config)
{
    std::string componentName;
    if (!config.empty())
    {
        componentName = config.getMemberNames()[0];
    }
    else
    {
        throw std::runtime_error("Component name not found in config when setting log levels");
    }

    auto configValueJson = config[componentName];
    if (configValueJson.isMember(CONSOLE_LOG_LEVEL_KEY))
    {
        if (!configValueJson[CONSOLE_LOG_LEVEL_KEY].isString())
        {
            throw std::runtime_error("Invalid ConsoleLogLevel in "+componentName+" configuration");
        }
        fConsoleSink->set_level(logger::fromStringLogLevel(configValueJson[CONSOLE_LOG_LEVEL_KEY].asString()));
    }
    else
    {
        MCF_WARN("No ConsoleLogLevel set in "+componentName+" configuration");
    }

    if (configValueJson.isMember(VALUE_STORE_LOG_LEVEL_KEY))
    {
        if (!configValueJson[VALUE_STORE_LOG_LEVEL_KEY].isString())
        {
            throw std::runtime_error("Invalid ValueStoreLogLevel in "+componentName+" configuration");
        }
        fValueStoreSink->set_level(logger::fromStringLogLevel(configValueJson[VALUE_STORE_LOG_LEVEL_KEY].asString()));
    }
    else
    {
        MCF_WARN("No ValueStoreLogLevel set in "+componentName+" configuration");
    }
}

void ComponentLogger::setName(const std::string& name)
{
    fName = name;
    auto distSink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    distSink->add_sink(fConsoleSink);
    distSink->add_sink(fValueStoreSink);
    fLogger = std::make_shared<spdlog::logger>(fName, distSink);
    fLogger->set_level(spdlog::level::trace);
}

void ComponentLogger::setValueStoreLogLevel(const int level)
{
    fValueStoreSink->set_level(logger::fromMcfLogLevel(level));
}

void ComponentLogger::setValueStoreLogLevel(const LogSeverity level)
{
    fValueStoreSink->set_level(level);
}

void ComponentLogger::setConsoleLogLevel(const LogSeverity level)
{
    fConsoleSink->set_level(level);
}


LogSeverity ComponentLogger::getConsoleLogLevel() const
{
    return fConsoleSink->level();
}

LogSeverity ComponentLogger::getValueStoreLogLevel() const
{
    return fValueStoreSink->level();
}


void ComponentLogger::log(LogSeverity severity, const std::string& message)
{
    fLogger->log(severity, message);
}

}