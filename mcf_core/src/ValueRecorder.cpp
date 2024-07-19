/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/ValueRecorder.h"
#include "mcf_core/ValueStore.h"
#include "mcf_core/ThreadName.h"
#include "mcf_core/LoggingMacros.h"

#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include "sys/time.h"
#include "sys/resource.h"
#if HAVE_ZLIB
#include <zlib.h>
#endif

namespace mcf 
{

namespace
{

/*
 * calc diff t2 - t1 in seconds
 */
float timeDiffSeconds(struct timeval& t2, struct timeval& t1) 
{
    struct timeval diff;
    timersub(&t2, &t1, &diff);
    return diff.tv_sec + ((float)diff.tv_usec)/1000000;
}

} // anonymous namespace

ValueRecorder::ValueRecorder(ValueStore& valueStore) :
        fValueStore(valueStore),
        fQueue(std::make_shared<Queue>()),
        fFile(-1),
        fStopRequest(false),
        fStatusMonitor(valueStore)
{}

ValueRecorder::~ValueRecorder()
{
    stop();
}

/*
 * Start recording all topics
 */
void ValueRecorder::start(const std::string& filename) 
{
    if (fFile >= 0)
    {
        std::cout << "ERROR: value recorder already started" << strerror(errno) << std::endl;
        return;
    }

    fFile = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666 );
    if (fFile >= 0) 
    {
        fValueStore.addAllTopicReceiver(fQueue);
        fStopRequest = false;

        fThread = std::thread([this] { writeThread(); });
    }
    else 
    {
        std::cout << "ERROR: opening record file: " << strerror(errno) << std::endl;
        return;
    }
}

void ValueRecorder::stop() {

    if (fFile >= 0) 
    {
        fStopRequest = true;
        fValueStore.removeAllTopicReceiver(fQueue);
        fThread.join();
        close(fFile);
        fFile = -1;
    }
    else 
    {
        // not started
    }
}

/*
 * enable serialization of ext mem values for a specific topic
 */
void ValueRecorder::enableExtMemSerialization(const std::string& topic) 
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    fEnabledExtMemTopics.insert(topic);
}

void ValueRecorder::enableExtMemCompression(const std::string& topic) 
{
#if HAVE_ZLIB
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    fCompressExtMemTopics.insert(topic);
    if(fEnabledExtMemTopics.find(topic) == fEnabledExtMemTopics.end())
    {
        MCF_WARN_NOFILELINE(
            "Setting ExtMem compression for topic '{}' has no effect. "
            "Its ExMem data is not recorded.",
            topic);
    }
#else
    MCF_WARN_NOFILELINE(
        "Setting ExtMem compression for topic '{}' has no effect. "
        "Compression is not available. Make sure HAVE_ZLIB is set.",
        topic);
#endif
}

void ValueRecorder::setWriteQueueSizeLimit(uint32_t limit)
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    fQueueSizeLimit = limit;
}

/*
 * disable serialization for a specific topic
 */
void ValueRecorder::disableSerialization(const std::string& topic) 
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    fDisabledTopics.insert(topic);
}

void ValueRecorder::writeThread() 
{
    setThreadName("ValueRecorderW");
    QueueEntry qe;
    fStatusMonitor.start();
    while(!fStopRequest) 
    {
        size_t queueSize = fQueue->pop(qe);
        while(qe.value != nullptr) 
        {
            size_t queueSizeLimit;
            {
                std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
                queueSizeLimit = fQueueSizeLimit;
            }
            if(queueSize < queueSizeLimit || qe.topic == "/mcf/recorder/status")
            {
                serialize(qe);
            }
            else
            {
                fStatusMonitor.reportDropped();
            }
            queueSize = fQueue->pop(qe);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool ValueRecorder::isExtMemEnabled(const std::string& topic) const 
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fEnabledExtMemTopics.find(topic) != fEnabledExtMemTopics.end();
}

bool ValueRecorder::isExtMemCompressionEnabled(const std::string& topic) const 
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fCompressExtMemTopics.find(topic) != fCompressExtMemTopics.end();
}

bool ValueRecorder::isTopicEnabled(const std::string& topic) const 
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fDisabledTopics.find(topic) == fDisabledTopics.end();
}

void ValueRecorder::serialize(QueueEntry& qe) 
{
    auto typeinfoPtr = fValueStore.getTypeInfo(*qe.value);

    if (isTopicEnabled(qe.topic)) 
    {
        if (typeinfoPtr != nullptr) 
        {
            fStatusMonitor.serializeBegin(fQueue->size(), qe.time);

            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> pk(&buffer);

            PacketHeader pHeader;
            pHeader.time = std::chrono::duration_cast<std::chrono::milliseconds>(
                qe.time.time_since_epoch()).count();
            pHeader.topic = qe.topic;
            pHeader.tid = typeinfoPtr->id;
            pHeader.vid = qe.value->id();
            pk.pack(pHeader);

            const void* ptr        = nullptr;
            size_t uncompressedLen = 0;
            size_t size            = 0;

            bool extMemEnabled = isExtMemEnabled(qe.topic);
            bool compressExtMem = isExtMemCompressionEnabled(qe.topic);

            typeinfoPtr->packFunc(pk, qe.value, ptr, uncompressedLen, extMemEnabled);

            bool packExtMem = (uncompressedLen > 0) && extMemEnabled;

            ExtMemHeader mHeader{};
            mHeader.extmemSize = uncompressedLen;
            mHeader.extmemPresent = packExtMem;
#if HAVE_ZLIB
            std::unique_ptr<Bytef[]> compressed = nullptr;
            if(packExtMem && compressExtMem)
            {
                uLongf compressedLen = uncompressedLen*2;
                const Bytef* srcPtr = reinterpret_cast<const Bytef*>(ptr);
                compressed = std::make_unique<Bytef[]>(compressedLen);

                int ret = compress(compressed.get(), &compressedLen, srcPtr, uncompressedLen);
                if(ret == Z_OK)
                {
                    size = compressedLen;
                    mHeader.extmemSizeCompressed = compressedLen;
                    ptr = compressed.get();
                }
                else
                {
                    std::string warning = fmt::format(
                        "Could not compress extmem data on {}. "
                        "Falling back to non-compressed recording.",
                        qe.topic
                    );
                    fStatusMonitor.reportWriteError(warning);
                    MCF_WARN(warning);
                    size = uncompressedLen;
                    mHeader.extmemSizeCompressed = 0;
                }
            }
            else
#endif
            {
                size = uncompressedLen;
                mHeader.extmemSizeCompressed = 0;
            }


            pk.pack(mHeader);

            if (write(fFile, buffer.data(), buffer.size()) < 0) 
            {
                fStatusMonitor.reportWriteError(std::string(strerror(errno)));
            }
            else 
            {
                fStatusMonitor.incBytesWritten(buffer.size());
            }
            if (packExtMem)
            {
                if (write(fFile, static_cast<const char*>(ptr), size) < 0) 
                {
                    fStatusMonitor.reportWriteError(std::string(strerror(errno)));
                }
                else
                {
                    fStatusMonitor.incBytesWritten(size);
                }
            }
            
            fStatusMonitor.serializeEnd();
        }
        else
        {
            // ignoring value which can't be serialized
        }
    }
}

ValueRecorder::Queue::Queue() : fMutex() 
{}

void ValueRecorder::Queue::receive(const std::string& topic, ValuePtr& value) 
{
    QueueValue qe;
    qe.time = std::chrono::high_resolution_clock::now();
    qe.value = value;
    auto topicId = std::hash<std::string>{}(topic);
    qe.topicId = topicId;
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    if (fTopicIdMap.find(topicId) == fTopicIdMap.end())
    {
        fTopicIdMap[topicId] = topic;
    }

    fDeque.push_back(qe);
}

size_t ValueRecorder::Queue::pop(QueueEntry& qe) 
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    if (!fDeque.empty()) 
    {
        auto value = fDeque.front();
        qe = QueueEntry{value.time, fTopicIdMap.at(value.topicId), value.value};
        fDeque.pop_front();
        return fDeque.size();
    }
    else 
    {
        qe = QueueEntry();
        return 0;
    }
}

size_t ValueRecorder::Queue::size() 
{
    std::lock_guard<mutex::PriorityInheritanceMutex> lk(fMutex);
    return fDeque.size();
}

ValueRecorder::StatusMonitor::StatusMonitor(ValueStore& valueStore)
: fValueStore(valueStore)
{}

void ValueRecorder::StatusMonitor::start() 
{
    fLastOutputTime = std::chrono::high_resolution_clock::now();
    getrusage(RUSAGE_THREAD, &fResourceUsage);
    initStatus();
}

void ValueRecorder::StatusMonitor::serializeBegin(
        size_t queueSize,
        std::chrono::high_resolution_clock::time_point& recordTime) 
        {
    unsigned long latency = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-recordTime).count();
    fTotalLatency += latency;
    fNumWrites += 1;
    if (latency > fRecorderStatus.maxLatencyMs) 
    {
        fRecorderStatus.maxLatencyMs = latency;
    }
    fTotalQueueSize += queueSize;
    if (queueSize > fRecorderStatus.maxQueueSize) 
    {
        fRecorderStatus.maxQueueSize = queueSize;
    }
}

void ValueRecorder::StatusMonitor::reportWriteError(const std::string& error) 
{
    fRecorderStatus.errorFlag = true;
    fRecorderStatus.errorDescs.insert(error);
}


void ValueRecorder::StatusMonitor::reportDropped() 
{
    fRecorderStatus.dropFlag = true;
    if(fDropCount < UINT_MAX) // counter is not increased if UINT_MAX is reached.
    {
        fDropCount += 1;
    }
}

void ValueRecorder::StatusMonitor::initStatus() 
{
    fRecorderStatus.dropFlag = false;
    fRecorderStatus.errorFlag = false;
    fRecorderStatus.maxLatencyMs = 0;
    fRecorderStatus.maxQueueSize = 0;
    fRecorderStatus.errorDescs.clear();
    fBytesWritten = 0;
    fTotalLatency = 0;
    fTotalQueueSize = 0;
    fNumWrites = 0;
}

void ValueRecorder::StatusMonitor::outputStatus() 
{
    float dt = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-fLastOutputTime).count();

    struct rusage resourceUsage;
    getrusage(RUSAGE_THREAD, &resourceUsage);

    float userTime = timeDiffSeconds(resourceUsage.ru_utime, fResourceUsage.ru_utime);
    float systemTime = timeDiffSeconds(resourceUsage.ru_stime, fResourceUsage.ru_stime);

    fRecorderStatus.outputBps = fBytesWritten / dt;
    fRecorderStatus.avgLatencyMs = fTotalLatency / fNumWrites;
    fRecorderStatus.avgQueueSize = fTotalQueueSize / fNumWrites;
    fRecorderStatus.cpuUsageUser = userTime * 100 / dt;
    fRecorderStatus.cpuUsageSystem = systemTime * 100 / dt;

    fValueStore.setValue("/mcf/recorder/status", fRecorderStatus);

    if(fRecorderStatus.dropFlag)
    {
        MCF_ERROR_NOFILELINE(
            "ValueRecorder has dropped {} values as it cannot process them fast enough.",
            fDropCount);
        fDropCount = 0u;
    }

    if(fRecorderStatus.avgLatencyMs > 1000)
    {
        MCF_WARN_NOFILELINE(
            "ValueRecorder writes are delayed by {} ms. Values are piling up in the ValueRecorder.", 
            fRecorderStatus.avgLatencyMs);
    }

    fLastOutputTime = std::chrono::high_resolution_clock::now();
    fResourceUsage = resourceUsage;
    initStatus();
}

} // namespace mcf

