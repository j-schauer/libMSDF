# libMSDF

Single glyph MSDF generation via WASM. Wraps Chlumsky's msdfgen C++ library compiled to WebAssembly via Emscripten. Works in Node, Browser, and Deno.

## Build

```bash
lulu build release
lulu run test
```

## Output (dist/)

- `libMSDF.js` / `libMSDF.wasm` - WASM module + JS wrapper (single ESM bundle)
- `libMSDF.d.ts` - TypeScript declarations
- `shader.js` - MSDF rendering shaders (GLSL + WGSL, Pixi v8 compatible)
- `api.md` - API documentation

## Usage

```typescript
import { initMSDF } from './libMSDF.js';

const msdf = await initMSDF('./libMSDF.wasm');
msdf.loadFont(fontBytes);
const glyph = msdf.generateMTSDF(65, 64, 8);
```

## Key Directories

- `src/` - TypeScript wrapper + C++ WASM binding
- `src/wasm/` - C++ core (wasm_binding.cpp, core.h)
- `submodule/msdf-atlas-gen/` - Upstream msdfgen (git submodule)
- `example/glyph/` - Shader test harness (WebGL2 / WebGPU / Pixi) -- visually verifies shader rendering
- `archive/` - Native macOS static libraries (for potential future native build)
