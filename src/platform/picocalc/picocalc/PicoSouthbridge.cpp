#if defined(__PICOCALC__)
#include "PicoSouthbridge.hpp"

extern "C" {
#include "drivers/southbridge.h"
}

namespace ol {

void PicoSouthbridge::init() {
    sb_init();
}

bool PicoSouthbridge::busIdle() const {
    return sb_available();
}

uint8_t PicoSouthbridge::batteryRaw() const {
    return sb_read_battery();
}

uint8_t PicoSouthbridge::batteryPercent() const {
    const uint8_t raw = sb_read_battery();
    const uint8_t percent = raw & 0x7F;
    return (percent <= 100) ? percent : 100;
}

bool PicoSouthbridge::batteryCharging() const {
    return (sb_read_battery() & 0x80u) != 0;
}

uint8_t PicoSouthbridge::lcdBacklightRaw() const {
    return sb_read_lcd_backlight();
}

uint8_t PicoSouthbridge::setLcdBacklightRaw(uint8_t brightness) {
    return sb_write_lcd_backlight(brightness);
}

uint8_t PicoSouthbridge::keyboardBacklightRaw() const {
    return sb_read_keyboard_backlight();
}

uint8_t PicoSouthbridge::setKeyboardBacklightRaw(uint8_t brightness) {
    return sb_write_keyboard_backlight(brightness);
}

bool PicoSouthbridge::supportsPowerOff() const {
    return sb_is_power_off_supported();
}

bool PicoSouthbridge::setPowerOffDelay(uint8_t delaySeconds) {
    return sb_write_power_off_delay(delaySeconds);
}

bool PicoSouthbridge::reset(uint8_t delaySeconds) {
    return sb_reset(delaySeconds);
}

} // namespace ol
#endif