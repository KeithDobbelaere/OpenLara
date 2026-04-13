#include "Psram.hpp"

#if defined(__PICOCALC_WIN__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace ol::psram {

namespace {

constexpr std::uint32_t kInvalidAddr   = 0xFFFFFFFFu;
constexpr std::uint32_t kCapacityBytes = 8u * 1024u * 1024u;

// Measured from PsramTest.cpp results.
// Bytes/second:
constexpr double kWrite1B      = 1282050.0;
constexpr double kWrite1BAsync = 1785713.0;
constexpr double kRead1B       =  688073.0;

constexpr double kWrite2B      = 2097898.0;
constexpr double kRead2B       = 1239668.0;

constexpr double kWrite4B      = 3191479.0;
constexpr double kRead4B       = 2120137.0;

constexpr double kWrite16B     = 6098114.0;
constexpr double kRead16B      = 4592725.0;

// Small per-call penalty to reflect transaction overhead.
constexpr double kReadCallOverheadNs  = 900.0;
constexpr double kWriteCallOverheadNs = 700.0;

std::uint8_t* g_mem = nullptr;
std::uint32_t g_allocHead = 0;
bool g_init = false;
bool g_enabled = true;
double g_timingScale = 1.0;
Stats g_stats{};

using Clock = std::chrono::steady_clock;

static std::uint32_t alignUp(std::uint32_t v, std::uint32_t a) {
    return (v + (a - 1)) & ~(a - 1);
}

static bool rangeOk(std::uint32_t addr, std::size_t len) {
    if (!g_init || !g_mem) return false;
    if (addr == kInvalidAddr) return false;
    if (len > kCapacityBytes) return false;
    if (addr > kCapacityBytes) return false;
    return len <= (kCapacityBytes - addr);
}

static double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

static double bwReadForSize(std::size_t len) {
    if (len <= 1)  return kRead1B;
    if (len <= 2)  return kRead2B;
    if (len <= 4)  return kRead4B;
    if (len >= 16) return kRead16B;

    const double t = double(len - 4) / double(16 - 4);
    return lerp(kRead4B, kRead16B, t);
}

static double bwWriteForSize(std::size_t len) {
    if (len <= 1)  return kWrite1B;
    if (len <= 2)  return kWrite2B;
    if (len <= 4)  return kWrite4B;
    if (len >= 16) return kWrite16B;

    const double t = double(len - 4) / double(16 - 4);
    return lerp(kWrite4B, kWrite16B, t);
}

static void applyDelayNs(std::uint64_t ns) {
    if (!g_enabled || g_timingScale <= 0.0 || ns == 0) {
        return;
    }

    const std::uint64_t scaled = std::uint64_t(double(ns) * g_timingScale);
    g_stats.totalDelayNs += scaled;

    if (scaled > 2000000ull) {
        const auto coarse = std::chrono::nanoseconds(scaled - 500000ull);
        std::this_thread::sleep_for(coarse);
    }

    const auto start = Clock::now();
    const auto target = start + std::chrono::nanoseconds(scaled);
    while (Clock::now() < target) {
    }
}

static std::uint64_t transferDelayNs(std::size_t len, double bwBytesPerSec, double callOverheadNs) {
    if (len == 0) {
        return 0;
    }
    const double transferNs = (double(len) / bwBytesPerSec) * 1.0e9;
    return std::uint64_t(callOverheadNs + transferNs + 0.5);
}

static bool doRead(std::uint32_t addr, void* dst, std::size_t len, double bwBytesPerSec) {
    if (!rangeOk(addr, len) || (!dst && len != 0)) {
        return false;
    }

    g_stats.readCalls++;
    g_stats.readBytes += len;
    if (len < 16) g_stats.tinyReadCalls++;

    applyDelayNs(transferDelayNs(len, bwBytesPerSec, kReadCallOverheadNs));
    std::memcpy(dst, g_mem + addr, len);
    return true;
}

static bool doWrite(std::uint32_t addr, const void* src, std::size_t len, double bwBytesPerSec) {
    if (!rangeOk(addr, len) || (!src && len != 0)) {
        return false;
    }

    g_stats.writeCalls++;
    g_stats.writeBytes += len;
    if (len < 16) g_stats.tinyWriteCalls++;

    applyDelayNs(transferDelayNs(len, bwBytesPerSec, kWriteCallOverheadNs));
    std::memcpy(g_mem + addr, src, len);
    return true;
}

} // namespace

bool init() {
    if (g_init) {
        return true;
    }

    void* mem = VirtualAlloc(nullptr, kCapacityBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!mem) {
        return false;
    }

    g_mem = static_cast<std::uint8_t*>(mem);
    std::memset(g_mem, 0, kCapacityBytes);

    g_allocHead = 0;
    g_stats = {};
    g_init = true;
    return true;
}

void shutdown() {
    if (g_mem) {
        VirtualFree(g_mem, 0, MEM_RELEASE);
        g_mem = nullptr;
    }

    g_allocHead = 0;
    g_init = false;
}

std::size_t capacity() {
    return kCapacityBytes;
}

std::uint32_t alloc(std::size_t size, std::size_t alignment) {
    if (!g_init || !g_mem || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return kInvalidAddr;
    }

    const std::uint32_t start = alignUp(g_allocHead, std::uint32_t(alignment));
    if (size > (kCapacityBytes - start)) {
        return kInvalidAddr;
    }

    g_allocHead = start + std::uint32_t(size);
    g_stats.allocCount++;
    g_stats.highWaterMark = max(g_stats.highWaterMark, g_allocHead);
    return start;
}

bool read(std::uint32_t addr, void* dst, std::size_t len) {
    return doRead(addr, dst, len, bwReadForSize(len));
}

bool write(std::uint32_t addr, const void* src, std::size_t len) {
    return doWrite(addr, src, len, bwWriteForSize(len));
}

std::uint8_t read8(std::uint32_t addr) {
    std::uint8_t v = 0;
    g_stats.read8Calls++;
    doRead(addr, &v, sizeof(v), kRead1B);
    return v;
}

std::uint16_t read16(std::uint32_t addr) {
    std::uint16_t v = 0;
    g_stats.read16Calls++;
    doRead(addr, &v, sizeof(v), kRead2B);
    return v;
}

std::uint32_t read32(std::uint32_t addr) {
    std::uint32_t v = 0;
    g_stats.read32Calls++;
    doRead(addr, &v, sizeof(v), kRead4B);
    return v;
}

bool write8(std::uint32_t addr, std::uint8_t value) {
    g_stats.write8Calls++;
    return doWrite(addr, &value, sizeof(value), kWrite1B);
}

bool write8_async(std::uint32_t addr, std::uint8_t value) {
    g_stats.write8AsyncCalls++;
    return doWrite(addr, &value, sizeof(value), kWrite1BAsync);
}

bool write16(std::uint32_t addr, std::uint16_t value) {
    g_stats.write16Calls++;
    return doWrite(addr, &value, sizeof(value), kWrite2B);
}

bool write32(std::uint32_t addr, std::uint32_t value) {
    g_stats.write32Calls++;
    return doWrite(addr, &value, sizeof(value), kWrite4B);
}

void setEnabled(bool enabled) {
    g_enabled = enabled;
}

bool enabled() {
    return g_enabled;
}

void setTimingScale(double scale) {
    g_timingScale = (scale < 0.0) ? 0.0 : scale;
}

double timingScale() {
    return g_timingScale;
}

void resetStats() {
    g_stats = {};
    g_stats.highWaterMark = g_allocHead;
}

const Stats& stats() {
    return g_stats;
}

void dumpStats() {
    std::printf(
        "PSRAM stats:\n"
        "  allocs           : %llu\n"
        "  high water mark  : %u / %u bytes\n"
        "  reads            : %llu calls, %llu bytes\n"
        "  writes           : %llu calls, %llu bytes\n"
        "  read8/16/32      : %llu / %llu / %llu\n"
        "  write8/a/16/32   : %llu / %llu / %llu / %llu\n"
        "  tiny r/w (<16B)  : %llu / %llu\n"
        "  simulated delay  : %.3f ms\n",
        (unsigned long long)g_stats.allocCount,
        g_stats.highWaterMark, kCapacityBytes,
        (unsigned long long)g_stats.readCalls,
        (unsigned long long)g_stats.readBytes,
        (unsigned long long)g_stats.writeCalls,
        (unsigned long long)g_stats.writeBytes,
        (unsigned long long)g_stats.read8Calls,
        (unsigned long long)g_stats.read16Calls,
        (unsigned long long)g_stats.read32Calls,
        (unsigned long long)g_stats.write8Calls,
        (unsigned long long)g_stats.write8AsyncCalls,
        (unsigned long long)g_stats.write16Calls,
        (unsigned long long)g_stats.write32Calls,
        (unsigned long long)g_stats.tinyReadCalls,
        (unsigned long long)g_stats.tinyWriteCalls,
        double(g_stats.totalDelayNs) / 1.0e6
    );
}
} // namespace ol::psram

#else

#include <cstdio>
#include <cstdint>

#include "rp2040-psram/psram_spi.h"

#include "hardware/pio.h"

namespace ol::psram {

namespace {

constexpr std::size_t kCapacity = 8u * 1024u * 1024u;
constexpr std::size_t kChunkSize = 16;
constexpr std::uint32_t kInvalidAddr = 0xFFFFFFFFu;

bool g_inited = false;
psram_spi_inst_t g_psram{};
std::uint32_t g_allocPtr = 0;

std::uint32_t alignUp(std::uint32_t value, std::uint32_t alignment) {
    const std::uint32_t mask = alignment - 1u;
    return (value + mask) & ~mask;
}

bool rangeOk(std::uint32_t addr, std::size_t len) {
    if (addr == kInvalidAddr) {
        return false;
    }
    if (len > kCapacity) {
        return false;
    }
    if (addr > kCapacity) {
        return false;
    }
    return len <= (kCapacity - addr);
}

} // namespace

bool init() {
    if (g_inited) {
        return true;
    }

    g_psram = psram_spi_init_clkdiv(pio1, -1, 1.0f, true);
    g_inited = true;
    g_allocPtr = 0;

    std::printf("PSRAM init OK, capacity=%u bytes\n", static_cast<unsigned>(kCapacity));
    std::fflush(stdout);
    return true;
}

void shutdown() {
    g_inited = false;
    g_allocPtr = 0;
}

std::size_t capacity() {
    return kCapacity;
}

std::uint32_t alloc(std::size_t size, std::size_t alignment) {
    if (!g_inited && !init()) {
        return kInvalidAddr;
    }
    if (alignment == 0 || (alignment & (alignment - 1u)) != 0u) {
        return kInvalidAddr;
    }
    if (size > kCapacity) {
        return kInvalidAddr;
    }

    const std::uint32_t start = alignUp(g_allocPtr, static_cast<std::uint32_t>(alignment));
    const std::uint32_t end   = start + static_cast<std::uint32_t>(size);

    if (end < start || end > kCapacity) {
        return kInvalidAddr;
    }

    g_allocPtr = end;
    return start;
}

bool read(std::uint32_t addr, void* dst, std::size_t len) {
    if (!g_inited && !init()) {
        return false;
    }
    if (!dst && len != 0) {
        return false;
    }
    if (!rangeOk(addr, len)) {
        return false;
    }

    auto* p = static_cast<std::uint8_t*>(dst);
    while (len > 0) {
        const std::size_t chunk = len > kChunkSize ? kChunkSize : len;
        psram_read(&g_psram, addr, p, chunk);
        addr += static_cast<std::uint32_t>(chunk);
        p    += chunk;
        len  -= chunk;
    }
    return true;
}

bool write(std::uint32_t addr, const void* src, std::size_t len) {
    if (!g_inited && !init()) {
        return false;
    }
    if (!src && len != 0) {
        return false;
    }
    if (!rangeOk(addr, len)) {
        return false;
    }

    const auto* p = static_cast<const std::uint8_t*>(src);
    while (len > 0) {
        const std::size_t chunk = len > kChunkSize ? kChunkSize : len;
        psram_write(&g_psram, addr, p, chunk);
        addr += static_cast<std::uint32_t>(chunk);
        p    += chunk;
        len  -= chunk;
    }
    return true;
}

std::uint8_t read8(std::uint32_t addr) {
    if (!g_inited && !init()) {
        return 0;
    }
    if (addr >= kCapacity) {
        return 0;
    }
    return psram_read8(&g_psram, addr);
}

std::uint16_t read16(std::uint32_t addr) {
    if (!g_inited && !init()) {
        return 0;
    }
    if (addr > kCapacity - sizeof(std::uint16_t)) {
        return 0;
    }
    return psram_read16(&g_psram, addr);
}

std::uint32_t read32(std::uint32_t addr) {
    if (!g_inited && !init()) {
        return 0;
    }
    if (addr > kCapacity - sizeof(std::uint32_t)) {
        return 0;
    }
    return psram_read32(&g_psram, addr);
}

bool write8(std::uint32_t addr, std::uint8_t value) {
    if (!g_inited && !init()) {
        return false;
    }
    if (addr >= kCapacity) {
        return false;
    }
    psram_write8(&g_psram, addr, value);
    return true;
}

bool write8_async(std::uint32_t addr, std::uint8_t value) {
    if (!g_inited && !init()) {
        return false;
    }
    if (addr >= kCapacity) {
        return false;
    }
    psram_write8_async(&g_psram, addr, value);
    return true;
}

bool write16(std::uint32_t addr, std::uint16_t value) {
    if (!g_inited && !init()) {
        return false;
    }
    if (addr > kCapacity - sizeof(std::uint16_t)) {
        return false;
    }
    psram_write16(&g_psram, addr, value);
    return true;
}

bool write32(std::uint32_t addr, std::uint32_t value) {
    if (!g_inited && !init()) {
        return false;
    }
    if (addr > kCapacity - sizeof(std::uint32_t)) {
        return false;
    }
    psram_write32(&g_psram, addr, value);
    return true;
}
} // namespace ol::psram

#endif // __PICOCALC_WIN__