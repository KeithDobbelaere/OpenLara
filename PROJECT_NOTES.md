# OpenLara PicoCalc / Pico 2 Notes

## What this is

This fork is for exploring an OpenLara port to the ClockworkPi PicoCalc, with the Pico 2 as the main target.

At the moment, the goal is not to jump straight into a full port. The first step is to understand how OpenLara is put together, keep a known-good desktop build working, and then carve out a small, clean platform stub for PicoCalc work.

## Current status

So far:

- fork created from `XProger/OpenLara`
- working branch created: `pico2-picocalc`
- Windows build is up and running
- TR2 is loading and playing correctly
- the local content folder was renamed from `TR1_PSX` to `TR2_PC` so it actually reflects what is in it

## General approach

The plan is to keep the Windows build around as a sanity check and reference point, but not to treat it as the eventual development target for PicoCalc.

The current direction is:

- use the Windows build to study startup, content loading, and platform boundaries
- inspect the existing platform backends
- create a fresh, minimal PicoCalc-oriented platform stub instead of trying to twist one of the existing ports into shape too early

That still leaves an open question: whether the existing `rpi` port turns out to contain useful ideas or code worth borrowing. That has not been investigated yet.

## Why a fresh stub still seems like the right direction

My instinct is to avoid making a mess by forcing the new work into an existing backend before I understand what that backend is really doing.

A fresh platform stub should make it easier to:

- keep the PicoCalc-specific work isolated
- see exactly what the platform layer needs to provide
- avoid dragging in assumptions from desktop or Linux code that may not help on Pico 2
- prototype one subsystem at a time

That said, the existing `sdl2` and `rpi` ports are still likely to be useful as references.

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
The full game, along with 1, 3, and 4 can be purchased from GOG for under $5.

## Things confirmed so far

### Game version selection

OpenLara does not appear to use a simple build-time setting for "which Tomb Raider game to run."

Instead, it looks at the files present in the content root and decides from there.

Examples from the source:

- TR1 PC: `DATA/GYM.PHD`
- TR1 PSX: `PSXDATA/GYM.PSX`
- TR2 PC: `data/ASSAULT.TR2`
- TR2 PSX: `DATA/ASSAULT.PSX`
- TR4 PC: `data/angkor1.tr4`

So naming a folder `TR2_PC` is just for my own clarity. The engine cares about the files inside it.

### Windows content lookup

On the Windows build, the game content being used is currently controlled by the Visual Studio debugger working directory.

## Files worth studying first

These are the main files to keep coming back to while figuring out the platform boundary:

- `src/platform/win/main.cpp`
- `src/platform/sdl2/main.cpp`
- `src/platform/rpi/main.cpp`
- `src/core.h`
- `src/gameflow.h`
- `src/format.h`

The goal is to figure out what the engine expects from the platform layer in terms of:

- rendering
- input
- timing
- audio
- file/content access
- save data / cache paths

## Near-term to-do list

1. Clean up ignore rules so local game data never gets pushed
2. Commit the repo hygiene changes before starting real port work
3. Read through the existing platform implementations, especially `rpi`
4. Decide what the smallest possible PicoCalc platform stub should look like
5. Start with a minimal bring-up target rather than "full game on hardware"

## Questions still open

- How useful is the existing `rpi` backend going to be?
- What is the smallest possible platform layer OpenLara can run on?
- What parts of the engine are likely to be too heavy for a first PicoCalc attempt?
- Is a no-audio / reduced-rendering prototype the right first milestone?
- What should the PicoCalc content layout look like on-device?