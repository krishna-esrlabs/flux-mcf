/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_TIMESTAMPTYPE_H_
#define MCF_TIMESTAMPTYPE_H_

#include <chrono>
#include <ostream>

namespace mcf {

/**
 * Helper class for converting between microseconds from epoch and chrono time_points
 */
class TimestampType {
public:

    TimestampType();

    explicit TimestampType(std::chrono::system_clock::time_point timestamp);
    
    /**
     * @param timestamp     Microseconds since epoch.
     */
    explicit TimestampType(uint64_t timestamp);

    /**
     * @param timestamp     Microseconds since epoch.
     */
    explicit TimestampType(std::chrono::microseconds timestamp);

    explicit operator uint64_t ();
    explicit operator std::chrono::system_clock::time_point();

    friend std::ostream& operator << (std::ostream &out, const TimestampType& obj);

    friend std::chrono::duration<int64_t, std::micro> operator+ (const TimestampType &lhs, const TimestampType &rhs);
    friend std::chrono::duration<int64_t, std::micro> operator- (const TimestampType &lhs, const TimestampType &rhs);

    friend bool operator== (const TimestampType &lhs, const TimestampType &rhs);
    friend bool operator!= (const TimestampType &lhs, const TimestampType &rhs);

    friend bool operator> (const TimestampType &lhs, const TimestampType &rhs);
    friend bool operator<= (const TimestampType &lhs, const TimestampType &rhs);
 
    friend bool operator< (const TimestampType &lhs, const TimestampType &rhs);
    friend bool operator>= (const TimestampType &lhs, const TimestampType &rhs);

private:

    void setTime(uint64_t timestamp);
    uint64_t getMicrosecondsSinceEpoch() const;

    std::chrono::system_clock::time_point fTime;

};

} // namespace mcf

#endif /* MCF_TIMESTAMPTYPE_H_ */
