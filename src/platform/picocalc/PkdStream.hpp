#pragma once
#pragma once
#include "common.h"

namespace ol {

    class PicoFile;

    class PkdStream
    {
    public:
        explicit PkdStream(FILE* file) : m_file(file), m_pos(0) {}

        bool seek(uint32 pos);
        bool read(void* dst, uint32 size);

        template <typename T>
        bool readT(T& value) {
            return read(&value, sizeof(T));
        }

        uint32 pos() const { return m_pos; }

    private:
        FILE* m_file;
        uint32 m_pos;
    };

} // namespace ol