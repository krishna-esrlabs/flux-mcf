/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/TimestampType.h"

namespace mcf
{
TimestampType::TimestampType() = default;

TimestampType::TimestampType(std::chrono::system_clock::time_point timestamp) 
: fTime(timestamp)
{}


TimestampType::TimestampType(uint64_t timestamp)
{
    setTime(timestamp);
}


TimestampType::TimestampType(std::chrono::microseconds timestamp)
{
    setTime(timestamp.count());
}


TimestampType::operator uint64_t ()
{
    return getMicrosecondsSinceEpoch();
}


TimestampType::operator std::chrono::system_clock::time_point ()
{
    return fTime;
}


std::ostream& operator << (std::ostream &out, const TimestampType& obj)
{
    out << std::chrono::duration_cast<std::chrono::microseconds>(obj.fTime.time_since_epoch()).count();
    return out;
}


std::chrono::duration<int64_t, std::micro> operator+ (const TimestampType &lhs, const TimestampType &rhs)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(lhs.fTime.time_since_epoch()) +
            std::chrono::duration_cast<std::chrono::microseconds>(rhs.fTime.time_since_epoch());
}


std::chrono::duration<int64_t, std::micro> operator- (const TimestampType &lhs, const TimestampType &rhs)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(lhs.fTime.time_since_epoch()) - 
            std::chrono::duration_cast<std::chrono::microseconds>(rhs.fTime.time_since_epoch()); 
}


bool operator== (const TimestampType &lhs, const TimestampType &rhs)
{
    return lhs.fTime == rhs.fTime;
}


bool operator!= (const TimestampType &lhs, const TimestampType &rhs)
{
    return lhs.fTime != rhs.fTime;
}


bool operator> (const TimestampType &lhs, const TimestampType &rhs)
{
    return lhs.fTime > rhs.fTime;
}

 
bool operator>= (const TimestampType &lhs, const TimestampType &rhs)
{
    return lhs.fTime >= rhs.fTime;
}
 

bool operator< (const TimestampType &lhs, const TimestampType &rhs)
{
    return lhs.fTime < rhs.fTime;
}
 

bool operator<= (const TimestampType &lhs, const TimestampType &rhs)
{
    return lhs.fTime <= rhs.fTime;
}


void TimestampType::setTime(uint64_t timestamp)
{
    std::chrono::duration<int64_t, std::micro> chronoTimestampTmp(timestamp);
    std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> chronoTimestamp(chronoTimestampTmp);

    fTime = chronoTimestamp;
}


uint64_t TimestampType::getMicrosecondsSinceEpoch() const
{
    return std::chrono::duration_cast<std::chrono::microseconds>(fTime.time_since_epoch()).count();
}

}