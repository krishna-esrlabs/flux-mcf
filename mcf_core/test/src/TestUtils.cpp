/**
 * Copyright (c) 2024 Accenture
 */
#include "test/TestUtils.h"

#include <thread>

namespace mcf
{
void
waitForValue(const ValueStore& valueStore, const std::string& topic)
{
    for (std::size_t i = 0; i < 10; ++i)
    {
        if (valueStore.hasValue(topic))
        {
            break;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
} // namespace mcf