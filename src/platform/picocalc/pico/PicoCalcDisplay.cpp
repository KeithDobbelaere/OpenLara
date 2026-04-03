#include "PicoCalcDisplay.hpp"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include <cstring>
#include <cstdio>

namespace GAPI {
    const uint32_t* getPresentBuffer();
    int getPresentWidth();
    int getPresentHeight();
}

namespace ol {

static uint g_baud = 0;
static int  g_dma_tx = -1;
static dma_channel_config g_dma_cfg;

bool PicoCalcDisplay::s_serialOutputEnabled = false;
uint16_t PicoCalcDisplay::s_present565[PicoCalcDisplay::RenderW];

PicoCalcDisplay::PicoCalcDisplay() {}
PicoCalcDisplay::~PicoCalcDisplay() {}

static inline void start_dma_pixels(const void* src, int pixelWords) {
    dma_channel_configure(
        g_dma_tx,
        &g_dma_cfg,
        &spi_get_hw(spi1)->dr,
        src,
        pixelWords,
        true
    );
}

static inline void wait_for_spi_dma_idle() {
    dma_channel_wait_for_finish_blocking(g_dma_tx);
    while (spi_get_hw(spi1)->sr & SPI_SSPSR_BSY_BITS) {
        tight_loop_contents();
    }
}

bool PicoCalcDisplay::serialOutputEnabled() {
    return s_serialOutputEnabled;
}

void PicoCalcDisplay::setSerialOutputEnabled(bool enabled) {
    s_serialOutputEnabled = enabled;
}

inline uint16_t PicoCalcDisplay::color32To565(uint32_t c) {
    const uint32_t r = (c >> 16) & 0xFF;
    const uint32_t g = (c >> 8)  & 0xFF;
    const uint32_t b = (c >> 0)  & 0xFF;
    return uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void PicoCalcDisplay::beginFrame() {
    initIfNeeded();
}

void PicoCalcDisplay::drawBitmap565(int x, int y, int w, int h, const uint16_t* pixels) {
    initIfNeeded();

    if (!pixels || w <= 0 || h <= 0) return;

    const int dstX0 = x;
    const int dstY0 = y;
    const int dstX1 = x + w - 1;
    const int dstY1 = y + h - 1;

    if (dstX1 < 0 || dstX0 >= W || dstY1 < 0 || dstY0 >= H) {
        return;
    }

    const int clipX0 = (dstX0 < 0) ? 0 : dstX0;
    const int clipY0 = (dstY0 < 0) ? 0 : dstY0;
    const int clipX1 = (dstX1 >= W) ? (W - 1) : dstX1;
    const int clipY1 = (dstY1 >= H) ? (H - 1) : dstY1;

    const int clipW = clipX1 - clipX0 + 1;
    const int clipH = clipY1 - clipY0 + 1;

    const int srcX0 = clipX0 - dstX0;
    const int srcY0 = clipY0 - dstY0;

    setAddrWindow(clipX0, clipY0, clipX1, clipY1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    if (srcX0 == 0 && clipW == w) {
        const uint16_t* src = pixels + srcY0 * w;
        start_dma_pixels(src, clipW * clipH);
        wait_for_spi_dma_idle();
    } else {
        for (int row = 0; row < clipH; ++row) {
            const uint16_t* src = pixels + (srcY0 + row) * w + srcX0;
            start_dma_pixels(src, clipW);
            wait_for_spi_dma_idle();
        }
    }

    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(PIN_CS, 1);
}

void PicoCalcDisplay::endFrame() {
    if (!inited_)
        return;

    presentPicoCalcFrame();
}

void PicoCalcDisplay::presentPicoCalcFrame() {
    const uint32_t* src = (const uint32_t*)GAPI::getPresentBuffer();
    const int srcW = GAPI::getPresentWidth();
    const int srcH = GAPI::getPresentHeight();

    if (s_serialOutputEnabled && src && srcW > 0 && srcH > 0) {
        static uint64_t lastDumpUs = 0;
        const uint64_t now = time_us_64();

        if (now - lastDumpUs >= 1000000) {
            lastDumpUs = now;

            const uint32_t p0 = src[0];
            const uint32_t p1 = src[(srcW > 1) ? 1 : 0];
            const uint32_t pm = src[(srcW * srcH) / 2];
            const uint32_t pl = src[srcW * srcH - 1];

            std::printf("GAPI buf %dx%d p0=%08lx p1=%08lx pm=%08lx pl=%08lx\n",
                        srcW, srcH,
                        (unsigned long)p0,
                        (unsigned long)p1,
                        (unsigned long)pm,
                        (unsigned long)pl);
        }
    }

    if (!src || srcW != RenderW || srcH != RenderH)
        return;

    lcdFillRectBlack(0, 0, W, PresentY);
    lcdFillRectBlack(0, PresentY + PresentH, W, H - (PresentY + PresentH));

    for (int y = 0; y < RenderH; ++y) {
        const uint32_t* srcRow = src + y * RenderW;
        for (int x = 0; x < RenderW; ++x) {
            s_present565[x] = color32To565(srcRow[x]);
        }

        drawBitmap565(PresentX, PresentY + y, RenderW, 1, s_present565);
    }
}

void PicoCalcDisplay::initIfNeeded() {
    if (inited_)
        return;

    g_baud = spi_init(spi1, SPI_BAUD_HZ);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS, GPIO_OUT);  gpio_put(PIN_CS, 1);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC, GPIO_OUT);  gpio_put(PIN_DC, 1);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT); gpio_put(PIN_RST, 1);

    lcdReset();
    lcdInit();

    g_dma_tx = dma_claim_unused_channel(true);
    g_dma_cfg = dma_channel_get_default_config(g_dma_tx);
    channel_config_set_transfer_data_size(&g_dma_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&g_dma_cfg, true);
    channel_config_set_write_increment(&g_dma_cfg, false);
    channel_config_set_dreq(&g_dma_cfg, spi_get_dreq(spi1, true));

    lcdFillBlack();

    if (s_serialOutputEnabled) {
    std::printf("PicoCalcDisplay init SPI:%u Render:%dx%d Present:%d,%d %dx%d\n",
                    g_baud,
                    RenderW, RenderH,
                    PresentX, PresentY, PresentW, PresentH);
    }

    inited_ = true;
}

void PicoCalcDisplay::lcdReset() {
    gpio_put(PIN_RST, 0);
    sleep_ms(20);
    gpio_put(PIN_RST, 1);
    sleep_ms(120);
}

void PicoCalcDisplay::writeCmd(uint8_t cmd) {
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(spi1, &cmd, 1);
    gpio_put(PIN_CS, 1);
    gpio_put(PIN_DC, 1);
}

void PicoCalcDisplay::writeData(const uint8_t* data, size_t n) {
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(spi1, data, (int)n);
    gpio_put(PIN_CS, 1);
}

void PicoCalcDisplay::writeDataByte(uint8_t b) {
    writeData(&b, 1);
}

void PicoCalcDisplay::lcdInit() {
    writeCmd(0x01);
    sleep_ms(150);

    writeCmd(0x11);
    sleep_ms(120);

    writeCmd(0x21);

    writeCmd(0x3A);
    writeDataByte(0x55);

    writeCmd(0x36);
    writeDataByte(0x48);

    writeCmd(0x29);
}

void PicoCalcDisplay::setAddrWindow(int x0, int y0, int x1, int y1) {
    writeCmd(0x2A);
    const uint8_t col[4] = {
        uint8_t(x0 >> 8), uint8_t(x0 & 0xFF),
        uint8_t(x1 >> 8), uint8_t(x1 & 0xFF)
    };
    writeData(col, 4);

    writeCmd(0x2B);
    const uint8_t row[4] = {
        uint8_t(y0 >> 8), uint8_t(y0 & 0xFF),
        uint8_t(y1 >> 8), uint8_t(y1 & 0xFF)
    };
    writeData(row, 4);

    writeCmd(0x2C);
}

void PicoCalcDisplay::lcdFillRectBlack(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0)
        return;

    const int x0 = x;
    const int y0 = y;
    const int x1 = x + w - 1;
    const int y1 = y + h - 1;

    if (x1 < 0 || x0 >= W || y1 < 0 || y0 >= H)
        return;

    const int clipX0 = (x0 < 0) ? 0 : x0;
    const int clipY0 = (y0 < 0) ? 0 : y0;
    const int clipX1 = (x1 >= W) ? (W - 1) : x1;
    const int clipY1 = (y1 >= H) ? (H - 1) : y1;

    const int clipW = clipX1 - clipX0 + 1;
    const int clipH = clipY1 - clipY0 + 1;

    if (clipW <= 0 || clipH <= 0)
        return;

    static uint16_t blackRow[W];
    std::memset(blackRow, 0, sizeof(blackRow));

    setAddrWindow(clipX0, clipY0, clipX1, clipY1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    for (int row = 0; row < clipH; ++row) {
        start_dma_pixels(blackRow, clipW);
        wait_for_spi_dma_idle();
    }

    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(PIN_CS, 1);
}

void PicoCalcDisplay::lcdFillBlack() {
    lcdFillRectBlack(0, 0, W, H);
}

} // namespace ol