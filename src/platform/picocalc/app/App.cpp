#include "App.hpp"
#include "pico/Keys.hpp"

namespace ol {

namespace {

constexpr uint32_t kFrameUs = 33333; // ~30 FPS

} // anon

int App::run(PicoPlatform *platform) {
    plat_ = platform;
    init(*plat_);

    uint32_t accumUs = 0;

    while (true) {
        uint32_t dtUs = plat_->dtUs();
        if (dtUs > 250000) {
            dtUs = 250000;
        }

        accumUs += dtUs;
        if (accumUs < kFrameUs) {
            continue;
        }

        accumUs -= kFrameUs;

        plat_->input().update();
        InputState in = pollInput();

        game_.update(in, kFrameUs);

        auto& display = plat_->display();
        display.beginFrame();
        game_.render(display);
        display.endFrame();
    }

    return 0;
}

void App::init(PicoPlatform& platform) {
    plat_->init();
    (void)plat_->fs().init();

    game_.reset();
    game_.setFileSystem(&platform.fs());
}

InputState App::pollInput() const {
    const PicoKeyboardInput& kb = plat_->input();

    InputState in{};
    in.forward        = kb.down(KEY_UP);
    in.backward       = kb.down(KEY_DOWN);
    in.left           = kb.down(KEY_LEFT);
    in.right          = kb.down(KEY_RIGHT);

    in.actionPressed = kb.pressed(KEY_SPACE);
    in.jumpPressed   = kb.pressed(KEY_MOD_ALT);
    in.drawPressed   = kb.pressed(KEY_MOD_CTRL);

    in.upPressed    = kb.pressed(KEY_UP);
    in.downPressed  = kb.pressed(KEY_DOWN);
    in.leftPressed  = kb.pressed(KEY_LEFT);
    in.rightPressed = kb.pressed(KEY_RIGHT);

    in.confirm = kb.pressed(KEY_ENTER) || kb.pressed(KEY_RETURN);
    in.back = kb.pressed(KEY_ESC);
    in.pausePressed = kb.pressed(KEY_POWER);

    return in;
}

} // namespace ol