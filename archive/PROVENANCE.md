# Archive Provenance

## Mac Native Static Libraries

Files: `libfreetype.a`, `libpng.a`, `libz.a`, `libbz2.a`

These are native Mac (arm64/x86_64) static libraries for building msdfgen natively on macOS.
Extracted from Homebrew/MacPorts builds.

- libfreetype.a - FreeType font rendering
- libpng.a - PNG support (freetype dependency)
- libz.a - zlib compression (freetype/png dependency)
- libbz2.a - bzip2 compression (freetype dependency)

Not used for WASM builds (emscripten has its own ports via `-sUSE_FREETYPE=1`).
Keep for potential future native Mac build target.
