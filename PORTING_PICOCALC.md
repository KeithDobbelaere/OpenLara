# PicoCalc Porting Notes

## Goal

Use a Windows software backend to get TR2 running well enough to understand the render path, texture usage, and asset requirements before trying to force the full problem onto PicoCalc.

The point of this phase is not to finish a PicoCalc port. It is to figure out what has to be cut, simplified, or rewritten so PicoCalc has a chance.

## Current direction

The current workflow is:

- keep working in Windows
- use Visual Studio for editing and debugging
- keep `_GAPI_PICOCALC` as its own backend
- use the software backend to study what TR2 is actually doing
- reduce assets and runtime requirements there first
- bring that reduced design back to PicoCalc later

## Why this approach

Trying to do platform bring-up, renderer work, asset handling, and memory reduction on PicoCalc at the same time was the wrong order.

Windows is the easier place to answer the important questions:

- what the engine is drawing
- what texture paths are in use
- what special cases exist
- what assets are too large
- what can be reduced or removed

That gives us something concrete to aim for on PicoCalc instead of guessing.

## Why not just use `_GAPI_SW`

`_GAPI_SW` is still a useful reference, but it is not the right long-term fit for this project.

It carries assumptions that do not line up cleanly with the eventual PicoCalc renderer, especially around texture handling and general software-renderer structure.

The plan is to borrow what is useful from `sw.h`, not turn PicoCalc into `_GAPI_SW`.

## What the software backend is for now

The Windows software backend is now the main test bed for:

- TR2 render behavior
- texture handling
- UV conventions
- 2D face / UI-style paths
- dynamic RGBA texture updates
- general asset and memory pressure

This is where we figure out what the game actually needs in order to look acceptable.

## Visual Studio’s role

Visual Studio is the practical place to do this work right now.

It makes it much easier to:

- inspect matrices, UVs, and textures
- compare backend revisions
- catch crashes in transform, texture, and raster code
- step through special cases quickly

That matters more at this stage than trying to keep everything PicoCalc-first.

## Immediate objective

The goal right now is to get TR2 rendering through `_GAPI_PICOCALC` on Windows well enough to study it.

That means:

- visible geometry
- enough texture support to understand the content path
- enough special-case handling for problem areas like 2D faces and RGBA-backed textures
- enough stability to use the build as a reduction tool

## Asset reduction strategy

The software backend is now also a way to decide what the final PicoCalc target can afford.

Likely reduction work includes:

- downscaling textures
- converting texture data into formats friendlier to RGB565 output
- avoiding retention of large source blobs where possible
- simplifying or removing expensive texture paths
- cutting nonessential content and features
- reducing memory duplication during load and update

That work is easier to reason about on Windows first.

## Long-term renderer direction

The final PicoCalc renderer will still likely look more like the Geometry Vibes 3D renderer than any desktop backend:

- SPI output
- RGB565
- tight memory limits
- slabbed or otherwise bounded rendering
- Core0/Core1 split where useful
- DMA-driven display transfer

That has not changed.

What has changed is the order: first understand and reduce the content on Windows, then shape the PicoCalc renderer around that reduced target.

## What should wait

For now, these can wait:

- polished on-device renderer integration
- full feature parity
- accurate effects
- audio polish
- save/load polish
- final PicoCalc display path

The current phase is about understanding the game’s needs and shrinking them.

## Near-term milestones

### Milestone 1
TR2 runs through `_GAPI_PICOCALC` in a Windows software backend.

### Milestone 2
Major scene types are visible and debuggable.

### Milestone 3
Texture paths and special cases are understood well enough to classify cleanly.

### Milestone 4
Start reducing assets and runtime requirements in the Windows build.

### Milestone 5
Define a reduced target that PicoCalc could plausibly run.

### Milestone 6
Move that reduced design back toward a real PicoCalc renderer.

## PSRAM note

The PicoCalc board includes external PSRAM, but it is not arranged like normal directly mapped system RAM (why???!).

It is not sitting on the Pico 2 module's QSPI/XIP path, so we should not assume it can be treated like ordinary pointer-addressable memory. In practice, this means PSRAM is better thought of as external storage with access overhead, not as a transparent heap extension.

That has two immediate design consequences:

- hot runtime data should stay in internal SRAM
- PSRAM-resident data will likely need an access wrapper or staging layer rather than raw pointer-style use

For large blob-like resources, a wrapper with indexed access, windowing, or explicit read/write helpers is probably the right direction. Pointer-rich engine structures should not be the first target for PSRAM.

## Bottom line

The current strategy is simple:

- get TR2 running in software on Windows
- use Visual Studio to understand it
- reduce the game to something PicoCalc can realistically handle
- then build the PicoCalc version around that reduced target