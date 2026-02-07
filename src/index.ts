/**
 * libMSDF - Single glyph MSDF generation via WASM
 *
 * Low-level MSDF glyph generation. For atlas management and caching, see kitAtlas.
 */

export { initMSDF, MSDFGenerator, MSDFGlyph, MSDFMetrics } from './msdf-generator.js';
export type { VariationAxis } from './msdf-generator.js';
