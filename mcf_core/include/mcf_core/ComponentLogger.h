/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENTLOGGER_H
#define MCF_COMPONENTLOGGER_H

#include "mcf_core/Messages.h"
#include "json/forwards.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Json {
class Value;
}

namespace mcf {

namespace msg {
class LogMessage;
}

using LogSeverity = spdlog::level::level_enum;
template<typename T> class SenderPort;

class ComponentLogger {

    public:

        ComponentLogger(std::string name, SenderPort<msg::LogMessage>& logMessagePort);

        static bool hasComponentLogger();

        static std::shared_ptr<spdlog::logger> getLocalLogger();

        /**
         * Sets the log level for the value store sink (integer log level overload)
         * @param level The log level, as integer
         */
        void setValueStoreLogLevel(int level);

        /**
         * Sets the log level for the value store sink (LogSeverity overload)
         * @param level The log level, as LogSeverity
         */
        void setValueStoreLogLevel(LogSeverity level);

        void setConsoleLogLevel(LogSeverity level);

        void setLogLevelsFromConfig(const Json::Value& config);

        void setName(const std::string& name);

        void injectLocalLogger();

        LogSeverity getConsoleLogLevel() const;

        LogSeverity getValueStoreLogLevel() const;

        void log(LogSeverity severity, const std::string& message);

private:

        /*
         * Shared pointers to the component logger and sinks
         */

        std::shared_ptr<spdlog::logger> fLogger;
        std::shared_ptr<spdlog::sinks::sink> fConsoleSink;
        std::shared_ptr<spdlog::sinks::sink> fValueStoreSink;

        static constexpr const char* CONSOLE_LOG_LEVEL_KEY = "ConsoleLogLevel";

        static constexpr const char* VALUE_STORE_LOG_LEVEL_KEY = "ValueStoreLogLevel";

        static constexpr const char* LOGGER_FORMAT 
                = "[%Y-%m-%d %H:%M:%S.%e %z] [%n] [%^%l%$] %v%@";

        std::string fName;

        SenderPort<msg::LogMessage>& fLogMessagePort;
};

}  // namespace mcf

#endif  // MCF_COMPONENTLOGGER_H
