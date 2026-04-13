#define _CRT_SECURE_NO_WARNINGS
#include "common.h"
#include "PkdStream.hpp"
#include "picocalc/Psram.hpp"

#include <cstdio>
#include <cstring>

extern int32 dynSectorsCount;

extern Model models[MAX_MODELS];
extern const Mesh* meshes[MAX_MESHES];
extern StaticMesh staticMeshes[MAX_STATIC_MESHES];
extern ExtraInfoEnemy enemiesExtra[MAX_ENEMIES];

extern uint32 gLevelTilesPsramAddr;
extern uint32 gLevelTilesPsramSize;

extern uint32 gLevelSoundPsramAddr;
extern uint32 gLevelSoundPsramSize;

extern uint8 gLightmap[256 * 32];

void initLevelRuntime();

namespace {

    enum LevelAllocTag
    {
        LAT_Palette = 0,
        LAT_Lightmap,
        LAT_Tiles,
        LAT_RoomsInfo,
        LAT_Floors,
        LAT_MeshesBlob,
        LAT_MeshOffsets,
        LAT_Anims,
        LAT_AnimStates,
        LAT_AnimRanges,
        LAT_AnimCommands,
        LAT_Nodes,
        LAT_AnimFrames,
        LAT_Models,
        LAT_StaticMeshes,
        LAT_Textures,
        LAT_Sprites,
        LAT_SpriteSequences,
        LAT_Cameras,
        LAT_SoundSources,
        LAT_Boxes,
        LAT_Overlaps,
        LAT_Zones,
        LAT_AnimTexData,
        LAT_ItemsInfo,
        LAT_CameraFrames,
        LAT_SoundMap,
        LAT_SoundsInfo,
        LAT_SoundData,
        LAT_SoundOffsets,
        LAT_RoomSprites,
        LAT_RoomPortals,
        LAT_RoomSectors,
        LAT_RoomLights,
        LAT_RoomMeshes,
        LAT_RoomVertices,
        LAT_RoomQuads,
        LAT_RoomTriangles,
        LAT_COUNT
    };

    static const char* kLevelAllocTagNames[LAT_COUNT] =
    {
        "palette",
        "lightmap",
        "tiles",
        "roomsInfo",
        "floors",
        "meshesBlob",
        "meshOffsets",
        "anims",
        "animStates",
        "animRanges",
        "animCommands",
        "nodes",
        "animFrames",
        "models",
        "staticMeshes",
        "textures",
        "sprites",
        "spriteSequences",
        "cameras",
        "soundSources",
        "boxes",
        "overlaps",
        "zones",
        "animTexData",
        "itemsInfo",
        "cameraFrames",
        "soundMap",
        "soundsInfo",
        "soundData",
        "soundOffsets",
        "roomSprites",
        "roomPortals",
        "roomSectors",
        "roomLights",
        "roomMeshes",
        "roomVertices",
        "roomQuads",
        "roomTriangles"
    };

    static uint32 gLevelArenaTagBytes[LAT_COUNT] = {};
    static uint32 gLevelArenaTagAllocs[LAT_COUNT] = {};

    static constexpr uint32 kLevelArenaChunkSize = 64 * 1024;
    static constexpr int    kLevelArenaMaxChunks = 64;

    struct LevelArenaChunk
    {
        uint8* data;
        uint32 used;
        uint32 capacity;
    };

    static LevelArenaChunk gLevelArena[kLevelArenaMaxChunks] = {};
    static int gLevelArenaCount = 0;

    static uint32 gLevelArenaBytesUsed = 0;
    static uint32 gLevelArenaBytesCommitted = 0;

    static void logLevelArenaBreakdown()
    {
        LOG("Level arena: chunks=%d used=%u committed=%u\n",
            gLevelArenaCount,
            gLevelArenaBytesUsed,
            gLevelArenaBytesCommitted);

        for (int i = 0; i < LAT_COUNT; i++)
        {
            if (gLevelArenaTagBytes[i] == 0)
                continue;

            LOG("  %-14s bytes=%u allocs=%u\n",
                kLevelAllocTagNames[i],
                gLevelArenaTagBytes[i],
                gLevelArenaTagAllocs[i]);
        }
    }

    static void resetLevelArena()
    {
        for (int i = 0; i < gLevelArenaCount; i++) {
            delete[] gLevelArena[i].data;
            gLevelArena[i].data = nullptr;
            gLevelArena[i].used = 0;
            gLevelArena[i].capacity = 0;
        }

        gLevelArenaCount = 0;
        gLevelArenaBytesUsed = 0;
        gLevelArenaBytesCommitted = 0;

        memset(gLevelArenaTagBytes, 0, sizeof(gLevelArenaTagBytes));
        memset(gLevelArenaTagAllocs, 0, sizeof(gLevelArenaTagAllocs));
    }

    static void* allocLevelBytes(uint32 size, uint32 align, LevelAllocTag tag)
    {
        if (!size)
            return nullptr;

        if (align < 1)
            align = 1;

        LevelArenaChunk* chunk = nullptr;

        if (gLevelArenaCount > 0) {
            chunk = &gLevelArena[gLevelArenaCount - 1];

            uintptr_t base = (uintptr_t)chunk->data;
            uintptr_t ptr = base + chunk->used;
            uintptr_t aligned = (ptr + (align - 1)) & ~(uintptr_t)(align - 1);
            uint32 padding = (uint32)(aligned - ptr);

            if (chunk->used + padding + size <= chunk->capacity) {
                chunk->used += padding;
                void* out = chunk->data + chunk->used;
                chunk->used += size;
                gLevelArenaBytesUsed += padding + size;
                gLevelArenaTagBytes[tag] += size;
                gLevelArenaTagAllocs[tag] += 1;
                return out;
            }
        }

        ASSERT(gLevelArenaCount < kLevelArenaMaxChunks);
        if (gLevelArenaCount >= kLevelArenaMaxChunks)
            return nullptr;

        uint32 capacity = kLevelArenaChunkSize;
        if (capacity < size + align)
            capacity = size + align;

        uint8* mem = new uint8[capacity];
        if (!mem)
            return nullptr;

        chunk = &gLevelArena[gLevelArenaCount++];
        chunk->data = mem;
        chunk->used = 0;
        chunk->capacity = capacity;

        gLevelArenaBytesCommitted += capacity;

        uintptr_t base = (uintptr_t)chunk->data;
        uintptr_t aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
        uint32 padding = (uint32)(aligned - base);

        chunk->used = padding;
        void* out = chunk->data + chunk->used;
        chunk->used += size;

        gLevelArenaBytesUsed += padding + size;
        gLevelArenaTagBytes[tag] += size;
        gLevelArenaTagAllocs[tag] += 1;
        return out;
    }

    static uint32 ptrOffset(const void* p)
    {
        return (uint32)(uintptr_t)p;
    }

    static void addOffset(uint32* offsets, int& count, uint32 value)
    {
        if (!value)
            return;

        for (int i = 0; i < count; i++) {
            if (offsets[i] == value)
                return;
        }

        offsets[count++] = value;
    }

    static void sortOffsets(uint32* offsets, int count)
    {
        for (int i = 0; i < count; i++) {
            for (int j = i + 1; j < count; j++) {
                if (offsets[j] < offsets[i]) {
                    uint32 t = offsets[i];
                    offsets[i] = offsets[j];
                    offsets[j] = t;
                }
            }
        }
    }

    static uint32 nextOffsetAfter(const uint32* offsets, int count, uint32 value, uint32 fileSize)
    {
        for (int i = 0; i < count; i++) {
            if (offsets[i] > value)
                return offsets[i];
        }
        return fileSize;
    }

    static bool readAt(ol::PkdStream& s, uint32 offset, void* dst, uint32 size)
    {
        if (!size)
            return true;

        return s.seek(offset) && s.read(dst, size);
    }

    static void* allocBytes(uint32 size, LevelAllocTag tag, uint32 align = 4)
    {
        return allocLevelBytes(size, align, tag);
    }

    template <typename T>
    static bool readArrayAt(ol::PkdStream& s, uint32 offset, uint32 count, T*& out, LevelAllocTag tag)
    {
        out = nullptr;

        const uint32 bytes = count * sizeof(*out);
        if (!bytes)
            return true;

        out = (T*)allocBytes(bytes, tag, alignof(T));
        if (!out)
            return false;

        return readAt(s, offset, (void*)out, bytes);
    }

    static bool readBlobAt(ol::PkdStream& s, uint32 offset, uint32 bytes, void*& out, LevelAllocTag tag)
    {
        out = nullptr;

        if (!bytes)
            return true;

        out = allocBytes(bytes, tag, 4);
        if (!out)
            return false;

        return readAt(s, offset, out, bytes);
    }

    static bool readBlobToPsramAt(ol::PkdStream& s, uint32 offset, uint32 bytes, uint32& psramAddr)
    {
        psramAddr = 0xFFFFFFFFu;

        if (bytes == 0) {
            return true;
        }

        const uint32 addr = ol::psram::alloc(bytes, 16);
        if (addr == 0xFFFFFFFFu) {
            LOG("readBlobToPsramAt: PSRAM alloc failed (offset=%u bytes=%u)\n", offset, bytes);
            return false;
        }

        static uint8 chunk[1024];

        uint32 done = 0;
        while (done < bytes) {
            uint32 want = bytes - done;
            if (want > sizeof(chunk)) {
                want = (uint32)sizeof(chunk);
            }

            if (!readAt(s, offset + done, chunk, want)) {
                LOG("readBlobToPsramAt: read failed (offset=%u done=%u want=%u)\n", offset, done, want);
                return false;
            }

            if (!ol::psram::write(addr + done, chunk, want)) {
                LOG("readBlobToPsramAt: PSRAM write failed (addr=%u done=%u want=%u)\n", addr, done, want);
                return false;
            }

            done += want;
        }

        psramAddr = addr;
        return true;
    }

    static void collectLevelOffsets(const Level& L, uint32* offsets, int& count)
    {
        count = 0;

        addOffset(offsets, count, ptrOffset(L.palette));
        addOffset(offsets, count, ptrOffset(L.lightmap));
        addOffset(offsets, count, ptrOffset(L.tiles));
        addOffset(offsets, count, ptrOffset(L.roomsInfo));
        addOffset(offsets, count, ptrOffset(L.floors));
        addOffset(offsets, count, ptrOffset(L.meshes));
        addOffset(offsets, count, ptrOffset(L.meshOffsets));
        addOffset(offsets, count, ptrOffset(L.anims));
        addOffset(offsets, count, ptrOffset(L.animStates));
        addOffset(offsets, count, ptrOffset(L.animRanges));
        addOffset(offsets, count, ptrOffset(L.animCommands));
        addOffset(offsets, count, ptrOffset(L.nodes));
        addOffset(offsets, count, ptrOffset(L.animFrames));
        addOffset(offsets, count, ptrOffset(L.models));
        addOffset(offsets, count, ptrOffset(L.staticMeshes));
        addOffset(offsets, count, ptrOffset(L.textures));
        addOffset(offsets, count, ptrOffset(L.sprites));
        addOffset(offsets, count, ptrOffset(L.spriteSequences));
        addOffset(offsets, count, ptrOffset(L.cameras));
        addOffset(offsets, count, ptrOffset(L.soundSources));
        addOffset(offsets, count, ptrOffset(L.boxes));
        addOffset(offsets, count, ptrOffset(L.overlaps));

        for (int a = 0; a < 2; a++) {
            for (int z = 0; z < ZONE_MAX; z++) {
                addOffset(offsets, count, ptrOffset(L.zones[a][z]));
            }
        }

        addOffset(offsets, count, ptrOffset(L.animTexData));
        addOffset(offsets, count, ptrOffset(L.itemsInfo));
        addOffset(offsets, count, ptrOffset(L.cameraFrames));
        addOffset(offsets, count, ptrOffset(L.soundMap));
        addOffset(offsets, count, ptrOffset(L.soundsInfo));
        addOffset(offsets, count, ptrOffset(L.soundData));
        addOffset(offsets, count, ptrOffset(L.soundOffsets));

        sortOffsets(offsets, count);
    }

    static bool readRoomHotData(ol::PkdStream& s, RoomInfo& info)
    {
        const RoomData disk = info.data;

        info.data.sprites = nullptr;
        info.data.portals = nullptr;
        info.data.sectors = nullptr;
        info.data.lights = nullptr;
        info.data.meshes = nullptr;
        info.data.vertices = nullptr;
        info.data.quads = nullptr;
        info.data.triangles = nullptr;

        if (info.spritesCount > 0) {
            if (!readArrayAt(s, ptrOffset(disk.sprites), info.spritesCount, info.data.sprites, LAT_RoomSprites))
                return false;
        }

        if (info.portalsCount > 0) {
            if (!readArrayAt(s, ptrOffset(disk.portals), info.portalsCount, info.data.portals, LAT_RoomPortals))
                return false;
        }

        {
            const uint32 sectorsCount = uint32(info.xSectors) * uint32(info.zSectors);
            if (sectorsCount > 0) {
                if (!readArrayAt(s, ptrOffset(disk.sectors), sectorsCount, info.data.sectors, LAT_RoomSectors))
                    return false;
            }
        }

        if (info.lightsCount > 0) {
            if (!readArrayAt(s, ptrOffset(disk.lights), info.lightsCount, info.data.lights, LAT_RoomLights))
                return false;
        }

        if (info.meshesCount > 0) {
            if (!readArrayAt(s, ptrOffset(disk.meshes), info.meshesCount, info.data.meshes, LAT_RoomMeshes))
                return false;
        }

        // Transitional: these still load to heap here, then osBuildRoomRenderPsram()
        // offloads them and invalidates the pointers afterward.
        if (info.verticesCount > 0) {
            if (!readArrayAt(s, ptrOffset(disk.vertices), info.verticesCount, info.data.vertices, LAT_RoomVertices))
                return false;
        }

        if (info.quadsCount > 0) {
            if (!readArrayAt(s, ptrOffset(disk.quads), info.quadsCount, info.data.quads, LAT_RoomQuads))
                return false;
        }

        if (info.trianglesCount > 0) {
            if (!readArrayAt(s, ptrOffset(disk.triangles), info.trianglesCount, info.data.triangles, LAT_RoomTriangles))
                return false;
        }

        return true;
    }

    static bool finalizeLevelAfterRead()
    {
        dynSectorsCount = 0;
        gAnimTexFrame = 0;

        memset(models, 0, sizeof(models));
        for (int32 i = 0; i < level.modelsCount; i++)
        {
            const Model* model = level.models + i;
            ASSERT(model->type < MAX_MODELS);
            models[model->type] = *model;
        }
        level.models = models;

        for (int32 i = 0; i < level.meshesCount; i++)
        {
            meshes[i] = (Mesh*)((uint8*)level.meshes + level.meshOffsets[i]);
        }
        level.meshes = meshes;

        memset(staticMeshes, 0, sizeof(staticMeshes));
        for (int32 i = 0; i < level.staticMeshesCount; i++)
        {
            const StaticMesh* staticMesh = level.staticMeshes + i;
            ASSERT(staticMesh->id < MAX_STATIC_MESHES);
            staticMeshes[staticMesh->id] = *staticMesh;
        }
        level.staticMeshes = staticMeshes;

        for (int32 i = 0; i < level.spriteSequencesCount; i++)
        {
            const SpriteSeq* spriteSeq = level.spriteSequences + i;

            if (spriteSeq->type >= TR1_ITEM_MAX)
                continue;

            Model* m = models + spriteSeq->type;
            m->count = int8(spriteSeq->count);
            m->start = spriteSeq->start;
        }

        return true;
    }

    static bool read_PKD_PicoCalc(ol::PkdStream& s, uint32 fileSize)
    {
        Level diskLevel;
        if (!readAt(s, 0, &diskLevel, sizeof(diskLevel)))
            return false;

        memset(&level, 0, sizeof(level));
        level = diskLevel;

        uint32 offsets[64];
        int offsetsCount = 0;
        collectLevelOffsets(diskLevel, offsets, offsetsCount);

        if (!readArrayAt(s, ptrOffset(diskLevel.palette), 256, level.palette, LAT_Palette))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.lightmap), 256 * 32, level.lightmap, LAT_Lightmap))
            return false;

        {
            const uint32 tilesBytes = uint32(level.tilesCount) * 256u * 256u;
            LOG("About to load tiles: count=%d bytes=%u\n", level.tilesCount, tilesBytes);
            gLevelTilesPsramAddr = 0xFFFFFFFFu;
            gLevelTilesPsramSize = 0;
            level.tiles = nullptr;

            if (tilesBytes > 0)
            {
                if (!readBlobToPsramAt(s, ptrOffset(diskLevel.tiles), tilesBytes, gLevelTilesPsramAddr))
                    return false;

                gLevelTilesPsramSize = tilesBytes;
            }
            LOG("Tiles load complete\n");
        }

        if (!readArrayAt(s, ptrOffset(diskLevel.roomsInfo), level.roomsCount, level.roomsInfo, LAT_RoomsInfo))
            return false;

        {
            const uint32 off = ptrOffset(diskLevel.floors);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.floors, LAT_Floors))
                return false;
        }

        {
            const uint32 off = ptrOffset(diskLevel.meshes);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.meshes, LAT_MeshesBlob))
                return false;
        }

        if (!readArrayAt(s, ptrOffset(diskLevel.meshOffsets), level.meshesCount, level.meshOffsets, LAT_MeshOffsets))
            return false;

        {
            const uint32 off = ptrOffset(diskLevel.anims);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.anims, LAT_Anims))
                return false;
        }

        {
            const uint32 off = ptrOffset(diskLevel.animStates);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.animStates, LAT_AnimStates))
                return false;
        }

        {
            const uint32 off = ptrOffset(diskLevel.animRanges);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.animRanges, LAT_AnimRanges))
                return false;
        }

        {
            const uint32 off = ptrOffset(diskLevel.animCommands);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.animCommands, LAT_AnimCommands))
                return false;
        }

        {
            const uint32 off = ptrOffset(diskLevel.nodes);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.nodes, LAT_Nodes))
                return false;
        }

        {
            const uint32 off = ptrOffset(diskLevel.animFrames);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.animFrames, LAT_AnimFrames))
                return false;
        }

        if (!readArrayAt(s, ptrOffset(diskLevel.models), level.modelsCount, level.models, LAT_Models))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.staticMeshes), level.staticMeshesCount, level.staticMeshes, LAT_StaticMeshes))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.textures), level.texturesCount, level.textures, LAT_Textures))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.sprites), level.spritesCount, level.sprites, LAT_Sprites))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.spriteSequences), level.spriteSequencesCount, level.spriteSequences, LAT_SpriteSequences))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.cameras), level.camerasCount, level.cameras, LAT_Cameras))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.soundSources), level.soundSourcesCount, level.soundSources, LAT_SoundSources))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.boxes), level.boxesCount, level.boxes, LAT_Boxes))
            return false;

        {
            const uint32 off = ptrOffset(diskLevel.overlaps);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.overlaps, LAT_Overlaps))
                return false;
        }

        for (int a = 0; a < 2; a++) {
            for (int z = 0; z < ZONE_MAX; z++) {
                const uint32 off = ptrOffset(diskLevel.zones[a][z]);
                const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
                const uint32 bytes = end - off;
                if (!readBlobAt(s, off, bytes, (void*&)level.zones[a][z], LAT_Zones))
                    return false;
            }
        }

        {
            const uint32 off = ptrOffset(diskLevel.animTexData);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.animTexData, LAT_AnimTexData))
                return false;
        }

        if (!readArrayAt(s, ptrOffset(diskLevel.itemsInfo), level.itemsCount, level.itemsInfo, LAT_ItemsInfo))
            return false;

        if (!readArrayAt(s, ptrOffset(diskLevel.cameraFrames), level.cameraFramesCount, level.cameraFrames, LAT_CameraFrames))
            return false;

        {
            const uint32 off = ptrOffset(diskLevel.soundMap);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.soundMap, LAT_SoundMap))
                return false;
        }

        {
            const uint32 off = ptrOffset(diskLevel.soundsInfo);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;
            if (!readBlobAt(s, off, bytes, (void*&)level.soundsInfo, LAT_SoundsInfo))
                return false;
        }

        {
            const uint32 off = ptrOffset(diskLevel.soundData);
            const uint32 end = nextOffsetAfter(offsets, offsetsCount, off, fileSize);
            const uint32 bytes = end - off;

            gLevelSoundPsramAddr = 0xFFFFFFFFu;
            gLevelSoundPsramSize = 0;
            level.soundData = nullptr;

            if (bytes > 0)
            {
                if (!readBlobToPsramAt(s, off, bytes, gLevelSoundPsramAddr))
                    return false;

                gLevelSoundPsramSize = bytes;
            }
        }

        if (!readArrayAt(s, ptrOffset(diskLevel.soundOffsets), level.soundOffsetsCount, level.soundOffsets, LAT_SoundOffsets))
            return false;

        for (int32 i = 0; i < level.roomsCount; i++)
        {
            Room* room = rooms + i;
            RoomInfo* info = const_cast<RoomInfo*>(level.roomsInfo) + i;

            if (!readRoomHotData(s, *info))
                return false;

            room->info = info;
            room->data = info->data;
            room->sectors = info->data.sectors;
            room->firstItem = NULL;
        }

#ifndef MODEHW
        gBrightness = -128;
        palSet(level.palette, gSettings.video_gamma << 4, gBrightness);
        memcpy(gLightmap, level.lightmap, sizeof(gLightmap));
#endif

#ifdef ROM_READ
        memcpy(textures, level.textures, level.texturesCount * sizeof(Texture));
        level.textures = textures;

        memcpy(sprites, level.sprites, level.spritesCount * sizeof(Sprite));
        level.sprites = sprites;

        memcpy(boxes, level.boxes, level.boxesCount * sizeof(Box));
        level.boxes = boxes;

        memcpy(cameras, level.cameras, level.camerasCount * sizeof(FixedCamera));
        level.cameras = cameras;
#endif

#ifdef __3DO__
        for (int32 i = 0; i < level.texturesCount; i++)
        {
            Texture* tex = level.textures + i;
            tex->data += intptr_t(RAM_TEX);
        }
#else
        for (int32 i = 0; i < level.texturesCount; i++) {
            level.textures[i].tile += gLevelTilesPsramAddr;
        }

        for (int32 i = 0; i < level.spritesCount; i++) {
            level.sprites[i].tile += gLevelTilesPsramAddr;
        }
#endif

        return finalizeLevelAfterRead();
    }

} // namespace

bool gameLoadLevelPicoCalc(LevelID id)
{
    drawLevelFree();
    resetLevelArena();
    osResetRoomRenderCache();
    osResetTileCache();

    gLevelSoundPsramAddr = 0xFFFFFFFFu;
    gLevelSoundPsramSize = 0;
    gLevelTilesPsramAddr = 0xFFFFFFFFu;
    gLevelTilesPsramSize = 0;

    memset(&gSaveGame, 0, sizeof(gSaveGame));
    memset(enemiesExtra, 0, sizeof(enemiesExtra));

    ItemObj::sFirstActive = NULL;
    ItemObj::sFirstFree = NULL;

    gCurTrack = -1;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "data/%s.PKD", (const char*)gLevelInfo[id].data);

    FILE* f = fopen(buf, "rb");
    if (!f) {
        LOG("Could not open \"%s\"\n", buf);
        return false;
    }

    fseek(f, 0, SEEK_END);
    const uint32 fileSize = (uint32)ftell(f);
    fseek(f, 0, SEEK_SET);

    ol::PkdStream s(f);
    const bool ok = read_PKD_PicoCalc(s, fileSize);
    fclose(f);

    if (!ok)
        return false;

    if (!osBuildRoomRenderPsram(&level)) {
        LOG("osBuildRoomRenderPsram failed\n");
        return false;
    }

    initLevelRuntime();

    logLevelArenaBreakdown();

    return true;
}