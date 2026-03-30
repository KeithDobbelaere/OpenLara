#pragma once

#include <cstdint>

namespace ol::color {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return uint16_t(
        ((uint16_t(r >> 3) & 0x1F) << 11) |
        ((uint16_t(g >> 2) & 0x3F) << 5)  |
        ((uint16_t(b >> 3) & 0x1F))
    );
}

// ---- Grayscale ----
static constexpr uint16_t Black      = 0x0000;
static constexpr uint16_t White      = 0xFFFF;
static constexpr uint16_t Gray       = 0x8410;
static constexpr uint16_t DarkGray   = rgb565(64, 64, 64);
static constexpr uint16_t LightGray  = rgb565(192, 192, 192);
static constexpr uint16_t Silver     = rgb565(192, 192, 192);
static constexpr uint16_t Charcoal   = rgb565(32, 32, 32);

// ---- Primary / secondary ----
static constexpr uint16_t Red        = 0xF800;
static constexpr uint16_t Green      = 0x07E0;
static constexpr uint16_t Blue       = 0x001F;
static constexpr uint16_t Yellow     = 0xFFE0;
static constexpr uint16_t Cyan       = 0x07FF;
static constexpr uint16_t Magenta    = 0xF81F;

// ---- Common bright colors ----
static constexpr uint16_t Orange     = rgb565(255, 165, 0);
static constexpr uint16_t Lime       = rgb565(50, 205, 50);
static constexpr uint16_t Purple     = rgb565(128, 0, 128);
static constexpr uint16_t Pink       = rgb565(255, 105, 180);
static constexpr uint16_t Brown      = rgb565(139, 69, 19);
static constexpr uint16_t Gold       = rgb565(255, 215, 0);

// ---- Useful darker tones ----
static constexpr uint16_t DarkRed    = rgb565(128, 0, 0);
static constexpr uint16_t DarkGreen  = rgb565(0, 100, 0);
static constexpr uint16_t DarkBlue   = rgb565(0, 0, 139);
static constexpr uint16_t Olive      = rgb565(128, 128, 0);
static constexpr uint16_t Teal       = rgb565(0, 128, 128);
static constexpr uint16_t Navy       = rgb565(0, 0, 128);
static constexpr uint16_t Maroon     = rgb565(128, 0, 0);

// ---- Lighter UI-friendly tones ----
static constexpr uint16_t LightRed   = rgb565(255, 128, 128);
static constexpr uint16_t LightGreen = rgb565(144, 238, 144);
static constexpr uint16_t LightBlue  = rgb565(173, 216, 230);
static constexpr uint16_t SkyBlue    = rgb565(135, 206, 235);
static constexpr uint16_t Violet     = rgb565(238, 130, 238);
static constexpr uint16_t Beige      = rgb565(245, 245, 220);

// ---- Status / semantic UI colors ----
static constexpr uint16_t Success    = Green;
static constexpr uint16_t Warning    = Yellow;
static constexpr uint16_t Error      = Red;
static constexpr uint16_t Info       = Cyan;

// ---- Menu / HUD suggestions ----
static constexpr uint16_t Text       = White;
static constexpr uint16_t TextDim    = Gray;
static constexpr uint16_t Highlight  = Yellow;
static constexpr uint16_t Accent     = Cyan;
static constexpr uint16_t Background = Black;

} // namespace ol::color