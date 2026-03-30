#include "Game.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "game.h"
#pragma GCC diagnostic pop

#include <cstring>

namespace ol {

namespace {

void setKey(InputKey key, bool down) {
    Input::setDown(key, down);
}

} // namespace

void Game::reset()
{
    Input::reset();

    // First bring-up choice:
    // pass nullptr and let OpenLara load its title/default level from contentDir.
    ::Game::init((const char*)nullptr);

    inited_ = true;
}

void Game::update(const InputState& in, uint32_t dtUs)
{
    (void)dtUs;

    if (!inited_) {
        return;
    }

    // Clear the keys we drive explicitly each frame.
    setKey(ikUp,     false);
    setKey(ikDown,   false);
    setKey(ikLeft,   false);
    setKey(ikRight,  false);
    setKey(ikSpace,  false);
    setKey(ikCtrl,   false);
    setKey(ikAlt,    false);
    setKey(ikEnter,  false);
    setKey(ikEscape, false);

    // Feed a minimal keyboard mapping into stock OpenLara input.
    setKey(ikUp,     in.forward    || in.upPressed);
    setKey(ikDown,   in.backward   || in.downPressed);
    setKey(ikLeft,   in.left       || in.leftPressed);
    setKey(ikRight,  in.right      || in.rightPressed);
    setKey(ikSpace,  in.actionPressed);
    setKey(ikAlt,    in.jumpPressed);
    setKey(ikCtrl,   in.drawPressed);
    setKey(ikEnter,  in.confirm);
    setKey(ikEscape, in.back || in.pausePressed);

    ::Game::update();
}

void Game::render(PicoCalcDisplay& display)
{
    (void)display;

    if (!inited_) {
        return;
    }

    ::Game::render();
}

} // namespace ol