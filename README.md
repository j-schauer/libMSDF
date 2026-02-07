# kitMSDF

Single glyph MSDF generation via WASM.

## Build

```bash
lulu build release
lulu run test
```

## Output

- `msdf-core.js` / `msdf-core.wasm` - WASM module
- `kitMSDF.js` / `kitMSDF.d.ts` - MSDFGenerator class
- `worker.js` - Node.js worker
- `worker-browser.js` - Browser worker
- `worker-deno.js` - Deno worker

## TODO

- [ ] Move `msdf-atlas-gen/` to `vendor/msdf-atlas-gen/` for lulu convention compliance
