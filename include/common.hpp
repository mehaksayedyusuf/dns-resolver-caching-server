#ifndef COMMON_HPP
#define COMMON_HPP

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

// DNS Constants (RFC 1035)
constexpr uint16_t DNS_PORT = 53;
constexpr uint16_t DEFAULT_SERVER_PORT = 8053;
constexpr size_t DNS_MAX_PACKET_SIZE = 512;
constexpr size_t DNS_HEADER_SIZE = 12;
constexpr size_t MAX_DECOMPRESSION_DEPTH = 16;

// Query Types
constexpr uint16_t QTYPE_A     = 1;
constexpr uint16_t QTYPE_NS    = 2;
constexpr uint16_t QTYPE_CNAME = 5;
constexpr uint16_t QTYPE_SOA   = 6;
constexpr uint16_t QTYPE_PTR   = 12;
constexpr uint16_t QTYPE_MX    = 15;
constexpr uint16_t QTYPE_TXT   = 16;
constexpr uint16_t QTYPE_AAAA  = 28;

// Query Classes
constexpr uint16_t QCLASS_IN   = 1;

// Response Codes (RCODE)
constexpr uint8_t RCODE_NOERROR  = 0;
constexpr uint8_t RCODE_FORMERR  = 1;
constexpr uint8_t RCODE_SERVFAIL = 2;
constexpr uint8_t RCODE_NXDOMAIN = 3;
constexpr uint8_t RCODE_NOTIMP   = 4;
constexpr uint8_t RCODE_REFUSED  = 5;

// Header Flag Bitmasks
constexpr uint16_t FLAG_QR = 0x8000;
constexpr uint16_t FLAG_AA = 0x0400;
constexpr uint16_t FLAG_TC = 0x0200;
constexpr uint16_t FLAG_RD = 0x0100;
constexpr uint16_t FLAG_RA = 0x0080;
constexpr uint16_t FLAG_RCODE_MASK = 0x000F;

// ============================================================================
// Native Thread Synchronization Primitives (RAII Wrapper over Win32)
// ============================================================================

// Mutual Exclusion Lock (Exclusive)
class Mutex {
private:
    CRITICAL_SECTION cs_;
public:
    Mutex() { InitializeCriticalSection(&cs_); }
    ~Mutex() { DeleteCriticalSection(&cs_); }
    void lock() { EnterCriticalSection(&cs_); }
    void unlock() { LeaveCriticalSection(&cs_); }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
};

// RAII Lock Guard for Mutex
class LockGuard {
private:
    Mutex& mtx_;
public:
    explicit LockGuard(Mutex& mtx) : mtx_(mtx) { mtx_.lock(); }
    ~LockGuard() { mtx_.unlock(); }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

// Reader-Writer Lock (Multi-reader, single-writer using Win32 SRWLOCK)
class SharedMutex {
private:
    SRWLOCK lock_;
public:
    SharedMutex() { InitializeSRWLock(&lock_); }
    ~SharedMutex() = default;

    void lock_shared() { AcquireSRWLockShared(&lock_); }
    void unlock_shared() { ReleaseSRWLockShared(&lock_); }

    void lock() { AcquireSRWLockExclusive(&lock_); }
    void unlock() { ReleaseSRWLockExclusive(&lock_); }

    SharedMutex(const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;
};

// RAII Read Lock Guard (Shared)
class ReadLockGuard {
private:
    SharedMutex& mtx_;
public:
    explicit ReadLockGuard(SharedMutex& mtx) : mtx_(mtx) { mtx_.lock_shared(); }
    ~ReadLockGuard() { mtx_.unlock_shared(); }

    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;
};

// RAII Write Lock Guard (Exclusive)
class WriteLockGuard {
private:
    SharedMutex& mtx_;
public:
    explicit WriteLockGuard(SharedMutex& mtx) : mtx_(mtx) { mtx_.lock(); }
    ~WriteLockGuard() { mtx_.unlock(); }

    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;
};

// Byte reading helpers (Big Endian wire network format)
inline uint16_t read_uint16_be(const uint8_t* buf) {
    return (static_cast<uint16_t>(buf[0]) << 8) | static_cast<uint16_t>(buf[1]);
}

inline uint32_t read_uint32_be(const uint8_t* buf) {
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8)  |
            static_cast<uint32_t>(buf[3]);
}

inline void write_uint16_be(uint8_t* buf, uint16_t val) {
    buf[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(val & 0xFF);
}

inline void write_uint32_be(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(val & 0xFF);
}

#endif // COMMON_HPP
