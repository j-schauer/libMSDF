/**
 * Worker thread for parallel glyph generation (Node.js)
 * Each worker loads its own WASM instance and font copy
 */

import { parentPort, workerData } from 'worker_threads';
import { readFile } from 'fs/promises';
import { pathToFileURL } from 'url';

interface WorkerInit {
    wasmPath: string;
    fontBytes: Uint8Array;
}

interface GlyphTask {
    type: 'generate';
    charCode: number;
    fontSize: number;
    pixelRange: number;
    glyphType: 'msdf' | 'mtsdf';
}

interface GlyphResult {
    charCode: number;
    success: boolean;
    metrics?: {
        width: number;
        height: number;
        advance: number;
        planeBounds: { l: number; b: number; r: number; t: number };
    };
    pixels?: Float32Array;
    timeMs: number;
}

let wasmModule: any = null;
let fontLoaded = false;
let fontLen = 0;

async function initWorker(init: WorkerInit) {
    // Load WASM module
    const wasmBinary = await readFile(init.wasmPath);
    const moduleUrl = pathToFileURL(init.wasmPath.replace('.wasm', '.js')).href;
    const { default: factory } = await import(moduleUrl);

    wasmModule = await factory({ wasmBinary: wasmBinary.buffer });

    // Load font
    fontLen = init.fontBytes.byteLength;
    const ptr = wasmModule._prepare_font_buffer(fontLen);
    wasmModule.HEAPU8.set(init.fontBytes, ptr);
    fontLoaded = true;

    parentPort!.postMessage({ type: 'ready' });
}

function generateGlyph(task: GlyphTask): GlyphResult {
    const startTime = performance.now();

    if (!fontLoaded) {
        return { charCode: task.charCode, success: false, timeMs: 0 };
    }

    const metricsPtr = wasmModule._malloc(40);

    try {
        const pixelsPtr = task.glyphType === 'mtsdf'
            ? wasmModule._generate_mtsdf_glyph(fontLen, task.charCode, task.fontSize, task.pixelRange, metricsPtr)
            : wasmModule._generate_glyph(fontLen, task.charCode, task.fontSize, task.pixelRange, metricsPtr);

        const metricsOffset = metricsPtr >> 2;
        const success = wasmModule.HEAPF32[metricsOffset + 0];

        if (success === 0.0) {
            return { charCode: task.charCode, success: false, timeMs: performance.now() - startTime };
        }

        const width = wasmModule.HEAPF32[metricsOffset + 1];
        const height = wasmModule.HEAPF32[metricsOffset + 2];
        const advance = wasmModule.HEAPF32[metricsOffset + 3];
        const l = wasmModule.HEAPF32[metricsOffset + 4];
        const b = wasmModule.HEAPF32[metricsOffset + 5];
        const r = wasmModule.HEAPF32[metricsOffset + 6];
        const t = wasmModule.HEAPF32[metricsOffset + 7];

        const channels = task.glyphType === 'mtsdf' ? 4 : 3;
        const pixelCount = width * height * channels;
        const pixels = new Float32Array(pixelCount);
        const pixelsOffset = pixelsPtr >> 2;
        pixels.set(wasmModule.HEAPF32.subarray(pixelsOffset, pixelsOffset + pixelCount));

        return {
            charCode: task.charCode,
            success: true,
            metrics: { width, height, advance, planeBounds: { l, b, r, t } },
            pixels,
            timeMs: performance.now() - startTime
        };
    } finally {
        wasmModule._free(metricsPtr);
    }
}

parentPort!.on('message', async (msg: any) => {
    if (msg.type === 'init') {
        await initWorker(msg.data as WorkerInit);
    } else if (msg.type === 'generate') {
        const result = generateGlyph(msg.data as GlyphTask);
        // Transfer the pixels buffer for zero-copy
        if (result.pixels) {
            parentPort!.postMessage({ type: 'result', data: result }, [result.pixels.buffer]);
        } else {
            parentPort!.postMessage({ type: 'result', data: result });
        }
    } else if (msg.type === 'dispose') {
        if (wasmModule) {
            wasmModule._free_buffers();
        }
        parentPort!.postMessage({ type: 'disposed' });
    }
});
