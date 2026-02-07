/**
 * libMSDF Tests - Single glyph MSDF generation
 */

import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

let passed = 0;
let failed = 0;

function assert(condition: boolean, msg: string) {
    if (!condition) throw new Error(msg);
}

async function runTest(name: string, fn: () => Promise<void> | void) {
    try {
        await fn();
        console.log(`  PASS: ${name}`);
        passed++;
    } catch (e: any) {
        console.log(`  FAIL: ${name}`);
        console.log(`        ${e.message}`);
        failed++;
    }
}

async function main() {
    console.log('\n=== libMSDF Test Suite ===\n');

    // Load module
    const { initMSDF } = await import(path.join(__dirname, 'libMSDF.js'));

    // Load font
    const fontPath = path.join(__dirname, 'assets/Poppins-Regular.ttf');
    const fontBytes = new Uint8Array(fs.readFileSync(fontPath));

    // Init
    let msdf: any;

    console.log('Init Tests:');
    await runTest('initMSDF() loads WASM', async () => {
        msdf = await initMSDF(path.join(__dirname, 'libMSDF.wasm'));
        assert(msdf !== null, 'msdf should not be null');
    });

    console.log('\nFont Tests:');
    await runTest('loadFont() accepts font data', async () => {
        msdf.loadFont(fontBytes);
    });

    await runTest('hasGlyph() returns true for existing glyph', async () => {
        assert(msdf.hasGlyph(65) === true, 'A should exist');
        assert(msdf.hasGlyph(97) === true, 'a should exist');
    });

    await runTest('hasGlyph() returns false for missing glyph', async () => {
        assert(msdf.hasGlyph(0x1F600) === false, 'emoji should not exist');
    });

    console.log('\nGeneration Tests:');
    await runTest('generate() returns valid 3-channel MSDF', async () => {
        const glyph = msdf.generate(65, 64, 8);
        assert(glyph !== null, 'glyph should not be null');
        assert(glyph.metrics.width > 0, 'width > 0');
        assert(glyph.metrics.height > 0, 'height > 0');
        assert(glyph.pixels instanceof Float32Array, 'pixels is Float32Array');
        assert(glyph.pixels.length === glyph.metrics.width * glyph.metrics.height * 3, '3 channels');
    });

    await runTest('generateMTSDF() returns valid 4-channel MTSDF', async () => {
        const glyph = msdf.generateMTSDF(65, 64, 8);
        assert(glyph !== null, 'glyph should not be null');
        assert(glyph.pixels.length === glyph.metrics.width * glyph.metrics.height * 4, '4 channels');
    });

    await runTest('different sizes produce different dimensions', async () => {
        const small = msdf.generate(65, 32, 8);
        const large = msdf.generate(65, 128, 8);
        assert(small.metrics.width < large.metrics.width, 'small < large');
    });

    // Variable font tests
    console.log('\nVariable Font Tests:');
    const interPath = path.join(__dirname, 'assets/Inter-VariableFont_opsz,wght.ttf');
    if (fs.existsSync(interPath)) {
        const interBytes = new Uint8Array(fs.readFileSync(interPath));

        await runTest('variable font weight affects glyph', async () => {
            msdf.loadFont(interBytes);
            msdf.setVariationAxes([{ tag: 'wght', value: 100 }]);
            const thin = msdf.generateMTSDFVar(65, 64, 8);
            msdf.setVariationAxes([{ tag: 'wght', value: 900 }]);
            const bold = msdf.generateMTSDFVar(65, 64, 8);
            assert(bold.metrics.advance > thin.metrics.advance, 'bold wider than thin');
            msdf.loadFont(fontBytes); // restore
        });
    } else {
        console.log('  SKIP: Inter variable font not found');
    }

    // Summary
    console.log('\n=== Summary ===');
    console.log(`Tests: ${passed} passed, ${failed} failed`);
    if (failed > 0) process.exit(1);
}

main().catch(e => {
    console.error(e);
    process.exit(1);
});
