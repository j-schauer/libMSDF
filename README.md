# libMSDF

Single-glyph MSDF generation via WebAssembly.

## What is MSDF?

MSDF (Multi-channel Signed Distance Field) is a technique for rendering resolution-independent text and shapes on the GPU. A standard SDF stores the distance from each pixel to the nearest edge of a glyph. This works well for smooth shapes but loses sharp corners. MSDF solves this by encoding distance information across three color channels (R, G, B), each representing distance to a different edge segment. A fragment shader takes the median of the three channels to reconstruct the shape with sharp corners preserved at any zoom level.

MTSDF (Multi-channel Signed Distance Field + True SDF) adds a fourth channel (alpha) containing a conventional single-channel SDF. The shader uses `min(median(rgb), alpha)` to get the benefits of MSDF corner accuracy with SDF's reliable interior coverage. MTSDF is generally preferred for text rendering.

Both MSDF and MTSDF require a specialized fragment shader to render. The raw texture looks like colored noise -- the shader interprets the distance fields to produce crisp edges. The shader source is included in `shader.js`.

## Upstream: msdfgen

The core math comes from Viktor Chlumsky's [msdfgen](https://github.com/Chlumsky/msdfgen) C++ library (MIT license). msdfgen implements the algorithms described in Chlumsky's master's thesis on multi-channel distance field generation. It handles glyph shape loading via FreeType, edge coloring (assigning channels to edge segments), and the actual distance field rasterization.

The C++ source lives in `vendor/msdf-atlas-gen/` as a git submodule. We use only the `msdfgen/core/` (math, rasterization) and `msdfgen/ext/` (FreeType font loading) directories. The atlas-packing, PNG export, and CLI components of msdf-atlas-gen are not used.

Our C++ binding layer (`src/wasm/core.h`, `src/wasm/wasm_binding.cpp`) provides a minimal Emscripten interface: load font bytes, generate one glyph at a time, return metrics + pixel data. The C++ is compiled to WASM via Emscripten with `-sUSE_FREETYPE=1` (Emscripten's FreeType port). The same C++ core could be compiled natively for macOS, Linux, or Windows -- WASM is the current target.

## API Overview

The library takes a TTF or OTF font file as raw bytes and generates MSDF/MTSDF bitmaps for individual Unicode codepoints. The main calls:

- `MSDFGenerator.init(modulePath)` -- async, loads the WASM module
- `loadFont(fontBytes)` -- load a TTF/OTF file into WASM memory
- `hasGlyph(charCode)` -- check if a codepoint exists in the font
- `generate(charCode, fontSize, pixelRange)` -- produce a 3-channel MSDF bitmap
- `generateMTSDF(charCode, fontSize, pixelRange)` -- produce a 4-channel MTSDF bitmap
- `setVariationAxes(axes)` / `clearVariationAxes()` -- configure variable font axes
- `generateVar()` / `generateMTSDFVar()` -- generate with current variation axes
- `dispose()` -- free WASM memory

Variable fonts are supported via FreeType's variation axis API. Standard axes: `wght` (weight), `wdth` (width), `opsz` (optical size), `ital` (italic), `slnt` (slant). Set axes before calling the `Var` generation methods.

See `api.md` for complete usage examples and output format.

## Output Format

Each generated glyph returns:

- **pixels**: `Float32Array` of raw float channel data. 3 floats per pixel for MSDF (RGB), 4 for MTSDF (RGBA). Values are distance field samples, typically in the 0.0-1.0 range with 0.5 representing the glyph edge. Layout is row-major, bottom-to-top.
- **width / height**: Bitmap dimensions in pixels. Includes padding from the pixel range parameter.
- **advance**: Horizontal advance width in pixels (distance to move the cursor after this glyph).
- **planeBounds** `{l, b, r, t}`: Glyph bounding box in pixels relative to the pen position (origin). `l` = left edge, `b` = bottom edge (typically negative, below baseline), `r` = right edge, `t` = top edge (above baseline). These are the physical bounds of the glyph shape scaled to the requested fontSize.

To position a glyph for rendering: place the bitmap at `(penX + planeBounds.l, baseline - planeBounds.t)`, draw the quad at `width x height` pixels, then advance the pen by `advance` pixels.

## Shader Test Harness

`example/glyph/index.html` is the gold standard for verifying that the MSDF rendering pipeline works end-to-end. It renders individual glyphs from pre-built atlas textures using the shaders from `shader.js` across three rendering backends: WebGL2, raw WebGPU, and Pixi.js v8 WebGPU.

This test harness must be kept working and consistent with the MSDF generation parameters. If the generation algorithm, pixel range, or distance field encoding changes, the shader must be updated to match. The harness provides interactive controls for smoothing, weight, outline, glow, blur, and debug visualization modes.

Run: `lulu run example`, open `http://localhost:8000/example/glyph/`

## Build

```bash
lulu build _dist      # compile WASM, bundle JS, generate types
lulu run tests        # build + run Node.js test suite
lulu run example      # start http-server for shader test harness
```

Requires: Emscripten (em++), Node.js, npm.

## Output (dist/)

- `libMSDF.js` -- ESM bundle (Emscripten glue + TypeScript wrapper, ~97KB)
- `libMSDF.wasm` -- compiled WASM module (~690KB)
- `libMSDF.d.ts` -- TypeScript declarations
- `shader.js` -- MSDF fragment/vertex shaders (GLSL for WebGL2, WGSL for WebGPU, Pixi v8 compatible)
- `api.md` -- API reference

## Project Layout

- `src/` -- TypeScript wrapper (`msdf-generator.ts`, `index.ts`) and shader source (`shader.js`)
- `src/wasm/` -- C++ Emscripten binding (`wasm_binding.cpp`, `core.h`)
- `vendor/msdf-atlas-gen/` -- upstream msdfgen C++ (git submodule, see `vendor/PROVENANCE.md`)
- `example/glyph/` -- shader test harness (WebGL2 / WebGPU / Pixi)
- `example/assets/` -- pre-built MSDF and MTSDF atlas textures for the test harness
- `tests/` -- Node.js test suite
- `assets/` -- test fonts (Poppins-Regular, Inter variable font)
- `archive/` -- native macOS static libraries (libfreetype, libpng, libz, libbz2) for potential future native compilation target. Not used in WASM builds. See `archive/PROVENANCE.md`.
