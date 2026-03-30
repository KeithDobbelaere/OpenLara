#pragma once

#include "PicoCalcDisplay.hpp"
#include "PicoFileSystem.hpp"
#include "PicoInput.hpp"
#include "PicoSouthbridge.hpp"
#include <cstddef>

namespace ol {

class PicoPlatform {
public:
    void init();

    uint64_t nowUs() const;

    uint32_t dtUs();

    uint8_t batteryLevelPercent() const;

    bool batteryCharging() const;

    bool serialOutputEnabled() const {
        return serialOutputEnabled_;
    }

    void setSerialOutputEnabled(bool enabled) {
        serialOutputEnabled_ = enabled;
        PicoCalcDisplay::setSerialOutputEnabled(enabled);
    }

    PicoCalcDisplay& display() { return disp_; }
    PicoFileSystem& fs() { return fs_; }
    PicoKeyboardInput& input() { return kb_; }

private:
    void refreshBatteryCache(bool force) const;

private:
    PicoCalcDisplay disp_{};
    PicoFileSystem fs_{};
    PicoKeyboardInput kb_{};
    PicoSouthbridge sb_{};

    mutable uint8_t batteryRaw_ = 0;
    mutable uint64_t batteryCacheUs_ = 0;
    mutable bool batteryCacheValid_ = false;

    bool serialOutputEnabled_ = false;
    bool collisionHighlightEnabled_ = false;

    const char* cacheDir_ = nullptr;

    uint64_t last_{};
};

} // namespace ol