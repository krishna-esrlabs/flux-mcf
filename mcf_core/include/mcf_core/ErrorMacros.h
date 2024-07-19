/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_ERRORMACROS_H_
#define MCF_ERRORMACROS_H_

// TODO: for some strange reason we have to include this
//       in order to make std exceptions work with nvcc
#include <memory>

#include <string>
#include <exception>

#define ERRORHELPER_STR_HELPER(x) #x
#define ERRORHELPER_STR(x) ERRORHELPER_STR_HELPER(x)

/**
 * Throw exception of specified class with given description and source code location
 *
 * Note: declaration of dummy function at the end serves for requiring a closing semicolon when using the macro
 */
#define MCF_THROW_ERROR(error_class, what)                                                         \
    {                                                                                              \
        throw error_class(                                                                         \
            std::string(what) + std::string(" in " __FILE__ ", " ERRORHELPER_STR(__LINE__)));      \
    }; void _DUMMY_FUNCTION_CYXT63TIO_()

/**
 * Throw runtime_error with given description and source code location
 *
 * Note: declaration of dummy function at the end serves for requiring a closing semicolon when using the macro
 */
#define MCF_THROW_RUNTIME(what)                                                                    \
    {                                                                                              \
        throw std::runtime_error(                                                                  \
            std::string(what) + std::string(" in " __FILE__ ", " ERRORHELPER_STR(__LINE__)));      \
    }; void _DUMMY_FUNCTION_CYXT63TIO_()

/**
 * Assertion with source code location in error case
 */
#define MCF_ASSERT_1(condition)                                                                    \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            throw std::runtime_error(                                                              \
                std::string("Assertion failed in " __FILE__ ", " ERRORHELPER_STR(__LINE__)));      \
        }                                                                                          \
    }

/**
 * Assertion with custom message and source code location in error case
 */
#define MCF_ASSERT_2(condition, what)                                                              \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            throw std::runtime_error(                                                              \
                std::string(what) + std::string(" in " __FILE__ ", " ERRORHELPER_STR(__LINE__)));  \
        }                                                                                          \
    }

// see https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros
#define MCF_ASSERT_GET_3RD_ARG(arg1, arg2, arg3, ...) arg3
#define MCF_ASSERT_CHOOSER(...) MCF_ASSERT_GET_3RD_ARG(__VA_ARGS__, MCF_ASSERT_2, MCF_ASSERT_1)

/*
 * Note: declaration of dummy function at the end serves for requiring a closing semicolon when using the macro
 */
#define MCF_ASSERT(...) MCF_ASSERT_CHOOSER(__VA_ARGS__)(__VA_ARGS__); void _DUMMY_FUNCTION_CYXT63TIO_()

#endif /* MCF_ERRORMACROS_H_ */

