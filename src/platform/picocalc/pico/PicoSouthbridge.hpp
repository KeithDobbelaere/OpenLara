#pragma once

#include <cstdint>

namespace ol {

class PicoSouthbridge {
public:
    void init();

    bool busIdle() const;

    uint8_t batteryRaw() const;
    uint8_t batteryPercent() const;
    bool batteryCharging() const;

    uint8_t lcdBacklightRaw() const;
    uint8_t setLcdBacklightRaw(uint8_t brightness);

    uint8_t keyboardBacklightRaw() const;
    uint8_t setKeyboardBacklightRaw(uint8_t brightness);

    bool supportsPowerOff() const;
    bool setPowerOffDelay(uint8_t delaySeconds);
    bool reset(uint8_t delaySeconds);
};

} // namespace ol