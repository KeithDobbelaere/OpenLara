#ifndef H_GAME
#define H_GAME

#include "common.h"
#include "room.h"
#include "camera.h"
#include "item.h"
#include "draw.h"
#include "nav.h"
#include "level.h"
#include "inventory.h"

EWRAM_DATA LevelID gNextLevel = LVL_MAX;

void nextLevel(LevelID next)
{
    if ((next == LVL_TR1_3A) && (inventory.state == INV_STATE_NONE)) // alpha version
    {
        inventory.open(players[0], INV_PAGE_END);
        return;
    }
    gNextLevel = next;
}

bool gameSave()
{
    gSaveGame.version = SAVEGAME_VER;
    gSaveGame.level = gLevelID;
    gSaveGame.track = gCurTrack;
    gSaveGame.randSeedLogic = gRandSeedLogic;
    gSaveGame.randSeedDraw = gRandSeedDraw;

    memset(gSaveGame.invSlots, 0, sizeof(gSaveGame.invSlots));
    memcpy(gSaveGame.invSlots, inventory.counts, sizeof(inventory.counts));

    uint8* ptr = gSaveData;
    ItemObj* item = items;
    for (int32 i = 0; i < level.itemsCount; i++, item++)
    {
        ptr = item->save(ptr);
    }
    gSaveGame.dataSize = ptr - gSaveData;

    return osSaveGame();
}

bool gameLoad()
{
    if (!osLoadGame())
    {
        if (gSaveGame.dataSize == 0)
            return false;
    }

    SaveGame tmp = gSaveGame;
    gLevelID = (LevelID)gSaveGame.level;
    startLevel(gLevelID);
    gSaveGame = tmp;

    inventory.setSlots(gSaveGame.invSlots);

    ItemObj::sFirstActive = NULL;
    ItemObj::sFirstFree = items + level.itemsCount;

    uint8* ptr = gSaveData;
    ItemObj* item = items;
    for (int32 i = 0; i < level.itemsCount; i++, item++)
    {
        ptr = item->load(ptr);

        if (item->flags & ITEM_FLAG_ACTIVE) {
            item->activate();
        }
    }

    if (gSaveGame.track != -1) {
        sndPlayTrack(gSaveGame.track);
    }

    gRandSeedLogic = gSaveGame.randSeedLogic;
    gRandSeedDraw = gSaveGame.randSeedDraw;

    return true;
}

void gameInit()
{
    drawInit();

    gSaveGame.dataSize = 0;

    gSettings.version = SETTINGS_VER;
    gSettings.controls_vibration = 1;
    gSettings.controls_swap = 0;
    gSettings.audio_sfx = 1;
    gSettings.audio_music = 1;
    gSettings.video_gamma = 0;
    gSettings.video_fps = 1;
    gSettings.video_vsync = 0;
    osLoadSettings();

    inventory.init();

    startLevel(gLevelID);
}

void gameFree()
{
    drawLevelFree();
    drawFree();
}

void resetLara(int32 index, int32 roomIndex, const vec3i &pos, int32 angleY)
{
    Lara* lara = players[index];

    lara->room->remove(lara);

    lara->pos = pos;
    lara->angle.y = angleY;
    lara->health = LARA_MAX_HEALTH;

    lara->extraL->camera.target.pos = lara->pos;
    lara->extraL->camera.target.room = lara->room;
    lara->extraL->camera.view = lara->extraL->camera.target;

    rooms[roomIndex].add(lara);
}

void initLevelRuntime()
{
    // prepare rooms
    for (int32 i = 0; i < level.roomsCount; i++)
    {
        rooms[i].reset();
    }

    // prepare items free list
    items[MAX_ITEMS - 1].nextItem = NULL;
    for (int32 i = MAX_ITEMS - 2; i >= level.itemsCount; i--)
    {
        items[i].nextItem = items + i + 1;
    }
    ItemObj::sFirstFree = items + level.itemsCount;

    for (int32 i = 0; i < MAX_PLAYERS; i++)
    {
        players[i] = NULL;
    }

    if (gLevelID == LVL_TR1_TITLE) {
        // init dummy Lara for updateInput()
        items->extraL = playersExtra;
        items->extraL->camera.mode = CAMERA_MODE_FOLLOW;
        inventory.open(items, INV_PAGE_TITLE);
    }
    else {
        inventory.page = INV_PAGE_MAIN;

        // init items
        for (int32 i = 0; i < level.itemsCount; i++)
        {
            const ItemObjInfo* info = level.itemsInfo + i;
            ItemObj* item = items + i;

            item->type = info->type;
            item->intensity = uint8(info->intensity);

            item->pos.x = info->pos.x + (rooms[info->roomIndex].info->x << 8);
            item->pos.y = info->pos.y;
            item->pos.z = info->pos.z + (rooms[info->roomIndex].info->z << 8);

            item->angle.y = ((info->flags >> 14) - 2) * ANGLE_90;
            item->flags = info->flags;

            if (item->type == ITEM_LARA) {
                players[0] = (Lara*)item;
            }

            item->init(rooms + info->roomIndex);
        }

        if (isCutsceneLevel())
        {
            gCinematicCamera.initCinematic();
        }
    }

    drawLevelInit();
}

void gameLoadLevel(const void* data)
{
    drawLevelFree();

    memset(&gSaveGame, 0, sizeof(gSaveGame));
    memset(enemiesExtra, 0, sizeof(enemiesExtra));

    ItemObj::sFirstActive = NULL;
    ItemObj::sFirstFree = NULL;

    gCurTrack = -1;

    readLevel((uint8*)data);
    initLevelRuntime();
}

void startLevel(LevelID id)
{
    gRandSeedLogic = osGetSystemTimeMS() * 3;
    gRandSeedDraw = osGetSystemTimeMS() * 7;

    sndStop();
    sndFreeSamples();

#if defined(__PICOCALC__) || defined(__PICOCALC_WIN__)
    if (!gameLoadLevelPicoCalc(id)) {
        LOG("gameLoadLevelPicoCalc failed");
        return;
    }
#else
    const void* data = osLoadLevel(id);
    gameLoadLevel(data);
#endif

    sndInitSamples();
    sndPlayTrack(getAmbientTrack());
}

void updateItems()
{
    ItemObj* item = ItemObj::sFirstActive;
    while (item)
    {
        ItemObj* next = item->nextActive;
        item->update();
        item = next;
    }

    if (isCutsceneLevel())
    {
        gCinematicCamera.updateCinematic();
    }
    else
    {
        for (int32 i = 0; i < MAX_PLAYERS; i++)
        {
            if (players[i]) {
                players[i]->update();
            }
        }
    }
}

void gameUpdate(int32 frames)
{
    PROFILE(CNT_UPDATE);

    if (frames > MAX_UPDATE_FRAMES) {
        frames = MAX_UPDATE_FRAMES;
    }

    if (!sndTrackIsPlaying()) {
        gCurTrack = -1;
        sndPlayTrack(getAmbientTrack());
    }

    if (inventory.state != INV_STATE_NONE)
    {
        Lara* lara = (Lara*)inventory.lara;
        ASSERT(lara);
        lara->updateInput();
        inventory.update(frames);

        if ((inventory.page != INV_PAGE_TITLE) && (inventory.state == INV_STATE_NONE))
        {
            if (lara->useItem(inventory.useSlot)) {
                inventory.useSlot = SLOT_MAX;
            }
        }
    }

    if ((inventory.page != INV_PAGE_TITLE) && (inventory.state == INV_STATE_NONE) && (gNextLevel == LVL_MAX))
    {
        for (int32 i = 0; i < frames; i++)
        {
            updateItems();
        }
        updateLevel(frames);
    }

    if ((gNextLevel != LVL_MAX) && (inventory.state == INV_STATE_NONE))
    {
        gLevelID = gNextLevel;
        gNextLevel = LVL_MAX;
        if (gLevelID == LVL_LOAD) {
            gameLoad();
        } else {
            startLevel(gLevelID);
        }
        gameUpdate(1);
    }
}

void gameRender()
{
    {
        PROFILE(CNT_RENDER);

        setViewport(RectMinMax(0, 0, FRAME_WIDTH, FRAME_HEIGHT));

        if (inventory.state == INV_STATE_NONE)
        {
            clear();

            for (int32 i = 0; i < MAX_PLAYERS; i++)
            {
                if (isCutsceneLevel()) {
                    drawCinematicRooms();
                }
                else if (players[i]) {
                    drawRooms(&players[i]->extraL->camera);
                }

                if (players[i])
                {
                    drawHUD(players[i]);
                }
            }
        } else {
            inventory.draw();
        }

        //if (inventory.state == INV_STATE_NONE)
        {
            drawFPS();
        }

        flush();
    }

#ifdef PROFILING
    drawProfiling();
    #ifndef PROFILE_SOUNDTIME
        PROFILE_CLEAR();
    #endif
#endif
}

#endif
