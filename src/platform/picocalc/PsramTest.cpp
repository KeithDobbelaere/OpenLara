#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/mutex.h"

#include "rp2040-psram/psram_spi.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>

const uint LEDPIN = 25;
psram_spi_inst_t* async_spi_inst = nullptr;

static constexpr uint32_t kPsramCapacity = 8u * 1024u * 1024u;
static constexpr size_t   kRawBulkChunk  = 16;

static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

static bool compare_buffers(const uint8_t* a, const uint8_t* b, size_t len) {
    return std::memcmp(a, b, len) == 0;
}

static bool raw_psram_write_chunked(psram_spi_inst_t* psram_spi, uint32_t addr, const uint8_t* src, size_t count) {
    if (!src) {
        return false;
    }
    if (addr > kPsramCapacity || count > (kPsramCapacity - addr)) {
        return false;
    }

    while (count > 0) {
        size_t chunk = count > kRawBulkChunk ? kRawBulkChunk : count;
        psram_write(psram_spi, addr, src, chunk);
        addr  += (uint32_t)chunk;
        src   += chunk;
        count -= chunk;
    }
    return true;
}

static bool raw_psram_read_chunked(psram_spi_inst_t* psram_spi, uint32_t addr, uint8_t* dst, size_t count) {
    if (!dst) {
        return false;
    }
    if (addr > kPsramCapacity || count > (kPsramCapacity - addr)) {
        return false;
    }

    while (count > 0) {
        size_t chunk = count > kRawBulkChunk ? kRawBulkChunk : count;
        psram_read(psram_spi, addr, dst, chunk);
        addr  += (uint32_t)chunk;
        dst   += chunk;
        count -= chunk;
    }
    return true;
}

static void fill_psram_region_raw(psram_spi_inst_t* psram_spi, uint32_t addr, size_t len, uint8_t value) {
    uint8_t buf[64];
    std::memset(buf, value, sizeof(buf));

    while (len > 0) {
        size_t n = len > sizeof(buf) ? sizeof(buf) : len;
        raw_psram_write_chunked(psram_spi, addr, buf, n);
        addr += (uint32_t)n;
        len  -= n;
    }
}

int psram_test(psram_spi_inst_t* psram_spi) {
    uint32_t psram_begin, psram_elapsed;
    float psram_speed;
    char buf[128];

    std::sprintf(buf, "Testing PSRAM...\n");
    std::printf("%s", buf);

    // **************** 8 bits testing ****************
    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < kPsramCapacity; ++addr) {
        psram_write8(psram_spi, addr, (uint8_t)(addr & 0xFF));
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "8 bit: PSRAM write 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < kPsramCapacity; ++addr) {
        psram_write8_async(psram_spi, addr, (uint8_t)(addr & 0xFF));
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "8 bit: PSRAM write async 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < kPsramCapacity; ++addr) {
        uint8_t result = psram_read8(psram_spi, addr);
        if ((uint8_t)(addr & 0xFF) != result) {
            std::sprintf(buf, "\nPSRAM failure at address %x (%x != %x)\n", addr, addr & 0xFF, result);
            std::printf("%s", buf);
            return 1;
        }
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "8 bit: PSRAM read 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    // **************** 16 bits testing ****************
    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < kPsramCapacity; addr += 2) {
        psram_write16(psram_spi, addr, (uint16_t)((((addr + 1) & 0xFF) << 8) | (addr & 0xFF)));
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "16 bit: PSRAM write 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < kPsramCapacity; addr += 2) {
        uint16_t result = psram_read16(psram_spi, addr);
        uint16_t expected = (uint16_t)((((addr + 1) & 0xFF) << 8) | (addr & 0xFF));
        if (expected != result) {
            std::sprintf(buf, "PSRAM failure at address %x (%x != %x)\n", addr, expected, result);
            std::printf("%s", buf);
            return 1;
        }
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "16 bit: PSRAM read 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    // **************** 32 bits testing ****************
    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < kPsramCapacity; addr += 4) {
        psram_write32(
            psram_spi,
            addr,
            (uint32_t)(
                (((addr + 3) & 0xFF) << 24) |
                (((addr + 2) & 0xFF) << 16) |
                (((addr + 1) & 0xFF) << 8)  |
                (addr & 0xFF))
        );
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "32 bit: PSRAM write 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < kPsramCapacity; addr += 4) {
        uint32_t result = psram_read32(psram_spi, addr);
        uint32_t expected = (uint32_t)(
            (((addr + 3) & 0xFF) << 24) |
            (((addr + 2) & 0xFF) << 16) |
            (((addr + 1) & 0xFF) << 8)  |
            (addr & 0xFF)
        );
        if (expected != result) {
            std::sprintf(buf, "PSRAM failure at address %x (%x != %x)\n", addr, expected, result);
            std::printf("%s", buf);
            return 1;
        }
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "32 bit: PSRAM read 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    // **************** raw bulk (16-byte slices) ****************
    uint8_t write_data[256];
    for (size_t i = 0; i < sizeof(write_data); ++i) {
        write_data[i] = (uint8_t)i;
    }

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < kPsramCapacity; addr += 256) {
        for (uint32_t step = 0; step < 256; step += 16) {
            psram_write(psram_spi, addr + step, write_data + step, 16);
        }
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "128 bit: PSRAM write 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    psram_begin = time_us_32();
    uint8_t read_data[16];
    for (uint32_t addr = 0; addr < kPsramCapacity; addr += 256) {
        for (uint32_t step = 0; step < 256; step += 16) {
            psram_read(psram_spi, addr + step, read_data, 16);
            if (std::memcmp(read_data, write_data + step, 16) != 0) {
                std::sprintf(buf, "PSRAM failure at address %x\n", addr);
                std::printf("%s", buf);
                return 1;
            }
        }
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0f * (float)kPsramCapacity / (float)psram_elapsed;
    std::sprintf(buf, "128 bit: PSRAM read 8MB in %u us, %u B/s\n", psram_elapsed, (uint32_t)psram_speed);
    std::printf("%s", buf);

    return 0;
}

static int sd_to_psram_stream_test_raw(psram_spi_inst_t* psram_spi,
                                       const char* path,
                                       size_t chunk_size,
                                       uint32_t psram_base) {
    char buf[192];
    std::snprintf(buf, sizeof(buf), "RAW SD->PSRAM stream test: file=%s chunk=%u base=%u\n",
                  path, (unsigned)chunk_size, psram_base);
    std::printf("%s", buf);

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::printf("Failed to open file: %s\n", path);
        return 1;
    }

    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::printf("fseek end failed\n");
        std::fclose(f);
        return 1;
    }

    long file_size_long = std::ftell(f);
    if (file_size_long <= 0) {
        std::printf("Invalid file size\n");
        std::fclose(f);
        return 1;
    }

    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::printf("fseek set failed\n");
        std::fclose(f);
        return 1;
    }

    const uint32_t file_size = (uint32_t)file_size_long;

    uint8_t* read_buf   = (uint8_t*)std::malloc(chunk_size);
    uint8_t* verify_buf = (uint8_t*)std::malloc(chunk_size);

    if (!read_buf || !verify_buf) {
        std::printf("malloc failed\n");
        std::free(read_buf);
        std::free(verify_buf);
        std::fclose(f);
        return 1;
    }

    if (psram_base > kPsramCapacity || file_size > (kPsramCapacity - psram_base)) {
        std::printf("Test file too large for PSRAM window\n");
        std::free(read_buf);
        std::free(verify_buf);
        std::fclose(f);
        return 1;
    }

    fill_psram_region_raw(psram_spi, psram_base, file_size, 0xA5);

    uint32_t crc_file  = 0;
    uint32_t crc_psram = 0;
    uint32_t done = 0;

    while (done < file_size) {
        size_t want = (size_t)(file_size - done);
        if (want > chunk_size) {
            want = chunk_size;
        }

        size_t got = std::fread(read_buf, 1, want, f);
        if (got != want) {
            std::printf("fread failed at offset %u\n", done);
            std::free(read_buf);
            std::free(verify_buf);
            std::fclose(f);
            return 1;
        }

        crc_file = crc32_update(crc_file, read_buf, want);

        if (!raw_psram_write_chunked(psram_spi, psram_base + done, read_buf, want)) {
            std::printf("raw chunked write failed at offset %u size %u\n", done, (unsigned)want);
            std::free(read_buf);
            std::free(verify_buf);
            std::fclose(f);
            return 1;
        }

        if (!raw_psram_read_chunked(psram_spi, psram_base + done, verify_buf, want)) {
            std::printf("raw chunked read failed at offset %u size %u\n", done, (unsigned)want);
            std::free(read_buf);
            std::free(verify_buf);
            std::fclose(f);
            return 1;
        }

        if (!compare_buffers(read_buf, verify_buf, want)) {
            size_t bad = 0;
            while (bad < want && read_buf[bad] == verify_buf[bad]) {
                ++bad;
            }

            std::printf("Mismatch at offset %u size %u first_bad=%u expected=%02X got=%02X\n",
                        done,
                        (unsigned)want,
                        (unsigned)bad,
                        bad < want ? read_buf[bad] : 0,
                        bad < want ? verify_buf[bad] : 0);

            std::free(read_buf);
            std::free(verify_buf);
            std::fclose(f);
            return 1;
        }

        crc_psram = crc32_update(crc_psram, verify_buf, want);
        done += (uint32_t)want;
    }

    std::printf("RAW stream copy OK: bytes=%u crc_file=%08X crc_psram=%08X\n",
                file_size, crc_file, crc_psram);

    std::free(read_buf);
    std::free(verify_buf);
    std::fclose(f);
    return (crc_file == crc_psram) ? 0 : 1;
}

static int sd_psram_seek_mix_test_raw(psram_spi_inst_t* psram_spi, const char* path) {
    std::printf("RAW SD<->PSRAM seek/interleave test: %s\n", path);

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::printf("Failed to open file: %s\n", path);
        return 1;
    }

    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::printf("fseek end failed\n");
        std::fclose(f);
        return 1;
    }

    long file_size_long = std::ftell(f);
    if (file_size_long <= 0) {
        std::printf("Invalid file size\n");
        std::fclose(f);
        return 1;
    }

    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::printf("fseek set failed\n");
        std::fclose(f);
        return 1;
    }

    const uint32_t file_size = (uint32_t)file_size_long;
    uint8_t src[256];
    uint8_t dst[256];

    const uint32_t offsets[] = {
        0u,
        512u,
        1024u,
        4096u,
        8192u,
        file_size > 16384u ? file_size - 16384u : 0u,
        file_size > 257u ? file_size - 257u : 0u
    };

    const size_t sizes[] = { 16, 32, 64, 128, 256 };
    uint32_t psram_addr = 0x300000;

    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        for (size_t j = 0; j < sizeof(sizes) / sizeof(sizes[0]); ++j) {
            uint32_t off = offsets[i];
            size_t want = sizes[j];

            if (off + want > file_size) {
                continue;
            }

            if (psram_addr > kPsramCapacity || want > (kPsramCapacity - psram_addr)) {
                std::printf("PSRAM test window exhausted\n");
                std::fclose(f);
                return 1;
            }

            if (std::fseek(f, (long)off, SEEK_SET) != 0) {
                std::printf("fseek failed at %u\n", off);
                std::fclose(f);
                return 1;
            }

            size_t got = std::fread(src, 1, want, f);
            if (got != want) {
                std::printf("fread failed at %u size %u\n", off, (unsigned)want);
                std::fclose(f);
                return 1;
            }

            fill_psram_region_raw(psram_spi, psram_addr, want, 0x5A);

            if (!raw_psram_write_chunked(psram_spi, psram_addr, src, want)) {
                std::printf("seek/interleave write failed off=%u size=%u\n", off, (unsigned)want);
                std::fclose(f);
                return 1;
            }

            if (!raw_psram_read_chunked(psram_spi, psram_addr, dst, want)) {
                std::printf("seek/interleave read failed off=%u size=%u\n", off, (unsigned)want);
                std::fclose(f);
                return 1;
            }

            if (!compare_buffers(src, dst, want)) {
                size_t bad = 0;
                while (bad < want && src[bad] == dst[bad]) {
                    ++bad;
                }

                std::printf("Seek/interleave mismatch off=%u size=%u first_bad=%u expected=%02X got=%02X\n",
                            off,
                            (unsigned)want,
                            (unsigned)bad,
                            bad < want ? src[bad] : 0,
                            bad < want ? dst[bad] : 0);

                std::fclose(f);
                return 1;
            }

            psram_addr += 512;
        }
    }

    std::printf("RAW seek/interleave OK\n");
    std::fclose(f);
    return 0;
}

static int sd_psram_chunk_sweep_test_raw(psram_spi_inst_t* psram_spi, const char* path) {
    const size_t sizes[] = { 1, 4, 16, 32, 64, 128, 256, 512, 1024 };
    bool all_ok = true;

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        const uint32_t base = 0x100000 + (uint32_t)(i * 0x80000);

        const int rc = sd_to_psram_stream_test_raw(psram_spi, path, sizes[i], base);
        if (rc != 0) {
            std::printf("RAW chunk sweep failed at size=%u base=%u\n",
                        (unsigned)sizes[i], base);
            all_ok = false;
        }
    }

    if (all_ok) {
        std::printf("RAW chunk sweep OK\n");
        return 0;
    }

    std::printf("RAW chunk sweep had failures\n");
    return 1;
}

static int repeat_sd_to_psram_stream_test_raw(psram_spi_inst_t* psram_spi,
                                              const char* path,
                                              size_t chunk_size,
                                              uint32_t psram_base,
                                              int iterations) {
    int failures = 0;

    for (int i = 0; i < iterations; ++i) {
        std::printf("Repeat RAW stream test iter=%d chunk=%u base=%u\n",
                    i, (unsigned)chunk_size, psram_base);

        const int rc = sd_to_psram_stream_test_raw(psram_spi, path, chunk_size, psram_base);
        if (rc != 0) {
            ++failures;
            std::printf("Repeat RAW stream test FAILED iter=%d chunk=%u base=%u\n",
                        i, (unsigned)chunk_size, psram_base);

            // Stop immediately on first failure:
            return 1;

            // Or, if you want to keep going instead, comment out the line above.
        }
    }

    std::printf("Repeat RAW stream test passed: iterations=%d chunk=%u base=%u failures=%d\n",
                iterations, (unsigned)chunk_size, psram_base, failures);
    return 0;
}

int main() {
    set_sys_clock_khz(150000, true);
    stdio_init_all();
    sleep_ms(5000);

    uart_init(uart0, 115200);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, false);

    gpio_init(LEDPIN);
    gpio_set_dir(LEDPIN, GPIO_OUT);

    gpio_put(LEDPIN, 1);
    sleep_ms(500);
    gpio_put(LEDPIN, 0);

    psram_spi_inst_t psram_spi = psram_spi_init_clkdiv(pio1, -1, 1.0f, true);

    sleep_ms(5000);
    if (psram_test(&psram_spi) == EXIT_SUCCESS) {
        std::printf("PSRAM test passed!\n");
    } else {
        std::printf("PSRAM test failed!\n");
    }

    const char* test_file = "data/TITLE.PKD";

    if (sd_to_psram_stream_test_raw(&psram_spi, test_file, 1024, 0x200000) != 0) {
        std::printf("RAW SD->PSRAM stream test failed!\n");
    } else {
        std::printf("RAW SD->PSRAM stream test passed!\n");
    }

    if (sd_psram_seek_mix_test_raw(&psram_spi, test_file) != 0) {
        std::printf("RAW SD<->PSRAM seek/interleave test failed!\n");
    } else {
        std::printf("RAW SD<->PSRAM seek/interleave test passed!\n");
    }

    if (sd_psram_chunk_sweep_test_raw(&psram_spi, test_file) != 0) {
        std::printf("RAW SD<->PSRAM chunk sweep failed!\n");
    } else {
        std::printf("RAW SD<->PSRAM chunk sweep passed!\n");
    }

    if (repeat_sd_to_psram_stream_test_raw(&psram_spi, test_file, 16, 0x180000, 100) != 0) {
        std::printf("REPEAT RAW SD->PSRAM 16-byte test failed!\n");
    } else {
        std::printf("REPEAT RAW SD->PSRAM 16-byte test passed!\n");
    }

    if (repeat_sd_to_psram_stream_test_raw(&psram_spi, test_file, 512, 0x200000, 100) != 0) {
        std::printf("REPEAT RAW SD->PSRAM 512-byte test failed!\n");
    } else {
        std::printf("REPEAT RAW SD->PSRAM 512-byte test passed!\n");
    }

    while (true) {
        sleep_ms(100);
    }
}