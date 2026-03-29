# PicoCalc Porting Notes



## Goal



Get OpenLara far enough along on PicoCalc/Pico 2 to study the render path and build a custom renderer around it.



This is not meant to start as a full-featured port. The first milestone is a clean backend skeleton that compiles, runs, and tells us what the engine is trying to draw.



## Current direction



The current plan is:



- keep the Windows build working as a reference

- avoid pretending PicoCalc is the existing `_GAPI_SW` backend

- add a new graphics backend: `_GAPI_PICOCALC`

- add a new backend header: `src/gapi/picocalc.h`

- keep the first backend very small and instrumented

- design the real renderer around the platform constraints instead of forcing desktop assumptions onto the hardware



## Why not just use `_GAPI_SW`



The software backend is useful as a reference, but it is not the right identity for this project.



It currently brings along assumptions that are too specific:



- TR1 PC only in parts of the format path

- tile/palette-oriented texture handling

- full-frame software renderer assumptions

- a renderer structure that does not match the slabbed RGB565 SPI pipeline



So the plan is to borrow ideas from `sw.h`, not inherit it wholesale.



## Renderer assumptions carried over from previous PicoCalc work



The current mental model is heavily informed by the Geometry Vibes 3D renderer:



- display output is over SPI

- output format is RGB565

- Core0 handles game/update-side work and binning

- Core1 handles rasterization

- DMA pushes finished ping-pong slab buffers to the display

- the renderer should avoid needing a full-screen framebuffer if possible

- bounded memory use matters more than elegance



That general shape is likely to be much closer to the final OpenLara PicoCalc renderer than any GL/EGL-style backend.



## First backend milestone



The first backend should not try to fully render the game.



It only needs to prove that:



- the engine accepts a new GAPI backend

- the backend is initialized

- `beginFrame()` / `endFrame()` are called

- `DIP(mesh, range)` is called

- mesh/index data can be inspected

- primitive counts and rough screen-space information can be gathered



That is enough to start making informed renderer decisions.



## Texture direction



The current display/render pipeline is RGB565.



That does **not** mean the backend has to fully solve textures immediately, but it does suggest that the backend should eventually prefer a texture representation that can feed an RGB565-oriented renderer cleanly.



For the first pass:



- keep texture ownership CPU-side

- avoid hard-wiring the backend to the old SW tile/palette path

- allow the backend to store general texture metadata and raw data

- convert or reduce textures later when the renderer path is clearer



## Immediate backend shape



Planned first-pass backend pieces:



- `Shader`: mostly stubbed

- `Texture`: CPU-owned metadata + raw data pointer

- `Mesh`: CPU-owned index/vertex copies

- global backend state for viewport, clear color, matrices, and simple render state

- `DIP(mesh, range)`: transform/count/instrument only



## What `DIP()` should do first



Before worrying about rasterization, `DIP()` should:



1. read the mesh/range data

2. transform the needed vertices

3. decode triangles/quads from the index stream

4. count primitives

5. optionally compute basic screen-space bounds

6. store simple per-frame stats



That will tell us what the game is actually submitting and how expensive the scene is likely to be.



## What should wait until later



These should stay out of the first pass if possible:



- full textured rasterization

- lighting accuracy

- particles/effects

- environment rendering

- special blending paths

- dynamic texture policy

- audio

- save/load polish



## Likely future renderer shape



The final renderer will probably want to look more like this:



- game/update work on Core0

- transformed/binned primitive work on Core0

- slab rasterization on Core1

- DMA transfer of RGB565 slab buffers

- possibly ping-pong slab ownership between cores

- reduced or simplified render features compared with desktop OpenLara



## Questions to answer next



- What primitive types dominate the scene?

- How many vertices and indices are typically submitted per frame?

- Can we get away with flat shading or reduced texturing early on?

- Which render features can be disabled first without breaking playability?

- Where is the cleanest seam between OpenLara scene generation and custom rasterization?

- Do we need a depth buffer for the first visible milestone, or can we prototype with something simpler?



## Good near-term milestones



### Milestone 1

Backend compiles and links.



### Milestone 2

Backend initializes and logs frame/render submission statistics.



### Milestone 3

Backend produces some visible output:

- clear color

- wireframe

- flat-shaded triangles

- or another minimal proof of life



### Milestone 4

Start adapting primitive submission toward the slabbed RGB565 renderer model.

