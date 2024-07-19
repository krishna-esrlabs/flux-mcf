/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/Mutexes.h"
#include "spdlog/fmt/fmt.h"

#include <fstream>
#include <thread>

namespace
{
TEST(MutexTest, StaticProperties)
{
    // check that the abstract mutexes cannot be instantiated
    static_assert(
        !std::is_default_constructible<mcf::mutex::AbstractPosixThreadMutex>::value,
        "Abstract mutex base classes must not be constructible");
    static_assert(
        !std::is_default_constructible<mcf::mutex::SimpleAbstractPosixThreadMutex>::value,
        "Abstract mutex base classes must not be constructible");
    static_assert(
        !std::is_copy_constructible<mcf::mutex::AbstractPosixThreadMutex>::value,
        "Abstract mutex base classes must not be copy-constructible");
    static_assert(
        !std::is_copy_constructible<mcf::mutex::SimpleAbstractPosixThreadMutex>::value,
        "Abstract mutex base classes must not be copy-constructible");
    static_assert(
        !std::is_move_constructible<mcf::mutex::AbstractPosixThreadMutex>::value,
        "Abstract mutex base classes must not be move-constructible");
    static_assert(
        !std::is_move_constructible<mcf::mutex::SimpleAbstractPosixThreadMutex>::value,
        "Abstract mutex base classes must not be move-constructible");

    // check that the derived mutexes cannot be copied or moved
    static_assert(
        !std::is_move_constructible<mcf::mutex::PosixThreadMutex>::value,
        "Mutex objects must not be move-constructible");
    static_assert(
        !std::is_move_constructible<mcf::mutex::PriorityCeilingMutex>::value,
        "Mutex objects must not be move-constructible");
    static_assert(
        !std::is_move_constructible<mcf::mutex::PriorityInheritanceMutex>::value,
        "Mutex objects must not be move-constructible");
    static_assert(
        !std::is_copy_constructible<mcf::mutex::PosixThreadMutex>::value,
        "Mutex objects must not be copy-constructible");
    static_assert(
        !std::is_copy_constructible<mcf::mutex::PriorityCeilingMutex>::value,
        "Mutex objects must not be copy-constructible");
    static_assert(
        !std::is_copy_constructible<mcf::mutex::PriorityInheritanceMutex>::value,
        "Mutex objects must not be copy-constructible");
}

TEST(MutexTest, Locking)
{
    std::vector<std::unique_ptr<mcf::mutex::AbstractPosixThreadMutex> > mutexes;
    mutexes.push_back(std::make_unique<mcf::mutex::PosixThreadMutex>());
    mutexes.push_back(std::make_unique<mcf::mutex::PriorityCeilingMutex>(1));
    mutexes.push_back(std::make_unique<mcf::mutex::PriorityInheritanceMutex>());

    for (const auto& mutex : mutexes)
    {
        // test lock-unlock cycle
        // first, the mutex should be unowned and lockable
        EXPECT_TRUE(mutex->try_lock());
        // now it is owned, so no further locking should be possible
        EXPECT_FALSE(mutex->try_lock());
        // unlocking should work
        EXPECT_NO_THROW(mutex->unlock());
        // then the mutex is unowned and lockable again
        EXPECT_TRUE(mutex->try_lock());
        // again, it is locked
        EXPECT_FALSE(mutex->try_lock());
        // release the mutex
        EXPECT_NO_THROW(mutex->unlock());
    }
}

TEST(MutexTest, MutuallyExclusiveOwnership)
{
    std::vector<std::unique_ptr<mcf::mutex::AbstractPosixThreadMutex> > mutexes;
    mutexes.push_back(std::make_unique<mcf::mutex::PosixThreadMutex>());
    mutexes.push_back(std::make_unique<mcf::mutex::PriorityCeilingMutex>(1));
    mutexes.push_back(std::make_unique<mcf::mutex::PriorityInheritanceMutex>());

    for (const auto& mutex : mutexes)
    {
        // test ownership: Spawn a thread and check if it waits
        bool flag = false;
        mutex->lock();

        auto thread = std::thread([&mutex, &flag]() {
            mutex->lock();
            EXPECT_TRUE(flag);
            flag = false;
            mutex->unlock();
        });
        // wait, the spawned thread should also wait
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        flag = true;
        mutex->unlock();
        // now the thread should run
        thread.join();
        EXPECT_FALSE(flag);
    }
}

} // namespace