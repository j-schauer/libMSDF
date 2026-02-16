// @ts-ignore - Emscripten generated module, bundled by esbuild
import LibMSDFFactory from '../build/libmsdf-core.js';

export interface MSDFMetrics {
    width: number;
    height: number;
    advance: number;
    planeBounds: { l: number, b: number, r: number, t: number };
    atlasBounds: { l: number, b: number };
}

// Variation axis: 4-letter tag + value
export interface VariationAxis {
    tag: string;   // 4-letter tag: "wght", "opsz", "slnt", "ital", "wdth"
    value: number; // axis value
}

export interface MSDFGlyph {
    metrics: MSDFMetrics;
    pixels: Float32Array; // RGB float data
}

export class MSDFGenerator {
    private module: any;
    private fontLoaded: boolean = false;
    private fontLen: number = 0;

    private constructor(wasmModule: any) {
        this.module = wasmModule;
    }

    /**
     * Initialize the MSDF Generator.
     * @param modulePath Path to the libMSDF.wasm file (URL in browser, file path in Node/Deno)
     */
    static async init(modulePath: string): Promise<MSDFGenerator> {
        const mod = await LibMSDFFactory({
            locateFile: (filename: string) => {
                if (filename.endsWith('.wasm')) return modulePath;
                return filename;
            }
        });
        return new MSDFGenerator(mod);
    }

    /**
     * Load a font into the shared WASM buffer.
     * @param fontBytes Raw TTF/OTF bytes
     */
    loadFont(fontBytes: Uint8Array) {
        this.fontLen = fontBytes.byteLength;
        
        // 1. Get pointer to persistent buffer
        const ptr = this.module._prepare_font_buffer(this.fontLen);
        
        // 2. Copy data
        this.module.HEAPU8.set(fontBytes, ptr);
        
        this.fontLoaded = true;
    }

    /**
     * Check if a glyph exists in the font (without generating it).
     * @param charCode Unicode codepoint
     * @returns true if glyph exists in font, false otherwise
     */
    hasGlyph(charCode: number): boolean {
        if (!this.fontLoaded) throw new Error("Font not loaded");
        return this.module._has_glyph(this.fontLen, charCode) !== 0;
    }

    /**
     * Generate a single glyph.
     * @param charCode Unicode codepoint
     * @param fontSize Target size in pixels
     * @param pixelRange MSDF range (default 4.0)
     */
    generate(charCode: number, fontSize: number = 32, pixelRange: number = 4.0): MSDFGlyph | null {
        if (!this.fontLoaded) throw new Error("Font not loaded");

        // 1. Allocate Metrics Output Array (10 floats = 40 bytes)
        const metricsPtr = this.module._malloc(40);

        try {
            // 2. Call C++
            const pixelsPtr = this.module._generate_glyph(
                this.fontLen, 
                charCode, 
                fontSize, 
                pixelRange, 
                metricsPtr
            );

            // 3. Read Metrics
            // Float32 view of the metrics buffer
            // HEAPF32 is divided by 4 (bytes per float)
            const metricsOffset = metricsPtr >> 2; 
            const success = this.module.HEAPF32[metricsOffset + 0];
            
            if (success === 0.0) return null;

            const width = this.module.HEAPF32[metricsOffset + 1];
            const height = this.module.HEAPF32[metricsOffset + 2];
            const advance = this.module.HEAPF32[metricsOffset + 3];
            const l = this.module.HEAPF32[metricsOffset + 4];
            const b = this.module.HEAPF32[metricsOffset + 5];
            const r = this.module.HEAPF32[metricsOffset + 6];
            const t = this.module.HEAPF32[metricsOffset + 7];
            // Atlas bounds (8,9) usually 0

            // 4. Validate dimensions (empty shapes return 1x1)
            if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
                console.warn(`Invalid glyph dimensions: ${width}x${height}`);
                return null;
            }

            // 5. Copy Pixels
            const pixelCount = width * height * 3;
            const pixels = new Float32Array(pixelCount);
            const pixelsOffset = pixelsPtr >> 2;
            
            // Copy from WASM Heap to JS Array
            pixels.set(
                this.module.HEAPF32.subarray(pixelsOffset, pixelsOffset + pixelCount)
            );

            return {
                metrics: {
                    width, height, advance,
                    planeBounds: { l, b, r, t },
                    atlasBounds: { l: 0, b: 0 }
                },
                pixels
            };

        } finally {
            this.module._free(metricsPtr);
        }
    }

    /**
     * Generate a single MTSDF glyph (4 channels).
     * @param charCode Unicode codepoint
     * @param fontSize Target size in pixels
     * @param pixelRange MSDF range (default 4.0)
     */
    generateMTSDF(charCode: number, fontSize: number = 32, pixelRange: number = 4.0): MSDFGlyph | null {
        if (!this.fontLoaded) throw new Error("Font not loaded");

        // 1. Allocate Metrics Output Array (10 floats = 40 bytes)
        const metricsPtr = this.module._malloc(40);

        try {
            // 2. Call C++
            const pixelsPtr = this.module._generate_mtsdf_glyph(
                this.fontLen, 
                charCode, 
                fontSize, 
                pixelRange, 
                metricsPtr
            );

            // 3. Read Metrics
            const metricsOffset = metricsPtr >> 2; 
            const success = this.module.HEAPF32[metricsOffset + 0];
            
            if (success === 0.0) return null;

            const width = this.module.HEAPF32[metricsOffset + 1];
            const height = this.module.HEAPF32[metricsOffset + 2];
            const advance = this.module.HEAPF32[metricsOffset + 3];
            const l = this.module.HEAPF32[metricsOffset + 4];
            const b = this.module.HEAPF32[metricsOffset + 5];
            const r = this.module.HEAPF32[metricsOffset + 6];
            const t = this.module.HEAPF32[metricsOffset + 7];

            // 4. Validate dimensions (empty shapes return 1x1)
            if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
                console.warn(`Invalid glyph dimensions: ${width}x${height} for charCode ${charCode}`);
                return null;
            }

            // 5. Copy Pixels (4 channels)
            const pixelCount = width * height * 4;
            const pixels = new Float32Array(pixelCount);
            const pixelsOffset = pixelsPtr >> 2;

            pixels.set(
                this.module.HEAPF32.subarray(pixelsOffset, pixelsOffset + pixelCount)
            );

            return {
                metrics: {
                    width, height, advance,
                    planeBounds: { l, b, r, t },
                    atlasBounds: { l: 0, b: 0 }
                },
                pixels
            };

        } finally {
            this.module._free(metricsPtr);
        }
    }

    /**
     * Set variation axes for subsequent generate calls.
     * Standard axes: wght (Weight), wdth (Width), opsz (Optical Size), ital (Italic), slnt (Slant)
     * @param axes Array of {tag, value} pairs
     */
    setVariationAxes(axes: VariationAxis[]): void {
        this.module._clear_variation_axes();
        for (const axis of axes) {
            if (axis.tag.length !== 4) {
                throw new Error(`Axis tag must be 4 characters: "${axis.tag}"`);
            }
            // Allocate string in WASM memory
            const tagPtr = this.module._malloc(5);
            for (let i = 0; i < 4; i++) {
                this.module.HEAPU8[tagPtr + i] = axis.tag.charCodeAt(i);
            }
            this.module.HEAPU8[tagPtr + 4] = 0; // null terminator
            this.module._add_variation_axis(tagPtr, axis.value);
            this.module._free(tagPtr);
        }
    }

    /**
     * Clear variation axes (use font defaults).
     */
    clearVariationAxes(): void {
        this.module._clear_variation_axes();
    }

    /**
     * Generate MSDF glyph with current variation axes (3 channels).
     */
    generateVar(charCode: number, fontSize: number = 32, pixelRange: number = 4.0): MSDFGlyph | null {
        if (!this.fontLoaded) throw new Error("Font not loaded");

        const metricsPtr = this.module._malloc(40);

        try {
            const pixelsPtr = this.module._generate_glyph_var(
                this.fontLen, charCode, fontSize, pixelRange, metricsPtr
            );

            const metricsOffset = metricsPtr >> 2;
            const success = this.module.HEAPF32[metricsOffset + 0];
            if (success === 0.0) return null;

            const width = this.module.HEAPF32[metricsOffset + 1];
            const height = this.module.HEAPF32[metricsOffset + 2];
            const advance = this.module.HEAPF32[metricsOffset + 3];
            const l = this.module.HEAPF32[metricsOffset + 4];
            const b = this.module.HEAPF32[metricsOffset + 5];
            const r = this.module.HEAPF32[metricsOffset + 6];
            const t = this.module.HEAPF32[metricsOffset + 7];

            if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
                console.warn(`Invalid glyph dimensions: ${width}x${height} for charCode ${charCode}`);
                return null;
            }

            const pixelCount = width * height * 3;
            const pixels = new Float32Array(pixelCount);
            const pixelsOffset = pixelsPtr >> 2;
            pixels.set(this.module.HEAPF32.subarray(pixelsOffset, pixelsOffset + pixelCount));

            return {
                metrics: { width, height, advance, planeBounds: { l, b, r, t }, atlasBounds: { l: 0, b: 0 } },
                pixels
            };
        } finally {
            this.module._free(metricsPtr);
        }
    }

    /**
     * Generate MTSDF glyph with current variation axes (4 channels).
     */
    generateMTSDFVar(charCode: number, fontSize: number = 32, pixelRange: number = 4.0): MSDFGlyph | null {
        if (!this.fontLoaded) throw new Error("Font not loaded");

        const metricsPtr = this.module._malloc(40);

        try {
            const pixelsPtr = this.module._generate_mtsdf_glyph_var(
                this.fontLen, charCode, fontSize, pixelRange, metricsPtr
            );

            const metricsOffset = metricsPtr >> 2;
            const success = this.module.HEAPF32[metricsOffset + 0];
            if (success === 0.0) return null;

            const width = this.module.HEAPF32[metricsOffset + 1];
            const height = this.module.HEAPF32[metricsOffset + 2];
            const advance = this.module.HEAPF32[metricsOffset + 3];
            const l = this.module.HEAPF32[metricsOffset + 4];
            const b = this.module.HEAPF32[metricsOffset + 5];
            const r = this.module.HEAPF32[metricsOffset + 6];
            const t = this.module.HEAPF32[metricsOffset + 7];

            if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
                console.warn(`Invalid glyph dimensions: ${width}x${height} for charCode ${charCode}`);
                return null;
            }

            const pixelCount = width * height * 4;
            const pixels = new Float32Array(pixelCount);
            const pixelsOffset = pixelsPtr >> 2;
            pixels.set(this.module.HEAPF32.subarray(pixelsOffset, pixelsOffset + pixelCount));

            return {
                metrics: { width, height, advance, planeBounds: { l, b, r, t }, atlasBounds: { l: 0, b: 0 } },
                pixels
            };
        } finally {
            this.module._free(metricsPtr);
        }
    }

    dispose() {
        this.module._free_buffers();
        this.fontLoaded = false;
    }
}