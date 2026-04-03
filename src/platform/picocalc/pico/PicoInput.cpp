#include "PicoInput.hpp"

extern "C" {
#include "drivers/southbridge.h"  // sb_init, sb_available, sb_read_keyboard, sb_read_keyboard_state
}

#define KEY_STATE_IDLE      (0)
#define KEY_STATE_PRESSED   (1)
#define KEY_STATE_HOLD      (2)
#define KEY_STATE_RELEASED  (3)

namespace ol {

void PicoKeyboardInput::clearEdgeBits() {
    for (auto& w : pressedBits_) {
        w = 0;
    }
    for (auto& w : releasedBits_) {
        w = 0;
    }
}

void PicoKeyboardInput::markPressed(uint8_t key) {
    pressedBits_[key >> 5] |= (1u << (key & 31));
}

void PicoKeyboardInput::markReleased(uint8_t key) {
    releasedBits_[key >> 5] |= (1u << (key & 31));
}

void PicoKeyboardInput::init() {
    sb_init();
    keyDown_.fill(0);
    clearEdgeBits();
}

void PicoKeyboardInput::update() {
    clearEdgeBits();

    // Drain FIFO (cap avoids pathological loops if something goes wrong)
    for (int i = 0; i < 64; ++i) {
        if (!sb_available()) {
            break;
        }

        const uint16_t ev   = sb_read_keyboard();
        const uint8_t st    = static_cast<uint8_t>(ev >> 8);
        const uint8_t code  = static_cast<uint8_t>(ev & 0xFF);

        if (st == 0) {
            break;
        }

        switch (st) {
            case KEY_STATE_PRESSED:
                keyDown_[code] = 1;
                markPressed(code);
                break;

            case KEY_STATE_HOLD:
                keyDown_[code] = 1;
                break;

            case KEY_STATE_RELEASED:
                keyDown_[code] = 0;
                markReleased(code);
                break;

            default:
                break;
        }
    }
}

} // namespace ol