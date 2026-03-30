#pragma once

#include "InputState.hpp"
#include "pico/PicoCalcDisplay.hpp"
#include "pico/PicoFileSystem.hpp"

namespace ol {

class Game {
public:
    void reset();
    void setFileSystem(ol::PicoFileSystem* fs) { fs_ = fs; }
    void update(const ol::InputState& in, uint32_t dtUs);
    void render(ol::PicoCalcDisplay& disp);

private:
    PicoFileSystem* fs_ = nullptr;
    bool inited_ = false;
};

} // namespace ol