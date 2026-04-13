# PicoCalc Porting Notes

## Goal

I’m porting the original GBA version of Tomb Raider I to PicoCalc.

I’ve moved away from trying to study and shrink TR2 through a custom Windows-first path. That was turning into a much bigger engine project than I actually wanted. The GBA port is already much closer to PicoCalc’s limits, so it makes more sense as the base.

## Current direction

Right now, the plan is simple:

- use the original GBA TR1 port as the base
- keep PicoCalc as the real target
- fix correctness first
- solve memory problems in a practical way
- optimize after the game is actually working

Windows is still useful when I need it, but it is no longer the center of the project.

## Why this changed

Trying to force a larger engine down onto PicoCalc was the wrong fight.

The GBA port already lives in a smaller world:
- simpler assumptions
- tighter rendering constraints
- more realistic content scale

That makes it a much better fit for PicoCalc.

## What matters right now

My priorities are:

- correct title and menu rendering
- correct palette and texture handling
- stable file loading
- sensible PSRAM usage
- getting gameplay scenes running correctly on-device

## PSRAM note

PSRAM is useful, but the way it is wired on this hardware is kind of torturous and backward.

It is not set up like normal RAM that the system can just treat as a natural extension of memory. In practice, it behaves much more like awkward external storage than true general-purpose RAM.

That means I should treat it carefully:
- hot runtime data stays in SRAM
- large blobs can live in PSRAM
- access should go through explicit helpers
- caching or staging is often the right answer

It helps, but it comes with real friction, and the hardware does not make it pleasant.

## Rendering direction

The renderer should stay small and practical:

- SPI display output
- RGB565 final image
- tight memory use
- bounded, predictable work
- no unnecessary complexity

The point is to make the GBA port fit PicoCalc well, not to grow it into a different engine.

## What can wait

For now, I can ignore:

- big renderer redesigns
- polish that does not help bring-up
- speculative optimization
- anything that distracts from getting the game running correctly

## Near-term milestones

### Milestone 1
Title and menu paths render correctly on PicoCalc.

### Milestone 2
Gameplay scenes load and display correctly.

### Milestone 3
Asset loading is stable, with PSRAM used where it makes sense.

### Milestone 4
The biggest bottlenecks are identified.

### Milestone 5
The game becomes meaningfully playable (Not sure this is possible at this point with the PSRAM limitations).

## Bottom line

The plan now is:

- port the original GBA TR1 version
- keep PicoCalc as the real target
- get correctness first
- use PSRAM carefully
- optimize once the port is solid

## Building and running

### PicoCalc build and run

For the real target, I build the PicoCalc version in VS Code.

General flow:

1. Open the project in VS Code.
2. Build from VS Code.
2. Connect the Pico 2 via micro USB.
4. Press the **Bootload** button on the Pico 2 so it mounts as a UF2 device.
5. Copy the built UF2 to it.
6. Make sure the required `data/` files are present on the PicoCalc SD card.
7. Boot the device and watch serial output for loader, PSRAM, and rendering issues via USB-C.

### Windows build and run

Windows is the easier place to inspect behavior quickly. The PSRAM object will emulate real-world latencies.

For Windows, I build and run through Visual Studio.

General flow:

1. Open the solution in Visual Studio.
2. Make sure you are building for x86.
3. Build and run from Visual Studio.
4. Make sure the required `data/` files are available to the executable.

### Notes

A few things matter in both environments:

- the `data/` folder needs to contain the files the game expects
- if something works in Windows but fails on PicoCalc, you should first suspect timing, memory, display, or I/O differences