# libMSDF API

Single-glyph MSDF generation via WASM. Generates MSDF (3-channel) or MTSDF (4-channel) distance field bitmaps from TTF/OTF font files.

## Quick Start

```typescript
import { initMSDF } from './libMSDF.js';

const msdf = await initMSDF('./libMSDF.wasm');

// Load a font (raw TTF/OTF bytes)
const fontBytes = new Uint8Array(fs.readFileSync('MyFont.ttf'));
msdf.loadFont(fontBytes);

// Generate a 4-channel MTSDF glyph for 'A' at 64px with 8px distance range
const glyph = msdf.generateMTSDF(65, 64, 8);
console.log(glyph.metrics);
// { width: 52, height: 55, advance: 42.5,
//   planeBounds: { l: -2.1, b: -3.8, r: 40.3, t: 47.2 },
//   atlasBounds: { l: 0, b: 0 } }
console.log(glyph.pixels.length); // 52 * 55 * 4 = 11440
```

## Initialization

### initMSDF(wasmPath)

```typescript
async function initMSDF(wasmPath: string): Promise<MSDFGenerator>
```

Loads the WASM module and returns a ready `MSDFGenerator`. Call once at startup.

- **wasmPath**: Path to `libMSDF.wasm`. In browser, this is a URL relative to the page. In Node/Deno, a file system path. Emscripten handles the platform-specific loading internally.

```typescript
// Browser
const msdf = await initMSDF('/assets/libMSDF.wasm');

// Node.js
const msdf = await initMSDF('./dist/libMSDF.wasm');

// Node.js with absolute path
import path from 'path';
const msdf = await initMSDF(path.join(__dirname, 'libMSDF.wasm'));
```

## Font Loading

### loadFont(fontBytes)

```typescript
loadFont(fontBytes: Uint8Array): void
```

Loads a TTF or OTF font into WASM memory. Must be called before any generation. Can be called again to switch fonts.

```typescript
// Node.js
const fontBytes = new Uint8Array(fs.readFileSync('Poppins-Regular.ttf'));
msdf.loadFont(fontBytes);

// Browser
const response = await fetch('/fonts/Poppins-Regular.ttf');
const fontBytes = new Uint8Array(await response.arrayBuffer());
msdf.loadFont(fontBytes);
```

### hasGlyph(charCode)

```typescript
hasGlyph(charCode: number): boolean
```

Check if a Unicode codepoint exists in the loaded font without generating a bitmap. Returns `false` for missing glyphs (glyph index 0).

```typescript
msdf.hasGlyph(65);      // true  -- 'A'
msdf.hasGlyph(0x00E9);  // true  -- 'e' with acute (if in font)
msdf.hasGlyph(0x1F600); // false -- emoji (not in most text fonts)
```

## Generation

All generation methods are synchronous. They return `MSDFGlyph | null`. Returns `null` if the glyph cannot be generated (missing glyph, empty shape).

Parameters:
- **charCode**: Unicode codepoint (e.g., 65 for 'A', 0x4E16 for CJK)
- **fontSize**: Target size in pixels. Controls the scale of the output bitmap. Default: 32.
- **pixelRange**: MSDF distance range in pixels. The distance field extends this many pixels beyond the glyph edge. Larger values give the shader more room for effects (outlines, glow) but produce larger bitmaps. Default: 4.0. Typical values: 4-8.

### generate(charCode, fontSize?, pixelRange?)

Generates a 3-channel MSDF bitmap (RGB). Each pixel is 3 floats.

```typescript
const glyph = msdf.generate(65, 64, 8); // 'A', 64px, 8px range
if (glyph) {
    // glyph.pixels is Float32Array with width * height * 3 elements
    // Upload to GPU as RGB float texture
}
```

### generateMTSDF(charCode, fontSize?, pixelRange?)

Generates a 4-channel MTSDF bitmap (RGBA). RGB = multi-channel distance, A = true SDF. Preferred for text rendering.

```typescript
const glyph = msdf.generateMTSDF(65, 64, 8);
if (glyph) {
    // glyph.pixels is Float32Array with width * height * 4 elements
    // Upload to GPU as RGBA float texture
}
```

### Generating multiple glyphs

```typescript
const charset = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
const glyphs = [];

for (const char of charset) {
    const code = char.codePointAt(0);
    if (!msdf.hasGlyph(code)) continue;

    const glyph = msdf.generateMTSDF(code, 48, 6);
    if (glyph) {
        glyphs.push({ char, code, ...glyph });
    }
}
```

## Variable Fonts

For variable fonts (e.g., Inter, Roboto Flex), set variation axes before generating. Axes are specified as 4-letter OpenType tags.

### setVariationAxes(axes)

```typescript
setVariationAxes(axes: VariationAxis[]): void
```

Set one or more variation axes. Applies to subsequent `generateVar()` / `generateMTSDFVar()` calls.

Standard axes:
| Tag    | Name         | Example values |
|--------|-------------|----------------|
| `wght` | Weight      | 100 (thin) to 900 (black) |
| `wdth` | Width       | 75 (condensed) to 125 (expanded) |
| `opsz` | Optical Size | 8 (caption) to 144 (display) |
| `ital` | Italic      | 0 (upright) to 1 (italic) |
| `slnt` | Slant       | -12 to 0 degrees |

```typescript
// Load a variable font
const interBytes = new Uint8Array(fs.readFileSync('Inter-Variable.ttf'));
msdf.loadFont(interBytes);

// Generate thin weight
msdf.setVariationAxes([{ tag: 'wght', value: 100 }]);
const thin = msdf.generateMTSDFVar(65, 64, 8);

// Generate bold weight
msdf.setVariationAxes([{ tag: 'wght', value: 700 }]);
const bold = msdf.generateMTSDFVar(65, 64, 8);

// Multiple axes at once
msdf.setVariationAxes([
    { tag: 'wght', value: 600 },
    { tag: 'opsz', value: 14 }
]);
const semibold = msdf.generateMTSDFVar(65, 64, 8);
```

### clearVariationAxes()

```typescript
clearVariationAxes(): void
```

Reset to font defaults. Subsequent `generateVar` / `generateMTSDFVar` calls use the font's default axis values.

```typescript
msdf.clearVariationAxes();
const defaultWeight = msdf.generateMTSDFVar(65, 64, 8);
```

### generateVar(charCode, fontSize?, pixelRange?)

3-channel MSDF with current variation axes.

### generateMTSDFVar(charCode, fontSize?, pixelRange?)

4-channel MTSDF with current variation axes.

## Cleanup

### dispose()

```typescript
dispose(): void
```

Frees WASM heap memory (font buffer, pixel buffer, axes buffer). Call when done generating glyphs. The generator instance cannot be used after disposal.

```typescript
msdf.dispose();
```

## Output Types

### MSDFGlyph

```typescript
interface MSDFGlyph {
    metrics: MSDFMetrics;
    pixels: Float32Array;
}
```

- **pixels**: Raw float channel data. Length = `width * height * channels` where channels is 3 (MSDF) or 4 (MTSDF). Values are distance field samples centered around 0.5 (the glyph edge). Layout is row-major, bottom-to-top (row 0 = bottom of glyph). The data is copied from WASM heap -- safe to hold across calls.

### MSDFMetrics

```typescript
interface MSDFMetrics {
    width: number;
    height: number;
    advance: number;
    planeBounds: { l: number; b: number; r: number; t: number };
    atlasBounds: { l: number; b: number };
}
```

All values are in pixels, scaled to the requested `fontSize`.

- **width**: Bitmap width in pixels.
- **height**: Bitmap height in pixels.
- **advance**: Horizontal advance. After rendering this glyph, move the pen position right by this many pixels.
- **planeBounds.l**: Left edge of glyph shape relative to pen position.
- **planeBounds.b**: Bottom edge relative to baseline (negative = descender).
- **planeBounds.r**: Right edge relative to pen position.
- **planeBounds.t**: Top edge relative to baseline (positive = ascender).
- **atlasBounds**: Always `{l: 0, b: 0}` for single-glyph generation. Used by atlas packers.

The bitmap dimensions (`width x height`) include padding from the `pixelRange` parameter. The actual glyph shape occupies the region defined by `planeBounds`, with `pixelRange / 2` pixels of padding on each side for the distance field falloff.

### VariationAxis

```typescript
interface VariationAxis {
    tag: string;   // 4-letter OpenType axis tag
    value: number; // axis value
}
```

## WASM Exports (low-level)

The raw C functions exported from the WASM module. Use `MSDFGenerator` instead -- these require manual WASM heap management.

```c
uint8_t* prepare_font_buffer(int size)
float*   generate_glyph(int fontLen, uint32_t charCode, double fontSize, double pixelRange, float* outMetrics)
float*   generate_mtsdf_glyph(int fontLen, uint32_t charCode, double fontSize, double pixelRange, float* outMetrics)
float*   generate_glyph_var(int fontLen, uint32_t charCode, double fontSize, double pixelRange, float* outMetrics)
float*   generate_mtsdf_glyph_var(int fontLen, uint32_t charCode, double fontSize, double pixelRange, float* outMetrics)
int      has_glyph(int fontLen, uint32_t charCode)
void     clear_variation_axes()
void     add_variation_axis(const char* tag, double value)
void     free_buffers()
```

The `outMetrics` array is 10 floats: `[success, width, height, advance, planeL, planeB, planeR, planeT, atlasL, atlasB]`. The `generate_*` functions return a pointer to the pixel buffer (owned by WASM, reused across calls) or `nullptr` on failure.
