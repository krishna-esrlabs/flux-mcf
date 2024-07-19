/**
 * Copyright (c) 2024 Accenture
 */

#include <memory>

namespace mcf {

class ComponentTraceEventGenerator;

/**
 * Thread-local component trace event generator
 */
thread_local std::shared_ptr<ComponentTraceEventGenerator> gComponentTraceEventGenerator(nullptr);

} // namespace mcf
