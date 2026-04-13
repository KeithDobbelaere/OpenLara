#pragma once

#include <cstddef>
#include <cstdint>

namespace ol::psram {

bool init();
void shutdown();

std::size_t capacity();

// Returns 0xFFFFFFFF on failure.
std::uint32_t alloc(std::size_t size, std::size_t alignment = 16);

bool read(std::uint32_t addr, void* dst, std::size_t len);
bool write(std::uint32_t addr, const void* src, std::size_t len);

// Scalar helpers
std::uint8_t  read8(std::uint32_t addr);
std::uint16_t read16(std::uint32_t addr);
std::uint32_t read32(std::uint32_t addr);

bool write8(std::uint32_t addr, std::uint8_t value);
bool write8_async(std::uint32_t addr, std::uint8_t value);
bool write16(std::uint32_t addr, std::uint16_t value);
bool write32(std::uint32_t addr, std::uint32_t value);

#if defined(__PICOCALC_WIN__)
// Debug / simulation controls
void setEnabled(bool enabled);
bool enabled();

void setTimingScale(double scale);     // 1.0 = measured speed, 0.0 = no delay
double timingScale();

void resetStats();
void dumpStats();

struct Stats {
    std::uint64_t allocCount = 0;
    std::uint64_t readCalls = 0;
    std::uint64_t writeCalls = 0;
    std::uint64_t readBytes = 0;
    std::uint64_t writeBytes = 0;

    std::uint64_t read8Calls = 0;
    std::uint64_t read16Calls = 0;
    std::uint64_t read32Calls = 0;
    std::uint64_t write8Calls = 0;
    std::uint64_t write8AsyncCalls = 0;
    std::uint64_t write16Calls = 0;
    std::uint64_t write32Calls = 0;

    std::uint64_t tinyReadCalls = 0;   // <16 bytes
    std::uint64_t tinyWriteCalls = 0;  // <16 bytes

    std::uint64_t totalDelayNs = 0;
    std::uint32_t highWaterMark = 0;
};

const Stats& stats();
#endif

} // namespace ol::psram