#pragma once

namespace ol {

struct InputState {
    // Continuous
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;

    // Pressed this frame
    bool upPressed = false;
    bool downPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;

    bool actionPressed = false;
    bool jumpPressed = false;
    bool drawPressed = false;

    bool confirm = false;
    bool back = false;
    bool pausePressed = false;

    char typedChar = '\0';
};

} // namespace ol