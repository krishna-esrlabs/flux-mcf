/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/Mcf.h"
#include "mcf_core/ValueRecorder.h"
#include "mcf_core/ExtMemValue.h"
#if HAVE_ZLIB
#include "zlib.h"
#endif

#include <fstream>
#include <cstdio>

namespace mcf
{

class ValueRecorderTest : public ::testing::Test
{
public:
    class TestValue : public mcf::Value
    {
    public:
        TestValue(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };

    class TestValueExtMem : public mcf::ExtMemValue<uint8_t>
    {
    public:
        TestValueExtMem(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };

    template<typename T>
    inline void registerValueTypes(T& r)
    {
        r.template registerType<TestValue>("TestValue");
        r.template registerType<TestValueExtMem>("TestValueExtMem");
    }

    std::string readFile(std::string fn)
    {

        std::ifstream t(fn, std::ios::binary);
        std::string str;

        t.seekg(0, std::ios::end);
        str.reserve(t.tellg());
        t.seekg(0, std::ios::beg);

        str.assign((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());

        return str;
    }
};

TEST_F(ValueRecorderTest, Simple)
{
    mcf::ValueStore valueStore;
    registerValueTypes(valueStore);
    mcf::ValueRecorder valueRecorder(valueStore);

    const std::string testfile = "record.bin";

    std::remove(testfile.c_str());
    uint64_t t0 = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now().time_since_epoch())
                      .count();
    valueRecorder.start(testfile);
    valueStore.setValue("/test1", TestValue(5));
    valueStore.setValue("/test1", TestValue(9999));

    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!valueRecorder.writeQueueEmpty());
    valueRecorder.stop();
    uint64_t t1 = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now().time_since_epoch())
                      .count();

    std::string str = readFile(testfile);

    size_t off = 0;

    auto pHeader = msgpack::unpack(str.data(), str.size(), off);
    auto value1Time = pHeader.get().via.array.ptr[0].as<uint64_t>();
    EXPECT_LE(t0, value1Time);
    EXPECT_EQ("/test1", pHeader.get().via.array.ptr[1].as<std::string>());
    EXPECT_EQ("TestValue", pHeader.get().via.array.ptr[2].as<std::string>());
    EXPECT_EQ(5, msgpack::unpack(str.data(), str.size(), off).get().as<std::vector<int>>()[0]);
    // extmem header
    msgpack::unpack(str.data(), str.size(), off);

    pHeader = msgpack::unpack(str.data(), str.size(), off);
    auto value2Time = pHeader.get().via.array.ptr[0].as<uint64_t>();
    EXPECT_LE(value1Time, value2Time);
    EXPECT_GE(t1, value2Time);
    EXPECT_EQ("/test1", pHeader.get().via.array.ptr[1].as<std::string>());
    EXPECT_EQ("TestValue", pHeader.get().via.array.ptr[2].as<std::string>());
    EXPECT_EQ(9999, msgpack::unpack(str.data(), str.size(), off).get().as<std::vector<int>>()[0]);
    // extmem header
    msgpack::unpack(str.data(), str.size(), off);

    // delete the created file
    std::remove(testfile.c_str());
}

TEST_F(ValueRecorderTest, ExtMem)
{
    mcf::ValueStore valueStore;
    registerValueTypes(valueStore);
    mcf::ValueRecorder valueRecorder(valueStore);

    valueRecorder.enableExtMemSerialization("/test1");
    valueRecorder.enableExtMemSerialization("/test2");
    valueRecorder.enableExtMemCompression("/test1");
    valueRecorder.setWriteQueueSizeLimit(1000);

    const std::string testfile = "record_extmem.bin";
    std::remove(testfile.c_str());
    uint64_t t0 = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now().time_since_epoch())
                      .count();
    valueRecorder.start(testfile);

    auto val = TestValueExtMem(5);
    val.extMemInit(10);
    val.extMemPtr()[0] = 0;
    val.extMemPtr()[1] = 1;
    val.extMemPtr()[9] = 9;
    valueStore.setValue("/test1", std::move(val));

    auto val2 = TestValueExtMem(5);
    val2.extMemInit(100);
    val2.extMemPtr()[0] = 0xff;
    val2.extMemPtr()[99] = 0xff;
    valueStore.setValue("/test2", std::move(val2));

    valueStore.setValue("/test3", TestValue(9999));

    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!valueRecorder.writeQueueEmpty());
    valueRecorder.stop();
    uint64_t t1 = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now().time_since_epoch())
                      .count();

    std::string str = readFile(testfile);

    size_t off = 0;

    // value1 header1
    auto pHeader = msgpack::unpack(str.data(), str.size(), off);
    auto valueTime = pHeader.get().via.array.ptr[0].as<uint64_t>();
    EXPECT_LE(t0, valueTime);
    EXPECT_LE(valueTime, t1);
    EXPECT_EQ("/test1", pHeader.get().via.array.ptr[1].as<std::string>());
    EXPECT_EQ("TestValueExtMem", pHeader.get().via.array.ptr[2].as<std::string>());
    EXPECT_EQ(5, msgpack::unpack(str.data(), str.size(), off).get().as<std::vector<int>>()[0]);

    // value1 extmem header
    auto mHeader = msgpack::unpack(str.data(), str.size(), off);
    unsigned long int uncompressedLen = 10;
    EXPECT_EQ(uncompressedLen, mHeader.get().via.array.ptr[0].as<uint32_t>());
    EXPECT_EQ(true, mHeader.get().via.array.ptr[1].as<bool>());
#if HAVE_ZLIB
    uLongf compressedLen = 13;
    EXPECT_EQ(compressedLen, mHeader.get().via.array.ptr[2].as<uint32_t>());

    // value1 compressed extmem data
    Bytef* compressed = reinterpret_cast<Bytef*>(&str[off]);
    std::unique_ptr<Bytef[]> extMemBuffer = std::make_unique<Bytef[]>(uncompressedLen);
    int ret = uncompress(extMemBuffer.get(), &uncompressedLen, compressed, compressedLen);
    EXPECT_EQ(ret, Z_OK);
    EXPECT_EQ(10, uncompressedLen);

    off += compressedLen;
#else
    EXPECT_EQ(0, mHeader.get().via.array.ptr[2].as<uint32_t>());
    std::string extMemBuffer = str.substr(off, 10);

    off += 10;
#endif
    EXPECT_EQ(0, extMemBuffer[0]);
    EXPECT_EQ(1, extMemBuffer[1]);
    EXPECT_EQ(9, extMemBuffer[9]);

    // value2 header
    pHeader = msgpack::unpack(str.data(), str.size(), off);
    valueTime = pHeader.get().via.array.ptr[0].as<uint64_t>();
    EXPECT_LE(t0, valueTime);
    EXPECT_LE(valueTime, t1);
    EXPECT_EQ("/test2", pHeader.get().via.array.ptr[1].as<std::string>());
    EXPECT_EQ("TestValueExtMem", pHeader.get().via.array.ptr[2].as<std::string>());
    EXPECT_EQ(5, msgpack::unpack(str.data(), str.size(), off).get().as<std::vector<int>>()[0]);

    // value2 extmem header
    mHeader = msgpack::unpack(str.data(), str.size(), off);

    EXPECT_EQ(100, mHeader.get().via.array.ptr[0].as<uint32_t>());
    EXPECT_EQ(true, mHeader.get().via.array.ptr[1].as<bool>());
    EXPECT_EQ(0, mHeader.get().via.array.ptr[2].as<uint32_t>());

    // value2 extmem data
    std::string extMem = str.substr(off, 100);
    EXPECT_EQ('\xFF', extMem[0]);
    EXPECT_EQ('\xFF', extMem[99]);

    off += 100;

    // value3 header
    pHeader = msgpack::unpack(str.data(), str.size(), off);
    EXPECT_EQ("/test3", pHeader.get().via.array.ptr[1].as<std::string>());
    EXPECT_EQ("TestValue", pHeader.get().via.array.ptr[2].as<std::string>());
    EXPECT_EQ(9999, msgpack::unpack(str.data(), str.size(), off).get().as<std::vector<int>>()[0]);

    // delete the created file
    std::remove(testfile.c_str());
}


TEST_F(ValueRecorderTest, DataRace)
{
    mcf::ValueStore valueStore;
    registerValueTypes(valueStore);
    mcf::ValueRecorder valueRecorder(valueStore);

    const std::string testfile = "record.bin";

    std::remove(testfile.c_str());
    valueRecorder.start(testfile);

    uint64_t t0 = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch())
                .count();
    // number of operations will be roughly weighted per task 
    int n = 100000;

    std::thread setThread = std::thread([&valueStore, n] { 
        for(int i = 0; i < n*40; ++i)
        {
            valueStore.setValue("/test1", TestValue(5));
        }
    });

    std::thread extmemEnableThread = std::thread([&valueRecorder, n] { 
        for(int i = 0; i < n*5; ++i)
        {
            valueRecorder.enableExtMemSerialization(std::to_string(i));
        }
    });

    std::thread topicDisableThread = std::thread([&valueRecorder, n] { 
        for(int i = 0; i < n*10; ++i)
        {
           valueRecorder.disableSerialization(std::to_string(i));
        }
    });

    for(int i = 0; i < n; ++i)
    {
        valueRecorder.enableExtMemSerialization("_" + std::to_string(i));
        valueRecorder.enableExtMemCompression("_" + std::to_string(i));
        valueRecorder.disableSerialization("_" + std::to_string(i));
    }

    extmemEnableThread.join();
    topicDisableThread.join();
    setThread.join();
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!valueRecorder.writeQueueEmpty());
    valueRecorder.stop();

    std::string str = readFile(testfile);

    size_t off = 0;

    int i = 0;
    uint64_t tOld = t0;
    while(i < n*40)
    {
        auto pHeader = msgpack::unpack(str.data(), str.size(), off);
        auto valueTime = pHeader.get().via.array.ptr[0].as<uint64_t>();
        EXPECT_LE(tOld, valueTime);
        tOld = valueTime;
        if(pHeader.get().via.array.ptr[1].as<std::string>() == "/mcf/recorder/status")
        {
            // advance offset counter, skip value
            msgpack::unpack(str.data(), str.size(), off); 
            msgpack::unpack(str.data(), str.size(), off);
            continue;
        }
        EXPECT_EQ("/test1", pHeader.get().via.array.ptr[1].as<std::string>());
        EXPECT_EQ("TestValue", pHeader.get().via.array.ptr[2].as<std::string>());
        EXPECT_EQ(5, msgpack::unpack(str.data(), str.size(), off).get().as<std::vector<int>>()[0]);
        // extmem header
        msgpack::unpack(str.data(), str.size(), off);
        ++i;
    }

    // delete the created file
    std::remove(testfile.c_str());
}

}
