#include "PkdStream.hpp"

namespace ol {

    bool PkdStream::seek(uint32 pos)
    {
        if (!m_file)
            return false;

        if (fseek(m_file, pos, SEEK_SET) != 0)
            return false;

        m_pos = pos;
        return true;
    }

    bool PkdStream::read(void* dst, uint32 size)
    {
        if (!m_file)
            return false;

        size_t got = fread(dst, 1, size, m_file);
        if (got != size)
            return false;

        m_pos += size;
        return true;
    }

} // namespace ol