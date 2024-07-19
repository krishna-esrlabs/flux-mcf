/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_TIME_SERVICE_H
#define MCF_TIME_SERVICE_H

#include "mcf_core/Mcf.h"

namespace mcf {
  
class TimeService : public Component {

public:
    TimeService() :
        mcf::Component("TimeService"),
        fPort10ms(*this, "10ms")
    {}

    void configure(IComponentConfig& config) {
        config.registerPort(fPort10ms, "/time/10ms");
    }

    void startup() {
        fTimeMs = nowMs();
        publishTimestamp();
        registerTriggerHandler(std::bind(&TimeService::tick, this));
        trigger();
    }

    void tick() {
        auto delta = nowMs() - fTimeMs;
        while (delta >= 10) {
            fTimeMs += 10;
            delta -= 10;
            publishTimestamp();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        trigger();
    }

private:

    void publishTimestamp() {
        msg::Timestamp ts;
        ts.ms = fTimeMs;
        fPort10ms.setValue(ts);
    }

    unsigned long nowMs() {
        auto t = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
    }

    unsigned long fTimeMs;
    SenderPort<msg::Timestamp> fPort10ms;
};

}

#endif

