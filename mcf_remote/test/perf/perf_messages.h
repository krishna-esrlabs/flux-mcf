/**
 * Copyright (c) 2024 Accenture
 */
#ifndef PERF_MESSAGES_H
#define PERF_MESSAGES_H

#include "mcf_core/Mcf.h"
#include "mcf_core/ExtMemValue.h"

namespace perf_msg {

struct Point : public mcf::Value {
    float x;
    float y;
    MSGPACK_DEFINE(x, y);
};

class TestValue : public mcf::Value {
public:
    unsigned long time;
    std::string str;
    std::vector<uint8_t> data;
    std::vector<Point> points;
    MSGPACK_DEFINE(time, str, data, points);
};

class Image : public mcf::ExtMemValue<uint8_t> {
public:
    unsigned int width;
    unsigned int height;
    MSGPACK_DEFINE(width, height);
};

template<typename T>
inline void registerValueTypes(T& r) {
    r.template registerType<TestValue>("TestValue");
    r.template registerType<Image>("Image");
}

}

#endif

