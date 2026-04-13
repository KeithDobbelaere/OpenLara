#define _CRT_SECURE_NO_WARNINGS
#include "game.h"

#include "picocalc/Psram.hpp"

EWRAM_DATA int32 fps;
EWRAM_DATA int32 frameIndex = 0;
EWRAM_DATA int32 fpsCounter = 0;
EWRAM_DATA uint32 curSoundBuffer = 0;

const void* TRACKS_AD4 = nullptr;
const void* TITLE_SCR = nullptr;
const void* levelData = nullptr;

uint32 gTitleScrPsramAddr = 0xFFFFFFFFu;
uint32 gTitleScrPsramSize = 0;

uint32 gTracksPsramAddr = 0xFFFFFFFFu;
uint32 gTracksPsramSize = 0;

uint32 gInvBgPsramAddr = 0xFFFFFFFFu;
uint32 gInvBgPsramSize = 0;

uint32 gLevelSoundPsramAddr = 0xFFFFFFFFu;
uint32 gLevelSoundPsramSize = 0;

uint32 gLevelTilesPsramAddr = 0xFFFFFFFFu;
uint32 gLevelTilesPsramSize = 0;

struct RoomRenderRef
{
    uint32 psramOffset;
    uint32 byteSize;

    uint16 verticesCount;
    uint16 quadsCount;
    uint16 trianglesCount;
};

struct RoomCacheSlot
{
    int16  roomIndex;
    uint16 pad;
    uint32 lastUseFrame;
    uint32 byteSize;

    uint8* buffer;
};

static constexpr int ROOM_CACHE_SLOTS = 8;

RoomRenderRef* gRoomRenderRefs = nullptr;
RoomCacheSlot  gRoomCache[ROOM_CACHE_SLOTS] = {};
uint8*         gRoomCacheStorage = nullptr;
uint32         gRoomCacheSlotSize = 0;
uint32         gRoomCacheFrameTag = 1;

// [track][0] = offset, [track][1] = size
int32  gTrackInfos[128][2] = {};
uint32 gTrackInfoCount = 0;

uint32 gRoomRenderTotalBytes = 0;
uint32 gRoomRenderMaxBytes = 0;
int32  gRoomRenderMaxRoom = -1;

uint32 gRoomCacheHits = 0;
uint32 gRoomCacheMisses = 0;
uint32 gRoomCacheHitsPrev = 0;
uint32 gRoomCacheMissesPrev = 0;

static constexpr uintptr_t kTitleScrPsramSentinel = 0x5449544Cu; // "TITL"
static constexpr uintptr_t kInvBgPsramSentinel = 0x494E5642u; // "INVB"
static constexpr uintptr_t kLevelSoundPsramSentinel = 0x534E4450u; // "SNDP"

bool gUseGrayPalette = false;

void osSetGrayPalette(bool enabled)
{
    gUseGrayPalette = enabled;
}

const void* osGetInventoryBackgroundHandle()
{
    return (const void*)kInvBgPsramSentinel;
}

bool osSnapshotInventoryBackground()
{
    const uint32 size = FRAME_WIDTH * FRAME_HEIGHT;

    if (gInvBgPsramAddr == 0xFFFFFFFFu) {
        gInvBgPsramAddr = ol::psram::alloc(size, 16);
        if (gInvBgPsramAddr == 0xFFFFFFFFu)
            return false;
    }

    if (!ol::psram::write(gInvBgPsramAddr, (const void*)fb, size))
        return false;

    gInvBgPsramSize = size;
    return true;
}

bool osHasLevelSoundDataInPsram()
{
    return gLevelSoundPsramAddr != 0xFFFFFFFFu && gLevelSoundPsramSize > 0;
}

bool osReadLevelSoundData(uint32 offset, void* dst, uint32 size)
{
    if (!dst)
        return false;

    if (gLevelSoundPsramAddr == 0xFFFFFFFFu)
        return false;

    if (offset > gLevelSoundPsramSize || size > gLevelSoundPsramSize - offset)
        return false;

    return ol::psram::read(gLevelSoundPsramAddr + offset, dst, size);
}

void osResetRoomRenderCache()
{
    delete[] gRoomRenderRefs;
    gRoomRenderRefs = nullptr;

    delete[] gRoomCacheStorage;
    gRoomCacheStorage = nullptr;

    gRoomCacheSlotSize = 0;
    gRoomCacheFrameTag = 1;

    for (int i = 0; i < ROOM_CACHE_SLOTS; i++) {
        gRoomCache[i].roomIndex = -1;
        gRoomCache[i].lastUseFrame = 0;
        gRoomCache[i].byteSize = 0;
        gRoomCache[i].buffer = nullptr;
    }

    gRoomRenderTotalBytes = 0;
    gRoomRenderMaxBytes = 0;
    gRoomRenderMaxRoom = -1;

    gRoomCacheHits = 0;
    gRoomCacheMisses = 0;
    gRoomCacheHitsPrev = 0;
    gRoomCacheMissesPrev = 0;
}

bool osBuildRoomRenderPsram(Level* level)
{
    osResetRoomRenderCache();

    if (!level || level->roomsCount <= 0)
        return false;

    const int32 roomCount = level->roomsCount;

    gRoomRenderRefs = new RoomRenderRef[roomCount];
    memset(gRoomRenderRefs, 0, sizeof(RoomRenderRef) * roomCount);

    uint32 maxRoomBlobSize = 0;

    for (int32 i = 0; i < roomCount; i++)
    {
        Room* room = rooms + i;

        const uint32 verticesBytes = room->info->verticesCount * sizeof(RoomVertex);
        const uint32 quadsBytes = room->info->quadsCount * sizeof(RoomQuad);
        const uint32 trianglesBytes = room->info->trianglesCount * sizeof(RoomTriangle);
        const uint32 totalBytes = verticesBytes + quadsBytes + trianglesBytes;

        RoomRenderRef& ref = gRoomRenderRefs[i];
        ref.verticesCount = room->info->verticesCount;
        ref.quadsCount = room->info->quadsCount;
        ref.trianglesCount = room->info->trianglesCount;
        ref.byteSize = totalBytes;

        gRoomRenderTotalBytes += totalBytes;

        if (totalBytes > gRoomRenderMaxBytes) {
            gRoomRenderMaxBytes = totalBytes;
            gRoomRenderMaxRoom = i;
        }

        if (totalBytes == 0) {
            ref.psramOffset = 0xFFFFFFFFu;

            room->data.vertices = nullptr;
            room->data.quads = nullptr;
            room->data.triangles = nullptr;
            continue;
        }

        uint32 addr = ol::psram::alloc(totalBytes, 16);
        if (addr == 0xFFFFFFFFu)
            return false;

        ref.psramOffset = addr;

        uint32 offset = 0;

        if (verticesBytes) {
            if (!ol::psram::write(addr + offset, room->data.vertices, verticesBytes))
                return false;
            offset += verticesBytes;
        }

        if (quadsBytes) {
            if (!ol::psram::write(addr + offset, room->data.quads, quadsBytes))
                return false;
            offset += quadsBytes;
        }

        if (trianglesBytes) {
            if (!ol::psram::write(addr + offset, room->data.triangles, trianglesBytes))
                return false;
            offset += trianglesBytes;
        }

        room->data.vertices = nullptr;
        room->data.quads = nullptr;
        room->data.triangles = nullptr;

        if (totalBytes > maxRoomBlobSize)
            maxRoomBlobSize = totalBytes;
    }

    gRoomCacheSlotSize = maxRoomBlobSize;
    gRoomCacheStorage = new uint8[gRoomCacheSlotSize * ROOM_CACHE_SLOTS];

    for (int i = 0; i < ROOM_CACHE_SLOTS; i++) {
        gRoomCache[i].roomIndex = -1;
        gRoomCache[i].lastUseFrame = 0;
        gRoomCache[i].byteSize = 0;
        gRoomCache[i].buffer = gRoomCacheStorage + i * gRoomCacheSlotSize;
    }

    LOG("Room render PSRAM built: rooms=%d total=%u maxRoom=%d maxBytes=%u slotSize=%u slots=%d\n",
        roomCount,
        gRoomRenderTotalBytes,
        gRoomRenderMaxRoom,
        gRoomRenderMaxBytes,
        gRoomCacheSlotSize,
        ROOM_CACHE_SLOTS);

    LOG("Room render resident pointers invalidated; renderer now uses PSRAM cache\n");

    return true;
}

#ifdef __PICOCALC_WIN__
static int32 gStatsTime = 0;

HWND hWnd;

LARGE_INTEGER g_timer;
LARGE_INTEGER g_current;

#define WND_WIDTH   (320 * 4)
#define WND_HEIGHT  (240 * 4)

uint16 MEM_PAL_BG[256];
uint16 MEM_PAL_BG_GRAY[256];
uint32 SCREEN[FRAME_WIDTH * FRAME_HEIGHT];

extern int8 soundBuffer[2 * SND_SAMPLES + 32];

HWAVEOUT waveOut;
WAVEFORMATEX waveFmt = { WAVE_FORMAT_PCM, 1, SND_OUTPUT_FREQ, SND_OUTPUT_FREQ, 1, 8, sizeof(waveFmt) };
WAVEHDR waveBuf[2];
HDC hDC;

namespace {

    static void buildGrayPalette(const uint16* src, uint16* dst)
    {
        static const uint8 grad[8] = { 1, 22, 21, 20, 19, 18, 17, 33 };
        uint8 remap[256];

        for (int32 i = 0; i < 256; i++)
        {
            uint16 p = src[i];
            uint8 r = (p & 31);
            uint8 g = ((p >> 5) & 31);
            uint8 b = ((p >> 10) & 31);

            int32 lum = (r * 77 + g * 150 + b * 29) >> (8 + 2);
            remap[i] = grad[lum];
        }

        for (int32 i = 0; i < 256; i++)
            dst[i] = src[remap[i]];
    }

    static bool loadFileToPsram(const char* path, uint32& outAddr, uint32& outSize)
    {
        outAddr = 0xFFFFFFFFu;
        outSize = 0;

        FILE* f = fopen(path, "rb");
        if (!f) {
            LOG("Could not open file \"%s\"\n", path);
            return false;
        }

        fseek(f, 0, SEEK_END);
        int32 size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0) {
            fclose(f);
            LOG("File \"%s\" is empty\n", path);
            return false;
        }

        uint32 addr = ol::psram::alloc(size, 16);
        if (addr == 0xFFFFFFFFu) {
            fclose(f);
            LOG("PSRAM alloc failed for \"%s\" (%d bytes)\n", path, size);
            return false;
        }

        uint8 chunk[1024];
        int32 offset = 0;

        while (offset < size)
        {
            int32 want = size - offset;
            if (want > (int32)sizeof(chunk))
                want = (int32)sizeof(chunk);

            int32 got = (int32)fread(chunk, 1, want, f);
            if (got != want) {
                fclose(f);
                LOG("Read failed for \"%s\" at offset %d\n", path, offset);
                return false;
            }

            if (!ol::psram::write(addr + (uint32)offset, chunk, (size_t)want)) {
                fclose(f);
                LOG("PSRAM write failed for \"%s\" at offset %d\n", path, offset);
                return false;
            }

            offset += want;
        }

        fclose(f);

        outAddr = addr;
        outSize = (uint32)size;
        return true;
    }

    static bool loadTracksToPsram(const char* path)
    {
        if (gTracksPsramAddr != 0xFFFFFFFFu)
            return true;

        FILE* f = fopen(path, "rb");
        if (!f) {
            LOG("Could not open file \"%s\"\n", path);
            return false;
        }

        fseek(f, 0, SEEK_END);
        int32 size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0) {
            fclose(f);
            LOG("Invalid TRACKS.AD4 size\n");
            return false;
        }

        uint32 addr = ol::psram::alloc(size, 16);
        if (addr == 0xFFFFFFFFu) {
            fclose(f);
            LOG("PSRAM alloc failed for TRACKS.AD4 (%d bytes)\n", size);
            return false;
        }

        uint8 chunk[4096];
        uint8 headerBuf[sizeof(gTrackInfos)] = {};
        const uint32 headerBytes = (uint32)sizeof(headerBuf);

        int32 offset = 0;
        while (offset < size)
        {
            int32 want = size - offset;
            if (want > (int32)sizeof(chunk))
                want = (int32)sizeof(chunk);

            int32 got = (int32)fread(chunk, 1, want, f);
            if (got != want) {
                fclose(f);
                LOG("Failed reading TRACKS.AD4 at offset %d\n", offset);
                return false;
            }

            if (!ol::psram::write(addr + (uint32)offset, chunk, (size_t)want)) {
                fclose(f);
                LOG("PSRAM write failed for TRACKS.AD4 at offset %d\n", offset);
                return false;
            }

            if ((uint32)offset < headerBytes) {
                uint32 copyOffset = (uint32)offset;
                uint32 copyBytes = (uint32)want;
                if (copyOffset + copyBytes > headerBytes)
                    copyBytes = headerBytes - copyOffset;
                memcpy(headerBuf + copyOffset, chunk, copyBytes);
            }

            offset += want;
        }

        fclose(f);

        memcpy(gTrackInfos, headerBuf, sizeof(gTrackInfos));
        gTrackInfoCount = 128;
        gTracksPsramAddr = addr;
        gTracksPsramSize = (uint32)size;

        TRACKS_AD4 = (const void*)1;
        LOG("TRACKS.AD4 -> PSRAM addr=%u size=%u\n", gTracksPsramAddr, gTracksPsramSize);
        return true;
    }

} // namespace

void osSetPalette(const uint16* palette)
{
    memcpy(MEM_PAL_BG, palette, 256 * 2);
    buildGrayPalette(MEM_PAL_BG, MEM_PAL_BG_GRAY);
}

int32 osGetSystemTimeMS()
{
    return (int32)GetTickCount64();
}

bool osSaveSettings()
{
    FILE* f = fopen("settings.dat", "wb");
    if (!f) return false;
    fwrite(&gSettings, sizeof(gSettings), 1, f);
    fclose(f);
    return true;
}

bool osLoadSettings()
{
    FILE* f = fopen("settings.dat", "rb");
    if (!f) return false;

    uint8 version;
    fread(&version, 1, 1, f);
    if (version != gSettings.version) {
        fclose(f);
        return false;
    }

    fread((uint8*)&gSettings + 1, sizeof(gSettings) - 1, 1, f);
    fclose(f);
    return true;
}

bool osCheckSave()
{
    FILE* f = fopen("savegame.dat", "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

bool osSaveGame()
{
    FILE* f = fopen("savegame.dat", "wb");
    if (!f) return false;
    fwrite(&gSaveGame, sizeof(gSaveGame), 1, f);
    fwrite(&gSaveData, gSaveGame.dataSize, 1, f);
    fclose(f);
    return true;
}

bool osLoadGame()
{
    FILE* f = fopen("savegame.dat", "rb");
    if (!f) return false;

    uint32 version;
    fread(&version, sizeof(version), 1, f);

    if (SAVEGAME_VER != version)
    {
        fclose(f);
        return false;
    }

    fread(&gSaveGame.dataSize, sizeof(gSaveGame) - sizeof(version), 1, f);
    fread(&gSaveData, gSaveGame.dataSize, 1, f);
    fclose(f);
    return true;
}

void osJoyVibrate(int32 index, int32 L, int32 R) {}

void soundInit()
{
    sndInit();

    if (waveOutOpen(&waveOut, WAVE_MAPPER, &waveFmt, (INT_PTR)hWnd, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR)
        return;

    memset(&waveBuf, 0, sizeof(waveBuf));
    for (int i = 0; i < 2; i++)
    {
        WAVEHDR* waveHdr = waveBuf + i;
        waveHdr->dwBufferLength = SND_SAMPLES;
        waveHdr->lpData = (LPSTR)(soundBuffer + i * SND_SAMPLES);
        waveOutPrepareHeader(waveOut, waveHdr, sizeof(WAVEHDR));
        waveOutWrite(waveOut, waveHdr, sizeof(WAVEHDR));
    }
}

void soundFill()
{
    WAVEHDR* waveHdr = waveBuf + curSoundBuffer;
    waveOutUnprepareHeader(waveOut, waveHdr, sizeof(WAVEHDR));
    sndFill((int8*)waveHdr->lpData);
    waveOutPrepareHeader(waveOut, waveHdr, sizeof(WAVEHDR));
    waveOutWrite(waveOut, waveHdr, sizeof(WAVEHDR));
    curSoundBuffer ^= 1;
}

void blit()
{
    const uint16* pal = gUseGrayPalette ? MEM_PAL_BG_GRAY : MEM_PAL_BG;

    for (int i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; i++)
    {
        uint16 c = pal[fb[i]];
        SCREEN[i] = (((c << 3) & 0xFF) << 16) |
            ((((c >> 5) << 3) & 0xFF) << 8) |
            ((c >> 10 << 3) & 0xFF) |
            0xFF000000;
    }

    const BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), FRAME_WIDTH, -FRAME_HEIGHT, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
    StretchDIBits(hDC, 0, 0, WND_WIDTH, WND_HEIGHT,
        0, 0, FRAME_WIDTH, FRAME_HEIGHT,
        SCREEN, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ACTIVATE:
        keys = 0;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
    {
        InputKey key = IK_NONE;
        switch (wParam)
        {
        case 'W':           key = IK_UP;     break;
        case 'D':           key = IK_RIGHT;  break;
        case 'S':           key = IK_DOWN;   break;
        case 'A':           key = IK_LEFT;   break;
        case VK_SPACE:      key = IK_B;      break;
        case VK_CONTROL:    key = IK_A;      break;
        case VK_LEFT:       key = IK_L;      break;
        case VK_RIGHT:      key = IK_R;      break;
        case VK_RETURN:     key = IK_START;  break;
        case VK_SHIFT:      key = IK_SELECT; break;
        case 'Z':           key = IK_C;      break;
        case 'X':           key = IK_X;      break;
        case 'C':           key = IK_Y;      break;
        case 'V':           key = IK_Z;      break;
        case 'B':           key = IK_LT;     break;
        case 'N':           key = IK_RT;     break;
        }

        if (wParam == '1') players[0]->extraL->goalWeapon = WEAPON_PISTOLS;
        if (wParam == '2') players[0]->extraL->goalWeapon = WEAPON_MAGNUMS;
        if (wParam == '3') players[0]->extraL->goalWeapon = WEAPON_UZIS;
        if (wParam == '4') players[0]->extraL->goalWeapon = WEAPON_SHOTGUN;

        if (msg != WM_KEYUP && msg != WM_SYSKEYUP)
            keys |= key;
        else
            keys &= ~key;

        break;
    }

    case MM_WOM_DONE:
        soundFill();
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 0;
}

const void* osLoadScreen(LevelID id)
{
    (void)id;

    if (!TITLE_SCR)
    {
        if (!loadFileToPsram("data/TITLE.SCR", gTitleScrPsramAddr, gTitleScrPsramSize))
            return NULL;

        TITLE_SCR = (const void*)kTitleScrPsramSentinel;
    }

    return TITLE_SCR;
}

const void* osLoadLevel(LevelID id)
{
    char buf[32];

    if (levelData) {
        delete[](uint8*)levelData;
        levelData = nullptr;
    }

    sprintf(buf, "data/%s.PKD", (const char*)gLevelInfo[id].data);

    FILE* f = fopen(buf, "rb");
    if (!f) {
        LOG("Could not open file \"%s\"", buf);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    int32 size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8* data = new uint8[size];
    fread(data, 1, size, f);
    fclose(f);

    levelData = data;

    if (gTracksPsramAddr == 0xFFFFFFFFu) {
        if (!loadTracksToPsram("data/TRACKS.AD4"))
            return NULL;
    }

    return levelData;
}

int main(void)
{
    RECT r = { 0, 0, WND_WIDTH, WND_HEIGHT };

    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);
    int wx = (GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2;
    int wy = (GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) / 2;

    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    hWnd = CreateWindowW(
        L"static",
        L"OpenLara PicoCalc",
        WS_OVERLAPPEDWINDOW,
        wx + r.left,
        wy + r.top,
        r.right - r.left,
        r.bottom - r.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    hDC = GetDC(hWnd);

    SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)wndProc);
    ShowWindow(hWnd, SW_SHOWDEFAULT);

    if (!ol::psram::init()) {
        LOG("PSRAM init failed\n");
        return 1;
    }

    soundInit();
    gameInit();

    MSG msg;
    int32 startTime = (int32)GetTickCount64() - 33;
    int32 lastFrame = 0;

    do {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
#ifdef _DEBUG
            Sleep(4);
#endif
            int32 frame = ((int32)GetTickCount64() - startTime) / 33;
            if (GetAsyncKeyState('R')) frame /= 10;

            int32 count = frame - lastFrame;
            if (GetAsyncKeyState('T')) count *= 10;

            gameUpdate(count);
            lastFrame = frame;

            gameRender();

            int32 now = (int32)GetTickCount64();
            if (now - gStatsTime >= 1000) {
                gStatsTime = now;
                uint32 hitDelta = gRoomCacheHits - gRoomCacheHitsPrev;
                uint32 missDelta = gRoomCacheMisses - gRoomCacheMissesPrev;
                gRoomCacheHitsPrev = gRoomCacheHits;
                gRoomCacheMissesPrev = gRoomCacheMisses;

                LOG("roomCache hits=%u misses=%u hitDelta=%u missDelta=%u total=%u maxRoom=%d maxBytes=%u slot=%u\n",
                    gRoomCacheHits,
                    gRoomCacheMisses,
					hitDelta,
					missDelta,
                    gRoomRenderTotalBytes,
                    gRoomRenderMaxRoom,
                    gRoomRenderMaxBytes,
                    gRoomCacheSlotSize);
            }

            blit();
        }
    } while (msg.message != WM_QUIT);

    return 0;
}

#else
#include "picocalc/PicoFileSystem.hpp"
#include "picocalc/PicoCalcDisplay.hpp"
#include "picocalc/PicoInput.hpp"
#include "picocalc/Keys.hpp"

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/mutex.h"

#include <cstring>
#include <cstdint>

ol::PicoCalcDisplay   g_display;
ol::PicoKeyboardInput g_keyboard;

namespace {
    uint16 gPalette[256] = {};
    uint16 gPaletteGray[256] = {};

    static void buildGrayPalette(const uint16* src, uint16* dst)
    {
        static const uint8 grad[8] = { 1, 22, 21, 20, 19, 18, 17, 33 };
        uint8 remap[256];

        for (int32 i = 0; i < 256; i++)
        {
            uint16 p = src[i];
            uint8 r = (p & 31);
            uint8 g = ((p >> 5) & 31);
            uint8 b = ((p >> 10) & 31);

            int32 lum = (r * 77 + g * 150 + b * 29) >> (8 + 2);
            remap[i] = grad[lum];
        }

        for (int32 i = 0; i < 256; i++)
            dst[i] = src[remap[i]];
    }

    const void* loadFileToHeapOnce(const char* path, const void*& slot)
    {
        if (slot)
            return slot;

        FILE* f = fopen(path, "rb");
        if (!f) {
            LOG("Could not open \"%s\"\n", path);
            return nullptr;
        }

        const size_t size = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : 0;
        if (!size) {
            fclose(f);
            LOG("Empty file \"%s\"\n", path);
            return nullptr;
        }

        uint8* data = new uint8[size];

        size_t readBytes = 0;
        const bool ok = fread(data, 1, size, f) == size;
        fclose(f);

        if (!ok || readBytes != size) {
            delete[] data;
            LOG("Failed to read \"%s\"\n", path);
            return nullptr;
        }

        slot = data;
        return slot;
    }

    bool loadFileToPsram(const char* path, uint32& outAddr, uint32& outSize)
    {
        outAddr = 0xFFFFFFFFu;
        outSize = 0;

        FILE* f = fopen(path, "rb");
        if (!f) {
            LOG("Could not open \"%s\"\n", path);
            return false;
        }

        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            LOG("Seek-end failed for \"%s\"\n", path);
            return false;
        }

        const long sizeLong = ftell(f);
        if (sizeLong <= 0) {
            fclose(f);
            LOG("Empty or invalid file \"%s\"\n", path);
            return false;
        }

        if (fseek(f, 0, SEEK_SET) != 0) {
            fclose(f);
            LOG("Seek-set failed for \"%s\"\n", path);
            return false;
        }

        const size_t size = (size_t)sizeLong;

        uint32 addr = ol::psram::alloc(size, 16);
        if (addr == 0xFFFFFFFFu) {
            fclose(f);
            LOG("PSRAM alloc failed for \"%s\" (%u bytes)\n", path, (unsigned)size);
            return false;
        }

        static uint8 chunk[1024];
        size_t offset = 0;

        while (offset < size) {
            size_t want = size - offset;
            if (want > sizeof(chunk)) {
                want = sizeof(chunk);
            }

            const size_t got = fread(chunk, 1, want, f);
            if (got != want) {
                fclose(f);
                LOG("Read failed for \"%s\" at offset %u (got=%u want=%u)\n",
                            path,
                            (unsigned)offset,
                            (unsigned)got,
                            (unsigned)want);
                return false;
            }

            if (!ol::psram::write(addr + (uint32)offset, chunk, want)) {
                fclose(f);
                LOG("PSRAM write failed for \"%s\" at offset %u\n",
                            path,
                            (unsigned)offset);
                return false;
            }

            offset += want;
        }

        fclose(f);

        outAddr = addr;
        outSize = (uint32)size;
        return true;
    }

    bool loadTracksToPsram(const char* path)
    {
        if (gTracksPsramAddr != 0xFFFFFFFFu)
            return true;

        FILE* f = fopen(path, "rb");
        if (!f) {
            LOG("Could not open \"%s\"\n", path);
            return false;
        }

        const size_t size = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : 0;
        if (!size) {
            fclose(f);
            LOG("Invalid TRACKS.AD4 size\n");
            return false;
        }

        uint32 addr = ol::psram::alloc(size, 16);
        if (addr == 0xFFFFFFFFu) {
            fclose(f);
            LOG("PSRAM alloc failed for TRACKS.AD4 (%u bytes)\n", (unsigned)size);
            return false;
        }

        uint8 chunk[4096];
        uint8 headerBuf[sizeof(gTrackInfos)] = {};
        const uint32 headerBytes = (uint32)sizeof(headerBuf);

        size_t offset = 0;
        while (offset < size)
        {
            size_t want = size - offset;
            if (want > sizeof(chunk))
                want = sizeof(chunk);

            size_t got = 0;
            if (!fread(chunk, 1, want, f) || got != want) {
                fclose(f);
                LOG("Failed reading TRACKS.AD4 at offset %u\n", (unsigned)offset);
                return false;
            }

            if (!ol::psram::write(addr + (uint32)offset, chunk, want)) {
                fclose(f);
                LOG("PSRAM write failed for TRACKS.AD4 at offset %u\n", (unsigned)offset);
                return false;
            }

            if ((uint32)offset < headerBytes) {
                uint32 copyOffset = (uint32)offset;
                uint32 copyBytes = (uint32)want;
                if (copyOffset + copyBytes > headerBytes)
                    copyBytes = headerBytes - copyOffset;
                memcpy(headerBuf + copyOffset, chunk, copyBytes);
            }

            offset += want;
        }

        fclose(f);

        memcpy(gTrackInfos, headerBuf, sizeof(gTrackInfos));
        gTrackInfoCount = 128;
        gTracksPsramAddr = addr;
        gTracksPsramSize = (uint32)size;

        TRACKS_AD4 = (const void*)1;
        LOG("TRACKS.AD4 -> PSRAM addr=%u size=%u\n",
            (unsigned)gTracksPsramAddr,
            (unsigned)gTracksPsramSize);
        return true;
    }
}

void osSetPalette(const uint16* palette)
{
    if (!palette)
        return;

    memcpy(gPalette, palette, sizeof(gPalette));
    buildGrayPalette(gPalette, gPaletteGray);
}

int32 osGetSystemTimeMS()
{
    return static_cast<int>(to_ms_since_boot(get_absolute_time()));
}

bool osSaveSettings() { return true; }
bool osLoadSettings() { return true; }
bool osCheckSave() { return true; }
bool osSaveGame() { return true; }
bool osLoadGame() { return osCheckSave(); }

void osJoyVibrate(int32 index, int32 L, int32 R)
{
    (void)index;
    (void)L;
    (void)R;
}

extern int8 soundBuffer[2 * SND_SAMPLES + 32];

void soundInit()
{
    // TODO: Implement sound output using PIO + DMA
    sndInit();
}

void soundFill()
{
    // TODO: Fill soundBuffer with audio data
}

void vblank()
{
    frameIndex++;
    soundFill();
}

const void* osLoadScreen(LevelID id)
{
    (void)id;

    if (!TITLE_SCR)
    {
        if (!loadFileToPsram("data/TITLE.SCR", gTitleScrPsramAddr, gTitleScrPsramSize))
            return nullptr;

        TITLE_SCR = (const void*)kTitleScrPsramSentinel;
    }

    return TITLE_SCR;
}

const void* osLoadLevel(LevelID id)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "data/%s.PKD", (const char*)gLevelInfo[id].data);

    if (levelData) {
        delete[](uint8*)levelData;
        levelData = nullptr;
    }

    FILE* f = fopen(buf, "rb");
    if (!f) {
        LOG("Could not open \"%s\"\n", buf);
        return nullptr;
    }

    const size_t size = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : 0;
    if (!size) {
        fclose(f);
        LOG("Empty level \"%s\"\n", buf);
        return nullptr;
    }

    uint8* data = new uint8[size];

    size_t readBytes = 0;
    const bool ok = fread(data, 1, size, f);
    fclose(f);

    if (!ok || readBytes != size) {
        delete[] data;
        LOG("Failed to read \"%s\"\n", buf);
        return nullptr;
    }

    levelData = data;

    if (gTracksPsramAddr == 0xFFFFFFFFu) {
        if (!loadTracksToPsram("data/TRACKS.AD4")) {
            delete[] data;
            levelData = nullptr;
            return nullptr;
        }
    }

    return levelData;
}

void updateInput()
{
    g_keyboard.update();

    uint32 newKeys = 0;

    if (g_keyboard.down(KEY_UP))        newKeys |= IK_UP;
    if (g_keyboard.down(KEY_RIGHT))     newKeys |= IK_RIGHT;
    if (g_keyboard.down(KEY_DOWN))      newKeys |= IK_DOWN;
    if (g_keyboard.down(KEY_LEFT))      newKeys |= IK_LEFT;

    if (g_keyboard.down('A'))           newKeys |= IK_A;
    if (g_keyboard.down('B'))           newKeys |= IK_B;

    if (g_keyboard.down('C'))           newKeys |= IK_C;
    if (g_keyboard.down('X'))           newKeys |= IK_X;
    if (g_keyboard.down('Y'))           newKeys |= IK_Y;
    if (g_keyboard.down('Z'))           newKeys |= IK_Z;

    if (g_keyboard.down('L'))           newKeys |= IK_L;
    if (g_keyboard.down('R'))           newKeys |= IK_R;
    if (g_keyboard.down('Q'))           newKeys |= IK_LT;
    if (g_keyboard.down('W'))           newKeys |= IK_RT;

    if (g_keyboard.down(KEY_ENTER) || g_keyboard.down(KEY_RETURN))
        newKeys |= IK_START;

    if (g_keyboard.down(KEY_TAB) || g_keyboard.down(KEY_ESC))
        newKeys |= IK_SELECT;

    keys = newKeys;
}

int main(void)
{
    stdio_init_all();
    sleep_ms(5000);

    uart_init(uart0, 115200);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, false);

    LOG("Initializing PSRAM...\n");
    if (!ol::psram::init()) {
        LOG("PSRAM init failed\n");
        return 1;
    }

    LOG("Initializing keyboard...\n");
    g_keyboard.init();

    LOG("Initializing file system...\n");
    if (!ol::file_system::init()) {
        LOG("File system init failed\n");
    }
    else {
        LOG("File system ready\n");
    }

    LOG("Initializing display...\n");
    g_display.beginFrame();
    g_display.endFrame();

    LOG("Calling gameInit()\n");
    gameInit();
    LOG("Game initialized\n");

    soundInit();

    int32 startTime = osGetSystemTimeMS() - 33;
    int32 lastFrame = 0;

    int32 fpsTime = osGetSystemTimeMS();
    fpsCounter = 0;
    fps = 0;

    while (1)
    {
        updateInput();

        const int32 now = osGetSystemTimeMS();
        const int32 frame = (now - startTime) / 33;
        const int32 delta = frame - lastFrame;

        if (delta <= 0) {
            sleep_ms(1);
            continue;
        }

        gameUpdate(delta);
        lastFrame = frame;

        gameRender();

        g_display.beginFrame();
        g_display.presentIndexed8(
            fb,
            FRAME_WIDTH,
            FRAME_HEIGHT,
            gUseGrayPalette ? gPaletteGray : gPalette
        );
        g_display.endFrame();

        fpsCounter++;

        if (now - fpsTime >= 1000) {
            fps = fpsCounter;
            fpsCounter = 0;
            fpsTime += 1000;

            LOG("FPS:%d roomCache hits=%u misses=%u total=%u maxRoom=%d maxBytes=%u slot=%u\n",
                fps,
                (unsigned)gRoomCacheHits,
                (unsigned)gRoomCacheMisses,
                (unsigned)gRoomRenderTotalBytes,
                (int)gRoomRenderMaxRoom,
                (unsigned)gRoomRenderMaxBytes,
                (unsigned)gRoomCacheSlotSize);
        }
    }

    return 0;
}

#endif