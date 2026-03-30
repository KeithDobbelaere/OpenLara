#pragma once

#include "game/Game.hpp"
#include "pico/PicoPlatform.hpp"

#include <cstdint>

namespace ol {

class App {
public:
    int run(PicoPlatform *platform);

private:
    void init(PicoPlatform& platform);
    InputState pollInput() const;

private:
    PicoPlatform* plat_ = nullptr;
    Game game_{};
};

} // namespace ol