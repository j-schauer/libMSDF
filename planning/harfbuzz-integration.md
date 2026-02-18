# libMSDF v2: HarfBuzz Integration Plan
Date: 2026-02-09

## Goal

Expand libMSDF from a single-glyph MSDF generator into a complete font module
that replaces fontkit. One WASM module handles font loading, font info/metrics,
text layout (HarfBuzz), and MSDF glyph generation. The font is loaded once into
WASM memory and referenced by handle for all subsequent operations.

## Why

Currently the kit project uses two separate font systems:
- **fontkit** (JS): parses fonts, provides metrics, does text layout (shaping)
- **libMSDF** (WASM/FreeType): generates MSDF distance field bitmaps

This means the same font file is loaded twice (once into fontkit's JS heap, once
into libMSDF's WASM heap). fontkit reimplements OpenType shaping in JS. HarfBuzz
is the industry-standard C/C++ shaper used by Chrome, Firefox, Android, iOS.

The new architecture: FreeType (font loading + metrics) + HarfBuzz (text layout)
+ msdfgen (MSDF rasterization) in a single WASM module. One font load, native
speed for everything.

## Current State (v0.1.1)

libMSDF currently exposes:
- `MSDFGenerator.init(modulePath)` → loads WASM
- `loadFont(fontBytes)` → loads ONE font into WASM (stateful, stays in memory)
- `hasGlyph(charCode)` → check codepoint exists
- `generate/generateMTSDF(charCode, fontSize, pixelRange)` → MSDF bitmap
- `setVariationAxes/clearVariationAxes` → variable font axis control
- `generateVar/generateMTSDFVar` → generate with variation
- `dispose()` → free WASM memory

Current C++ binding: `src/wasm/wasm_binding.cpp` + `src/wasm/core.h`
Current build: Emscripten with `-sUSE_FREETYPE=1`, msdfgen sources from vendor/

The module is stateful but only handles ONE font at a time. The new design needs
multi-font support via handles.

## New Architecture

### WASM-side: Handle-based multi-font

A global FT_Library shared across all fonts. Each loaded font gets a FontHandle:
- font bytes (kept alive in WASM heap)
- FT_Face (FreeType face)
- hb_font_t (HarfBuzz font, bridged from FT_Face via hb-ft)
- current pixel size (for hb_ft_font_changed)

Handles stored in a map<uint32_t, FontHandle*>. JS gets integer handle IDs back.

Multiple fonts can be loaded simultaneously. Typical font is 100KB-2MB. With
ALLOW_MEMORY_GROWTH=1, WASM heap grows to ~2GB. 10-50 fonts is trivially fine.
Must dispose handles when done to avoid leaks.

### JS API (5 calls)

#### 1. loadFont(bytes, faceIndex?) → handle
One-time per font file. Creates FT_Face + hb_font. Returns integer handle.
For TTC files, faceIndex selects which face (default 0).

#### 2. getFontInfo(handle) → FontInfo
One-time query per font. Returns everything needed for FontDefinition:
- names: familyName, fullName, postscriptName
- unitsPerEm
- weightClass (from OS/2)
- isItalic (from italicAngle + OS/2 fsSelection)
- isVariable (has variation axes)
- variationAxes: array of {tag, name, min, default, max}
- isMonospace (FT_IS_FIXED_WIDTH or width comparison)
- hasKerning (FreeType FT_HAS_KERNING or probe via HarfBuzz)
- availableFeatures: string array (from hb_ot_layout_table_get_feature_tags)
- supportedCodepoints: array of ints (from FT_Get_First_Char/FT_Get_Next_Char)

#### 3. getFontMetrics(handle, size, variationAxes?) → FontMetrics
Sets variation axes if provided, sets pixel size. Returns:
- Typographic: typoAscender, typoDescender, typoLineGap (from OS/2, fallback hhea)
- Latin bounds: measured from actual glyph bboxes of accented caps + descenders
  - Accented caps: U+00C5 (A-ring), U+00C9 (E-acute), U+00D6 (O-diaeresis), U+00D1 (N-tilde)
  - Descenders: g, j, p, q, y
- Bounding: font bbox (minX, minY, maxX, maxY)
- Reference: xHeight, capHeight
- Underline: position, thickness
- provenance: which table the typo metrics came from

All values in font units. JS scales by (size / unitsPerEm).

#### 4. layout(handle, text, size, features?, variationAxes?) → LayoutResult
The HarfBuzz shaping call. Sets size + variation, shapes text, returns:
- count: number of output glyphs
- glyphIds[]: uint32 array (font-internal glyph IDs)
- clusters[]: uint32 array (maps back to byte offset in input text)
- xAdvances[]: int32 array (26.6 fixed point, divide by 64 for pixels)
- yAdvances[]: int32 array
- xOffsets[]: int32 array
- yOffsets[]: int32 array

Kerning is baked into xAdvances (GPOS pair adjustment). Ligatures show as
single glyphs mapping to multiple input clusters. Features default to
kern=1, liga=1 (HarfBuzz defaults). Can pass feature overrides.

#### 5. generateMSDF(handle, glyphId, fontSize, pixelRange) → MSDFGlyph
Existing MSDF generation, but now takes glyphId (from layout output) instead of
charCode. Uses the same FT_Face. Returns bitmap + metrics as today.

Also generateMTSDF variant (4-channel).

#### destroyFont(handle)
Frees FT_Face, hb_font, and font bytes from WASM heap.

### Return format for multi-value results

Same pattern as current libMSDF: write into WASM heap, JS reads as typed arrays.

For layout: write flat buffer [count, glyphIds..., clusters..., xAdv..., yAdv..., xOff..., yOff...].
JS wraps with new Uint32Array(HEAPU32.buffer, ptr, count) etc.

For getFontInfo: use separate C exports for different data types:
- font_get_info_numeric(handle, outPtr) → writes numeric fields to struct
- font_get_name(handle, nameId) → returns string pointer
- font_get_codepoints(handle, outPtr, maxCount) → writes codepoint array
- font_get_variation_axes(handle, outPtr, maxCount) → writes axes array
- font_get_features(handle, outPtr, maxCount) → writes feature tags

For getFontMetrics: single struct write to outPtr, all floats.

## Building HarfBuzz

### Emscripten port (try first)
Emscripten may have `-sUSE_HARFBUZZ=1` as a built-in port (like USE_FREETYPE).
Test: `em++ -sUSE_HARFBUZZ=1 -sUSE_FREETYPE=1 -c test.cpp -o /dev/null`
If this works, it's by far the simplest path.

### Amalgamation build (fallback)
HarfBuzz has `src/harfbuzz.cc` - a single amalgamation file that includes all
68 .cc source files. Compile this one file:

```
em++ -O3 -std=c++17 -DNDEBUG -fno-exceptions -fno-rtti \
  -DHAVE_FREETYPE=1 \
  -Ivendor/harfbuzz/src \
  -sUSE_FREETYPE=1 \
  -c vendor/harfbuzz/src/harfbuzz.cc \
  -o build/harfbuzz.o
```

Use HB_LEAN (not HB_TINY) because HB_TINY disables variable font support.
HB_LEAN keeps variable fonts but strips ICU, Graphite, color fonts, threading.
Expected WASM size increase: ~500-700KB uncompressed.

Add harfbuzz as git submodule: vendor/harfbuzz

### Key HarfBuzz-FreeType bridge

```c
#include <hb.h>
#include <hb-ft.h>

// Bridge FT_Face to HarfBuzz (one-time per font load)
hb_font_t *hb_font = hb_ft_font_create_referenced(ft_face);

// When size or variation axes change on FT_Face:
FT_Set_Pixel_Sizes(ft_face, 0, ppem);
hb_ft_font_changed(hb_font);

// Shape text
hb_buffer_t *buf = hb_buffer_create();
hb_buffer_add_utf8(buf, text, -1, 0, -1);
hb_buffer_guess_segment_properties(buf);
hb_shape(hb_font, buf, features, num_features);

// Read results
hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, &count);
hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, &count);
// info[i].codepoint = glyph ID, pos[i].x_advance = advance in 26.6 fixed point
```

## What fontkit does today (complete list of API access points)

### Font parsing
- FKIT.create(Uint8Array) → Font | FontCollection
- FontCollection.fonts[] → individual fonts from TTC files

### Font identity
- Font.familyName, Font.fullName, Font.postscriptName

### Variable font detection + creation
- Font.variationAxes → {tag: {name, min, default, max}, ...}
- Font.getVariation(traits) → new Font instance at axis values

### Weight determination
- Font["OS/2"].usWeightClass (fixed fonts)
- Font.variationAxes.wght.default (variable fonts)

### Italic detection
- Font.italicAngle (nonzero = italic)
- Font["OS/2"].fsSelection.italic

### Kerning detection (3 methods)
- Font.tables.kern.pairs → legacy kern table
- Font.GPOS.lookupList.lookups → GPOS pair adjustment (types 2, 8, 9)
- Font.layout(pair, {kern:true}) → empirical probe on pairs like "AV", "To"

### Monospace detection
- Font.glyphForCodePoint(cp).advanceWidth → compare across narrow/wide chars

### Supported glyphs
- Font.characterSet → array of all supported Unicode codepoints

### Available OpenType features
- Font.availableFeatures → string array of feature tags

### Font metrics (all in font units, scaled by size/unitsPerEm)
- Font.unitsPerEm
- Font["OS/2"].typoAscender / typoDescender / typoLineGap (preferred)
- Font["hhea"].ascent / descent / lineGap (fallback)
- Font.ascent / descent (bbox-based, last resort)
- Font.xHeight, Font.capHeight
- Font.underlinePosition, Font.underlineThickness
- Font.bbox → {minX, minY, maxX, maxY}

### Latin bounds (per-glyph measurement for tight line spacing)
- Font.glyphForCodePoint(0xC5).bbox.maxY → accented capital ascent
- Font.glyphForCodePoint(0x67).bbox.minY → descender depth
- Measures: U+00C5 U+00C9 U+00D6 U+00D1 for ascent, g j p q y for descent

### Text layout
- Font.layout(text, features?, script?, language?, direction?) → GlyphRun
- GlyphRun.glyphs[] → glyph objects (id, codePoints, advanceWidth, isMark, isLigature)
- GlyphRun.positions[] → {xAdvance, yAdvance, xOffset, yOffset} per glyph
- GlyphRun.advanceWidth → total run width
- GlyphRun.bbox → run bounding box
- In practice: callers never pass script/language/direction, rarely pass features

## FreeType + HarfBuzz equivalents for each fontkit API

| fontkit API | FreeType/HarfBuzz equivalent |
|---|---|
| FKIT.create(bytes) | FT_New_Memory_Face(lib, bytes, len, faceIndex, &face) |
| Font.familyName/fullName/postscriptName | hb_ot_name_get_utf8(face, HB_OT_NAME_ID_*) |
| Font.unitsPerEm | face->units_per_EM |
| Font["OS/2"].usWeightClass | FT_Get_Sfnt_Table(face, FT_SFNT_OS2)->usWeightClass |
| Font["OS/2"].typoAscender etc | FT_Get_Sfnt_Table(face, FT_SFNT_OS2)->sTypoAscender |
| Font["hhea"].ascent etc | FT_Get_Sfnt_Table(face, FT_SFNT_HHEA)->Ascender |
| Font.italicAngle | face->style_flags & FT_STYLE_FLAG_ITALIC, or OS/2 |
| Font.variationAxes | FT_Get_MM_Var(face, &mm) → mm->axis[] |
| Font.getVariation(traits) | FT_Set_Var_Design_Coordinates + hb_ft_font_changed |
| Font.characterSet | FT_Get_First_Char / FT_Get_Next_Char iteration |
| Font.availableFeatures | hb_ot_layout_table_get_feature_tags(face, GSUB/GPOS) |
| Font.glyphForCodePoint(cp) | FT_Get_Char_Index + FT_Load_Glyph |
| glyph.advanceWidth | face->glyph->advance.x (or FT_Get_Advance) |
| glyph.bbox | FT_Glyph_Get_CBox or face->glyph->metrics |
| glyph.isMark | FT_Get_Char_Index + check Unicode category or GDEF |
| Font.bbox | face->bbox |
| Font.xHeight / capHeight | OS/2 table sxHeight / sCapHeight |
| Font.underlinePosition/Thickness | face->underline_position / face->underline_thickness |
| FT_IS_FIXED_WIDTH(face) | monospace detection |
| Font.layout(text, features) | hb_shape(hb_font, buf, features, n) |

## Multi-font capacity

Each loaded font costs: ~few KB (FT_Face + hb_font state) + font file size in heap.
Typical font files: 100KB (simple) to 2MB (CJK/variable). With ALLOW_MEMORY_GROWTH=1,
WASM heap grows to ~2GB. Loading 10-50 fonts simultaneously is trivially fine.
All fonts share one FT_Library (just a context/allocator, created once at module init).
Dispose handles when fonts are no longer needed to free heap memory.

## Implementation order

1. Add HarfBuzz to build (try -sUSE_HARFBUZZ=1, fallback to amalgamation)
2. Refactor C++ to handle-based multi-font (FontHandle struct, global map)
3. Implement loadFont → handle (FT_New_Memory_Face + hb_ft_font_create_referenced)
4. Implement getFontInfo (table reads, axis enumeration, feature enumeration)
5. Implement getFontMetrics (OS/2 + hhea + glyph bbox measurement)
6. Implement layout (hb_shape, return flat buffer)
7. Refactor existing MSDF generation to use handle + glyphId instead of charCode
8. Update TypeScript wrapper (MSDFGenerator class → new API)
9. Update tests, docs, api.md
10. Publish

## Files to modify

### C++ (src/wasm/)
- `wasm_binding.cpp` → major rewrite: handle-based API, HarfBuzz integration
- `core.h` → update for new function signatures
- New: `font_handle.h` or similar for FontHandle struct

### Build
- `Makefile.wasm` → add HarfBuzz sources/flags, new exported functions
- `lulu.yaml` → update if needed

### TypeScript (src/)
- `msdf-generator.ts` → major rewrite: new multi-font, layout-aware API
- `index.ts` → updated exports

### Docs
- `docs/api.md` → complete rewrite for new API
- `README.md` → update API overview section

### Dependencies
- Add: vendor/harfbuzz (git submodule) OR use -sUSE_HARFBUZZ=1
- Update: vendor/PROVENANCE.md with HarfBuzz provenance
- Update: lulu.yaml deps section

## Risk areas

- HarfBuzz amalgamation compile with Emscripten: may need config tweaks
- HB_LEAN vs HB_TINY: must use HB_LEAN to keep variable font support
- 26.6 fixed-point values from HarfBuzz: JS wrapper must divide by 64
- hb_ft_font_changed: must call after every size/variation change
- MSDF generation currently uses charCode, needs refactor to use glyphId
- Latin bounds measurement: need FT_Load_Glyph for specific codepoints
- TTC file support: FT_New_Memory_Face with faceIndex > 0
