#if defined(__PICOCALC__)
#include "PicoFileSystem.hpp"
#include <sys/stat.h>
#include <cstdio>

extern "C" {
#include "sdcard.h"
#include "fat32.h"
}

namespace ol::file_system {

static bool inited_ = false;

bool init() {
    if (inited_) return true;

    sd_init();
    if (sd_card_init() != SD_OK) return false;

    fat32_init();
    if (fat32_mount() != FAT32_OK) return false;

    inited_ = true;
    return true;
}

} // namespace ol::file_system
#endif