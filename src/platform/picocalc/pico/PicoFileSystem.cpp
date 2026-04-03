#include "PicoFileSystem.hpp"
#include <sys/stat.h>
#include <cstdio>

extern "C" {
#include "sdcard.h"
#include "fat32.h"
}

namespace ol {

bool PicoFile::read(void* dst, size_t bytes, size_t& outRead) {
    outRead = 0;
    if (!f_) return false;

    outRead = std::fread(dst, 1, bytes, f_);
    return !std::ferror(f_);
}

bool PicoFile::write(const void* src, size_t bytes, size_t& outWritten) {
    outWritten = 0;
    if (!f_) return false;

    outWritten = std::fwrite(src, 1, bytes, f_);
    return (outWritten == bytes) && !std::ferror(f_);
}

bool PicoFile::seek(size_t absOffset) {
    if (!f_) return false;
    return std::fseek(f_, static_cast<long>(absOffset), SEEK_SET) == 0;
}

size_t PicoFile::tell() const {
    if (!f_) return 0;
    long p = std::ftell(f_);
    return (p < 0) ? 0u : static_cast<size_t>(p);
}

size_t PicoFile::size() const {
    if (!f_) {
        return 0;
    }

    const long oldPos = std::ftell(f_);
    if (oldPos < 0) {
        return 0;
    }

    if (std::fseek(f_, 0, SEEK_END) != 0) {
        return 0;
    }

    const long endPos = std::ftell(f_);

    // Restore original position best-effort.
    std::fseek(f_, oldPos, SEEK_SET);

    size_t result = (endPos >= 0) ? static_cast<size_t>(endPos) : 0;
    std::printf("file size: %ld\n", (long)result);
    return result;
}

void PicoFile::close() {
    if (f_) {
        std::fclose(f_);
        f_ = nullptr;
    }

    // This file wrapper is heap-allocated by PicoFileSystem::openRead/openWrite().
    delete this;
}

bool PicoFileSystem::init() {
    if (inited_) return true;

    sd_init();
    if (sd_card_init() != SD_OK) return false;

    fat32_init();
    if (fat32_mount() != FAT32_OK) return false;

    inited_ = true;
    return true;
}

PicoFile* PicoFileSystem::openRead(const char* path) {
    if (!inited_ || !path || !*path) return nullptr;

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::printf("openRead FAILED: \"%s\"\n", path ? path : "(null)");
        return nullptr;
    }

    std::printf("openRead: \"%s\"\n", path ? path : "(null)");

    return new PicoFile(f);
}

PicoFile* PicoFileSystem::openWrite(const char* path, bool truncate) {
    if (!inited_ || !path || !*path) return nullptr;

    FILE* f = nullptr;

    if (truncate) {
        f = std::fopen(path, "w+b");
    } else {
        f = std::fopen(path, "r+b");
        if (!f) {
            f = std::fopen(path, "w+b");
        }
    }

    if (!f) return nullptr;

    return new PicoFile(f);
}

bool PicoFileSystem::exists(const char* path) const {
    if (!inited_ || !path || !*path) return false;

    struct stat st{};
    return ::stat(path, &st) == 0;
}

} // namespace ol