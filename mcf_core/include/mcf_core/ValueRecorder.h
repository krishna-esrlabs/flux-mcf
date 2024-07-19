/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_VALUE_RECORDER_H
#define MCF_VALUE_RECORDER_H

#include "ValueStore.h"
#include <fstream>
#include <mutex>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include "sys/time.h"
#include "sys/resource.h"

namespace mcf {

class ValueRecorder {

public:
    explicit ValueRecorder(ValueStore& valueStore);

    ~ValueRecorder();

    /**
     * Start recording all topics
     *
     * @param filename  the filename to use for recording
     */
    void start(const std::string& filename);

    /**
     * Check if the queue holding the values to be written is empty
     *
     * @return true if the queue is empty, false otherwise
     */
    bool writeQueueEmpty() { return fQueue->size() == 0ul; }

    void stop();

    /**
     * enable serialization of ext mem values for a specific topic
     */
    void enableExtMemSerialization(const std::string& topic);

    /**
     * enable compression of ext mem values for a specific topic
     * Enablin compression only has an effect on topics with serialization enabled
     */
    void enableExtMemCompression(const std::string& topic);

    /**
     * set a hard limit for the number of elements in the write buffer queue
     */
    void setWriteQueueSizeLimit(uint32_t limit);

    /**
     * disable serialization for a specific topic
     */
    void disableSerialization(const std::string& topic);

private:

    struct PacketHeader {
        uint64_t time;
        std::string topic;
        std::string tid;
        uint64_t vid;
        MSGPACK_DEFINE(time, topic, tid, vid)
    };

    struct ExtMemHeader {
        uint32_t extmemSize;
        bool extmemPresent;
        uint32_t extmemSizeCompressed;
        MSGPACK_DEFINE(extmemSize, extmemPresent, extmemSizeCompressed)
    };

    struct QueueEntry {
        std::chrono::high_resolution_clock::time_point time;
        std::string topic = "";
        ValuePtr value = nullptr;
    };

    void writeThread();

    bool isExtMemEnabled(const std::string& topic) const;

    bool isExtMemCompressionEnabled(const std::string& topic) const;

    bool isTopicEnabled(const std::string& topic) const;

    void serialize(QueueEntry& qe);

    class Queue : public IValueReceiver {
    public:
        Queue();

        void receive(const std::string& topic, ValuePtr& value) override;

        size_t pop(QueueEntry& qe);

        size_t size();

    private:
        struct QueueValue {
            std::chrono::high_resolution_clock::time_point time;
            std::size_t topicId;
            ValuePtr value;
        };

        mutex::PriorityInheritanceMutex fMutex;
        std::deque<QueueValue> fDeque;
        std::map<std::size_t, std::string> fTopicIdMap;
    };


    class StatusMonitor {
    public:
        explicit StatusMonitor(ValueStore& valueStore);

        void start();

        void serializeBegin(size_t queueSize, std::chrono::high_resolution_clock::time_point& recordTime);

        void serializeEnd() {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-fLastOutputTime).count() > 1000) {
                outputStatus();
            }
        }

        void reportWriteError(const std::string& error);

        void reportDropped();

        void incBytesWritten(size_t num) {
            fBytesWritten += num;
        }

    private:

        void initStatus();
        void outputStatus();

        ValueStore& fValueStore;
        msg::RecorderStatus fRecorderStatus;
        unsigned long fBytesWritten = 0UL;
        unsigned long fTotalLatency = 0UL;
        unsigned long fTotalQueueSize = 0UL;
        unsigned long fNumWrites = 0UL;
        std::chrono::high_resolution_clock::time_point fLastOutputTime;
        struct rusage fResourceUsage = {};
        uint32_t fDropCount = 0u;
    };


    ValueStore& fValueStore;
    std::shared_ptr<Queue> fQueue;
    std::thread fThread;
    int fFile;
    std::atomic<bool> fStopRequest;
    std::unordered_set<std::string> fDisabledTopics;
    std::unordered_set<std::string> fEnabledExtMemTopics;
    std::unordered_set<std::string> fCompressExtMemTopics;
    StatusMonitor fStatusMonitor;
    uint32_t fQueueSizeLimit = UINT_MAX;

    mutable mutex::PriorityInheritanceMutex fMutex;
    
};

}


#endif
