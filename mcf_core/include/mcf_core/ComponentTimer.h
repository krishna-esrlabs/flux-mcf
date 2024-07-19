/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_COMPONENTTIMER_H
#define MCF_COMPONENTTIMER_H

#include "mcf_core/Component.h"

namespace mcf {

/**
 * RAII style job timing class
 */
class ComponentTimer
{
    public:

        /**
         * Constructor, optionally indicating beginning of job
         *
         * @param tag       job tag
         * @param severity  severity of output message
         * @param doBegin   if true, automatically begins job, otherwise begin()
         *                  must be called explicitly to begin the
         * @param doEvent   if true and an event recorder is available for this thread,
         *                  the measured run time will be recorded as an event.
         * @param doLog     if true, the measured run time will output by the component logger.
         */
        explicit ComponentTimer(
                std::string tag,
                mcf::LogSeverity severity = mcf::LogSeverity::info,
                bool doBegin = true,
                bool doEvent = false,
                bool doLog = true);

        /**
         * Destructor, automatically ends the job
         */
        ~ComponentTimer();

        /**
         * Indicate beginning of the job, if not yet done
         */
        void begin();

        /**
         * Indicate end of the job, if not yet done
         */
        void end();

    private:

    /**
     * The component logger name, will be derived from component logger,
     * and is used to create unique timer tags
     */
    std::string fLoggerName;

    /**
     * The job tag
     */
    std::string fTag;

    /**
     * The output message severity
     */
    mcf::LogSeverity fSeverity;

    /**
     * Flag indicating whether begin has been called
     */
    bool fIsActive = false;

    /**
     * Flag indicating whether event recording is enabled
     */
    bool fDoEvent = false;

    /**
     * Flag indicating whether event recording is enabled
     */
    bool fDoLog = false;
};

}  // namespace mcf

#endif  // MCF_COMPONENTTIMER_H
