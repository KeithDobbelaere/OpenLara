#pragma once

#include <cstdint>
#include <cstddef>

namespace ol {

class PicoCalcDisplay {
public:
    PicoCalcDisplay();
    ~PicoCalcDisplay();

    int width() const { return W; }
    int height() const { return H; }

    void beginFrame();
    void drawBitmap565(int x, int y, int w, int h, const uint16_t* pixels);
    void endFrame();

    static bool serialOutputEnabled();
    static void setSerialOutputEnabled(bool enabled);

    static constexpr int W = 320;
    static constexpr int H = 320;

    static constexpr int RenderW = 320;
    static constexpr int RenderH = RenderW * 9 / 16;

    static constexpr int PresentX = 0;
    static constexpr int PresentY = (H - RenderH) / 2;
    static constexpr int PresentW = RenderW;
    static constexpr int PresentH = RenderH;

private:
    static constexpr unsigned SPI_BAUD_HZ = 75'000'000;

    static constexpr int PIN_SCK  = 10;
    static constexpr int PIN_MOSI = 11;
    static constexpr int PIN_CS   = 13;
    static constexpr int PIN_DC   = 14;
    static constexpr int PIN_RST  = 15;

    bool inited_ = false;

    static bool s_serialOutputEnabled;

    static uint16_t s_present565[RenderW * RenderH];

private:
    void initIfNeeded();
    void lcdReset();
    void lcdInit();
    void lcdFillRectBlack(int x, int y, int w, int h);
    void lcdFillBlack();
    void setAddrWindow(int x0, int y0, int x1, int y1);
    void writeCmd(uint8_t cmd);
    void writeData(const uint8_t* data, size_t n);
    void writeDataByte(uint8_t b);

    void presentPicoCalcFrame();

    static inline uint16_t color32To565(uint32_t c);
};

} // namespace ol