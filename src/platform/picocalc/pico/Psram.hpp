#pragma once

#include <cstddef>
#include <cstdint>

namespace ol::psram {

bool init();
bool selfTest();

bool read(uint32_t addr, void* dst, std::size_t len);
bool write(uint32_t addr, const void* src, std::size_t len);

std::size_t capacity();

// Simple linear allocator for large cold buffers.
// This is intentionally dumb and reset-oriented.
void resetAlloc();
uint32_t alloc(std::size_t size, std::size_t alignment = 16);

} // namespace ol::psram