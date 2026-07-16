# TODO

## FreeType upgrade

- [ ] Vendor FreeType **2.10.4** (C89-compatible ceiling; enables `FT_LOAD_COLOR`, CBDT/CBLC, COLR/CPAL v0).
- [ ] Re-port Amiga `ftsystem.c` (DOS I/O + memory pools) against the new tree.
- [ ] Update makefiles / object lists for the new FreeType source layout (keep TT + sfnt + smooth + raster; enable color bitmap support as needed).
- [ ] Confirm existing mono/gray rendering still works unchanged before adding color paths.

## Emoji / color glyph support

- [ ] Load color glyphs with `FT_LOAD_COLOR` when the face has color (`FT_HAS_COLOR`).
- [ ] Add BGRA (`FT_PIXEL_MODE_BGRA`) blit path for RGB RastPorts (OS4 / MorphOS first; planar 8-bit out of scope).
- [ ] Extend glyph cache to store and look up color (BGRA) bitmaps.
- [ ] Optional: COLR/CPAL v0 via FreeType’s composed color render (or layer API).
- [ ] Document supported emoji formats (CBDT vs COLR v0) and memory/cache implications.

## Faster / optimised rendering

- [x] Stop always using `FT_LOAD_TARGET_MONO` on the antialiased path; use an appropriate load target for AA.
- [x] Tighten / review bitmap cache behaviour around `CACHELIMIT` (evict until under limit).
- [x] Finish inverse-gamma path: fill `re_AntiGamma` and use decode→lerp→encode in `blend()`.
- [ ] Avoid full ARGB `ReadPixelArray` / `WritePixelArray` round-trips where a direct path exists (lean into BGRA32 / existing direct-render callback).
- [ ] Optional fast paths: skip gamma/blend tables for opaque JAM1 hot paths.

## Suggested order

1. Bump FreeType to 2.10.4 and keep mono + gray working.
2. Add BGRA emoji blit for RGB RastPorts + cache support.
3. Optimise the existing gray alphablend path.
