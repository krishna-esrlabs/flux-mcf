/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_TEST_VALUE_H
#define MCF_TEST_VALUE_H

#include "mcf_core/Mcf.h"

namespace mcf
{
namespace test
{
class TestValue : public mcf::Value
{
public:
    TestValue(int val = 0) : val(val){};
    int val;
    MSGPACK_DEFINE(val);
};

} // namespace test
} // namespace mcf

#endif // MCF_TEST_VALUE_H