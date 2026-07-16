# Changelog

All notable changes to ttengine.library are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [7.3] - 2026-07-14

- Added SAS/C build (`Source/smakefile`, `SCOPTIONS`) for AmigaOS 3.x / 68020.
- Added FT_AMIGA_SASC / FreeType error-prefix glue for SAS/C
- Fixed ftsystem CreatePool/DeletePool and swap helpers for SAS/C
- Fixed Expunge FreeMem pointer arithmetic under SAS/C
- Removed GCC-only reg()/__saveds usage from public entry points
- Added CLI `TTEngineTest` harness under `Source/test`.
- Fixed FreeType module error enums and Amiga glue for SAS/C.
- Fixed libcall conventions (`__ASM__` / `__SAVE_DS__` / `__REG__`) for SAS/C to use compiler-specific.h macros.
- Fixed antialiased glyph loads to use `FT_LOAD_TARGET_NORMAL` instead of always mono.
- Fixed cache eviction to flush unused sizes until under `CACHELIMIT`.
- Fixed incomplete inverse-gamma path: fill `re_AntiGamma` and blend via decode→lerp→encode.

## [7.2] - 2005-12-22

- Final version of direct rendering callback for BGRA32 screenmode.
- Added handling of fonts with MS Symbol names encoding (Marlett).
- Fixed possible hits in the database parser.

## [7.1] - 2005-03-15

- Native AmigaOS 4 version compiled by Andreas Falkenhahn.
- Mask and render buffers are now allocated for every RastPort, no more global. It fixes problems with multiple processes accessing TTEngine simultaneously (trashes instead of text).

## [7.0] - 2005-01-13

- Realtime preview in the font requester and some new requester tags.
- Introduced new metrics mode designed for text-oriented applications, especially using JAM2 drawmode. Mode is turned on by specifying `TT_DiskFontMetrics, TRUE` in `TT_SetAttrs()`. In this mode filled JAM2 area has always height equal to `TT_FontHeight` (which is a sum of `TT_FontMaxTop` and `TT_FontMaxBottom`). Horizontally JAM2 area is set to cursor advance (it creates continuous filling even if `TT_Text` is called char-by-char).
- New metrics attributes: `TT_FontHeight` (sum of `TT_FontMaxTop` and `TT_FontMaxBottom`), `TT_FontBaseline` (alias for `TT_FontMaxTop`) and `TT_FontWidth` (cursor advance, only for monospaced fonts).
- TTEngine now distincts monospaced fonts. `TT_FontFixedWidth` can be specified for `TT_OpenFont()`, function will fail on proportional font. The attribute is also gettable with `TT_GetAttrs()`. `TTRQ_FixedWidthOnly` attribute may be passed to `TT_Request()` to filter proportional fonts out.
- TTEManager 1.2: cache size and global gamma settings. Fixed font window refresh deadlock under MorphOS.
- New API functions: `TT_ObtainFamilyListA()`, `TT_FreeFamilyList()`.

## [6.6] - 2004-05-04

- Bugfix: COMPLEMENT draw mode with antialiased fonts no more leaves trashes when done twice at the same place. It allows for using COMPLEMENT mode as an easy way to make moving text brushes. Works as well combined with INVERSVID and transparency. Bug reported by Marcin Kwiatkowski.
- Bugfix: Missing render mode initialization in `TT_TextExtent()`. Glyph bitmaps were not cached if `TT_TextExtent()` was called before any `TT_Text()` or `TT_TextLength()` on this rastport. It caused significant slowdown of `TT_TextExtent()`. Bug reported by Darius Brewka.
- Bugfix: If temporary RastPort was used with NULL BitMap, this NULL pointer was passed to `GetCyberMapAttr()`. It might lead to Enforcer read hits. Now if RastPort's BitMap is NULL, antialiased render mode is assumed and `GetCyberMapAttr()` is not called.
- Fixed "DetachedRastPort" example code to work on RGB screens. Color 1 in colormap of a window opened on Workbench is usually black. Rendering black text on black (`BMF_CLEAR`) bitmap usually results just in black window contents... Changed text color to 2 (usually white).

## [6.5] - 2003-12-10

- Automatic italic font generation now works even when an application does not use the font database, but opens the font directly using `TT_FontFile` tag. In the case specifying `TT_FontStyle_Italic` will shear the font regardless its design style. Feature suggested by Andreas Falkenhahn.

## [6.4] - 2003-03-18

- `mathieeesingtrans.library` was opened in too high 38 version, while Workbench 3.1 contains v37 one. Fixed.
- Library base semaphore was initialized too late in initialization code. It could result in obtaining uninitialized semaphore and Enforcer hits. Fixed.
- If initialization of TTEngine failed, function freeing resources was called two times producing "memory freed twice" alerts and Enforcer hits. Fixed.
- All the above bugs have been reported by and fixed with help of Halvadjian Georges.

## [6.3] - 2003-03-10

- Alphablending system redesigned completely based on ideas by Fabio Alemagna, now TTEngine uses real gamma correction and gamma coefficient is adjustable by user.
- New tag `TT_Gamma` for gamma coefficient adjust.
- FPU version is discontinued. New alphablending code does not use FPU at all and is faster even than FPU 6.2 version. Using FPU gives no further speedup.

## [6.2] - 2003-02-07

- Compiled FPU (math coprocessor) version of the library. It gives speedup of 30 to 50% in antialiased modes.
- Automatic italic font generation. If chosen font family does not have italic faces, they will be generated by shearing regular faces right of 15 degrees. NOTE: It only works when fonts are opened via database.
- Added 'Style' and 'Weight' selection gadgets in the font requester.

## [6.1] - 2003-02-02

- Internal RastPort environment list also protected by semaphore.
- Font requester gadgets did not remember their state after window resizing. Fixed.
- Requester layout bug prevented the window to open in some circumstances. Fixed.
- Font handles can be now safely shared between tasks (it enables TTEngine usage in BOOPSI images and gadgets).

## [6.0] - 2003-01-05

- Added basic font requester and a set of three functions in API: `TT_AllocRequest()`, `TT_Request[A]()`, `TT_FreeRequest()`.
- Bitmap cache autooptimization algorithm reduces the number of cache search iterations of about 70%. It results in faster text output.
- Bitmap cache size limit feature added. You can put `CACHELIMIT=<size in bytes>` line in `ttengine.database` file (at any place). Every not used font size cache beyond the limit will be flushed and memory released to the system. Default cache size limit is 512 kB.
- Internal system-wide TTEngine lists are protected by semaphores. It increases reliability of the library when is opened by multiple applications.
- "PlainTest" example program modified to show use of TTEngine font requester.

## [5.2] - 2002-12-01

- Added UTF-8 encoding.
- Initial byte order mark (`$0000FEFF`) is skipped in UTF-16BE and UTF-32BE.
- Added UTF-32LE, UTF-32, UTF-16LE and UTF-16.

## [5.1] - 2002-11-24

- Added support for following font encodings: ISO-8859-1, ISO-8859-2, ISO-8859-3, ISO-8859-4, ISO-8859-5, ISO-8859-6, ISO-8859-7, ISO-8859-8, ISO-8859-9, ISO-8859-10, ISO-8859-11, ISO-8859-13, ISO-8859-14, ISO-8859-15, ISO-8859-16, UTF-16BE, UTF-32BE. Internally prepared for other encodings (callback functions system). WARNING! `TT_Encoding_ISO8859_1` and `TT_Encoding_Unicode` tags changed, please recompile if you use them. Now all encoding values are IANA encoding numbers. `TT_Encoding_Unicode` is now defined as `TT_Encoding_UTF16_BE`.
- Added a few global metrics attributes: `TT_FontRealAscender` (capital letters), `TT_FontRealDescender` ('y', 'j', 'g', 'p', 'q' bottom), `TT_FontAccentedAscender` (accented capital letters), `TT_FontMaxTop`, `TT_FontMaxBottom`, `TT_FontDesignHeight`.
- Added possibility for using custom 8-bit to Unicode transcoding with `TT_CustomEncoding` tag.
- Added "light" library version compiled with autohinter instead of TrueType bytecode interpreter.
- Recompiled with FreeType 2.1.3 sources. Note however in TTEngine bytecode interpreter is preferred over autohinter.

## [5.0] - 2002-11-09

- When rendering with JAM2 and/or INVERSVID, background fills the whole vertical line space.

## [5.0-beta5] - 2002-11-07

- Bug fixed: if the last rendered character of a string was space (containing no visible pixels), it was truncated. Reported by Andreas Falkenhahn.

## [5.0-beta4] - 2002-11-05

- `TT_Transparency` was inversed (0 should mean opaque, 255 almost invisible). Fixed.
- Added `TT_DoneRastPort()` function. It disposes rendering environment TTEngine creates when a RastPort is first used by it (through `TT_SetAttrs()` or `TT_SetFont()` call). This function must be called just before RastPort gets disposed (for example before `CloseWindow()` or `CloseScreen()`).
- All example programs updated with `TT_DoneRastPort()` call.
- Fixed bug in "DrawModes" example program - `rp` variable was not initialized.

## [5.0-beta3] - 2002-11-04

- Fixed major bug causing system hangup on palette screens (both bitplane and 8-bit chunky).
- INVERSVID mode was completely broken on <= 8-bit targets. Fixed.
- Some tags were referred in autodoc as having "TTA" prefix instead of "TT". Fixed (reported by Olivier Fabre).

## [5.0-beta2] - 2002-11-03

- Added luminance based antialiasing with RGB correction.
- `TT_SoftStyle` implemented with underline and double underline.
- JAM2 mode usable again.
- INVERSVID mode works in both antialiased and not-antialiased mode.
- Fixed library init code, now `cybergraphics.library` is optional as it should be (reported by Andreas Falkenhahn).
- Non-antialiased output in `TT_Text()` and `TT_GetPixmap()` works again.
- COMPLEMENT mode works in all combinations with other drawmodes and TTEngine options.
- Overstriked line and double overstriked line softstyles added.
- Transparency works again (broken in 5.0 beta 1).
- `TT_Foreground` and `TT_Background` tags allow for direct RGB color setting on >= 15-bit RastPorts.
- "DrawModes" test program added (tests RastPort drawmodes, direct RGB color setting and transparency).

## [5.0-beta1] - 2002-10-30

- Fixed bug in autodocs regarding `TT_Antialias` tag possible values (these have "TTA" prefix instead of "TT"). Reported by Andreas Falkenhahn.
- Fixed bug in autodocs, `TT_FontAscender` and `TT_FontDescender` tags were named wrong as `TT_Ascender` and `TT_Descender`. Reported by Andreas Falkenhahn.
- Fixed bug in `TT_GetAttrs()`, `TT_FontAscender` and `TT_FontDescender` returned value had most significant bit set. Reported by Andreas Falkenhahn.
- Fixed bug in `TT_GetAttrs()`, `TT_FontDescender` returned positive values instead of negative as described in the autodoc.
- Added `TT_GetPixmapA()` function.
- Added `TT_FreePixmap()` function.
- Complete blitting engine rework.

## [4.3] - 2002-10-14

- Implemented affine transform "kernel". Transformed fonts are not cached, JAM2 mode and metrics functions are switched off.
- Implemented X and Y axes scaling (`TT_ScaleX` and `TT_ScaleY` tags).
- `TT_TextFit()` returned garbage in TextExtent structure if no font was `TT_SetFont()`-ed for given rastport. Now TextExtent is cleared to all zeros.
- Added "Scaling" example program.
- Removed Enforcer hit in `TT_OpenFont()` generated when font opening failed.
- Fixed `libraries/ttengine.h` header file to follow Style Guide.

## [4.2] - 2002-10-11

- Implemented open counter in the LoadedFont structure. It fixes problems when one font is set many times for different RastPorts.
- `TT_SetAttrs()` now is able to create RenderEnv context when called before any `TT_SetFont()` for given RastPort.
- Changed `TT_SetFont()` return value type from VOID to BOOL because it can fail if runs out of memory.
- Added "MoreFonts" example program.

## [4.1] - 2002-09-29

- Added SYMBOL_CS character mapping for fonts with non-standard mapping like Symbol or Wingdings. Now these fonts can be used with Unicode 16-bit encoding.
- Added new attributes `TT_FontName`, `TT_FontFamily`, `TT_FontSubfamily` for `TT_GetAttrs()`. Font names are extracted from the font file. If the font file contains names in multiple languages, language is selected according to "preferred languages" locale settings.
- Added "FontNames" example program demonstrating feature described above.
- Added `TT_Transparency` tag allowing for transparent text.
- Added "Background" example program.

## [4.0-beta4] - 2002-09-20

- Library API changed slightly, `TT_SetFont()` introduced.
- Library initialization failed if `ENV:ttengine.database` could not be opened. Fixed.

## [4.0-beta3] - 2002-09-18

- Library name changed to "ttengine.library" to avoid backward compatibility problems.
- `TT_TextFit()` implemented partially (no right-to-left fitting). Test program ('txfit') with sources included into distribution.
- Gamma correction for antialiased dark text on light background implemented.

## [4.0-beta2] - 2002-09-17

- Library caused Avail FLUSH to crash if opened more than one time, system memory lists were corrupted. Fixed.
- `TT_TextExtent()` tested thoroughly. Test program ('txex') with sources included into distribution.

## [4.0-beta1] - 2002-09-16

- API changed completely as well as some library internals.

## [3.5] - 2002-06-10

- Full 8-bit chunky and AGA/ECS/OCS planar bitmaps. Not sure of HAM6/HAM8... ;-). Well not that slow I've expected.

## [3.4] - 2002-06-10

- The library now recognizes (but not yet supports) planar and 8-bit chunky destination bitmaps.

## [3.3] - 2002-06-09

- More internal changes, much of common code moved into subroutines, 140 bytes of executable saved. Minor speedups here and there.
- Internal `tt_putstr()` calls `tt_strwidth()` only when JAM2 drawmode is used, a minor speedup.

## [3.2] - 2002-06-09

- Internal changes, some bytes of executable saved.

## [3.1] - 2002-06-09

- Significant speedup of rendering: In 3.0 version render buffer was allocated/deallocated with every single rendered character. Now the buffer is reallocated only when it have to be bigger.

## [3.0] - 2002-05-28

- Added font database and `TT_OpenFontTagList()` function.
- `TT_SetFont()` is now obsolete.
- New example program testing font database and fallback font functionality of `TT_OpenFontTagList()`.

## [2.0] - 2002-04-20

- Added font and string metrics functions: `TT_StrWidth()`, `TT_UStrWidth()`, `TT_GetFontAttrsTagList()` with `TTFA_Ascender` and `TTFA_Descender` attributes.
- New, useful example program TrueView (simple textviewer using TrueType fonts).

## [1.2] - 2002-04-02

- JAM2 mode was broken. Fixed.
- Library description file changed to new SFD format.
- Test program enhanced of JAM2 mode and kerning test.

## [1.1] - 2002-03-30

- First public release.
