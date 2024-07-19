/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_TEST_UTILS_H
#define MCF_TEST_UTILS_H

#include "mcf_core/ValueStore.h"

namespace mcf
{
void waitForValue(const ValueStore& valueStore, const std::string& topic);
} // namespace mcf

#endif // MCF_TEST_UTILS_H