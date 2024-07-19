/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_PORTTRIGGERHANDLER_H
#define MCF_PORTTRIGGERHANDLER_H

#include "mcf_core/ITriggerable.h"

#include <functional>
#include <memory>
#include <mutex>

namespace mcf {

class EventFlag;
class ComponentTraceEventGenerator;

class PortTriggerHandler {
public:
    explicit PortTriggerHandler(std::function<void()> func,
                                std::string name="",
                                const std::shared_ptr<ComponentTraceEventGenerator>& eventGenerator = nullptr);

    std::shared_ptr<EventFlag> getEventFlag() const {
        return fEventFlag;
    }

    /**
     * Method that will be executed by the a component thread when fEventFlag is active
     */
    void call() {
        fFunc();
    }

    const std::string& getName() const {
        return fName;
    }

private:

    /**
     * A TriggerTracer is registered to the EventFlag to trace EventFlag activations
     */
    class TriggerTracer : public ITriggerable
    {
    public:

        TriggerTracer(std::shared_ptr<EventFlag> eventFlag,
                      std::shared_ptr<ComponentTraceEventGenerator> eventGenerator);

        /**
         * Method that will be triggered by the EventFlag when it gets activated.
         * We use it to trace trigger activations
         */
        void trigger() override;

    private:

        std::shared_ptr<EventFlag> fEventFlag;
        std::shared_ptr<ComponentTraceEventGenerator> fTraceEventGenerator;
    };

    std::function<void()> fFunc;
    std::shared_ptr<EventFlag> fEventFlag;
    std::string fName;
    std::shared_ptr<TriggerTracer> fTriggerTracer;
};

} // namespace mcf

#endif // MCF_PORTTRIGGERHANDLER_H

