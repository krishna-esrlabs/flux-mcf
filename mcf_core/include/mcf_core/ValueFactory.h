/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_VALUEFACTORY_H_
#define MCF_VALUEFACTORY_H_

#include <memory>

namespace mcf
{

/**
 * Factory class for "standard" values, designed to be used with a child class of mcf::Value.
 * It creates a value from an instance of such a class and wrapping it into a std::shared_ptr to a const.
 */
class ValueFactory
{
public:
    ValueFactory() = default;

    template<typename T>
    std::shared_ptr<const std::remove_reference_t<T>> createValue(T&& value) const
    {
        return std::make_shared<const std::remove_reference_t<T>>(std::forward<T>(value));
    }

    template<typename T>
    std::shared_ptr<const T> createValue(std::unique_ptr<T>& value) const
    {
        return std::shared_ptr<const T>(value.release());
    }

};

} // end namespace mcf

#endif