# OpenLara PicoCalc / Pico 2 Notes

## What this is

This fork is for exploring an OpenLara port to the ClockworkPi PicoCalc, with Pico 2 as the main target.

The current goal is not a full port straight away. The first job is to get TR2 running through a Windows software backend, use that to understand the render path and content requirements, and then reduce the game to something PicoCalc can realistically handle.

## Current status

So far:

- fork created from `XProger/OpenLara`
- working branch created: `pico2-picocalc`
- Windows build is up and running
- TR2 is loading and playing correctly
- software rendering path is now the main development path
- the local content folder was renamed from `TR1_PSX` to `TR2_PC` so it actually reflects what is in it

## General approach

The Windows build is now the main working environment for this phase.

The current direction is:

- use the Windows software backend to study rendering, textures, and special-case paths
- debug and iterate in Visual Studio
- figure out what TR2 actually needs in order to run and look acceptable
- reduce assets and runtime requirements there first
- bring the reduced design back to PicoCalc later

This is a better order than trying to solve platform bring-up, rendering, and content reduction on PicoCalc all at once.

## Why a fresh backend still makes sense

It still makes sense to keep PicoCalc-specific work isolated instead of forcing it into an existing backend too early.

That makes it easier to:

- keep the PicoCalc work separate
- see what the platform layer actually needs
- avoid dragging in assumptions from desktop or Linux code that do not help on Pico 2
- prototype one subsystem at a time

Existing backends like `sdl2`, `win`, `rpi`, and `sw` are still useful references, but they are references, not the end state.

## Why the Windows software backend matters

The Windows software backend is now doing more than just proving the backend compiles.

It is the easiest place to:

- inspect transforms, UVs, and texture behavior
- debug 2D faces and special-case quads
- understand dynamic RGBA texture use
- see what the engine is really submitting
- identify what assets and features can be reduced or removed

That makes it the right place to shape the eventual PicoCalc target.

## PSRAM note

PicoCalc includes external PSRAM, but it should not be treated like ordinary directly mapped system RAM.

It is not on the Pico 2 module's QSPI/XIP path, so it is better thought of as external storage with access overhead than as a transparent heap extension.

That means:

- hot runtime data should stay in internal SRAM
- PSRAM is a better fit for larger cold or semi-cold blobs
- code should not assume PSRAM-backed data can be used like ordinary pointer-friendly RAM without extra thought

This matters for renderer and asset design.

## Local content setup

The local testing folder currently in use is:

`bin/TR2_PC`

That folder contains local game data for testing only and must never be committed.

The outer folder name is just for organization. What matters to the engine is the file layout inside it.

For TR2 PC, the important part is that the engine sees things like:

- `data/ASSAULT.TR2`
- `data/TITLE.TR2`
- `data/MAIN.SFX`

Optional extras like FMV and music can sit alongside that content root in their expected folders.

## Asset policy

Do not commit retail game data.

That includes:

- level files
- sound banks
- FMV/video files
- music files
- installer leftovers
- any other files copied from the original game distribution

The repo should only contain source, notes, and build/configuration changes.

## Things confirmed so far

### Game version selection

OpenLara does not appear to use a simple build-time setting for which Tomb Raider game to run.

Instead, it looks at the files present in the content root and decides from there.

Examples from the source:

- TR1 PC: `DATA/GYM.PHD`
- TR1 PSX: `PSXDATA/GYM.PSX`
- TR2 PC: `data/ASSAULT.TR2`
- TR2 PSX: `DATA/ASSAULT.PSX`
- TR4 PC: `data/angkor1.tr4`

So naming a folder `TR2_PC` is just for clarity. The engine cares about the files inside it.

### Windows content lookup

On the Windows build, the game content being used is currently controlled by the Visual Studio debugger working directory.

## Files worth studying first

These are still the main files to keep coming back to while figuring out the platform boundary and renderer behavior:

- `src/platform/win/main.cpp`
- `src/platform/sdl2/main.cpp`
- `src/platform/rpi/main.cpp`
- `src/gapi/sw.h`
- `src/gapi/picocalc.h`
- `src/core.h`
- `src/gameflow.h`
- `src/format.h`

The goal is to understand what the engine expects from the platform layer in terms of:

- rendering
- input
- timing
- audio
- file/content access
- save data / cache paths

## Current priority

The current priority is not "full game on PicoCalc."

It is:

1. keep TR2 working in the Windows software backend
2. understand the texture and render paths
3. identify special cases that need their own handling
4. trim assets and runtime assumptions
5. define a reduced target that PicoCalc could plausibly support

## Near-term to-do list

1. Keep the Windows software backend stable enough to use as a test bed
2. Continue cleaning up `_GAPI_PICOCALC` without turning it into `_GAPI_SW`
3. Reduce assets and texture requirements where possible
4. Keep local game data out of the repo
5. Revisit PicoCalc bring-up once the reduced target is clearer

## Questions still open

- How much of TR2 can be simplified before it stops being worth doing?
- Which texture paths matter most for a playable reduced target?
- Which systems should be cut entirely for a first PicoCalc attempt?
- How much of the final renderer should stay slab-based and RGB565-first?
- What is the cleanest way to separate hot SRAM data from colder external-storage-style data?