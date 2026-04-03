#include "game.h"

#include "pico/PicoCalcDisplay.hpp"
#include "pico/PicoInput.hpp"
#include "pico/Keys.hpp"
#include "pico/Psram.hpp"

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/mutex.h"

#include <cstdio>
#include <cstring>

// -------------------------------------------------------------------------------------------------
// Platform hooks expected by OpenLara
// -------------------------------------------------------------------------------------------------

int osGetTimeMS() {
    return static_cast<int>(to_ms_since_boot(get_absolute_time()));
}

bool osJoyReady(int index) {
    (void)index;
    return false;
}

void osJoyVibrate(int index, float L, float R) {
    (void)index;
    (void)L;
    (void)R;
}

namespace {

struct PicoRWLock {
    mutex_t mutex;
};

} // namespace

void* osMutexInit() {
    mutex_t* m = new mutex_t;
    mutex_init(m);
    return m;
}

void osMutexFree(void* obj) {
    delete static_cast<mutex_t*>(obj);
}

void osMutexLock(void* obj) {
    mutex_enter_blocking(static_cast<mutex_t*>(obj));
}

void osMutexUnlock(void* obj) {
    mutex_exit(static_cast<mutex_t*>(obj));
}

void* osRWLockInit() {
    PicoRWLock* lock = new PicoRWLock;
    mutex_init(&lock->mutex);
    return lock;
}

void osRWLockFree(void* obj) {
    delete static_cast<PicoRWLock*>(obj);
}

void osRWLockRead(void* obj) {
    auto* lock = static_cast<PicoRWLock*>(obj);
    mutex_enter_blocking(&lock->mutex);
}

void osRWUnlockRead(void* obj) {
    auto* lock = static_cast<PicoRWLock*>(obj);
    mutex_exit(&lock->mutex);
}

void osRWLockWrite(void* obj) {
    auto* lock = static_cast<PicoRWLock*>(obj);
    mutex_enter_blocking(&lock->mutex);
}

void osRWUnlockWrite(void* obj) {
    auto* lock = static_cast<PicoRWLock*>(obj);
    mutex_exit(&lock->mutex);
}

// -------------------------------------------------------------------------------------------------
// Pico host glue
// -------------------------------------------------------------------------------------------------

namespace {

ol::PicoCalcDisplay g_display;
ol::PicoKeyboardInput g_keyboard;

void setInputKey(InputKey key, bool down) {
    Input::setDown(key, down ? 1 : 0);
}

void pumpKeyboard() {
    g_keyboard.update();

    setInputKey(ikLeft,   g_keyboard.down(KEY_LEFT));
    setInputKey(ikRight,  g_keyboard.down(KEY_RIGHT));
    setInputKey(ikUp,     g_keyboard.down(KEY_UP));
    setInputKey(ikDown,   g_keyboard.down(KEY_DOWN));

    setInputKey(ikSpace,  g_keyboard.down(KEY_SPACE));
    setInputKey(ikTab,    g_keyboard.down(KEY_TAB));
    setInputKey(ikEnter,  g_keyboard.down(KEY_ENTER) || g_keyboard.down(KEY_RETURN));
    setInputKey(ikEscape, g_keyboard.down(KEY_ESC) || g_keyboard.down(KEY_POWER));

    setInputKey(ikShift,  g_keyboard.down(KEY_MOD_SHL));
    setInputKey(ikCtrl,   g_keyboard.down(KEY_MOD_CTRL));
    setInputKey(ikAlt,    g_keyboard.down(KEY_MOD_ALT));

    setInputKey(ik0, g_keyboard.down('0'));
    setInputKey(ik1, g_keyboard.down('1'));
    setInputKey(ik2, g_keyboard.down('2'));
    setInputKey(ik3, g_keyboard.down('3'));
    setInputKey(ik4, g_keyboard.down('4'));
    setInputKey(ik5, g_keyboard.down('5'));
    setInputKey(ik6, g_keyboard.down('6'));
    setInputKey(ik7, g_keyboard.down('7'));
    setInputKey(ik8, g_keyboard.down('8'));
    setInputKey(ik9, g_keyboard.down('9'));

    setInputKey(ikA, g_keyboard.down('A') || g_keyboard.down('a'));
    setInputKey(ikB, g_keyboard.down('B') || g_keyboard.down('b'));
    setInputKey(ikC, g_keyboard.down('C') || g_keyboard.down('c'));
    setInputKey(ikD, g_keyboard.down('D') || g_keyboard.down('d'));
    setInputKey(ikE, g_keyboard.down('E') || g_keyboard.down('e'));
    setInputKey(ikF, g_keyboard.down('F') || g_keyboard.down('f'));
    setInputKey(ikG, g_keyboard.down('G') || g_keyboard.down('g'));
    setInputKey(ikH, g_keyboard.down('H') || g_keyboard.down('h'));
    setInputKey(ikI, g_keyboard.down('I') || g_keyboard.down('i'));
    setInputKey(ikJ, g_keyboard.down('J') || g_keyboard.down('j'));
    setInputKey(ikK, g_keyboard.down('K') || g_keyboard.down('k'));
    setInputKey(ikL, g_keyboard.down('L') || g_keyboard.down('l'));
    setInputKey(ikM, g_keyboard.down('M') || g_keyboard.down('m'));
    setInputKey(ikN, g_keyboard.down('N') || g_keyboard.down('n'));
    setInputKey(ikO, g_keyboard.down('O') || g_keyboard.down('o'));
    setInputKey(ikP, g_keyboard.down('P') || g_keyboard.down('p'));
    setInputKey(ikQ, g_keyboard.down('Q') || g_keyboard.down('q'));
    setInputKey(ikR, g_keyboard.down('R') || g_keyboard.down('r'));
    setInputKey(ikS, g_keyboard.down('S') || g_keyboard.down('s'));
    setInputKey(ikT, g_keyboard.down('T') || g_keyboard.down('t'));
    setInputKey(ikU, g_keyboard.down('U') || g_keyboard.down('u'));
    setInputKey(ikV, g_keyboard.down('V') || g_keyboard.down('v'));
    setInputKey(ikW, g_keyboard.down('W') || g_keyboard.down('w'));
    setInputKey(ikX, g_keyboard.down('X') || g_keyboard.down('x'));
    setInputKey(ikY, g_keyboard.down('Y') || g_keyboard.down('y'));
    setInputKey(ikZ, g_keyboard.down('Z') || g_keyboard.down('z'));

    if (g_keyboard.pressed(KEY_POWER)) {
        Core::isQuit = true;
    }
}

void initPaths() {
    cacheDir[0] = '\0';
    saveDir[0] = '\0';
    contentDir[0] = '\0';

    std::strcpy(contentDir, "/openlara/");
    std::strcpy(saveDir,    "/openlara/");
    std::strcpy(cacheDir,   "/openlara/cache/");
}

void initHost() {
    stdio_init_all();
    sleep_ms(1000);

    std::printf("PicoCalc main start\n");
    std::fflush(stdout);

    initPaths();

    Core::width  = ol::PicoCalcDisplay::RenderW;
    Core::height = ol::PicoCalcDisplay::RenderH;

    g_keyboard.init();
    g_display.beginFrame();

    std::printf("Core size: %dx%d\n", Core::width, Core::height);
    std::printf("contentDir=\"%s\"\n", contentDir);
    std::printf("saveDir=\"%s\"\n", saveDir);
    std::printf("cacheDir=\"%s\"\n", cacheDir);
    std::fflush(stdout);
}

} // namespace

// -------------------------------------------------------------------------------------------------
// Entry
// -------------------------------------------------------------------------------------------------
#if 0
#include "rp2040-psram/psram_spi.h"
#include "hardware/clocks.h"

#include "hardware/gpio.h"
const uint LEDPIN = 25;
psram_spi_inst_t* async_spi_inst;

int psram_test(psram_spi_inst_t*psram_spi){
    uint32_t psram_begin, psram_elapsed;
    float psram_speed;
    char buf[128];
    sprintf(buf,"Testing PSRAM...\n");
    printf("%s",buf);

    // **************** 8 bits testing ****************
    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); ++addr) {
        psram_write8(psram_spi, addr, (addr & 0xFF));
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0 * 8 * 1024.0 * 1024 / psram_elapsed;
    sprintf(buf,"8 bit: PSRAM write 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); ++addr) {
        psram_write8_async(psram_spi, addr, (addr & 0xFF));
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0 * 8 * 1024.0 * 1024 / psram_elapsed;
    sprintf(buf,"8 bit: PSRAM write async 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); ++addr) {
        uint8_t result = psram_read8(psram_spi, addr);
        if ((uint8_t)(addr & 0xFF) != result) {
            sprintf(buf,"\nPSRAM failure at address %x (%x != %x)\n", addr, addr & 0xFF, result);
            printf("%s", buf);
            return 1;
        }
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0 * 8 * 1024.0 * 1024 / psram_elapsed;
    sprintf(buf,"8 bit: PSRAM read 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);

    // **************** 16 bits testing ****************
    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); addr += 2) {
        psram_write16(psram_spi, addr, (((addr + 1) & 0xFF) << 8) | (addr & 0xFF));
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0 * 8 * 1024.0 * 1024 / psram_elapsed;
    sprintf(buf,"16 bit: PSRAM write 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); addr += 2) {
        uint16_t result = psram_read16(psram_spi, addr);
        if ((uint16_t)(
                (((addr + 1) & 0xFF) << 8) |
                (addr & 0xFF)) != result
                ) {
            sprintf(buf,"PSRAM failure at address %x (%x != %x) ", addr, (
                    (((addr + 1) & 0xFF) << 8) |
                    (addr & 0xFF)), result
            );
            printf("%s", buf);
            return 1;
        }
    }
    psram_elapsed = (time_us_32() - psram_begin);
    psram_speed = 1000000.0 * 8 * 1024 * 1024 / psram_elapsed;
    sprintf(buf,"16 bit: PSRAM read 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);

    // **************** 32 bits testing ****************
    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); addr += 4) {
        psram_write32(
                psram_spi, addr,
                (uint32_t)(
                        (((addr + 3) & 0xFF) << 24) |
                        (((addr + 2) & 0xFF) << 16) |
                        (((addr + 1) & 0xFF) << 8)  |
                        (addr & 0XFF))
        );
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0 * 8 * 1024.0 * 1024 / psram_elapsed;
    sprintf(buf,"32 bit: PSRAM write 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); addr += 4) {
        uint32_t result = psram_read32(psram_spi, addr);
        if ((uint32_t)(
                (((addr + 3) & 0xFF) << 24) |
                (((addr + 2) & 0xFF) << 16) |
                (((addr + 1) & 0xFF) << 8)  |
                (addr & 0XFF)) != result
                ) {
            sprintf(buf,"PSRAM failure at address %x (%x != %x) ", addr, (
                    (((addr + 3) & 0xFF) << 24) |
                    (((addr + 2) & 0xFF) << 16) |
                    (((addr + 1) & 0xFF) << 8)  |
                    (addr & 0XFF)), result
            );
            printf("%s", buf);
            return 1;
        }
    }
    psram_elapsed = (time_us_32() - psram_begin);
    psram_speed = 1000000.0 * 8 * 1024 * 1024 / psram_elapsed;
    sprintf(buf,"32 bit: PSRAM read 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);
    // **************** n bits testing ****************
    uint8_t write_data[256];
    for (size_t i = 0; i < 256; ++i) {
        write_data[i] = i;
    }
    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); addr += 256) {
        for (uint32_t step = 0; step < 256; step += 16) {
            psram_write(psram_spi, addr + step, write_data + step, 16);
        }
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0 * 8 * 1024.0 * 1024 / psram_elapsed;
    sprintf(buf,"128 bit: PSRAM write 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);

    psram_begin = time_us_32();
    uint8_t read_data[16];
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); addr += 256) {
        for (uint32_t step = 0; step < 256; step += 16) {
            psram_read(psram_spi, addr + step, read_data, 16);
            if (memcmp(read_data, write_data + step, 16) != 0) {
                sprintf(buf,"PSRAM failure at address %x", addr);
                printf("%s", buf);
                return 1;
            }
        }
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0 * 8 * 1024.0 * 1024 / psram_elapsed;
    sprintf(buf,"128 bit: PSRAM read 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);
    printf("%s", buf);
    return 0;
}
int main() {
    set_sys_clock_khz(133'000, true);
    stdio_init_all();

    uart_init(uart0, 115200);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);  // 8-N-1
    uart_set_fifo_enabled(uart0, false);

    gpio_init(LEDPIN);
    gpio_set_dir(LEDPIN, GPIO_OUT);

        gpio_put(LEDPIN, 1);
    sleep_ms(500);
    gpio_put(LEDPIN, 0);

    psram_spi_inst_t psram_spi = psram_spi_init_clkdiv(pio1, -1,1.0f,true);
    sleep_ms(5000);
    if (psram_test(&psram_spi) == EXIT_SUCCESS) {
        printf("PSRAM test passed!\n");
    } else {
        printf("PSRAM test failed!\n");
    }

    //ol::psram::init();
    //ol::psram::selfTest();

    while(true)
    {
        sleep_ms(100);
    }
#else
int main() {
    //set_sys_clock_khz(133'000, true);
    stdio_init_all();

    uart_init(uart0, 115200);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);  // 8-N-1
    uart_set_fifo_enabled(uart0, false);

    initHost();

    Input::reset();

    std::printf("calling ::Game::init((const char*)nullptr)\n");
    std::fflush(stdout);
    Game::init((const char*)nullptr);
    std::printf("Game::init returned\n");
    std::fflush(stdout);

    while (!Core::isQuit) {
        pumpKeyboard();

        if (Game::update()) {
            g_display.beginFrame();
            Game::render();
            Core::waitVBlank();
            g_display.endFrame();
        }

        tight_loop_contents();
    }

    Game::deinit();
    return 0;
#endif
}