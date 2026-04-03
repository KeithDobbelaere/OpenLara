#pragma once

#include <array>
#include <cstdint>

namespace ol {

class PicoKeyboardInput {
public:
    void init();
    void update();

    bool down(uint8_t key) const { return keyDown_[key] != 0; }
    bool pressed(uint8_t key) const {
        return (pressedBits_[key >> 5] & (1u << (key & 31))) != 0;
    }
    bool released(uint8_t key) const {
        return (releasedBits_[key >> 5] & (1u << (key & 31))) != 0;
    }

private:
    void clearEdgeBits();
    void markPressed(uint8_t key);
    void markReleased(uint8_t key);

    std::array<uint8_t, 256> keyDown_{};       // 1 = currently down
    std::array<uint32_t, 8> pressedBits_{};    // pressed edge this frame
    std::array<uint32_t, 8> releasedBits_{};   // released edge this frame
};

} // namespace ol