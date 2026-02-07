# libMSDF API

Single glyph MSDF generation via WASM.

## Quick Start

```typescript
import { initMSDF } from './libMSDF.js';

const msdf = await initMSDF('./libMSDF.wasm');
msdf.loadFont(fontBytes);

const glyph = msdf.generateMTSDF(65, 64, 8); // 'A', 64px, 8px range
// glyph.metrics: { width, height, advance, planeBounds }
// glyph.pixels: Float32Array (RGBA)
```

## initMSDF

```typescript
initMSDF(wasmPath: string): Promise<MSDFGenerator>
```

Loads the WASM module and returns a ready generator. The path is a URL in browser or file path in Node/Deno.

## MSDFGenerator Methods

```typescript
loadFont(fontBytes: Uint8Array): void
hasGlyph(charCode: number): boolean
generate(charCode, fontSize?, pixelRange?): MSDFGlyph | null      // 3-channel RGB
generateMTSDF(charCode, fontSize?, pixelRange?): MSDFGlyph | null // 4-channel RGBA
setVariationAxes(axes: VariationAxis[]): void
clearVariationAxes(): void
generateVar(charCode, fontSize?, pixelRange?): MSDFGlyph | null
generateMTSDFVar(charCode, fontSize?, pixelRange?): MSDFGlyph | null
dispose(): void
```

## Types

```typescript
interface MSDFGlyph {
    metrics: MSDFMetrics;
    pixels: Float32Array;
}

interface MSDFMetrics {
    width: number;
    height: number;
    advance: number;
    planeBounds: { l: number; b: number; r: number; t: number };
}

interface VariationAxis {
    tag: string;   // "wght", "wdth", "opsz", "ital", "slnt"
    value: number;
}
```

## WASM Exports (low-level)

Use MSDFGenerator instead. These are the raw C functions:

```c
_prepare_font_buffer(length) -> ptr
_generate_glyph(fontLen, charCode, fontSize, pixelRange, metricsPtr)
_generate_mtsdf_glyph(...)
_generate_glyph_var(...)
_generate_mtsdf_glyph_var(...)
_has_glyph(fontLen, charCode) -> 0 or 1
_clear_variation_axes()
_add_variation_axis(tag, value)
_free_buffers()
```
