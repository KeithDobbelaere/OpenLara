#include "Psram.hpp"

#include <cstdio>
#include <cstring>

#include "rp2040-psram/psram_spi.h"

#include "hardware/pio.h"

namespace ol::psram {

namespace {

constexpr std::size_t kCapacity = 8u * 1024u * 1024u;

bool g_inited = false;
psram_spi_inst_t g_psram{};
uint32_t g_allocPtr = 0;

uint32_t alignUp(uint32_t value, uint32_t alignment) {
    const uint32_t mask = alignment - 1u;
    return (value + mask) & ~mask;
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

std::size_t capacity() {
    return kCapacity;
}

bool write(uint32_t addr, const void* src, std::size_t len) {
    if (!g_inited && !init()) {
        return false;
    }
    if (!src) {
        return false;
    }
    if (addr > kCapacity || len > (kCapacity - addr)) {
        return false;
    }

    psram_write(&g_psram, addr, static_cast<const uint8_t*>(src), len);
    return true;
}

bool read(uint32_t addr, void* dst, std::size_t len) {
    if (!g_inited && !init()) {
        return false;
    }
    if (!dst) {
        return false;
    }
    if (addr > kCapacity || len > (kCapacity - addr)) {
        return false;
    }

    psram_read(&g_psram, addr, static_cast<uint8_t*>(dst), len);
    return true;
}

bool selfTest() {
    if (!init()) {
        return false;
    }

    constexpr uint32_t kTestAddr = 0;
    constexpr std::size_t kTestSize = 256;

    uint8_t writeBuf[kTestSize];
    uint8_t readBuf[kTestSize];

    for (std::size_t i = 0; i < kTestSize; ++i) {
        writeBuf[i] = static_cast<uint8_t>(i);
        readBuf[i] = 0;
    }

    if (!write(kTestAddr, writeBuf, kTestSize)) {
        std::printf("PSRAM self-test write failed\n");
        std::fflush(stdout);
        return false;
    }

    if (!read(kTestAddr, readBuf, kTestSize)) {
        std::printf("PSRAM self-test read failed\n");
        std::fflush(stdout);
        return false;
    }

    if (std::memcmp(writeBuf, readBuf, kTestSize) != 0) {
        std::printf("PSRAM self-test compare failed\n");
        std::fflush(stdout);
        return false;
    }

    std::printf("PSRAM self-test PASS\n");
    std::fflush(stdout);
    return true;
}

void resetAlloc() {
    g_allocPtr = 0;
}

uint32_t alloc(std::size_t size, std::size_t alignment) {
    if (!g_inited && !init()) {
        return 0xFFFFFFFFu;
    }
    if (alignment == 0 || (alignment & (alignment - 1u)) != 0u) {
        return 0xFFFFFFFFu;
    }

    const uint32_t start = alignUp(g_allocPtr, static_cast<uint32_t>(alignment));
    const uint32_t end = start + static_cast<uint32_t>(size);

    if (end < start || end > kCapacity) {
        return 0xFFFFFFFFu;
    }

    g_allocPtr = end;
    return start;
}

} // namespace ol::psram