#pragma once

#include <cstdio>

namespace ol {

class PicoFile {
public:
    explicit PicoFile(FILE* f) : f_(f) {}

    bool valid() const { return f_ != nullptr; }

    bool read(void* dst, size_t bytes, size_t& outRead);
    bool write(const void* src, size_t bytes, size_t& outWritten);
    bool seek(size_t absOffset);
    size_t tell() const;
    size_t size() const;

    // close() closes the FILE and deletes this wrapper.
    void close();

private:
    FILE* f_ = nullptr;
};

class PicoFileSystem {
public:
    bool init();
    PicoFile* openRead(const char* path);
    PicoFile* openWrite(const char* path, bool truncate);
    bool exists(const char* path) const;

private:
    bool inited_ = false;
};

} // namespace ol