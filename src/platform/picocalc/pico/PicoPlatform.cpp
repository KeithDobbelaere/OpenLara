#include "PicoPlatform.hpp"
#include "PicoSouthbridge.hpp"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include <cstring>

extern char cacheDir[255];
extern char saveDir[255];
extern char contentDir[255];

// ------------ Platform hooks -------------

bool osJoyReady(int index) {
    (void)index;
    return false;
}

void osJoyVibrate(int index, float L, float R) {
    (void)index;
    (void)L;
    (void)R;
}

int osGetTimeMS() {
    return to_ms_since_boot(get_absolute_time());
}

namespace ol {

void PicoPlatform::init() {
    set_sys_clock_khz(150'000, true); // 150 MHz
    stdio_init_all();
    sleep_ms(500);
    last_ = time_us_64();

    std::strcpy(contentDir, "/openlara/");
    std::strcpy(saveDir,    "/openlara/");
    std::strcpy(cacheDir,   "/openlara/cache/");

    sb_.init();
    kb_.init();

    refreshBatteryCache(true);
    PicoCalcDisplay::setSerialOutputEnabled(serialOutputEnabled_);
}

uint64_t PicoPlatform::nowUs() const {
    return time_us_64();
}

uint32_t PicoPlatform::dtUs() {
    uint64_t now = time_us_64();
    uint64_t us = now - last_;
    last_ = now;
    return (uint32_t)us;
}

uint8_t PicoPlatform::batteryLevelPercent() const
{
    refreshBatteryCache(false);
    const uint8_t percent = batteryRaw_ & 0x7F;
    return (percent <= 100) ? percent : 100;
}

bool PicoPlatform::batteryCharging() const {
    refreshBatteryCache(false);
    return (batteryRaw_ & 0x80u) != 0;
}

void PicoPlatform::refreshBatteryCache(bool force) const {
    static constexpr uint64_t kBatteryCacheUs = 2000000; // 2 seconds

    const uint64_t now = time_us_64();
    if (!force && batteryCacheValid_ && (now - batteryCacheUs_) < kBatteryCacheUs) {
        return;
    }

    batteryRaw_ = sb_.batteryRaw();
    batteryCacheUs_ = now;
    batteryCacheValid_ = true;
}

} // namespace ol