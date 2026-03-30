#include "app/App.hpp"
#include "pico/PicoPlatform.hpp"

static ol::PicoPlatform platform;
static ol::App app;

int main() {
    return app.run(&platform);
}