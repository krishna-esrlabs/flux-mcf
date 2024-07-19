/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_MESSAGES_H
#define MCF_MESSAGES_H

#include "mcf_core/ComponentTraceMessages.h"

#include <msgpack.hpp>

namespace mcf {

namespace msg {

struct String : public mcf::Value {
    String() = default;
    explicit String(std::string value_) : value(std::move(value_)) {}
    std::string value;
    MSGPACK_DEFINE(value)
};

class RuntimeStatsEntry {
public:
    unsigned long start;
    unsigned long total;
    int count;
    int min;
    int max;
    int avg;
    int frq;
    MSGPACK_DEFINE(start, total, count, min, max, avg, frq)
};

class RuntimeStats : public Value {
public:
    std::map<std::string, RuntimeStatsEntry> entries;
    MSGPACK_DEFINE(entries)
};

class Timestamp : public Value {
public:
    unsigned long ms;
    MSGPACK_DEFINE(ms)
};

class LogMessage : public Value {
public:
    std::string message;
    int severity;

    MSGPACK_DEFINE(message, severity)
};

class LogControl : public Value {
public:
    int level;

    MSGPACK_DEFINE(level)
};

/**
 * Status message used by Value Recorder
 */
class RecorderStatus : public Value {
public:
    /**
     * Throughput into record file in bytes per second
     */
    unsigned long outputBps;
    /**
     * Average delay between value store write and record file write in milliseconds
     */
    unsigned long avgLatencyMs;
    /**
     * Maximum delay between value store write and record file write in milliseconds
     */
    unsigned long maxLatencyMs;
    unsigned long avgQueueSize;
    unsigned long maxQueueSize;
    /**
     * CPU usage of the value recorder write thread in percent of real time
     * A value of 100 means one cpu core worked 100% of the time
     */
    unsigned short cpuUsageUser;
    unsigned short cpuUsageSystem;
    /**
     * Set if values where dropped since last write due to overload
     */
    bool dropFlag;
    /**
     * Set if there is at least one write error since the last status
     */
    bool errorFlag;
    /**
     * Error descriptions of write errors
     */
    std::set<std::string> errorDescs;

    MSGPACK_DEFINE(outputBps,
        avgLatencyMs, maxLatencyMs,
        avgQueueSize, maxQueueSize,
        cpuUsageUser, cpuUsageSystem,
        dropFlag, errorFlag, errorDescs)
};

/**
 * Value which holds configuration directory string
 */
class ConfigDir : public mcf::Value
{
public:
    ConfigDir() = default;

    ConfigDir(std::string configDir) : configDir(std::move(configDir)){};

    std::string configDir;

    MSGPACK_DEFINE(configDir)
};

/**
 * Value which holds array of directory strings
 */
class ConfigDirs : public mcf::Value
{
public:

    std::vector<std::string> configDirs;

    MSGPACK_DEFINE(configDirs)
};

template<typename T>
inline void registerValueTypes(T& r) {
    r.template registerType<String>("String");
    r.template registerType<RuntimeStats>("mcf::RuntimeStats");
    r.template registerType<Timestamp>("mcf::Timpstamp");
    r.template registerType<LogMessage>("mcf::LogMessage");
    r.template registerType<LogControl>("mcf::LogControl");
    r.template registerType<RecorderStatus>("mcf::RecorderStatus");
    r.template registerType<ConfigDir>("mcf::ConfigDir");
    r.template registerType<ConfigDirs>("mcf::ConfigDirs");

    registerComponentTraceValueTypes(r);
}

} // namespace msg
} // namespace mcf

#endif
