#include "common.h"

uint8 ADPCM4_ADAPT[] = { // IWRAM !
    192,192,136,136,128,128,128,128, // -8..-1
    112,128,128,128,128,136,136,192, //  0..+7
};

#if defined(__PICOCALC_WIN__) || defined(__PICOCALC__)
#include "picocalc/Psram.hpp"

extern uint32 gTracksPsramAddr;
extern uint32 gTracksPsramSize;
extern int32 gTrackInfos[128][2];
extern uint32 gTrackInfoCount;
#elif defined(__GBA__) && defined(USE_ASM)
extern const uint8_t TRACKS_AD4[];
#else
extern const void* TRACKS_AD4;
#endif

int8 soundBuffer[2 * SND_SAMPLES + 32]; // 32 bytes of silence for DMA overrun while interrupt

static constexpr int SND_SAMPLE_CACHE_SLOTS = 8;
static constexpr int SND_SAMPLE_CACHE_BYTES = 64 * 1024;

struct SoundSampleCacheSlot
{
    int32 soundIndex;
    uint32 psramOffset;
    uint32 size;
    uint32 lastUseTick;
    uint8* data;
};

static uint8 gSoundSampleCacheStorage[SND_SAMPLE_CACHE_SLOTS][SND_SAMPLE_CACHE_BYTES];
static SoundSampleCacheSlot gSoundSampleCache[SND_SAMPLE_CACHE_SLOTS] = {};
static uint32 gSoundSampleCacheTick = 1;

extern uint32 gLevelSoundPsramAddr;
extern uint32 gLevelSoundPsramSize;

static const uint8* getSoundSampleFromPsram(int32 soundIndex, uint32 offset, uint32 size)
{
    for (int i = 0; i < SND_SAMPLE_CACHE_SLOTS; i++) {
        if (gSoundSampleCache[i].soundIndex == soundIndex &&
            gSoundSampleCache[i].psramOffset == offset &&
            gSoundSampleCache[i].size == size)
        {
            gSoundSampleCache[i].lastUseTick = gSoundSampleCacheTick++;
            return gSoundSampleCache[i].data;
        }
    }

    SoundSampleCacheSlot* slot = &gSoundSampleCache[0];
    for (int i = 1; i < SND_SAMPLE_CACHE_SLOTS; i++) {
        if (gSoundSampleCache[i].lastUseTick < slot->lastUseTick)
            slot = &gSoundSampleCache[i];
    }

    ASSERT(size <= SND_SAMPLE_CACHE_BYTES);
    if (size > SND_SAMPLE_CACHE_BYTES)
        return nullptr;

    slot->soundIndex = soundIndex;
    slot->psramOffset = offset;
    slot->size = size;
    slot->lastUseTick = gSoundSampleCacheTick++;
    slot->data = gSoundSampleCacheStorage[slot - gSoundSampleCache];

    if (!ol::psram::read(gLevelSoundPsramAddr + offset, slot->data, size))
        return nullptr;

    return slot->data;
}

#ifdef USE_ASM
    #define sndADPCM4_fill sndADPCM4_fill_asm
    #define sndPCM_fill    sndPCM_fill_asm
    #define sndPCM_mix     sndPCM_mix_asm
    #define sndClear       sndClear_asm

    extern "C" {
        void sndClear_asm(int8* buffer);
        void sndADPCM4_fill_asm(ADPCM4_STATE &state, int8* buffer, const uint8* data, int32 size);
        int32 sndPCM_fill_asm(int32 pos, int32 inc, int32 size, int32 volume, const uint8* data, int8* buffer);
        int32 sndPCM_mix_asm(int32 pos, int32 inc, int32 size, int32 volume, const uint8* data, int8* buffer);
    }
#else
    #define sndADPCM4_fill sndADPCM4_c
    #define sndPCM_fill    sndPCM_c
    #define sndPCM_mix     sndPCM_c
    #define sndClear(b)    dmaFill(b, SND_ENCODE(0), SND_SAMPLES * sizeof(b[0]))

#define DECODE_ADPCM4(n)\
    tap = zM2 + tap - (tap >> 3);\
    *buffer++ = SND_ENCODE(X_CLAMP(tap >> 8, SND_MIN, SND_MAX));\
    res = ((n&0xF) ^ 8) - 8;\
    out = res*quant + (zM1 - zM2);\
    zM2 = zM1;\
    zM1 = out;\
    quant = (quant*(int32)ADPCM4_ADAPT[res+8] + 127) >> 7;\

void sndADPCM4_c(ADPCM4_STATE &state, int8* buffer, const uint8* data, int32 size)
{
    int32 zM1   = state.zM1;
    int32 zM2   = state.zM2;
    int32 tap   = state.tap;
    int32 quant = state.quant;
    int32 res, out;
    
    for (int32 i=0; i < size; i++)
    {
        uint32 n = *data++;
        DECODE_ADPCM4(n);
        n >>= 4;
        DECODE_ADPCM4(n);
    }
    
    state.zM1   = zM1;
    state.zM2   = zM2;
    state.tap   = tap;
    state.quant = quant;
}
#if defined (__PICOCALC__) || defined (__PICOCALC_WIN__)
int32 sndPCM_c(int32 pos, int32 inc, int32 size, int32 volume, uint32 dataOffset, int8* buffer)
{
    static constexpr int32 kStageSize = 256;
    uint8 stage[kStageSize];

    int32 last = pos + SND_SAMPLES * inc;
    if (last > size) {
        last = size;
    }

    uint32 cachedByteBase = 0xFFFFFFFFu;
    uint32 cachedCount = 0;

    while (pos < last)
    {
        const uint32 byteIndex = uint32(pos >> SND_FIXED_SHIFT);

        if (!(byteIndex >= cachedByteBase && byteIndex < cachedByteBase + cachedCount))
        {
            cachedByteBase = byteIndex;
            cachedCount = uint32(size >> SND_FIXED_SHIFT) - cachedByteBase;
            if (cachedCount > kStageSize) {
                cachedCount = kStageSize;
            }

            if (!osReadLevelSoundData(dataOffset + cachedByteBase, stage, cachedCount))
            {
                break;
            }
        }

        const uint8 src = stage[byteIndex - cachedByteBase];
        int32 amp = SND_DECODE(*(uint8*)buffer) + ((SND_DECODE(src) * volume) >> SND_VOL_SHIFT);
        *buffer++ = SND_ENCODE(X_CLAMP(amp, SND_MIN, SND_MAX));

        pos += inc;
    }

    return pos;
}
#else
int32 sndPCM_c(int32 pos, int32 inc, int32 size, int32 volume, const uint8* data, int8* buffer)
{
    int32 last = pos + SND_SAMPLES * inc;
    if (last > size) {
        last = size;
    }

    while (pos < last)
    {
        int32 amp = SND_DECODE(*(uint8*)buffer) + ((SND_DECODE(data[pos >> SND_FIXED_SHIFT]) * volume) >> SND_VOL_SHIFT);
        *buffer++ = SND_ENCODE(X_CLAMP(amp, SND_MIN, SND_MAX));
        pos += inc;
    }

    return pos;
}
#endif
#endif

struct Music
{
#if defined(__PICOCALC_WIN__) || defined(__PICOCALC__)
    uint32        psramAddr;
    int32         size;
    int32         pos;
    ADPCM4_STATE  state;
    uint8         cache[SND_SAMPLES >> 1];

    void fill(int8* buffer)
    {
        int32 len = X_MIN(size - pos, SND_SAMPLES >> 1);

        if (!ol::psram::read(psramAddr + (uint32)pos, cache, (size_t)len))
        {
            psramAddr = 0xFFFFFFFFu;
            memset(buffer, 0, SND_SAMPLES * sizeof(buffer[0]));
            return;
        }

        sndADPCM4_fill(state, buffer, cache, len);

        pos += len;

        if (pos >= size)
        {
            psramAddr = 0xFFFFFFFFu;
            memset(buffer + (len << 1), 0, (SND_SAMPLES - (len << 1)) * sizeof(buffer[0]));
        }
    }

    bool active() const
    {
        return psramAddr != 0xFFFFFFFFu;
    }
#else
    const uint8* data;
    int32         size;
    int32         pos;
    ADPCM4_STATE  state;

    void fill(int8* buffer)
    {
        int32 len = X_MIN(size - pos, SND_SAMPLES >> 1);

        sndADPCM4_fill(state, buffer, data + pos, len);

        pos += len;

        if (pos >= size)
        {
            data = NULL;
            memset(buffer + (len << 1), 0, (SND_SAMPLES - (len << 1)) * sizeof(buffer[0]));
        }
    }

    bool active() const
    {
        return data != NULL;
    }
#endif
};

#if defined (__PICOCALC__) || defined(__PICOCALC_WIN__)
static const uint32 INVALID_SOUND_OFFSET = 0xFFFFFFFFu;
#endif

struct Sample
{
    int32        pos;
    int32        inc;
    int32        size;
    int32        volume;
#if defined (__PICOCALC__) || defined(__PICOCALC_WIN__)
    uint32 dataOffset;
    void mix(int8* buffer)
    {
        pos = sndPCM_mix(pos, inc, size, volume, dataOffset, buffer);

        if (pos >= size)
        {
            dataOffset = INVALID_SOUND_OFFSET;
        }
    }

    void fill(int8* buffer)
    {
        pos = sndPCM_fill(pos, inc, size, volume, dataOffset, buffer);

        if (pos >= size)
        {
            dataOffset = INVALID_SOUND_OFFSET;
        }
    }
#else
    const uint8* data;
    void mix(int8* buffer)
    {
        pos = sndPCM_mix(pos, inc, size, volume, data, buffer);

        if (pos >= size)
        {
            data = NULL;
        }
    }

    void fill(int8* buffer)
    {
        pos = sndPCM_fill(pos, inc, size, volume, data, buffer);

        if (pos >= size)
        {
            data = NULL;
        }
    }
#endif
};

EWRAM_DATA Music  music;
EWRAM_DATA Sample channels[SND_CHANNELS];
EWRAM_DATA int32  channelsCount;

#if defined(__PICOCALC_WIN__) || defined(__PICOCALC__)
static void initMusicState()
{
    music.psramAddr = INVALID_SOUND_OFFSET;
    music.size = 0;
    music.pos = 0;
    music.state.zM1 = 0;
    music.state.zM2 = 0;
    music.state.tap = 0;
    music.state.quant = 0x0800;
}
#endif

#define CALC_INC (((SND_SAMPLE_FREQ << SND_FIXED_SHIFT) / SND_OUTPUT_FREQ) * pitch >> SND_PITCH_SHIFT)

void sndInit()
{
    // initialized in main.cpp
}

void sndInitSamples()
{
    // nothing to do
}

void sndFreeSamples()
{
    // nothing to do
}

#if defined (__PICOCALC__) || defined (__PICOCALC_WIN__)
void* sndPlaySample(int32 index, int32 volume, int32 pitch, int32 mode)
{
    if (!gSettings.audio_sfx)
        return NULL;

    const uint32 dataOffset = level.soundOffsets[index];

    int32 size = 0;
    if (!osReadLevelSoundData(dataOffset, &size, sizeof(size)))
        return NULL;

    const uint32 sampleOffset = dataOffset + 4;

    if (mode == UNIQUE || mode == REPLAY)
    {
        for (int32 i = 0; i < channelsCount; i++)
        {
            Sample* sample = channels + i;

            if (sample->dataOffset != sampleOffset)
                continue;

            sample->inc = CALC_INC;
            sample->volume = volume;

            if (mode == REPLAY)
            {
                sample->pos = 0;
            }

            return sample;
        }
    }

    if (channelsCount >= SND_CHANNELS)
        return NULL;

    Sample* sample = channels + channelsCount++;
    sample->dataOffset = sampleOffset;
    sample->size = size << SND_FIXED_SHIFT;
    sample->pos = 0;
    sample->inc = CALC_INC;
    sample->volume = volume;

    return sample;
}
#else
void* sndPlaySample(int32 index, int32 volume, int32 pitch, int32 mode)
{
    if (!gSettings.audio_sfx)
        return NULL;

    const uint8 *data = level.soundData + level.soundOffsets[index];
    int32 size = *(int32*)data;
    data += 4;

    if (mode == UNIQUE || mode == REPLAY)
    {
        for (int32 i = 0; i < channelsCount; i++)
        {
            Sample* sample = channels + i;

            if (sample->data != data)
                continue;

            sample->inc = CALC_INC;
            sample->volume = volume;

            if (mode == REPLAY)
            {
                sample->pos = 0;
            }

            return sample;
        }
    }

    if (channelsCount >= SND_CHANNELS)
        return NULL;

    Sample* sample = channels + channelsCount++;
    sample->data = data;
    sample->size = size << SND_FIXED_SHIFT;
    sample->pos  = 0;
    sample->inc  = CALC_INC;
    sample->volume = volume;

    return sample;
}
#endif

void sndPlayTrack(int32 track)
{
    if (!gSettings.audio_music)
        return;

    if (track == gCurTrack)
        return;

    gCurTrack = track;

    if (track == -1) {
        sndStopTrack();
        return;
    }

#if defined(__PICOCALC_WIN__) || defined(__PICOCALC__)
    if (gTracksPsramAddr == INVALID_SOUND_OFFSET)
        return;

    if (track < 0 || (uint32)track >= gTrackInfoCount)
        return;

    const int32 offset = gTrackInfos[track][0];
    const int32 size = gTrackInfos[track][1];

    if (!size)
        return;

    music.psramAddr = INVALID_SOUND_OFFSET;
    music.size = size;
    music.pos = 0;
    music.state.zM1 = 0;
    music.state.zM2 = 0;
    music.state.tap = 0;
    music.state.quant = 0x0800;
    music.psramAddr = gTracksPsramAddr + (uint32)offset;
#else
    struct TrackInfo {
        int32 offset;
        int32 size;
    };

    const TrackInfo* info = (const TrackInfo*)TRACKS_AD4 + track;

    if (!info->size)
        return;

    music.data = NULL;
    music.size = info->size;
    music.pos = 0;
    music.state.zM1 = 0;
    music.state.zM2 = 0;
    music.state.tap = 0;
    music.state.quant = 0x0800;
    music.data = (const uint8*)TRACKS_AD4 + info->offset;
#endif
}

void sndStopTrack()
{
#if defined(__PICOCALC_WIN__) || defined(__PICOCALC__)
    music.psramAddr = INVALID_SOUND_OFFSET;
#else
    music.data = NULL;
#endif
    music.size = 0;
    music.pos = 0;
}

bool sndTrackIsPlaying()
{
    return music.active();
}

void sndStopSample(int32 index)
{
    int32 i = channelsCount;

#if defined(__PICOCALC__) || defined(__PICOCALC_WIN__)
    const uint32 sampleOffset = level.soundOffsets[index] + 4;
    while (--i >= 0)
    {
         if (channels[i].dataOffset == sampleOffset)
         {
            channels[i] = channels[--channelsCount];
		 }
#else
    const uint8 *data = level.soundData + level.soundOffsets[index] + 4;
    while (--i >= 0)
    {
        if (channels[i].data == data)
        {
            channels[i] = channels[--channelsCount];
        }
#endif
    }
}

void sndStop()
{
    channelsCount = 0;
#if defined(__PICOCALC_WIN__) || defined(__PICOCALC__)
    music.psramAddr = INVALID_SOUND_OFFSET;
#else
    music.data = NULL;
#endif
}

void sndFill(int8* buffer)
{
#ifdef PROFILE_SOUNDTIME
    PROFILE_CLEAR();
    PROFILE(CNT_SOUND);
#endif
    bool mix = music.active();

    if (mix) {
        music.fill(buffer);
    } else {
        sndClear(buffer);
    }

    int32 ch = channelsCount;
    while (ch--)
    {
        Sample* sample = channels + ch;

        if (mix)
            sample->mix(buffer);
        else
            sample->fill(buffer);

#if defined(__PICOCALC__) || defined(__PICOCALC_WIN__)
        if (sample->dataOffset == INVALID_SOUND_OFFSET) {
            channels[ch] = channels[--channelsCount];
        }
#else
        if (!sample->data) {
            channels[ch] = channels[--channelsCount];
        }
#endif
        mix = true;
    }
}
