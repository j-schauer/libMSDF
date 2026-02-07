# kitMSDF API

Single glyph MSDF generation via WASM.

## Quick Start

```typescript
import MSdfCoreFactory from './msdf-core.js';
import { MSDFGenerator } from './kitMSDF.js';

const msdf = await MSDFGenerator.init(MSdfCoreFactory);
msdf.loadFont(fontBytes);

const glyph = msdf.generateMTSDF(65, 64, 8); // 'A', 64px, 8px range
// glyph.metrics: { width, height, advance, planeBounds }
// glyph.pixels: Float32Array (RGBA)
```

## MSDFGenerator

### Static Methods

```typescript
MSDFGenerator.init(factory, wasmBinary?): Promise<MSDFGenerator>
```

### Instance Methods

```typescript
loadFont(fontBytes: Uint8Array): void
hasGlyph(charCode: number): boolean
generate(charCode, fontSize?, pixelRange?): MSDFGlyph | null      // 3-channel RGB
generateMTSDF(charCode, fontSize?, pixelRange?): MSDFGlyph | null // 4-channel RGBA
setVariationAxes(axes: VariationAxis[]): void
clearVariationAxes(): void
generateMTSDFVar(charCode, fontSize, pixelRange): MSDFGlyph | null
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

## WASM Exports

Low-level functions (use MSDFGenerator instead):

```c
_prepare_font_buffer(length) → ptr
_generate_glyph(fontLen, charCode, fontSize, pixelRange, metricsPtr)
_generate_mtsdf_glyph(...)
_generate_glyph_var(...)
_generate_mtsdf_glyph_var(...)
_has_glyph(fontLen, charCode) → 0 or 1
_clear_variation_axes()
_add_variation_axis(tag, value)
_free_buffers()
```
