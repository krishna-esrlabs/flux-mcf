/**
 * Copyright (c) 2024 Accenture
 */
#include <thread>
#include "gtest/gtest.h"
#include "mcf_core/Mcf.h"
#include "mcf_core/ExtMemValue.h"

namespace mcf {

class ValueStoreTest : public ::testing::Test {
public:
    class TestValue : public mcf::Value {
    public:
        TestValue(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };
    struct TestValueExtMem : public ExtMemValue<uint8_t> {
    public:
        TestValueExtMem(int val=0) : val(val) {};
        int val;
        MSGPACK_DEFINE(val);
    };
};

class CustomIdGenerator : public IidGenerator
{
public:
    virtual void injectId(Value& value) const override
    {
        setId(value, customId);
    }
    uint64_t customId = 0;
};


TEST_F(ValueStoreTest, ReadWrite) {
  mcf::ValueStore valueStore;

  EXPECT_EQ(valueStore.setValue("/test1", TestValue(5)), 0);
  auto ptr = valueStore.getValue<TestValue>("/test1");
  EXPECT_EQ(5, ptr->val);
  EXPECT_EQ(0, ptr->id());

  TestValue ctv(7);
  CustomIdGenerator cig;
  cig.customId = 17;
  cig.injectId(ctv);
  EXPECT_EQ(valueStore.setValue("/test1", ctv), 0);
  ptr = valueStore.getValue<TestValue>("/test1");
  EXPECT_EQ(17, ptr->id());

  std::shared_ptr<const Value> stv = std::make_shared<const TestValue>(TestValue(11));
  EXPECT_EQ(valueStore.setValue("/test1", stv), 0);
  ptr = valueStore.getValue<TestValue>("/test1");
  EXPECT_EQ(11, ptr->val);

  std::shared_ptr<const Value> stv1 = std::make_shared<const TestValue>(ctv);
  EXPECT_EQ(valueStore.setValue("/test1", stv1), 0);
  ptr = valueStore.getValue<TestValue>("/test1");
  EXPECT_EQ(7, ptr->val);
  EXPECT_EQ(17, ptr->id());
}

TEST_F(ValueStoreTest, ReadWriteExtMemRValue) {
  mcf::ValueStore valueStore;

  EXPECT_EQ(valueStore.setValue("/test1", TestValueExtMem(5)), 0);
  auto ptr = valueStore.getValue<TestValueExtMem>("/test1");
  EXPECT_EQ(5, ptr->val);
}

//
// The following test case is invalid and does not build, because ExtMemValues are not copyable
// and thus cannot be put into the value store as an lvalue reference.
//
//TEST_F(ValueStoreTest, ReadWriteExtMemLValue) {
//  mcf::ValueStore valueStore;
//
//  TestValueExtMem val(5);
//  CustomIdGenerator cig;
//  cig.customId = 3;
//  cig.injectId(val);
//  valueStore.setValue("/test1", val);
//  auto ptr = valueStore.getValue<TestValueExtMem>("/test1");
//  EXPECT_EQ(5, ptr->val);
//  EXPECT_EQ(3, ptr->id());
//}
// Instead, we statically test for non-copyability
TEST_F(ValueStoreTest, ExtMemValueIsNotCopyable) {
    EXPECT_FALSE(
        std::is_copy_constructible<TestValueExtMem>::value
        || std::is_copy_assignable<TestValueExtMem>::value);
}

TEST_F(ValueStoreTest, ReadWriteExtMemMove) {
  mcf::ValueStore valueStore;

  TestValueExtMem val(5);
  EXPECT_EQ(valueStore.setValue("/test1", std::move(val)), 0);
  auto ptr = valueStore.getValue<TestValueExtMem>("/test1");
  EXPECT_EQ(5, ptr->val);
}

TEST_F(ValueStoreTest, OverWrite) {
  mcf::ValueStore valueStore;

  EXPECT_EQ(valueStore.setValue("/test1", TestValue(5)), 0);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(6)), 0);
  auto ptr = valueStore.getValue<TestValue>("/test1");
  EXPECT_EQ(6, ptr->val);
}

TEST_F(ValueStoreTest, Queue) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::ValueQueue>();

  valueStore.addReceiver("/test1", queue);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2)), 0);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(3)), 0);

  EXPECT_EQ(1, queue->pop<TestValue>()->val);
  EXPECT_EQ(2, queue->pop<TestValue>()->val);
  EXPECT_EQ(3, queue->pop<TestValue>()->val);
  EXPECT_TRUE(queue->empty());
}

TEST_F(ValueStoreTest, QueueSetMaxLength) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::ValueQueue>(3);

  valueStore.addReceiver("/test1", queue);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2)), 0);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(3)), 0);

  queue->setMaxLength(2);

  EXPECT_EQ(2, queue->pop<TestValue>()->val);
  EXPECT_EQ(3, queue->pop<TestValue>()->val);
  EXPECT_TRUE(queue->empty());
}

TEST_F(ValueStoreTest, QueueEmpty) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::ValueQueue>();
  EXPECT_TRUE(queue->empty());

  valueStore.addReceiver("/test1", queue);
  EXPECT_TRUE(queue->empty());

  EXPECT_THROW(queue->pop<TestValue>(), mcf::QueueEmptyException);
  EXPECT_THROW(queue->popWithTopic<TestValue>(), mcf::QueueEmptyException);
}

TEST_F(ValueStoreTest, QueueOverflow) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::ValueQueue>(2);

  valueStore.addReceiver("/test1", queue);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2)), 0);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(3)), 0);

  EXPECT_EQ(2, queue->pop<TestValue>()->val);
  EXPECT_EQ(3, queue->pop<TestValue>()->val);
  EXPECT_TRUE(queue->empty());
}

TEST_F(ValueStoreTest, QueueMulti) {
  mcf::ValueStore valueStore;
  auto queue1 = std::make_shared<mcf::ValueQueue>();
  auto queue2 = std::make_shared<mcf::ValueQueue>();

  valueStore.addReceiver("/test1", queue1);
  valueStore.addReceiver("/test1", queue2);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);

  EXPECT_EQ(1, queue1->pop<TestValue>()->val);
  EXPECT_TRUE(queue1->empty());

  EXPECT_EQ(1, queue2->pop<TestValue>()->val);
  EXPECT_TRUE(queue2->empty());
}

TEST_F(ValueStoreTest, QueueRemove) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::ValueQueue>();

  valueStore.addReceiver("/test1", queue);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);
  valueStore.removeReceiver("/test1", queue);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2)), 0);

  EXPECT_EQ(1, queue->pop<TestValue>()->val);
  EXPECT_TRUE(queue->empty());
}

TEST_F(ValueStoreTest, QueueRemoveMulti) {
  mcf::ValueStore valueStore;
  auto queue1 = std::make_shared<mcf::ValueQueue>();
  auto queue2 = std::make_shared<mcf::ValueQueue>();

  valueStore.addReceiver("/test1", queue1);
  valueStore.addReceiver("/test1", queue2);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);
  valueStore.removeReceiver("/test1", queue1);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2)), 0);

  EXPECT_EQ(1, queue1->pop<TestValue>()->val);
  EXPECT_TRUE(queue1->empty());

  EXPECT_EQ(1, queue2->pop<TestValue>()->val);
  EXPECT_EQ(2, queue2->pop<TestValue>()->val);
  EXPECT_TRUE(queue2->empty());
}

TEST_F(ValueStoreTest, QueueAutoRemove) {
  mcf::ValueStore valueStore;

  for (int i=0; i<1000; i++) {
    auto queue = std::make_shared<mcf::ValueQueue>();
    valueStore.addReceiver("/test1", queue);
    EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);
  }

  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2)), 0);
}

TEST_F(ValueStoreTest, QueuePopWithTopic) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::ValueQueue>();

  valueStore.addReceiver("/test1", queue);
  valueStore.addReceiver("/test2", queue);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);
  EXPECT_EQ(valueStore.setValue("/test2", TestValue(2)), 0);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(3)), 0);

  auto t1 = queue->popWithTopic<TestValue>();
  EXPECT_EQ(1, std::get<0>(t1)->val);
  EXPECT_EQ("/test1", std::get<1>(t1));
  auto t2 = queue->popWithTopic<TestValue>();
  EXPECT_EQ(2, std::get<0>(t2)->val);
  EXPECT_EQ("/test2", std::get<1>(t2));
  auto t3 = queue->popWithTopic<TestValue>();
  EXPECT_EQ(3, std::get<0>(t3)->val);
  EXPECT_EQ("/test1", std::get<1>(t3));
  EXPECT_TRUE(queue->empty());
}

TEST_F(ValueStoreTest, EventQueue) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::EventQueue>();

  valueStore.addReceiver("/test1", queue);

  EXPECT_TRUE(queue->empty());

  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);
  EXPECT_EQ("/test1", queue->pop());
}

TEST_F(ValueStoreTest, EventQueueEmpty) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::EventQueue>();

  EXPECT_TRUE(queue->empty());
  EXPECT_THROW(queue->pop(), mcf::QueueEmptyException);
}

TEST_F(ValueStoreTest, GetWithoutSet) {
  mcf::ValueStore valueStore;

  auto ptr = valueStore.getValue<TestValue>("/test1");
  EXPECT_EQ(0, ptr->val);
}

TEST_F(ValueStoreTest, GetWithoutSetAndPreviousRegister) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::EventQueue>();

  valueStore.addReceiver("/test1", queue);
  auto ptr = valueStore.getValue<TestValue>("/test1");
  EXPECT_EQ(0, ptr->val);
}

TEST_F(ValueStoreTest, BlockingReceiverNotBlocking) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::ValueQueue>(1, true);

  valueStore.addReceiver("/test1", queue);
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(1)), 0);

  EXPECT_EQ(1, queue->pop<TestValue>()->val);
  EXPECT_TRUE(queue->empty());
}

void blockingReceiverDelayedPop(std::shared_ptr<mcf::ValueQueue> queue, std::chrono::duration<double, std::milli> delay) {
  std::this_thread::sleep_for(delay);
  queue->pop<ValueStoreTest::TestValue>();
}

TEST_F(ValueStoreTest, BlockingReceiver) {
  mcf::ValueStore valueStore;
  auto queue = std::make_shared<mcf::ValueQueue>(1, true);

  valueStore.addReceiver("/test1", queue);
  valueStore.setValue("/test1", TestValue(1));

  std::thread thread(blockingReceiverDelayedPop, queue, std::chrono::milliseconds(1000));
  auto startTime = std::chrono::system_clock::now();
  // blocking writes should be unsuccessful now
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2), false), EAGAIN);
  // this should block for about 1s and return successfully
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2), true), 0);
  // expect blocking time of 0.5s (conservative value to account for any imprecision in timing)
  EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-startTime).count(), 500);
  thread.join();

  EXPECT_EQ(2, queue->pop<TestValue>()->val);
  EXPECT_TRUE(queue->empty());
}

TEST_F(ValueStoreTest, BlockingReceiverMulti) {
  mcf::ValueStore valueStore;
  auto queue1 = std::make_shared<mcf::ValueQueue>(1, true);
  auto queue2 = std::make_shared<mcf::ValueQueue>(1, true);
  auto queue3 = std::make_shared<mcf::ValueQueue>(1);

  valueStore.addReceiver("/test1", queue1);
  valueStore.addReceiver("/test1", queue2);
  valueStore.addReceiver("/test1", queue3);

  valueStore.setValue("/test1", TestValue(1));
  EXPECT_EQ(1, queue1->pop<TestValue>()->val);

  std::thread thread(blockingReceiverDelayedPop, queue2, std::chrono::milliseconds(1000));
  auto startTime = std::chrono::system_clock::now();
  // blocking writes should be unsuccessful now
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2), false), EAGAIN);
  // this should block for about 1s and return successfully
  EXPECT_EQ(valueStore.setValue("/test1", TestValue(2)), 0);
  // expect blocking time of 0.5s (conservative value to account for any imprecision in timing)
  EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-startTime).count(), 500);
  thread.join();

  EXPECT_EQ(2, queue1->pop<TestValue>()->val);
  EXPECT_EQ(2, queue2->pop<TestValue>()->val);
  EXPECT_EQ(2, queue3->pop<TestValue>()->val);
  EXPECT_TRUE(queue1->empty());
  EXPECT_TRUE(queue2->empty());
  EXPECT_TRUE(queue3->empty());
}
}

