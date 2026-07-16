/* $VER: lib.c 7.3 (14.7.2026) by Grzegorz Kraszewski */

/// preliminary AutoDocs
/****** ttengine.library/background *****************************************
*
*
*   PURPOSE
*
*   The library is fast, AmigaOS friendly TrueType render engine. It has
*   nothing to do with Amiga outline font system. This system has a lot of
*   limitations which make it useless for high speed and quality text output.
*   If someone wants an integration of TrueType with AmigaOS bullet.library
*   like outline font system, should consider using ttf.library. This library
*   opens TrueType font by itself and renders high quality glyphs directly
*   into any RastPort.
*
*   FREETYPE2 BASED
*
*   The render engine of the library is based on FreeType2 project
*   (http://www.freetype.org). This version of ttengine.library uses 2.1.3
*   FreeType build.
*
*   REQUIREMENTS
*
*   - OS 3.0+.
*   - 68020 or better processor.
*   - for antialiased output graphics board with at least 15-bit screenmode
*     and RTG system: CyberGraphX 3.x+ or Picasso96 2.x+.
*
*   FEATURES
*
*   The library expands FreeType functionality making usage of TrueType fonts
*   easy and comfortable. Below you can find key features:
*
*   - renders whole strings (not single glyphs) with kerning.
*   - antialiasing to any (not neccesarily uniform color) background.
*   - system compatible output to any (including planar) RastPort.
*   - supports JAM1. JAM2, INVERSVID, COMPLEMENT RastPort modes.
*   - supports 8-bit to Unicode mapping with "ENV:ttfcodepage" table
*     compatible with ttf.library.
*   - allows for direct 16-bit Unicode string rendering.
*   - easy to use, taglist based API almost identical to graphics.library
*     font API.
*   - efficient system-wide glyph cache system.
*
*   CACHE SYSTEM
*
*   The library uses my own (not that experimental FreeType one) cache system
*   speeding up strings rendering alot. The cache is system-wide, it means it
*   is common to every application using ttengine.library. Only used glyphs of
*   given font face are cached. If the library encounters cache miss, missing
*   glyph is loaded and rasterized on the fly. Cache system is totally
*   transparent to the library user, so there are no cache functions in the
*   library API. Cache uses one single Exec memory pool avoiding memory
*   fragmentation.
*
*****************************************************************************
*
*/

/****** ttengine.library/font_database **************************************
*
*   GENERAL INFORMATION
*
*   'ttengine.database' is a plain text file (placed in 'ENV:' directory)
*   containing informations about TrueType fonts available in the system.
*   The database is generated and edited with TTManager usually, some tweaks
*   can be done by hand however. Every line contains one keyword followed by
*   parameter value. Name and value can be separated by space(s) or equality
*   sign, exactly the same as shell command parameters (to say more -
*   database is parsed by the same ReadArgs() system call as shell command
*   parameters are). Parameter values containing spaces should be quoted.
*   Here are some examples:
*
*   FAMILY Tahoma
*   FAMILY=Times
*   FAMILY = "Times New Roman"
*   FAMILY "Weird Font"
*
*   KEYWORDS
*
*   FAMILY - defines the name used for font opening. All variants of the font
*   (italic, bold, black, heavy, light, demi etc.) will have the same family
*   name. Family names (and aliases described below) are case insensitive.
*
*   ALIAS - defines another name for the same family. It generally has three
*   purposes:
*
*   1. Similar names for the same font, like "Times", "TimesNewRoman" and
*   "Times New Roman".
*
*   2. One replacement font for some very similar ones, for example
*   "Helvetica" and "Switzerland" can be aliased to "Arial".
*
*   3. Generic names like "default", "serif", "sans-serif", "monospaced". An
*   application can request generic name without specific family name, or
*   generic font can be used as a fallback if given name can't be found in
*   the database. For example you can alias "monospaced" to "Courier", and an
*   application (be it CSS compatible HTML viewer) can request
*   'LucidaConsole, monospaced'. If there is no LucidaConsole font in the
*   system, Courier will be used. The same way you can alias any font to any
*   other, for example Times to Arial, this makes not much sense however.
*   'default' font can be used as a last resort.
*
*   FILE - defines single TrueType font file and its attributes, which are:
*     FILE itself - defines a path to the font file. Font files can be placed
*       anywhere in the filesystem(s). There is no default value for the
*       attribute, it must be given explicitly.
*     WEIGHT - defines font weight in Cascading Style Sheets manner, where 0
*       means the lightest and 999 means the heaviest weight. 400 is typical
*       value for normal weight, 700 for bold. Default value is 400 (normal).
*     STYLE - "regular" or "italic" for italic and oblique faces. Default
*       value is "regular".
*     SMOOTHSMALL - controls antialiasing of small sizes. Any sizes smaller or
*       equal to SMOOTHSMALL will be antialiased. Note: can be overriden by
*       application. Default value is 9.
*     SMOOTHBIG - controls antialiasing of big sizes. Any sizes bigger or
*       equal to SMOOTHBIG will be antialiased. Note: can be overriden by
*       application. Default value is 18.
*
*     Every FILE is automatically added to the nearest FAMILY defined above
*     in the database file. Every ALIAS or FILE placed before the first
*     FAMILY is rejected quietly.
*
*   COMMENTS
*
*   You can place any full-line comments in the file starting a line with
*   hash or semicolon. End-line comments are not supported.
*
*****************************************************************************
*
*/
///

#define __NOLIBBASE__

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_GLYPH_H
#include <freetype/ttnameid.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/locale.h>
#include <proto/layers.h>

#ifndef __amigaos4__
#include <proto/mathieeesingbas.h>
#include <proto/mathieeesingtrans.h>
#endif

#include <exec/libraries.h>
#include <exec/resident.h>
#include <exec/memory.h>
#include <cybergraphx/cybergraphics.h>
#include <string.h>
#include <math.h>

#define MAX_PATH_LENGHT  1024       /* maximum length of file path including ending zero */
#define DEFAULT_NO_ANTIALIAS_MIN  10
#define DEFAULT_NO_ANTIALIAS_MAX  18
#define NO_SCALE                  0xFFFFFFFF
#define EXTENT_INTERNAL           1

#include "lib.h"
#include "getnextchar.h"
#include "ttengine.h"

struct TTEngineBase *Ttb;

#ifndef __amigaos4__
struct Library *SysBase;
#endif

#if !defined(__MORPHOS__) && !defined(__amigaos4__)
struct Library *__UtilityBase;    /* required for libnix 64-bit arithmetic */
#endif

/*--- Functions prototypes -------------------------------------------------*/

/* information about affine transformation */

struct TransformInfo
{
	struct RenderEnv *ti_RenderEnv;
	FLOAT             ti_ScaleX;   /* <-100, -0.01> and <0.01, 100> */
	FLOAT             ti_ScaleY;   /* <-100, -0.01> and <0.01, 100> */
	FLOAT             ti_Shear;    /* <-89, 89> in degrees */
	FLOAT             ti_Rotate;   /* <0, 360> in degrees, counterclockwise */
};

/* codepage handling functions prototypes */

static void load_codepage(struct TTEngineBase *ttb);
static void free_codepage(struct TTEngineBase *ttb);

/* === CODE STARTS HERE === */

//==========================================================================================================================================================
// calculate_gamma()
//==========================================================================================================================================================

#if defined(__MORPHOS__) || defined(__amigaos4__)

void calculate_gamma(struct TTEngineBase *ttb, struct RenderEnv *re)
{
	WORD i;
	float gcf;
	float agcf;

	ttb = ttb;
	/* GammaCf is gamma * 1000; encode uses 1/gamma, decode uses gamma. */
	gcf = (float)1000.0 / (float)re->re_GammaCf;
	agcf = (float)re->re_GammaCf / (float)1000.0;

	for (i = 0; i < 256; i++)
	{
		float n;

		n = (float)i / 255;
		re->re_Gamma[i] = (UBYTE)(pow(n, gcf) * (float)255);
		re->re_AntiGamma[i] = (UBYTE)(pow(n, agcf) * (float)255);
	}
	re->re_Gamma[0] = 0;
	re->re_Gamma[255] = 255;
	re->re_AntiGamma[0] = 0;
	re->re_AntiGamma[255] = 255;
}

#else

void calculate_gamma(struct TTEngineBase *ttb, struct RenderEnv *re)
{
	USELIB(MathIeeeSingBasBase, 0, 0);
	USELIB(MathIeeeSingTransBase, 0, 0);

	WORD i;
	FLOAT gcf;
	FLOAT agcf;
	FLOAT n;
	FLOAT two55;

	LD(kp(""));

	/* IEEESPPow(y, x) returns x^y on Amiga (exponent first). */
	gcf = IEEESPDiv(1000, IEEESPFlt(re->re_GammaCf));
	agcf = IEEESPDiv(IEEESPFlt(re->re_GammaCf), 1000);
	two55 = IEEESPFlt(255);

	for (i = 0; i < 256; i++)
	{
		n = IEEESPDiv(IEEESPFlt(i), two55);
		re->re_Gamma[i] = IEEESPFix(IEEESPMul(IEEESPPow(gcf, n), two55));
		re->re_AntiGamma[i] = IEEESPFix(IEEESPMul(IEEESPPow(agcf, n), two55));
	}
	re->re_Gamma[0] = 0;
	re->re_Gamma[255] = 255;
	re->re_AntiGamma[0] = 0;
	re->re_AntiGamma[255] = 255;
}

#endif

//==========================================================================================================================================================
// expand_pixmap()
//==========================================================================================================================================================

static BOOL expand_pixmap(struct TTEngineBase *ttb, struct RenderEnv *re, ULONG new_size)
{
	USESYSBASE;

	if (re->re_Pixmap)
	{
		FreePooled(ttb->ttb_MemPool, re->re_Pixmap, re->re_PixmapSize);
	}

	re->re_Pixmap = NULL;
	re->re_PixmapSize = 0;
    
	if ((re->re_Pixmap = AllocPooled(ttb->ttb_MemPool, new_size)))
	{
		re->re_PixmapSize = new_size;
		return TRUE;
	}

	return FALSE;
}

//==========================================================================================================================================================
// antialias_off()
//==========================================================================================================================================================

#ifdef __SASC
__inline void antialias_off(struct RenderEnv *re)
#else
inline void antialias_off(struct RenderEnv *re)
#endif
{
	re->re_RenderMode = ft_render_mode_mono;
	re->re_BitmapMode = ft_pixel_mode_mono;
}

//==========================================================================================================================================================
// antialias_on()
//==========================================================================================================================================================

#ifdef __SASC
__inline void antialias_on(struct RenderEnv *re)
#else
inline void antialias_on(struct RenderEnv *re)
#endif
{
	re->re_RenderMode = 0;
	re->re_BitmapMode = ft_pixel_mode_grays;
}

//==========================================================================================================================================================
// set_render_mode()
//==========================================================================================================================================================

static void set_render_mode(struct TTEngineBase *ttb, struct RenderEnv *re)
{
	USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
	struct BitMap *bm = re->re_TargetRPort->BitMap;

	antialias_on(re);

	switch (re->re_Antialias)
	{
		case TT_Antialias_Off: antialias_off(re); break;

		case TT_Antialias_Auto:
		{
			UWORD smin, smax, fsize;

			smin = re->re_LoadedFont->lf_NoAntialiasMin;
			smax = re->re_LoadedFont->lf_NoAntialiasMax;
			fsize = re->re_LoadedFont->lf_CachedSize->cs_PixelSize;

			if ((fsize >= smin) && (fsize <= smax)) antialias_off(re);
		}
	}

	#ifdef __amigaos4__
	if (ICyberGfx)
	#else
	if (CyberGfxBase)
	#endif
	{
		if (bm)
		{
			if (GetCyberMapAttr(bm, CYBRMATTR_ISCYBERGFX) == 0xFFFFFFFF)
			{
				if (GetCyberMapAttr(bm, CYBRMATTR_PIXFMT) == PIXFMT_LUT8)
				{
					antialias_off(re);
					re->re_DestBitmapType = DEST_LUT;
				}
				else re->re_DestBitmapType = DEST_RGB;
			}
			else
			{
				antialias_off(re);
				re->re_DestBitmapType = DEST_LUT;
			}
		}
	}
	else
	{
		antialias_off(re);
		re->re_DestBitmapType = DEST_LUT;
	}
}

//==========================================================================================================================================================
// clear_extent()
//==========================================================================================================================================================

void clear_extent(struct RenderEnv *re)
{
	re->re_TextExtent.te_Width = 0;
	re->re_TextExtent.te_Height = 0;
	re->re_TextExtent.te_Extent.MinX = 0;
	re->re_TextExtent.te_Extent.MinY = 0;
	re->re_TextExtent.te_Extent.MaxX = 0;
	re->re_TextExtent.te_Extent.MaxY = 0;
}

//==========================================================================================================================================================
// get_glyph_bitmap()
//==========================================================================================================================================================

struct CachedBitmap *get_glyph_bitmap(struct TTEngineBase *ttb, struct RenderEnv *re,
	ULONG index)
{
	USESYSBASE;
	struct CachedBitmap *cb;
	FT_Face face;
	FT_Int32 load_flags;

	face = re->re_LoadedFont->lf_Face;

	if (!(cb = cache_find_bitmap(ttb, re->re_LoadedFont->lf_CachedSize, index, re->re_BitmapMode)))
	{
		ObtainSemaphore(&re->re_LoadedFont->lf_Lock);

		/* Hint for the mode we will render: mono loads on the AA path
		 * wasted work and produced worse gray outlines on cache miss. */
		if (re->re_BitmapMode == ft_pixel_mode_mono)
			load_flags = FT_LOAD_TARGET_MONO;
		else
			load_flags = FT_LOAD_TARGET_NORMAL;

		if (!FT_Load_Glyph(face, index, load_flags))
		{
			if (!FT_Render_Glyph(face->glyph, re->re_RenderMode))
			{
				cb = cache_add_bitmap(ttb, re->re_LoadedFont->lf_CachedSize, face->glyph, index);
			}
		}

		ReleaseSemaphore(&re->re_LoadedFont->lf_Lock);
	}
	return cb;
}

//==========================================================================================================================================================
// pixmap_blit()
//==========================================================================================================================================================

const UBYTE mask[8] = {128, 64, 32, 16, 8, 4, 2, 1};

void pixmap_blit(struct CachedBitmap *scb, struct TT_Pixmap *dpm, LONG dx, LONG dy)
{
	UBYTE *dststart, *src;
	UWORD xi, yi, min_y = 0, max_y = scb->cb_Bitmap.rows, min_x = 0, max_x = scb->cb_Bitmap.width;

	dx += scb->cb_OffsetX;
	dy += scb->cb_OffsetY;
	dy = -dy;

	/* vertical clipping */

	if (dy < 0) min_y = -dy;
	if (dy + scb->cb_Bitmap.rows > (LONG)dpm->ttp_Height) max_y = dpm->ttp_Height - dy;

	/* horizontal clipping */

	if (dx < 0) min_x = -dx;
	if (dx + scb->cb_Bitmap.width > (LONG)dpm->ttp_Width) max_x = dpm->ttp_Width - dx;

	dststart = dpm->ttp_Data + dx + dy * dpm->ttp_Width;
	src = scb->cb_Bitmap.buffer;

	switch (scb->cb_Bitmap.pixel_mode)
	{
		case ft_pixel_mode_grays:
			for (yi = min_y; yi < max_y; yi++)
			{
				for (xi = min_x; xi < max_x; xi++) if (src[xi]) dststart[xi] = src[xi];
				src += scb->cb_Bitmap.pitch;
				dststart += dpm->ttp_Width;
			}
		break;

		case ft_pixel_mode_mono:
			for (yi = min_y; yi < max_y; yi++)
			{
				for (xi = min_x; xi < max_x; xi++) if (src[xi >> 3] & mask[xi & 7]) dststart[xi] = 0xFF;
				src += scb->cb_Bitmap.pitch;
				dststart += dpm->ttp_Width;
			}
		break;
	}
}

//==========================================================================================================================================================
// put_line()
//==========================================================================================================================================================

void put_line(struct TT_Pixmap *ttp, ULONG vpos)
{
	ULONG x;
	UBYTE *p;

	p = ttp->ttp_Data + vpos * ttp->ttp_Width;
	for (x = 0; x < ttp->ttp_Width; x++) *p++ = 0xFF;
}

//==========================================================================================================================================================
// singleline()
//==========================================================================================================================================================

void singleline(struct RenderEnv *re, struct TT_Pixmap *ttp, LONG where)
{
	LONG vpos, y;

	vpos = -re->re_TextExtent.te_Extent.MinY - where - (re->re_LoadedFont->lf_UnderThk >> 1);
	for (y = 0; y < re->re_LoadedFont->lf_UnderThk; y++) put_line(ttp, vpos + y);
}

//==========================================================================================================================================================
// doubleline()
//==========================================================================================================================================================

void doubleline(struct RenderEnv *re, struct TT_Pixmap *ttp, LONG where)
{
	LONG vpos, y, thk;

	thk = re->re_LoadedFont->lf_UnderThk;
	vpos = -re->re_TextExtent.te_Extent.MinY - where - (thk >> 1) - thk;

	for (y = 0; y < thk; y++)
	{
		put_line(ttp, vpos + y);
		put_line(ttp, vpos + y + (thk << 1));
	}
}

//==========================================================================================================================================================
// get_string_pixmap()
//==========================================================================================================================================================

void get_string_pixmap(struct TTEngineBase *ttb, struct RenderEnv *re, struct
	TT_Pixmap *pm, APTR string, ULONG count)
{
	UBYTE *string_pointer = string;    /* current position in input stream */
	ULONG i, prev_char, cur_char = 0;
	LONG xpos = 0, ypos = 0;
	BOOL kerning;
	FT_Face face = re->re_LoadedFont->lf_Face;
	struct CachedBitmap *cb;

	kerning = FT_HAS_KERNING(face);
	if (re->re_ForceFixed) kerning = 0;
	re->re_UnicodeInitial = TRUE;

	/* blitting glyph bitmaps */

	for (i = 0; i < count; i++)
	{
		prev_char = cur_char;
		cur_char = (*re->re_GetNextChar)(ttb, re, &string_pointer);
		cur_char = FT_Get_Char_Index(face, cur_char);
        
		if (prev_char && kerning && cur_char)
		{
			FT_Vector delta;

			FT_Get_Kerning(face, prev_char, cur_char, ft_kerning_unfitted, &delta);
			xpos += delta.x;
		}

		if ((cb = get_glyph_bitmap(ttb, re, cur_char)))
		{
			if (re->re_Flags & REF_AMIGAMETRICS)
			{
				pixmap_blit(cb, pm, (xpos >> 6), (ypos >> 6) - re->re_LoadedFont->lf_MaxTop);
			}
			else
			{
				pixmap_blit(cb, pm, (xpos >> 6) - re->re_TextExtent.te_Extent.MinX, re->re_TextExtent.te_Extent.MinY + (ypos >> 6));
			}

			/* Workaround for some broken monospaced fonts like Bitstream Vera Sans Mono */

			if (FT_IS_FIXED_WIDTH(face)) xpos += face->size->metrics.max_advance;
			else xpos += cb->cb_AdvanceX;
			ypos -= cb->cb_AdvanceY;
		}
	}

	/* perform software generated styles */

	if (re->re_SoftStyle & TT_SoftStyle_Underlined) singleline(re, pm, re->re_LoadedFont->lf_UnderPos);
	if (re->re_SoftStyle & TT_SoftStyle_DblUnderlined) doubleline(re, pm, re->re_LoadedFont->lf_UnderPos);
	if (re->re_SoftStyle & TT_SoftStyle_Overstriked) singleline(re, pm, re->re_LoadedFont->lf_StrikePos);
	if (re->re_SoftStyle & TT_SoftStyle_DblOverstriked) doubleline(re, pm, re->re_LoadedFont->lf_StrikePos);
}

//==========================================================================================================================================================
// extent_loop()
//==========================================================================================================================================================

void extent_loop(struct TTEngineBase *ttb, struct RenderEnv *re, APTR str, ULONG count)
{
	UBYTE *string_pointer = str;    /* current position in input stream */
	ULONG i, prev_char, cur_char = 0;
	LONG xpos = 0, ypos = 0, maxtop = 0, maxbot = 0, umax = -1000000;
	BOOL kerning, first_char = TRUE;
	FT_Face face = re->re_LoadedFont->lf_Face;
	struct CachedBitmap *cb = NULL;

	LD(kp("");)

	kerning = FT_HAS_KERNING(face);
	if (re->re_ForceFixed) kerning = FALSE;

	clear_extent(re);
	re->re_UnicodeInitial = TRUE;

	for (i = 0; i < count; i++)
	{
		prev_char = cur_char;
		cur_char = (*re->re_GetNextChar)(ttb, re, &string_pointer);
		cur_char = FT_Get_Char_Index(face, cur_char);

		if (prev_char && kerning && cur_char)
		{
			FT_Vector delta;
			FT_Get_Kerning(face, prev_char, cur_char, ft_kerning_unfitted, &delta);
			xpos += delta.x;
		}

		if ((cb = get_glyph_bitmap(ttb, re, cur_char)))
		{
			if (first_char)
			{
				first_char = FALSE;
				re->re_TextExtent.te_Extent.MinX = cb->cb_LeftBearing >> 6;
			}
                        
			if (cb->cb_TopBearing < maxtop) maxtop = cb->cb_TopBearing;
			if (cb->cb_BottomBearing > maxbot) maxbot = cb->cb_BottomBearing;

			/* Workaround for some broken monospaced fonts like Bitstream Vera Sans Mono */

			if (FT_IS_FIXED_WIDTH(face)) xpos += face->size->metrics.max_advance;
			else xpos += cb->cb_AdvanceX;
			ypos -= cb->cb_AdvanceY;
		}
	}
        
	re->re_TextExtent.te_Width = xpos >> 6;
	re->re_TextExtent.te_Height = re->re_LoadedFont->lf_CachedSize->cs_PixelSize;
	re->re_TextExtent.te_Extent.MaxX = (xpos >> 6) - 1;

	if (cb)
	{
		if (cb->cb_RightBearing != cb->cb_AdvanceX)
		{
			re->re_TextExtent.te_Extent.MaxX -= cb->cb_RightBearing >> 6;
		}
	}

	/* soft italic MaxX and length correction */

	if (re->re_LoadedFont->lf_Node.ln_Name[0] == '/')
	{
		LONG cr = (-cb->cb_TopBearing + 128) >> 8;

		re->re_TextExtent.te_Extent.MaxX += cr;
	}

	re->re_TextExtent.te_Extent.MinY = maxtop >> 6;
	re->re_TextExtent.te_Extent.MaxY = (maxbot >> 6) - 1;

	/* account MaxY for underline styles */

	if (re->re_SoftStyle & TT_SoftStyle_Underlined)
	{
		umax = -re->re_LoadedFont->lf_UnderPos - (re->re_LoadedFont->lf_UnderThk >> 1) + re->re_LoadedFont->lf_UnderThk;
	}

	if (re->re_SoftStyle & TT_SoftStyle_DblUnderlined)
	{
	umax = -re->re_LoadedFont->lf_UnderPos - (re->re_LoadedFont->lf_UnderThk >> 1) + re->re_LoadedFont->lf_UnderThk * 3;
	}

	if (umax > re->re_TextExtent.te_Extent.MaxY) re->re_TextExtent.te_Extent.MaxY = umax;
}

//=====================================================================================================
// render hook
//=====================================================================================================

#ifdef __amigaos4__

ULONG render_hook(struct Hook *hook, struct RastPort *rp, struct CRHookMsg *msg)
{

#elif __MORPHOS__

ULONG render_hook(void)
{
	struct Hook *h = (struct Hook*)REG_A0;
	struct RastPort *rp = (struct RastPort*)REG_A2;
	struct CRHookMsg *msg = (struct CRHookMsg*)REG_A1;

#else

__ASM__ __SAVE_DS__ ULONG render_hook(__REG__(a0, struct Hook *h), __REG__(a2, struct RastPort *rp), __REG__(a1, struct CRHookMsg *msg))
{

#endif

	struct CrhUData *crhud = (struct CrhUData*)h->h_Data;
	struct TTEngineBase *ttb = crhud->ttb;
	USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
	USELIB(GfxBase, IGraphics, GraphicsIFace);
	APTR bitmap_lock;
	struct DirBlitData dbd;

	dbd.px = crhud->px;
	dbd.re = crhud->re;
	dbd.srcx = msg->offsetx - crhud->srcx;
	dbd.srcy = msg->offsety - crhud->srcy;
	dbd.bounds = msg->bounds;
	dbd.ttb = crhud->ttb;
	dbd.background = GetDrMd(crhud->re->re_TargetRPort) & JAM2;
	dbd.xor = GetDrMd(crhud->re->re_TargetRPort) & COMPLEMENT;

	/* Time to lock bitmap and get its base address and modulo. */

	if (bitmap_lock = LockBitMapTags(rp->BitMap,
		LBMI_BYTESPERROW, (ULONG)&dbd.bitmap_mod,
		LBMI_BASEADDRESS, (ULONG)&dbd.bitmap_base,
	TAG_END))
	{
		crhud->blitfunc(&dbd);
		UnLockBitMap(bitmap_lock);
	}

	return 0;
}

//==========================================================================================================================================================
// render()
//==========================================================================================================================================================

void render(struct TTEngineBase *ttb, struct RenderEnv *re, APTR str, ULONG count)
{
	struct TT_Pixmap px;
	USELIB(GfxBase, IGraphics, GraphicsIFace);
	USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
	USELIB(LayersBase, ILayers, LayersIFace);
//	USESYSBASE;
	ULONG px_size, drawmode;
	LONG x_pos, y_pos;
	struct RastPort *rp = re->re_TargetRPort;

	LD(kp("");)

	drawmode = GetDrMd(rp);

	/* get text extent first */

	extent_loop(ttb, re, str, count);

	/* calculate pixmap size */

	px.ttp_Size = sizeof(struct TT_Pixmap);

	if (re->re_Flags & REF_AMIGAMETRICS)
	{
		px.ttp_Height = re->re_LoadedFont->lf_MaxTop + re->re_LoadedFont->lf_MaxBottom;
		px.ttp_Width = re->re_TextExtent.te_Width;
	}
	else
	{
		px.ttp_Height = re->re_TextExtent.te_Extent.MaxY - re->re_TextExtent.te_Extent.MinY + 1;
		px.ttp_Width = re->re_TextExtent.te_Extent.MaxX - re->re_TextExtent.te_Extent.MinX + 1;
	}
	px_size = px.ttp_Width * px.ttp_Height;

	/* expand pixmap if too small */

	if (px_size > re->re_PixmapSize)
	{
		if (!expand_pixmap(ttb, re, px_size)) return;
	}

	/* clear pixmap */

	px.ttp_Data = re->re_Pixmap;
	bzero(re->re_Pixmap, re->re_PixmapSize);

	/* fill pixmap with glyphs */

	get_string_pixmap(ttb, re, &px, str, count);

	LD(kp("string pixmap filled.");)

	/* check for INVERSE VIDEO mode, negate pixmap if set (only RGB destination) */

	if ((drawmode & INVERSVID) && (re->re_DestBitmapType == DEST_RGB))
	{
		ULONG i;

		for (i = 0; i < px_size; i++) px.ttp_Data[i] = ~px.ttp_Data[i];
	}

	/* draw position */

	if (re->re_Flags & REF_AMIGAMETRICS)
	{
		x_pos = rp->cp_x;
		y_pos = rp->cp_y - re->re_LoadedFont->lf_MaxTop;
	}
	else
	{
		x_pos = rp->cp_x + re->re_TextExtent.te_Extent.MinX;
		y_pos = rp->cp_y + re->re_TextExtent.te_Extent.MinY;
	}

	/* alphablend or draw mask buffer to RastPort. If destination bitmap is  */
	/* LUT, transparency and antialiasing is ignored, COMPLEMENT is handled  */
	/* automatically by WritePixel()                                         */

	if (re->re_DestBitmapType == DEST_LUT)
	{
		if (drawmode & JAM2) draw_jam2(ttb, &px, re, x_pos, y_pos);
		else draw_jam1(ttb, &px, re, x_pos, y_pos);
	}
	else
	{
		/* Destination bitmap is direct RGB color of some format. Then we check this format */
    /* and if it is one of directly supported formats, we call DoHookClipRects(). Else  */
    /* we just fall back to traditional ReadPixelArray()/WritePixelArray() code. It can */
    /* be assumed here, destination bitmap is CGX one, so I do not check it again.      */
		/* Almost all boards known to CGX have linear video memory, but it is checked here  */
		/* just to be on the safe side. If memory is not linear, standard code is called.   */

		ULONG pixfmt, is_linear;
		void(*blitfunc)(struct DirBlitData*) = NULL;

		pixfmt = GetCyberMapAttr(re->re_TargetRPort->BitMap, CYBRMATTR_PIXFMT);
		is_linear = GetCyberMapAttr(re->re_TargetRPort->BitMap, CYBRMATTR_ISLINEARMEM);

		if (!is_linear) pixfmt = 0xFFFFFFFF;   // non existing mode, to get to 'default' switch entry.

		switch (pixfmt)
		{

			case PIXFMT_BGRA32: blitfunc = dirblit_bgra32; break;

      default:
				if (re->re_Transparency)
				{
					if (drawmode & JAM2)
					{
						if (drawmode & COMPLEMENT) transp_jam2_xor(ttb, &px, re, x_pos, y_pos);
						else transp_jam2(ttb, &px, re, x_pos, y_pos);
					}
					else
					{
						if (drawmode & COMPLEMENT) transp_jam1_xor(ttb, &px, re, x_pos, y_pos);
						else transp_jam1(ttb, &px, re, x_pos, y_pos);
					}
				}
				else
				{
					if (drawmode & JAM2)
					{
						if (drawmode & COMPLEMENT) blend_jam2_xor(ttb, &px, re, x_pos, y_pos);
						else blend_jam2(ttb, &px, re, x_pos, y_pos);
					}
					else
					{
						if (drawmode & COMPLEMENT) blend_jam1_xor(ttb, &px, re, x_pos, y_pos);
						else blend_jam1(ttb, &px, re, x_pos, y_pos);
					}
				}
			break;
		}

		LD(kp1("blitfunc = $%08lx.", blitfunc);)

		if (blitfunc)
		{
			struct Rectangle hrect;
			struct CrhUData crhud;
			struct Hook h = {{NULL, NULL}, NULL, NULL, NULL};

			#ifdef __MORPHOS__
			struct EmulLibEntry gate = {TRAP_LIB, 0, (void(*)(void))&render_hook};
			#endif

			hrect.MinX = x_pos;
			hrect.MinY = y_pos;
			hrect.MaxX = x_pos + px.ttp_Width - 1;
			hrect.MaxY = y_pos + px.ttp_Height - 1;
			crhud.px = &px;
			crhud.re = re;
			crhud.ttb = ttb;
			crhud.srcx = x_pos;
			crhud.srcy = y_pos;
			crhud.blitfunc = blitfunc;
			h.h_Data = (APTR)&crhud;

			#ifdef __MORPHOS__
			h.h_Entry = (HOOKFUNC)&gate;
			#else
			h.h_Entry = (HOOKFUNC)render_hook;
			#endif

			DoHookClipRects(&h, re->re_TargetRPort, &hrect);
		}
	}

	/* advance RastPort pen positions */

	rp->cp_x += re->re_TextExtent.te_Width;
}




/* ========== FONT NAMES EXTRACTION ========== */

/// language crosstables

struct LangTbl
{
	STRPTR name;
	UWORD  win;
};

#define MS_SUBLANG_MASK 0x03FF

struct LangTbl MSLangTable[] = {
	{"english", TT_MS_LANGID_ENGLISH_UNITED_STATES & MS_SUBLANG_MASK},
	{"polski", TT_MS_LANGID_POLISH_POLAND & MS_SUBLANG_MASK},
	{"catal?", TT_MS_LANGID_CATALAN_SPAIN & MS_SUBLANG_MASK},
	{"hrvatski", TT_MS_LANGID_CROATIAN_CROATIA & MS_SUBLANG_MASK},
	{"czech", TT_MS_LANGID_CZECH_CZECH_REPUBLIC & MS_SUBLANG_MASK},
	{"dansk", TT_MS_LANGID_DANISH_DENMARK & MS_SUBLANG_MASK},
	{"nederlands", TT_MS_LANGID_DUTCH_NETHERLANDS & MS_SUBLANG_MASK},
	{"suomi", TT_MS_LANGID_FINNISH_FINLAND & MS_SUBLANG_MASK},
	{"fran?ais", TT_MS_LANGID_FRENCH_FRANCE & MS_SUBLANG_MASK},
	{"deutsch", TT_MS_LANGID_GERMAN_GERMANY & MS_SUBLANG_MASK},
	{"magyar", TT_MS_LANGID_HUNGARIAN_HUNGARY & MS_SUBLANG_MASK},
	{"?slenska", TT_MS_LANGID_ICELANDIC_ICELAND & MS_SUBLANG_MASK},
	{"italiano", TT_MS_LANGID_ITALIAN_ITALY & MS_SUBLANG_MASK},
	{"nihongo", TT_MS_LANGID_JAPANESE_JAPAN & MS_SUBLANG_MASK},
	{"norsk", TT_MS_LANGID_NORWEGIAN_NORWAY_BOKMAL & MS_SUBLANG_MASK},
	{"farsi", TT_MS_LANGID_FARSI_IRAN & MS_SUBLANG_MASK},
	{"portugu?s", TT_MS_LANGID_PORTUGUESE_PORTUGAL & MS_SUBLANG_MASK},
	{"portugu?s-brasil", TT_MS_LANGID_PORTUGUESE_BRAZIL & MS_SUBLANG_MASK},
	{"russian", TT_MS_LANGID_RUSSIAN_RUSSIA & MS_SUBLANG_MASK},
	{"srpski", TT_MS_LANGID_SERBIAN_SERBIA_LATIN & MS_SUBLANG_MASK},
	{"slovak", TT_MS_LANGID_SLOVAK_SLOVAKIA & MS_SUBLANG_MASK},
	{"slovensko", TT_MS_LANGID_SLOVENE_SLOVENIA & MS_SUBLANG_MASK},
	{"espa?ol", TT_MS_LANGID_SPANISH_SPAIN_TRADITIONAL_SORT & MS_SUBLANG_MASK},
	{"svenska", TT_MS_LANGID_SWEDISH_SWEDEN & MS_SUBLANG_MASK},
	{"t?rk?e", TT_MS_LANGID_TURKISH_TURKEY & MS_SUBLANG_MASK},
	{"greek", TT_MS_LANGID_GREEK_GREECE & MS_SUBLANG_MASK},
	{"hellenic", TT_MS_LANGID_GREEK_GREECE & MS_SUBLANG_MASK},
	{NULL, 0}
};

STRPTR DefLanguages[] = {"english", NULL};

///
/// search_ms_langtable()

UWORD search_ms_langtable(struct TTEngineBase *ttb, STRPTR language)
  {
    USELIB(UtilityBase, IUtility, UtilityIFace);
    struct LangTbl *ptr;
    LONG index = 0;

    LD(kp(""));
    for (ptr = MSLangTable; ptr->name; ptr++)
      {
        if (Stricmp(ptr->name, language) == 0) return ptr->win;
        index++;
      }
    return -1;
  }

///
/// font_name_find()

LONG font_name_find(APTR font, UWORD plat, UWORD enco, UWORD lang, UWORD name)
  {
    LONG i, max;
    struct LoadedFont *lf = (struct LoadedFont*)font;
    FT_SfntName nm;

    LD(kp(""));
    max = FT_Get_Sfnt_Name_Count(lf->lf_Face);

    for (i = 0; i < max; i++)
      {
        FT_Get_Sfnt_Name(lf->lf_Face, i, &nm);

        /* Drop Microsoft sublanguages */

        if (nm.platform_id == TT_PLATFORM_MICROSOFT) nm.language_id &= MS_SUBLANG_MASK;

        if ((nm.platform_id == plat) && (nm.encoding_id == enco) &&
          (nm.language_id == lang) && (nm.name_id == name)) return i;
      }
    return -1;
  }

///
/// extract_name()

VOID extract_name(struct TTEngineBase *ttb, struct LoadedFont *lf, LONG index, struct FontName *fn)
  {
    USESYSBASE;
    UWORD i;
    FT_SfntName nm;

    LD(kp(""));
    FT_Get_Sfnt_Name(lf->lf_Face, index, &nm);
    fn->fn_Length = nm.string_len >> 1;

    if ((fn->fn_Text = AllocPooled(ttb->ttb_MemPool, fn->fn_Length + 1)))
      {
        for (i = 0; i < nm.string_len; i += 2) fn->fn_Text[i >> 1] = nm.string[i + 1];
        fn->fn_Text[nm.string_len >> 1] = 0x00;
      }
  }

///
//==========================================================================================================================================================
// extract_name8()
//==========================================================================================================================================================

void extract_name8(struct TTEngineBase *ttb, struct LoadedFont *lf, LONG index, struct FontName *fn)
{
	USESYSBASE;
	UWORD i;
	FT_SfntName nm;

	FT_Get_Sfnt_Name(lf->lf_Face, index, &nm);
	fn->fn_Length = nm.string_len;

	if ((fn->fn_Text = AllocPooled(ttb->ttb_MemPool, fn->fn_Length + 1)))
	{
		for (i = 0; i < nm.string_len; i++) fn->fn_Text[i] = nm.string[i];
		fn->fn_Text[nm.string_len] = 0x00;
	}
}

//==========================================================================================================================================================
// font_name_get()
//==========================================================================================================================================================

void font_name_get(struct TTEngineBase *ttb, APTR font, UWORD type)
{
	USELIB(LocaleBase, ILocale, LocaleIFace);
	LONG index = -1, mslang = -1;
	struct Locale *locale;
	STRPTR *langtable = DefLanguages;
	struct LoadedFont *lf = (struct LoadedFont*)font;
	struct FontName *fn;
	WORD i;

	switch (type)
	{
		case TT_NAME_ID_FONT_FAMILY:      fn = &lf->lf_Family;    break;
		case TT_NAME_ID_FONT_SUBFAMILY:   fn = &lf->lf_Name;      break;
		case TT_NAME_ID_FULL_NAME:
		default:                          fn = &lf->lf_FullName;
	}

	#ifdef __amigaos4__
	if (ILocale)
	#else
	if (LocaleBase)
	#endif
	{
		locale = OpenLocale(NULL);
		langtable = locale->loc_PrefLanguages;
	}

	for (i = 0; i < 10; i++)
	{
		if (langtable[i])
		{
			if ((mslang = search_ms_langtable(ttb, langtable[i])) != -1)
			{
				if ((index = font_name_find(font, TT_PLATFORM_MICROSOFT, TT_MS_ID_UNICODE_CS, mslang, type)) != -1)
				{
					extract_name(ttb, lf, index, fn);
					return;
				}
			}
		}
		else break;
	}

	for (i = 0; i < 10; i++)
	{
		if (langtable[i])
		{
			if ((mslang = search_ms_langtable(ttb, langtable[i])) != -1)
			{
				if ((index = font_name_find(font, TT_PLATFORM_MICROSOFT, TT_MS_ID_SYMBOL_CS, mslang, type)) != -1)
				{
					extract_name(ttb, lf, index, fn);
					return;
				}
			}
		}
		else break;
	}

	if ((index = font_name_find(font, TT_PLATFORM_APPLE_UNICODE, TT_APPLE_ID_DEFAULT, 0, type)) != -1)
	{
		extract_name(ttb, lf, index, fn);
		return;
	}

	if ((index = font_name_find(font, TT_PLATFORM_MACINTOSH, TT_MAC_ID_ROMAN, TT_MAC_LANGID_ENGLISH, type)) != -1)
	{
		extract_name8(ttb, lf, index, fn);
		return;
	}
}

///

/* ========== FONT LIST FUNCTIONS ========== */

/// fontlist_init()

void fontlist_init(struct TTEngineBase *ttb)
  {
    LD(kp(""));
    ttb->ttb_FontList.lh_Head = (struct Node*)&ttb->ttb_FontList.lh_Tail;
    ttb->ttb_FontList.lh_Tail = NULL;
    ttb->ttb_FontList.lh_TailPred = (struct Node*)&ttb->ttb_FontList.lh_Head;
    return;
  }

///
/// fontlist_find()

struct LoadedFont *fontlist_find(struct TTEngineBase *ttb, STRPTR path, UWORD size)
{
        USESYSBASE;
        struct LoadedFont *lf;

        ObtainSemaphoreShared(ttb->ttb_Semaphore);
        lf = (struct LoadedFont*)&ttb->ttb_FontList;

        while ((lf = (struct LoadedFont*)FindName((struct List*)lf, path)))
        {
                if (lf->lf_CachedSize->cs_PixelSize == size) break;
        }

        ReleaseSemaphore(ttb->ttb_Semaphore);
        return lf;
}

///
/// fontlist_add()

struct LoadedFont *fontlist_add(struct TTEngineBase *ttb, STRPTR name)
{
        USESYSBASE;
        struct LoadedFont *new_lf;


        if ((new_lf = AllocPooled(ttb->ttb_MemPool, sizeof(struct LoadedFont))))
        {
                new_lf->lf_Name.fn_Text = NULL;
                new_lf->lf_Family.fn_Text = NULL;
                new_lf->lf_FullName.fn_Text = NULL;
                new_lf->lf_OpenCnt = 0;
                new_lf->lf_Flags = 0;
                InitSemaphore(&new_lf->lf_Lock);

                if ((new_lf->lf_Node.ln_Name = AllocPooled(ttb->ttb_MemPool, strlen(name) + 1)))
                {
                        strcpy(new_lf->lf_Node.ln_Name, name);
                        ObtainSemaphore(ttb->ttb_Semaphore);
                        AddHead(&ttb->ttb_FontList, &new_lf->lf_Node);
                        ReleaseSemaphore(ttb->ttb_Semaphore);
                        return new_lf;
                }
                else FreePooled(ttb->ttb_MemPool, new_lf, sizeof(struct LoadedFont));
        }
        return NULL;
}

///
/// fontlist_remove()

void fontlist_remove(struct TTEngineBase *ttb, struct LoadedFont *lf)
  {
    USESYSBASE;

    LD(kp(""));
    ObtainSemaphore(ttb->ttb_Semaphore);
    Remove(&lf->lf_Node);
    ReleaseSemaphore(ttb->ttb_Semaphore);
    if (lf->lf_Name.fn_Text) FreePooled(ttb->ttb_MemPool, lf->lf_Name.fn_Text,
      lf->lf_Name.fn_Length + 1);
    if (lf->lf_Family.fn_Text) FreePooled(ttb->ttb_MemPool, lf->lf_Family.fn_Text,
      lf->lf_Family.fn_Length + 1);
    if (lf->lf_FullName.fn_Text) FreePooled(ttb->ttb_MemPool, lf->lf_FullName.fn_Text,
      lf->lf_FullName.fn_Length + 1);
    if (lf->lf_Node.ln_Name) FreePooled(ttb->ttb_MemPool, lf->lf_Node.ln_Name,
      strlen(lf->lf_Node.ln_Name) + 1);
    FreePooled(ttb->ttb_MemPool, lf, sizeof(struct LoadedFont));
  }

///

/* ========== ENVIRONMENT LIST FUNCTIONS ========== */

//==========================================================================================================================================================
// envlist_init()
//==========================================================================================================================================================

void envlist_init(struct TTEngineBase *ttb)
{
	LD(kp(""));
	ttb->ttb_EnvList.mlh_Head = (struct MinNode*)&ttb->ttb_EnvList.mlh_Tail;
	ttb->ttb_EnvList.mlh_Tail = NULL;
	ttb->ttb_EnvList.mlh_TailPred = (struct MinNode*)&ttb->ttb_EnvList.mlh_Head;
	return;
}

//==========================================================================================================================================================
// envlist_find()
//==========================================================================================================================================================

struct RenderEnv *envlist_find(struct TTEngineBase *ttb, struct RastPort *rp)
{
	USESYSBASE;
	struct RenderEnv *re, *found_re = NULL;

	LD(kp("");)

	ObtainSemaphoreShared(ttb->ttb_Semaphore);

  for (re = (struct RenderEnv*)ttb->ttb_EnvList.mlh_Head; re->re_Node.mln_Succ; re = (struct RenderEnv*)re->re_Node.mln_Succ)
	{
		if (rp == re->re_TargetRPort) found_re = re;
	}

	ReleaseSemaphore(ttb->ttb_Semaphore);

	return found_re;
}

//==========================================================================================================================================================
// envlist_add()
//==========================================================================================================================================================

struct RenderEnv *envlist_add(struct TTEngineBase *ttb, struct RastPort *rp)
{
	USESYSBASE;
	struct RenderEnv *new_re;

	LD(kp(""));

	if ((new_re = AllocPooled(ttb->ttb_MemPool, sizeof(struct RenderEnv))))
	{
		new_re->re_TargetRPort = rp;
		new_re->re_LoadedFont = NULL;
		new_re->re_Encoding = TT_Encoding_Default;
		set_encoding_callback(ttb, new_re);
		new_re->re_Flags = 0;
		new_re->re_SoftStyle = 0;
		new_re->re_Matrix.xx = 0x00010000;
		new_re->re_Matrix.xy = 0x00000000;
		new_re->re_Matrix.yx = 0x00000000;
		new_re->re_Matrix.yy = 0x00010000;
		new_re->re_Background = TT_Background_UseRastPort;
		new_re->re_Foreground = TT_Foreground_UseRastPort;
		new_re->re_CustomEncTable = NULL;
		new_re->re_GammaCf = ttb->ttb_DefGamma;
		new_re->re_ForceFixed = 0;
		new_re->re_Pixmap = NULL;
		new_re->re_PixmapSize = 0;
		new_re->re_RenderBuffer = NULL;
		new_re->re_RendBufSize = 0;
		calculate_gamma(ttb, new_re);

		ObtainSemaphore(ttb->ttb_Semaphore);
		AddHead((struct List*)&ttb->ttb_EnvList, (struct Node*)&new_re->re_Node);
		ReleaseSemaphore(ttb->ttb_Semaphore);
		return new_re;
	}
	return NULL;
}

//==========================================================================================================================================================
// envlist_remove()
//==========================================================================================================================================================

void envlist_remove(struct TTEngineBase *ttb, struct RastPort *rp)
{
	USESYSBASE;

	struct RenderEnv *rem_re;

	LD(kp(""));

	if ((rem_re = envlist_find(ttb, rp)))
	{
		ObtainSemaphore(ttb->ttb_Semaphore);
		Remove((struct Node*)&rem_re->re_Node);
		ReleaseSemaphore(ttb->ttb_Semaphore);
		if (rem_re->re_RenderBuffer) FreePooled(ttb->ttb_MemPool, rem_re->re_RenderBuffer, rem_re->re_RendBufSize);
		if (rem_re->re_Pixmap) FreePooled(ttb->ttb_MemPool, rem_re->re_Pixmap, rem_re->re_PixmapSize);
		FreePooled(ttb->ttb_MemPool, rem_re, sizeof(struct RenderEnv));
	}
}

/// load_codepage()
static void load_codepage(struct TTEngineBase *ttb)
  {
    USESYSBASE;
    USELIB(DOSBase, IDOS, DOSIFace);
    BPTR codepage;

    /* load codepage */

    LD(kp(""));
    ttb->ttb_CodePage = NULL;

    if ((codepage = Open("ENV:ttfcodepage", MODE_OLDFILE)))
      {
        if ((ttb->ttb_CodePage = AllocPooled(ttb->ttb_MemPool, 512)))
          {
            if (Read(codepage, ttb->ttb_CodePage, 512) != 512)
              {
                 FreePooled(ttb->ttb_MemPool, ttb->ttb_CodePage, 512);
                 ttb->ttb_CodePage = NULL;
              }
          }
        Close(codepage);
      }
  }

///
/// free_codepage()

static void free_codepage(struct TTEngineBase *ttb)
  {
    USESYSBASE;

    LD(kp(""));
    if (ttb->ttb_CodePage) FreePooled(ttb->ttb_MemPool, ttb->ttb_CodePage, 512);
  }

///

/* ========== FONT DATABASE ========== */

#define MAXLINE 160

/// check_pattern()

static BOOL check_pattern(struct TTEngineBase *ttb, STRPTR line, STRPTR pattern, struct Parser *parser)
  {
    USELIB(DOSBase, IDOS, DOSIFace);

    parser->args->RDA_Source.CS_Buffer = line;
    parser->args->RDA_Source.CS_Length = strlen(line);
    parser->args->RDA_Source.CS_CurChr = 0;
    parser->args->RDA_DAList = 0;
    parser->args->RDA_Buffer = NULL;
    parser->args->RDA_BufSiz = 0;
    parser->args->RDA_ExtHelp = NULL;
    parser->args->RDA_Flags = RDAF_NOPROMPT;
    if (ReadArgs(pattern, parser->params, parser->args)) return TRUE;
    return FALSE;
  }

///
/// parse_line()

void parse_line(struct TTEngineBase *ttb, STRPTR line, struct Parser *parser)
{
        USESYSBASE;
        USELIB(DOSBase, IDOS, DOSIFace);
        USELIB(UtilityBase, IUtility, UtilityIFace);

        LONG def_weight = 400;
        LONG def_smoothsmall = DEFAULT_NO_ANTIALIAS_MIN - 1;
        LONG def_smoothbig = DEFAULT_NO_ANTIALIAS_MAX + 1;
        LONG def_fixed = 0;
        STRPTR def_style = "regular";

        /* set defaults */

        parser->params[0] = 0;
        parser->params[1] = (LONG)&def_weight;
        parser->params[2] = (LONG)def_style;
        parser->params[3] = (LONG)&def_smoothsmall;
        parser->params[4] = (LONG)&def_smoothbig;
        parser->params[5] = (LONG)&def_fixed;

        if (check_pattern(ttb, line, "FAMILY/A/K", parser))
        {
                struct TTFamily *family = NULL;

                if ((family = AllocPooled(ttb->ttb_MemPool, sizeof(struct TTFamily))))
                {
                        if ((family->node.ln_Name = AllocPooled(ttb->ttb_MemPool, strlen((STRPTR)parser->params[0]) + 1)))
                        {
                                strcpy(family->node.ln_Name, (STRPTR)parser->params[0]);
                                family->alias = NULL;
                                family->faces.lh_Head = (struct Node*)&family->faces.lh_Tail;
                                family->faces.lh_Tail = NULL;
                                family->faces.lh_TailPred = (struct Node*)&family->faces.lh_Head;
                                AddTail(&ttb->ttb_DataBase, &family->node);
                                parser->current_family = family;
                        }
                        else FreePooled(ttb->ttb_MemPool, family, sizeof(struct TTFamily));
                }
                
    parser->current_family = family;
                FreeArgs(parser->args);
                return;
        }

        if (check_pattern(ttb, line, "ALIAS/A/K", parser))
        {
                struct TTFamily *alias;

                if ((alias = AllocPooled(ttb->ttb_MemPool, sizeof(struct TTFamily))))
                {
                        if ((alias->node.ln_Name = AllocPooled(ttb->ttb_MemPool, strlen((STRPTR)parser->params[0]) + 1)))
                        {
                                strcpy(alias->node.ln_Name, (STRPTR)parser->params[0]);
                                alias->alias = parser->current_family;
                                alias->faces.lh_Head = (struct Node*)&alias->faces.lh_Tail;
                                alias->faces.lh_Tail = NULL;
                                alias->faces.lh_TailPred = (struct Node*)&alias->faces.lh_Head;
                                AddTail(&ttb->ttb_DataBase, &alias->node);
                        }
                        else FreePooled(ttb->ttb_MemPool, alias, sizeof(struct TTFamily));
                }
                
    FreeArgs(parser->args);
                return;
        }

        if (check_pattern(ttb, line, "FILE/A/K,WEIGHT/N/K,STYLE/K,SMOOTHSMALL/N/K,SMOOTHBIG/N/K,FIXED/N/K", parser))
        {
					struct TTFont *font;
					
					if (parser->current_family)
					{
						if ((font = AllocPooled(ttb->ttb_MemPool, sizeof(struct TTFont))))
						{
							font->node.ln_Name = "face";

							if ((font->filepath = AllocPooled(ttb->ttb_MemPool, strlen((STRPTR)parser->params[0]) + 1)))
							{
								strcpy(font->filepath, (STRPTR)parser->params[0]);
								font->weight = *(LONG*)parser->params[1];
								font->style = TT_FontStyle_Regular;
								if (Stricmp((STRPTR)parser->params[2], "ITALIC") == 0) font->style |= TT_FontStyle_Italic;
								font->smoothsmall = *(ULONG*)parser->params[3];
								font->smoothbig = *(ULONG*)parser->params[4];
								font->fixed = *(ULONG*)parser->params[5];
								AddTail(&parser->current_family->faces, &font->node);
							}
							else FreePooled(ttb->ttb_MemPool, font, sizeof(struct TTFont));
						}
					}
					FreeArgs(parser->args);
					return;
        }

        if (check_pattern(ttb, line, "CACHELIMIT/A/N/K", parser))
        {
                ttb->ttb_MemCacheLimit = *(ULONG*)parser->params[0];
                return;
        }

        if (check_pattern(ttb, line, "GAMMA/A/N/K", parser))
        {
                ttb->ttb_DefGamma = *(ULONG*)parser->params[0];
                return;
        }
}

///
/// load_database()

static BOOL load_database(struct TTEngineBase *ttb)
  {
    USELIB(DOSBase, IDOS, DOSIFace);

    struct Parser parser;
    UBYTE line[MAXLINE];
    BPTR base;

    LD(kp(""));
    if ((parser.args = AllocDosObject(DOS_RDARGS, NULL)))
      {
        parser.current_family = NULL;
        if ((base = Open("ENV:ttengine.database", MODE_OLDFILE)))
          {
            while (FGets(base, line, MAXLINE))
              {
                parse_line(ttb, line, &parser);
              }
            Close(base);
            return TRUE;
          }
      }
    return FALSE;
  }

///
/// dispose_database()

void dispose_database(struct TTEngineBase *ttb)
  {
    USESYSBASE;

    struct TTFamily *family, *next_family;
    struct TTFont *font, *next_font;

    LD(kp(""));
    family = (struct TTFamily*)ttb->ttb_DataBase.lh_Head;

    while ((next_family = (struct TTFamily*)family->node.ln_Succ))
      {
        Remove(&family->node);
        font = (struct TTFont*)family->faces.lh_Head;
        while ((next_font = (struct TTFont*)font->node.ln_Succ))
          {
            Remove(&font->node);
            if (font->filepath) FreePooled(ttb->ttb_MemPool, font->filepath,
              strlen(font->filepath) + 1);
            FreePooled(ttb->ttb_MemPool, font, sizeof(struct TTFont));
            font = next_font;
          }
        if (family->node.ln_Name) FreePooled(ttb->ttb_MemPool, family->node.ln_Name,
          strlen(family->node.ln_Name) + 1);
        FreePooled(ttb->ttb_MemPool, family, sizeof(struct TTFamily));
        family = next_family;
      }
    return;
  }

///

/* ========== SYSTEM LIBRARY INTERFACE ========== */

/// LibInit()

/* moved to lib_header.c */

///
/// init_resources()

static struct TTEngineBase *init_resources(struct TTEngineBase *ttb)
{
	USESYSBASE;
	LONG ft_error;

	if (!(ttb->ttb_Semaphore = AllocMem(sizeof(struct SignalSemaphore), MEMF_PUBLIC | MEMF_CLEAR))) return NULL;

	InitSemaphore(ttb->ttb_Semaphore);

	if (!(ttb->ttb_UtilityBase = OpenLibrary("utility.library", 39))) return NULL;

	#if !defined(__MORPHOS__) && !defined(__amigaos4__)
	__UtilityBase = ttb->ttb_UtilityBase;
	#endif

	if (!(ttb->ttb_GfxBase = OpenLibrary("graphics.library", 39))) return NULL;
	if (!(ttb->ttb_DOSBase = OpenLibrary("dos.library", 38))) return NULL;
	if (!(ttb->ttb_LayersBase = OpenLibrary("layers.library", 39))) return NULL;
	
	#ifndef __amigaos4__
	if (!(ttb->ttb_MathIeeeSingBasBase = OpenLibrary("mathieeesingbas.library", 37))) return NULL;
	if (!(ttb->ttb_MathIeeeSingTransBase = OpenLibrary("mathieeesingtrans.library", 37))) return NULL;
	#endif

	if (!(ttb->ttb_MemPool = CreatePool(MEMF_ANY | MEMF_CLEAR, 4096, 4096))) return NULL;

	ttb->ttb_LocaleBase = OpenLibrary("locale.library", 39); /* optional */
	ttb->ttb_CyberGfxBase = OpenLibrary("cybergraphics.library", 41); /* optional */

	#ifdef __amigaos4__
	if(!(ttb->ttb_IUtility = (struct UtilityIFace *) GetInterface(ttb->ttb_UtilityBase, "main", 1, NULL))) return NULL;
	if(!(ttb->ttb_IGraphics = (struct GraphicsIFace *) GetInterface(ttb->ttb_GfxBase, "main", 1, NULL))) return NULL;
	if(!(ttb->ttb_IDOS = (struct DOSIFace *) GetInterface(ttb->ttb_DOSBase, "main", 1, NULL))) return NULL;
	if(!(ttb->ttb_ILocale = (struct LocaleIFace *) GetInterface(ttb->ttb_LocaleBase, "main", 1, NULL))) return NULL;
	if(!(ttb->ttb_ICyberGfx = (struct CyberGfxIFace *) GetInterface(ttb->ttb_CyberGfxBase, "main", 1, NULL))) return NULL;
	if(!(ttb->ttb_ILayers = (struct LayersIFace *) GetInterface(ttb->ttb_LayersBase, "main", 1, NULL))) return NULL;
	#endif

	load_codepage(ttb);
	load_database(ttb);
	Ttb = ttb;

	if ((ft_error = FT_Init_FreeType(&ttb->ttb_FTLib)) != 0)
	{
		ttb->ttb_FTLib = NULL;
		return NULL;
	}

	ttb->ttb_InitFlag = TRUE;
	return ttb;
}

///
/// free_resources()

static void free_resources(struct TTEngineBase *ttb)
{
	USESYSBASE;

	LD(kp(""));
	if (!ttb->ttb_InitFlag) return;   /* do not free resources twice */

	if (ttb->ttb_FTLib) FT_Done_FreeType(ttb->ttb_FTLib);

	#ifdef __amigaos4__
	if (ttb->ttb_ILayers) DropInterface((struct Interface*) ttb->ttb_ILayers);
	if (ttb->ttb_ILocale) DropInterface((struct Interface*) ttb->ttb_ILocale);
	if (ttb->ttb_ICyberGfx) DropInterface((struct Interface*) ttb->ttb_ICyberGfx);
	if (ttb->ttb_IDOS) DropInterface((struct Interface*) ttb->ttb_IDOS);
	if (ttb->ttb_IGraphics) DropInterface((struct Interface*) ttb->ttb_IGraphics);
	if (ttb->ttb_IUtility) DropInterface((struct Interface*) ttb->ttb_IUtility);
	#endif

	if (ttb->ttb_LayersBase) CloseLibrary(ttb->ttb_LayersBase);
	if (ttb->ttb_LocaleBase) CloseLibrary(ttb->ttb_LocaleBase);
	if (ttb->ttb_CyberGfxBase) CloseLibrary(ttb->ttb_CyberGfxBase);
	if (ttb->ttb_DOSBase) CloseLibrary(ttb->ttb_DOSBase);
	if (ttb->ttb_GfxBase) CloseLibrary(ttb->ttb_GfxBase);
	if (ttb->ttb_UtilityBase) CloseLibrary(ttb->ttb_UtilityBase);

	#ifndef __amigaos4__
	if (ttb->ttb_MathIeeeSingBasBase) CloseLibrary(ttb->ttb_MathIeeeSingBasBase);
	if (ttb->ttb_MathIeeeSingTransBase) CloseLibrary(ttb->ttb_MathIeeeSingTransBase);
	#endif

	cache_flush_all(ttb);
	if (ttb->ttb_Semaphore)
	{
		Forbid();
		ObtainSemaphore(ttb->ttb_Semaphore);
		ReleaseSemaphore(ttb->ttb_Semaphore);
		FreeMem(ttb->ttb_Semaphore, sizeof(struct SignalSemaphore));
		Permit();
	}
	dispose_database(ttb);
	free_codepage(ttb);
	if (ttb->ttb_MemPool) DeletePool(ttb->ttb_MemPool);
	ttb->ttb_InitFlag = FALSE;
	return;
}

///
/// LibOpen()

#ifdef __amigaos4__

struct TTEngineBase *LibOpen(struct LibraryManagerInterface *Self, ULONG version)
{
    struct TTEngineBase *ttb = (struct TTEngineBase *)Self->Data.LibBase;

#elif __MORPHOS__

struct TTEngineBase *LibOpen(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;

#else

__ASM__ __SAVE_DS__ struct TTEngineBase *LibOpen (__REG__(a6, struct TTEngineBase *ttb))
{

#endif

  LD(kp(""));
  if (!ttb->ttb_InitFlag)
  {
    if (!init_resources(ttb))
    {
      free_resources(ttb);
      return NULL;
    }
  }
  ttb->ttb_Lib.lib_OpenCnt++;
  ttb->ttb_Lib.lib_Flags &= ~LIBF_DELEXP;
  return ttb;
}

///
/// LibClose()

#ifdef __amigaos4__

APTR LibClose(struct LibraryManagerInterface *Self)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

LONG LibClose(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;

#else

__ASM__ __SAVE_DS__ long LibClose(__REG__(a6, struct TTEngineBase *ttb))
  {

#endif

    LD(kp(""));
    if (!(--ttb->ttb_Lib.lib_OpenCnt))
      {
        #ifdef __amigaos4__
        if (ttb->ttb_Lib.lib_Flags & LIBF_DELEXP) return ((long)LibExpunge(Self));
        #elif __MORPHOS__
        if (ttb->ttb_Lib.lib_Flags & LIBF_DELEXP) return ((long)MyLibExpunge(ttb));
        #else
        if (ttb->ttb_Lib.lib_Flags & LIBF_DELEXP) return ((long)LibExpunge(ttb));
        #endif
      }
    return 0;
  }

///
/// LibExpunge()

#ifdef __amigaos4__

APTR LibExpunge(struct LibraryManagerInterface *Self)
{
    struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

APTR LibExpunge(VOID)
  {
    struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;

    return MyLibExpunge(ttb);
  }

APTR MyLibExpunge(struct TTEngineBase *ttb)

#else

__ASM__ __SAVE_DS__ APTR LibExpunge (__REG__(a6, struct TTEngineBase *ttb))

#endif

{
  USESYSBASE;
  APTR seglist;

  LD(kp(""));
  if (ttb->ttb_Lib.lib_OpenCnt)
  {
    ttb->ttb_Lib.lib_Flags |= LIBF_DELEXP;
    return 0;
  }

  seglist = ttb->ttb_SegList;

  #ifdef __amigaos4__
  free_resources(ttb);
  Remove((struct Node *) ttb);
  DeleteLibrary((struct Library *) ttb);
  #else
  Forbid();
  Remove((struct Node*)ttb);
  Permit();
  free_resources(ttb);
  /* SAS/C: APTR is void *; use BYTE* for address arithmetic. */
  FreeMem((APTR)((BYTE *)ttb - ttb->ttb_Lib.lib_NegSize),
    (LONG)ttb->ttb_Lib.lib_PosSize + (LONG)ttb->ttb_Lib.lib_NegSize);
  #endif

  LD(kp("library expunged."));
  return seglist;
}

#ifdef __amigaos4__
}
#endif

///
/// LibReserved()

#ifndef __amigaos4__

__ASM__ long LibReserved(void)
{
	return 0;
}

#endif

///

/* ========== APPLICATION PROGRAMMER INTERFACE ========== */

/// tt_OpenFontA()

/****** ttengine.library/TT_OpenFontA ***************************************
*
*   NAME
*       TT_OpenFontA -- Opens TrueType font. (V4)
*
*   SYNOPSIS
*       font = TT_OpenFontA (taglist)
*                               A0
*
*       APTR TT_OpenFontA (struct TagItem*);
*
*       font = TT_OpenFont (Tag1, ...)
*
*       APTR TT_OpenFont (Tag, ...);
*
*   FUNCTION
*       Opens a TrueType font preparing it to use with any RastPort. Font may
*       be specified directly as a path to "*.ttf" file, or indirectly via
*       set of attributes, database search will be performed for this set and
*       best matching font will be opened. It is graphics.library OpenFont()
*       counterpart but taglist is used instead TextAttr structure.
*
*   INPUTS
*       taglist - a list of tags containing requested font attributes.
*              Following tags are recognized:
*         TT_FontFile - this tag is a pointer to string containing a full
*              path to TrueType font file to be opened. This tag allows for
*              overriding database search and match. That means
*              TT_FamilyTable, TT_FontStyle and TT_FontWeight are ignored if
*              TT_FontFile is specified. Useful for opening application
*              specific fonts, containing for example musical notes or some
*              other kind of symbols. This tag has no default value.
*         TT_FamilyTable - NULL-terminated table of pointers to font family
*              names from the most desired to the last resort fallback. It is
*              intended for easy implementing things like HTML 'FONT FACE'
*              attribute. You can use real family names here, as well as
*              generic names: 'serif', 'sans-serif', 'monospaced', 'cursive',
*              'fantasy', and 'default', especially useful as a fallbacks.
*              Family names are case insensitive.
*              The default value for the tag is {"default", NULL};
*         TT_FontSize - font size in pixels defined as the distance between
*              baselines of two following text lines. The default value for
*              this tag is 14 pixels.
*         TT_FontStyle - there are three styles defined:
*              TT_FontStyle_Regular (default),
*              TT_FontStyle_Italic,
*         TT_FontWeight - defined with Cascading Style Sheets manner as a
*              number from 0 (the lightest face) to 999 (the heaviest face).
*              You can use TT_FontWeight_Normal (equal to 400) and
*              TT_FontWeight_Bold (equal to 700) shortcuts.
*              TT_FontWeight_Normal is the default value.
*         TT_FixedWidth - function checks if the opened font is
*              monospaced (fixed width). If it isn't, font is closed and
*              TT_OpenFont() returns NULL. Default value for this tag is
*              FALSE. (V6.7)
*
*   RESULT
*       success - font pointer (for use with TT_SetFont() and TT_CloseFont())
*                 if the font has been opened successfully, NULL
*                 otherwise. This function can fail for four reasons:
*                 1. There is no matching face in the database (NOTE: even
*                    'default' can fail if not defined in the database!)
*                 2. File not found or malformed.
*                 3. Zero font size.
*                 4. No memory for requested (too big?) size.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_SetFont(), TT_CloseFont(), font_database
*
*****************************************************************************
*
*/

STRPTR default_family_table[] = {"DEFAULT", NULL};

static BOOL set_nonstandard_charmap(FT_Face face)
  {
    WORD m;
    FT_CharMap cm;

    for (m = 0; m < face->num_charmaps; m++)
      {
        cm = face->charmaps[m];
        if (cm->platform_id == TT_PLATFORM_MICROSOFT && cm->encoding_id == TT_MS_ID_SYMBOL_CS)
          {
            return (FT_Set_Charmap(face, cm));
          }
      }

    for (m = 0; m < face->num_charmaps; m++)
      {
        cm = face->charmaps[m];
        if (cm->platform_id == TT_PLATFORM_MACINTOSH &&
          cm->encoding_id == TT_APPLE_ID_DEFAULT)
          {
            return (FT_Set_Charmap(face, cm));
          }
      }
    return FALSE;
  }


struct TTFont *find_best_weight(struct TTFamily *f, LONG weight, LONG style)
{
        LONG weight_match = 1000;
        struct TTFont *font, *found_font = NULL;

        for (font = (struct TTFont*)f->faces.lh_Head; font->node.ln_Succ; font = (struct TTFont*)font->node.ln_Succ)
        {
                if (font->style == style)
                {
                        LONG delta;

                        delta = weight - font->weight;
                        if (delta < 0) delta = -delta;
                        if (delta < weight_match)
                        {
                                found_font = font;
                                weight_match = delta;
                        }
                }
        }
        return found_font;
}

#ifdef __amigaos4__

APTR VARARGS68K _TTEngine_TT_OpenFont(struct TTEngineIFace *Self,
       ...)
{
    va_list ap;
    struct TagItem *tags;

    va_startlinear(ap, Self);
    tags = va_getlinearva(ap, struct TagItem *);

    return Self->TT_OpenFontA(
        tags);

}

APTR VARARGS68K _TTEngine_TT_OpenFontA(struct TTEngineIFace *Self,
       struct TagItem * taglist)
{
    struct TTEngineBase *ttb = (struct TTEngineBase *)Self->Data.LibBase;

#elif __MORPHOS__

APTR tt_OpenFontA(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
  struct TagItem *taglist = (struct TagItem*)REG_A0;

#else

__ASM__ __SAVE_DS__ APTR tt_OpenFontA(__REG__(a6, struct TTEngineBase *ttb), __REG__(a0, struct TagItem *taglist))
{

#endif

  USESYSBASE;
  USELIB(DOSBase, IDOS, DOSIFace);
  USELIB(UtilityBase, IUtility, UtilityIFace);

        BOOL success = FALSE, do_shear = FALSE;    /* algorithmic italicizing */
  UBYTE shr_sign = 0x00;
  WORD i;
        LONG size, weight, style, font_name_length;
        STRPTR font_file, *family_table, int_font_file = NULL;
  struct TTFamily *family, *found_family = NULL;
  struct TTFont *found_font = NULL;
  BPTR testlock;
  struct LoadedFont *lf = NULL;

  LD(kp(""));

  /* determine (maybe some default) font attributes */

  family_table = (STRPTR*)GetTagData(TT_FamilyTable, (ULONG)default_family_table, taglist);
  size = GetTagData(TT_FontSize, 14, taglist);
  weight = GetTagData(TT_FontWeight, TT_FontWeight_Normal, taglist);
  style = GetTagData(TT_FontStyle, TT_FontStyle_Regular, taglist);
  font_file = (STRPTR)GetTagData(TT_FontFile, 0, taglist);

  /* If no explicit TT_FontFile is given, search the database */

  if (!font_file)
  {
    /* search for matching family (or maybe alias) */

    for (i = 0; family_table[i]; i++)
    {
      for (family = (struct TTFamily*)ttb->ttb_DataBase.lh_Head; family->node.ln_Succ;
       family = (struct TTFamily*)family->node.ln_Succ)
      {
        if (Stricmp(family_table[i], family->node.ln_Name) == 0)
        {
          found_family = family;
          break;
        }
      }
      if (found_family) break;
    }

    /* return with failure if no matching family found */

    if (!found_family) return FALSE;

    /* replace possible alias with the real thing */

    if (found_family->alias) found_family = found_family->alias;

    /* get best matching font (match style and weight) */

    if (style == TT_FontStyle_Regular)
    {
      found_font = find_best_weight(found_family, weight, TT_FontStyle_Regular);
      if (!found_font) found_font = find_best_weight(found_family, weight, TT_FontStyle_Italic);
    }
    else
    {
      found_font = find_best_weight(found_family, weight, TT_FontStyle_Italic);
      if (!found_font)
      {
        if ((found_font = find_best_weight(found_family, weight, TT_FontStyle_Regular)))
          do_shear = TRUE;
      }
    }

    /* return with failure if no matching face found */

    if (!found_font) return FALSE;
    font_file = found_font->filepath;
  }
  else
  {
    if (style & TT_FontStyle_Italic) do_shear = TRUE;
  }

        font_name_length = strlen(font_file) + 1;

        if (int_font_file = AllocPooled(ttb->ttb_MemPool, font_name_length))
        {
                strcpy(int_font_file, font_file);

                /* do nothing if an application opens already opened font */
                /* from the same file and with the same size */

                if (do_shear)                 /* make sure library does not return regular */
                {                             /* face opened previously if soft italic is requested */
                        shr_sign = *int_font_file;  /* if soft italic was opened before it will be returned */
                        *int_font_file = '/';
                }

                if ((lf = fontlist_find(ttb, int_font_file, size)))
                {
                        lf->lf_OpenCnt++;
                        return lf;
                }

          /* Font has not been loaded, add it into the list */

                if ((lf = fontlist_add(ttb, int_font_file)))
          {
                        if (do_shear) *int_font_file = shr_sign;

            /* setup antialias configuration */

            if (found_font)
            {
              lf->lf_NoAntialiasMin = found_font->smoothsmall + 1;
              lf->lf_NoAntialiasMax = found_font->smoothbig - 1;
            }
            else
            {
              lf->lf_NoAntialiasMin = DEFAULT_NO_ANTIALIAS_MIN;
              lf->lf_NoAntialiasMax = DEFAULT_NO_ANTIALIAS_MAX;
            }

            /* check font file existence */

                        if ((testlock = Lock(int_font_file, ACCESS_READ)))
            {
              LONG e;

              /* font file found, Tank, load us up :-) */

              ObtainSemaphore(&lf->lf_Lock);
              LD(kp1("font $%08lx locked", lf));
                                
                                if (!(e = FT_New_Face(ttb->ttb_FTLib, int_font_file, 0, &lf->lf_Face)))
              {
                LONG asc, desc;
                struct CachedFont *cf = NULL;
                struct CachedSize *cs = NULL;

                                        /* Check if font is monospaced. If not, and TT_FixedWidth is set */
                                        /* to TRUE, reject the font. */

                                        if ((GetTagData(TT_FontFixedWidth, FALSE, taglist) == FALSE) ||
                                                (FT_IS_FIXED_WIDTH(lf->lf_Face)))
                                        {

                                                font_name_get(ttb, lf, TT_NAME_ID_FONT_FAMILY);
                                                font_name_get(ttb, lf, TT_NAME_ID_FONT_SUBFAMILY);
                                                font_name_get(ttb, lf, TT_NAME_ID_FULL_NAME);

                                                /* check for non-standard character maps */

                                                if (!lf->lf_Face->charmap) set_nonstandard_charmap(lf->lf_Face);

                                                if (lf->lf_Face->charmap)
                                                {
                                                        struct RenderEnv lre;

                                                        /* Apply transform matrix if algorithmic italicizing is requested. Also change the first letter */
                                                        /* to '/' to distinct from regular form                                                         */

                                                        if (do_shear)
                                                        {
                                                                FT_Matrix shr = {0x00010000, 0x00004242, 0x00000000, 0x00010000};

                                                                FT_Set_Transform(lf->lf_Face, &shr, NULL);
                                                                lf->lf_Node.ln_Name[0] = '/';
                                                        }

                                                        /* compute correct pixel size for given height */

                                                        FT_Set_Pixel_Sizes(lf->lf_Face, 0, size);

                                                        /* Find or create bitmap cache. Cache should not be globally */
                                                        /* visible for transformed fonts */

                                                        if (!(cf = cache_find_font(ttb, lf->lf_Node.ln_Name)))
                                                                cf = cache_add_font(ttb, lf->lf_Node.ln_Name);
                                                        if (cf)
                                                        {
                                                                if (!(cs = cache_find_size(ttb, cf, size)))
                                                                        cs = cache_add_size(ttb, cf, size);
                                                        }

                                                        if (cs) lf->lf_CachedSize = cs;
                                                        cs->cs_Users++;

                                                        /* global metrics calculation */

                                                        lf->lf_DesignHeight = (lf->lf_Face->height * size +
                                                                lf->lf_Face->units_per_EM - 1) / lf->lf_Face->units_per_EM;

                                                        lf->lf_MaxTop = (lf->lf_Face->bbox.yMax * size +
                                                                lf->lf_Face->units_per_EM - 1) /
                                                                lf->lf_Face->units_per_EM;

                                                        lf->lf_MaxBottom = (-lf->lf_Face->bbox.yMin * size +
                                                                lf->lf_Face->units_per_EM - 1) /
                                                                lf->lf_Face->units_per_EM;

                                                        asc = (lf->lf_Face->size->metrics.ascender + 0x1F) >> 6;
                                                        desc = (0x1F - lf->lf_Face->size->metrics.descender) >> 6;
                                                        lf->lf_Ascender = asc + ((size - (asc + desc)) >> 1);
                                                        lf->lf_Descender = size - lf->lf_Ascender;

                                                        /* underline position and thickness */

                                                        lf->lf_UnderPos = (lf->lf_Face->underline_position * lf->lf_Face->size->metrics.y_scale >> 22) + 1;
                                                        lf->lf_UnderThk = lf->lf_Face->underline_thickness * lf->lf_Face->size->metrics.y_scale >> 22;
                                                        if (lf->lf_UnderThk < 1) lf->lf_UnderThk = 1;
                                                        if (lf->lf_UnderPos > -1) lf->lf_UnderPos = -1;

                                                        /* strikethrough position */

                                                        lre.re_TargetRPort = NULL;
                                                        lre.re_TargetCMap = NULL;
                                                        lre.re_LoadedFont = lf;
                                                        lre.re_Antialias = TT_Antialias_Off;
                                                        lre.re_Encoding = TT_Encoding_ISO8859_1;
                                                        set_encoding_callback(ttb, &lre);
                                                        lre.re_Transparency = 0;
                                                        lre.re_Flags = 0;
                                                        lre.re_SoftStyle = TT_SoftStyle_None;
                                                        lre.re_Matrix.xx = 0x00010000;
                                                        lre.re_Matrix.xy = 0x00000000;
                                                        lre.re_Matrix.yx = 0x00000000;
                                                        lre.re_Matrix.yy = 0x00010000;
                                                        antialias_off(&lre);

                                                        extent_loop(ttb, &lre, "x", 1);
                                                        lf->lf_StrikePos = ((lre.re_TextExtent.te_Extent.MaxY -
                                                                lre.re_TextExtent.te_Extent.MinY) >> 1) -
                                                                lre.re_TextExtent.te_Extent.MaxY;

                                                        extent_loop(ttb, &lre, "MGA", 3);
                                                        lf->lf_RealAscender = -lre.re_TextExtent.te_Extent.MinY;

                                                        extent_loop(ttb, &lre, "??", 2);
                                                        lf->lf_AccentedAscender = -lre.re_TextExtent.te_Extent.MinY;

                                                        extent_loop(ttb, &lre, "gpyj_", 5);
                                                        lf->lf_RealDescender = lre.re_TextExtent.te_Extent.MaxY + 1;

                                                        /* calculate width for fixedwidth fonts */

                                                        if (FT_IS_FIXED_WIDTH(lf->lf_Face))
                                                        {
                                                                lf->lf_Width = lre.re_TextExtent.te_Width / 5;
                                                        }

                                                        success = TRUE;
                                                }
                                        }
              }

              ReleaseSemaphore(&lf->lf_Lock);
              UnLock(testlock);
            }
                }
                FreePooled(ttb->ttb_MemPool, int_font_file, font_name_length);
  }
        if (success) lf->lf_OpenCnt++;
        else
        {
                fontlist_remove(ttb, lf);
                lf = NULL;
        }
  return lf;
}

///
/// tt_CloseFont()

/****** ttengine.library/TT_CloseFont ***************************************
*
*   NAME
*       TT_CloseFont -- Closes TrueType font. (V4)
*
*   SYNOPSIS
*       TT_CloseFont (font)
*                     A0
*
*       VOID TT_CloseFont (APTR);
*
*   FUNCTION
*       Closes font opened by TT_OpenFontA()
*
*   INPUTS
*       font - the result of TT_OpenFontA(). Can be NULL, no action is taken
*           in the case.
*
*   RESULT
*       none
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_OpenFontA()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

VOID VARARGS68K _TTEngine_TT_CloseFont(struct TTEngineIFace *Self,
       APTR lf)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

void tt_CloseFont(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
  APTR lf = (APTR)REG_A0;

#else

__ASM__ __SAVE_DS__ void tt_CloseFont(__REG__(a6, struct TTEngineBase *ttb),__REG__(a0, APTR lf))
{

#endif

  struct LoadedFont *lf2 = (struct LoadedFont*)lf;

  LD(kp(""));

  if (!lf2) return;
  lf2->lf_CachedSize->cs_Users--;

  if (--lf2->lf_OpenCnt == 0)
  {
    /* close FreeType face, remove LoadedFont from the font list */

    FT_Done_Face(lf2->lf_Face);
    fontlist_remove(ttb, lf2);
  }
}

///
/// tt_SetFont()

/****** ttengine.library/TT_SetFont ******************************************
*
*   NAME
*       TT_SetFont -- Sets TrueType font for a RastPort. (V4)
*
*   SYNOPSIS
*       TT_SetFont (rastport, font)
*                   A1        A0
*
*       BOOL TT_SetFont (struct RastPort*, APTR);
*
*   FUNCTION
*       Sets a font previously opened with TT_OpenFontA() for use with given
*       RastPort. All further calls of text rendering or metrics function for
*       this RastPort will use font set.
*
*   INPUTS
*       rastport - The font will be set for this RastPort.
*       font - Pointer returned by TT_OpenFontA(). May be NULL, function
*           returns immediately in the case.
*
*   RESULT
*       TRUE if success, false otherwise.
*
*   EXAMPLE
*       \* use two fonts to render in one RastPort *\
*
*       APTR times, arial;
*
*       times = TT_OpenFont(
*          TT_FamilyTable, (ULONG){"Times", "serif", "default", NULL},
*          TT_FontSize, 26,
*       TAG_END);
*
*       arial = TT_OpenFont(
*          TT_FamilyTable, (ULONG){"Arial", "sans-serif", "default", NULL},
*          TT_FontSize, 22,
*          TT_FontWeight, TT_FontWeight_Bold,
*       TAG_END);
*
*       if (times && arial)
*         {
*           SetAPen(win->RPort, 1);
*           SetDrMd(win->RPort, JAM1);
*           Move(win->RPort, 100, 100);
*           TT_SetAttrs(win->RPort, TT_Window, win, TAG_END);
*
*           if (TT_SetFont(win->RPort, times))
*             TT_Text(win->RPort, "Times 26 points", 15);
*           if (TT_SetFont(win->RPort, arial))
*             TT_Text(win->RPort, "Arial 22 points", 15);
*         }
*       if (arial) TT_CloseFont(arial);
*       if (times) TT_CloseFont(times);
*
*   NOTES
*     Function can fail returning FALSE if:
*     1. RastPort pointer is NULL.
*     2. Font pointer is NULL.
*     3. Font pointer is not found on opened fonts list (it means the pointer
*        was not obtained as a result of TT_OpenFontA() call).
*     4. Memory allocation during environment creation fails.
*
*   BUGS
*
*   SEE ALSO
*       TT_OpenFontA(), TT_SetAttrsA(), TT_CloseFont()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

BOOL VARARGS68K _TTEngine_TT_SetFont(struct TTEngineIFace *Self,
       struct RastPort * rp,
       APTR lf1)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

BOOL tt_SetFont(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
  struct RastPort *rp = (struct RastPort*)REG_A1;
  APTR lf1 = (APTR)REG_A0;

#else

__ASM__ __SAVE_DS__ BOOL tt_SetFont(__REG__(a6, struct TTEngineBase *ttb), __REG__(a1, struct RastPort *rp),__REG__(a0, APTR lf1))
{

#endif

  BOOL lf_found = FALSE;
  struct LoadedFont *lf = (struct LoadedFont*)lf1;
  struct Node *lf2;
  struct RenderEnv *re;

  LD(kp(""));

  /* Check arguments */

  if (!rp) return FALSE;
  if (!lf) return FALSE;

  /* Try to find LoadedFont structure in the FontList */

	LD(kp("seraching for LoadedFont...");)

  for (lf2 = ttb->ttb_FontList.lh_Head; lf2->ln_Succ; lf2 = lf2->ln_Succ)
  {
    if (lf2 == &lf->lf_Node)
    {
      lf_found = TRUE;
      break;
    }
  }

	LD(kp1("LoadedFont at $%08lx.", lf);)

  if (!lf_found) return FALSE;

  /* Try to find an environment. If not found, create a new one */

	LD(kp("seraching for RenderEnv...");)

  if (!(re = envlist_find(ttb, rp)))
  {
		LD(kp1("RenderEnv not found, creating new one for RastPort $%08lx.", rp);)

    if (!(re = envlist_add(ttb, rp)))
    {
      return FALSE;
    }
  }

	LD(kp2("RenderEnv $%08lx for RastPort $%08lx.", re, rp);)

  /* set the font for RenderEnv */

  re->re_LoadedFont = lf;

	LD(kp1("LoadedFont $%08lx.", lf);)

  return TRUE;
}

///
///tt_Text()

/****** ttengine.library/TT_Text ********************************************
*
*   NAME
*       TT_Text -- Renders string into RastPort. (V4)
*
*   SYNOPSIS
*       TT_Text (rastport, string, count)
*                A1        A0      D0
*
*       VOID TT_Text (struct RastPort*, APTR, ULONG);
*
*   FUNCTION
*       Renders the string using current ttengine.library settings, and
*       current RastPort settings (pen, drawmode). String is rendered at
*       current RastPort (x, y) position, where 'y' means position of font
*       baseline. String is converted to Unicode according to
*       current (set by TT_SetAttrs()) encoding table.
*
*   INPUTS
*       rastport - Target RastPort for rendering.
*       string - String to render to (need not to be NULL terminated).
*       count - How many characters to render (note that NULL code will
*           be rendered as well).
*
*   RESULT
*       none
*
*   EXAMPLE
*       \* write a text with pen 1 and transp. background at (100, 100) *\
*       \* rendering is done to a system window                         *\
*
*       SetAPen(win->RPort, 1);
*       SetDrMd(win->RPort, JAM1);
*       Move(win->RPort, 100, 100);
*       TT_SetAttrs(win->RPort, TT_Window, win, TAG_END);
*       TT_Text(win->RPort, "some text", 9);
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_OpenFontA(), TT_SetAttrsA()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

VOID VARARGS68K _TTEngine_TT_Text(struct TTEngineIFace *Self,
       struct RastPort * rp,
       APTR str,
       ULONG count)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

void tt_Text(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
  struct RastPort *rp = (struct RastPort*)REG_A1;
  APTR str = (APTR)REG_A0;
  ULONG count = REG_D0;

#else

__ASM__ __SAVE_DS__ void tt_Text(__REG__(a6, struct TTEngineBase *ttb), __REG__(a1, struct RastPort *rp),__REG__(a0, APTR str),__REG__(d0, ULONG count))
{

#endif

  struct RenderEnv *re;

  LD(kp(""));

  if ((re = envlist_find(ttb, rp)))
  {
    if (re->re_TargetRPort && re->re_TargetCMap && re->re_LoadedFont)
    {
      set_render_mode(ttb, re);
      render(ttb, re, str, count);
    }
  }
}

///
/// tt_SetAttrsA()

/****** ttengine.library/TT_SetAttrsA ***************************************
*
*   NAME
*       TT_SetAttrsA -- Sets rendering engine and font attributes. (V4)
*
*   SYNOPSIS
*       count = TT_SetAttrsA (rastport, taglist)
*                             A1        A0
*
*       ULONG TT_SetAttrsA (struct RastPort*, struct TagItem*);
*
*       count = TT_SetAttrs (rastport, Tag1, ...)
*
*       ULONG TT_SetAttrs (struct RastPort*, Tag, ...);
*
*   FUNCTION
*       Sets render engine settings for given RastPort. Every following
*       TrutType render and metrics calls for this RastPort will use these
*       settings. Here is a list of attributes:
*
*       TT_ColorMap - (struct ColorMap*) - ColorMap used to convert pen
*         number to RGB color value.
*
*       TT_Screen - (struct Screen*) - useful shortcut for TT_ColorMap,
*         automatically sets screen ColorMap.
*
*       TT_Window - (struct Window*) - useful shortcut for TT_ColorMap,
*         automatically sets window ColorMap.
*
*       TT_Antialias - (BOOL) - controls antialiasing (on, off or
*         automatic):
*         TT_Antialias_Off - turns antialias off
*         TT_Antialias_On - turns antialias on
*         TT_Antialias_Auto (default) - antialias state depends on font
*           size. Typically sizes of 9 or less pixels are antialiased, sizes
*           from 10 to 19 pixels are not antialiased, sizes of 20 of more
*           pixels are antialiased. These settings can be changed in the font
*           database separately for every font face.
*
*       TT_Encoding - selects font encoding table to be used:
*         TT_Encoding_Default - loads encoding table from "ENV:ttfcodepage"
*           file, compatible with ttf.library. If no such file is found,
*           ISO-8859-1 encoding (Amiga default) is used.
*         TT_Encoding_ISO8859_x - where 'x' can be one of 1, 2, 3, 4, 5, 6,
*           7, 8, 9, 10, 11, 13, 14, 15, 16. One of 8-bit encodings defined
*           in ISO-8859 normative.
*         TT_Encoding_UTF16_BE - 16-bit Unicode, big endian (68k/PPC style).
*           Endian mark (0xFEFF) is ignored.
*         TT_Encoding_UTF32_BE - 32-bit Unicode, big endian (68k/PPC style).
*           Endian mark (0x0000FEFF) is ignored.
*         TT_Encoding_UTF16_LE - 16-bit Unicode, little endian (x86 style).
*           Endian mark (0xFFFE) is ignored.
*         TT_Encoding_UTF32_LE - 32-bit Unicode, little endian (x86 style).
*           Endian mark (0xFFFE0000) is ignored.
*         TT_Encoding_UTF16 - 16-bit Unicode, with endian determined by
*           endian mark. If no endian mark encountered big endian is assumed.
*         TT_Encoding_UTF32 - 32-bit Unicode, with endian determined by
*           endian mark. If no endian mark encountered big endian is assumed.
*         TT_Encoding_UTF8 - 8-bit Unicode encapsulation (RFC 2279).
*
*       TT_CustomEncoding - select custom 8-bit encoding table. Tag data
*         should point to 256 UWORDs, every containing one 16-bit big endian
*         Unicode character code. 8-bit character is used as an index of the
*         table. This tag overrides TT_Encoding regardless of its position in
*         the taglist if both are specified in the same taglist.
*
*       TT_Transparency - Allows for rendering transparent text (the
*         background shines through the text). 0 value means no transparency
*         (this is the default), 255 is maximum transparency.
*
*       TT_ScaleX - a floating point (IEEE single precision) horizontal scale
*         factor. Its absolute value will be limited to <0.01, 100> range. It
*         may be negative (it means horizontal mirror around starting pen
*         position, string will be rendered to the left).
*
*       TT_ScaleY - a floating point (IEEE single precision) vertical scale
*         factor. Its absolute value will be limited to <0.01, 100> range. It
*         may be negative (it means vertical mirror around the baseline,
*         string will be rendered mostly under the baseline).
*
*       TT_SoftStyle - controls software generated font modifications. Any of
*         software styles can be combined (OR-ing the values).
*
*         TT_SoftStyle_None - clears all software styles.
*         TT_SoftStyle_Underlined - boolean tag controlling text underline.
*         TT_SoftStyle_DblUnderlined - boolean tag controlling text double
*           underline. Combining it with TT_Underlined gives one thick line.
*         TT_SoftStyle_Overstriked - boolean tag controlling text overstriked
*           line.
*         TT_SoftStyle_DblOverstriked - boolean tag controlling text
*           overstriked double line. Combining it with TT_Overstriked gives
*           one thick line.
*
*       TT_Foreground - allows for using direct RGB value of foreground color
*         instead of RastPort 'A' pen. On LUT screens this tag will be
*         ignored. Special value TT_Foreground_UseRastPort returns foreground
*         color control back to the RastPort and its 'A' pen.
*
*       TT_Background - allows for using direct RGB value of background color
*         (used in JAM2 drawmode only) instead of RastPort 'B' pen. On LUT
*         screens this tag will be ignored. Special value
*         TT_Background_UseRastPort returns background color control back to
*         the RastPort and its 'B' pen.
*
*       TT_Gamma - adjustable gamma correction used for antialiasing. Partial
*         coverage pixels are blended in approximate linear light (via inverse
*         gamma), then encoded with x' = x ^ 1/gamma. TT_Gamma is 'gamma'
*         multiplied by 1000. Default is 2500 (2.5), typical for CRT. Default
*         can be set in 'ttengine.database'.
*
*       TT_DiskFontMetrics - changes TTEngine rendering to be diskfont-like.
*         In this mode JAM2 filled area extends vertically between
*         TT_FontAccentedAscender and TT_FontRealDescender and as such is
*               equal to TT_FontHeight. Also if the     left bearing of the first
*               glyph or the right bearing of the last glyph is negative, JAM2
*               fill area is extended to the left or right accordingly. Note
*         however, glyphs higher than TT_FontAccentedAscender or extending
*         below TT_FontRealDescender will be clipped. This mode is useful
*         especially for text-oriented applications using JAM2 draw mode.
*         Defaults to FALSE.
*
*   INPUTS
*       rastport - attributes will be set for this RastPort.
*       taglist - the list of attributes.
*
*   RESULT
*       counter - the count of recognized tags.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_OpenFontA(), TT_Text(), TT_GetAttrs()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

ULONG VARARGS68K _TTEngine_TT_SetAttrs(struct TTEngineIFace *Self, struct RastPort * rp, ...)
{
	va_list ap;
	struct TagItem *tags;

	va_startlinear(ap, rp);
	tags = va_getlinearva(ap, struct TagItem *);

	return Self->TT_SetAttrsA(rp, tags);
}

ULONG VARARGS68K _TTEngine_TT_SetAttrsA(struct TTEngineIFace *Self, struct RastPort * rp, struct TagItem * taglist)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

ULONG tt_SetAttrsA(void)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
	struct RastPort *rp = (struct RastPort*)REG_A1;
	struct TagItem *taglist = (struct TagItem*)REG_A0;

#else

__ASM__ __SAVE_DS__ ULONG tt_SetAttrsA(__REG__(a6, struct TTEngineBase *ttb), __REG__(a1, struct RastPort *rp), __REG__(a0, struct TagItem *taglist))
{

#endif

	USELIB(UtilityBase, IUtility, UtilityIFace);

	struct RenderEnv *re;
	struct TagItem *tag, *tagptr = taglist;
	struct TransformInfo ti;
	ULONG count = 0;
	BOOL custom_encoding = FALSE;

	if (!(re = envlist_find(ttb, rp)))
	{
		if (!(re = envlist_add(ttb, rp))) return 0;
	}

	ti.ti_RenderEnv = re;
	ti.ti_ScaleX = 1.0;
	ti.ti_ScaleY = 1.0;
	ti.ti_Shear = 0.0;
	ti.ti_Rotate = 0.0;

	while ((tag = NextTagItem(&tagptr)))
	{
		switch (tag->ti_Tag)
		{
			case TT_ColorMap:
				re->re_TargetCMap = (struct ColorMap*)tag->ti_Data;
				count++;
			break;

			case TT_Screen:
				re->re_TargetCMap = ((struct Screen*)tag->ti_Data)->ViewPort.ColorMap;
				count++;
			break;

			case TT_Window:
				re->re_TargetCMap = ((struct Window*)tag->ti_Data)->WScreen->ViewPort.ColorMap;
				count++;
			break;

			case TT_Antialias:
				re->re_Antialias = tag->ti_Data;
				count++;
			break;

			case TT_Encoding:
				if (!custom_encoding)
				{
					re->re_Encoding = tag->ti_Data;
					if (set_encoding_callback(ttb, re)) count++;
				}
			break;

			case TT_CustomEncoding:
			{
				custom_encoding = TRUE;
				if (tag->ti_Data)
				{
					re->re_CustomEncTable = (UWORD*)tag->ti_Data;
					re->re_Encoding = TT_Encoding_Custom;
					set_encoding_callback(ttb, re);
				}
			}
			break;

			case TT_Transparency:
				re->re_Transparency = (UBYTE)tag->ti_Data;
				count++;
			break;

			case TT_SoftStyle:
				re->re_SoftStyle = tag->ti_Data &0xFFFF;
				count++;
			break;

			case TT_Foreground:
				re->re_Foreground = tag->ti_Data;
				count++;
			break;

			case TT_Background:
				re->re_Background = tag->ti_Data;
				count++;
			break;

			case TT_Gamma:
				re->re_GammaCf = tag->ti_Data;
				calculate_gamma(ttb, re);
				count++;
			break;

			case TT_DiskFontMetrics:
				if (tag->ti_Data) re->re_Flags |= REF_AMIGAMETRICS;
				else re->re_Flags &= ~REF_AMIGAMETRICS;
				count++;
			break;

			case TT_ForceFixedWidth:
				re->re_ForceFixed = tag->ti_Data;
				count++;
			break;
		}
	}
	return count;
}

///
/// tt_GetAttrsA()

/****** ttengine.library/TT_GetAttrsA ***************************************
*
*   NAME
*       TT_GetAttrsA -- Gets current font and render engine attributes (V4).
*
*   SYNOPSIS
*       count = TT_GetAttrsA (rastport, taglist)
*                             A1        A0
*
*       ULONG TT_GetAttrsA (struct RastPort*, struct TagItem*);
*
*       count = TT_GetAttrs (rastport, Tag1, ...)
*
*       ULONG TT_GetAttrs (struct RastPort*, Tag, ...);
*
*   FUNCTION
*       Gets current font or render engine attributes for given RastPort. The
*       value of every attribute is written to an ULONG pointed by ti_Data
*       field of the TagItem. Here is a list of attributes:
*
*       TT_Antialias - Current state of an antialias switch (on, off or
*          auto).
*
*       TT_FontAscender - This is a distance (in pixels) between the baseline
*         and top elements of the font (typically these elements are accents
*         of capital letters). NOTE: many shareware TT fonts have wrong ascen-
*         der value.
*
*       TT_FontDescender - This is a distance (in pixels) between the
*         baseline and bottom elements of the font (typically in letters 'p',
*         'q', 'g' etc.). NOTE: many shareware TT fonts have wrong descender
*         value. Descender value is typically negative (as bottom elements
*         usually are placed below the baseline).
*
*       TT_FontName - full font name as found in the font file. Pointer to a
*         NULL-terminated string is written. This string is localized if the
*         font file contains names in different languages. Language
*         precedence is loaded from "Preferred languages" table of
*         locale.library.
*
*       TT_FamilyName - font family name as found in the font file. Pointer
*         to a NULL-terminated string is written. This string is localized if
*         the font file contains names in different languages. Language
*         precedence is loaded from "Preferred languages" table of
*         locale.library.
*
*       TT_SubfamilyName - font subfamily name (typically describing font
*         variant like "italic" or "bold") as found in the font file. Pointer
*         to a NULL-terminated string is written. This string is localized if
*         the font file contains names in different languages. Language
*         precedence is loaded from "Preferred languages" table of
*         locale.library.
*
*       TT_Transparency - returns actual transparency setting.
*
*       TT_FontRealAscender - returns the height of capital letters in pixels.
*         Precisely it is the distance between the baseline and top of capital
*         letter.
*
*       TT_FontAccentedAscender - returns the height of capital accented
*         letters in pixels including accents (grave, acute, caron etc.).
*         Precisely it is the distance between the baseline and top of
*         accent.
*
*       TT_FontRealDescender - returns a distance between the baseline and
*         lowest pixels of letters like g, j, p, q, y.
*
*       TT_FontMaxTop - returns a distance between the baseline and the
*         topmost element of the whole font face.
*
*       TT_FontMaxBottom - returns a distance between the baseline and the
*         lowest element of the whole font face.
*
*       TT_FontDesignHeight - returns a distance between two baselines of
*         text as stored in the font file. This is the line spacing font is
*         designed for. Design height is usually greater than font size.
*
*       TT_FontBaseline (V6.7) - the same as TT_FontAccentedAscender.
*
*       TT_FontHeight (V6.7) - equal to TT_FontAccentedAscender +
*         TT_FontRealDescender.
*
*       TT_FontWidth (V6.7) - cursor horizontal advance for one character in
*         pixels. For monospaced fonts only, proportional ones return zero.
*
*       TT_FontFixedWidth (V6.7) - returns true if the loaded font is
*               monospaced.
*
*       TT_Gamma (V7.2) - adjustable gamma correction used for antialiasing.
*        	After RGB alphablending gamma correction is applied to resulting
*        	pixel colour according to x' = x ^ 1/gamma, where x is a RGB
*        	component. TT_Gamma is 'gamma' exponent multiplied by 1000 to avoid
*        	messing with floating point numbers. Default TT_Gamma is 2500 (2.5)
*        	which is typical for CRT display. Default gamma exponent can be set
*        	by user in 'ttengine.database' file.
*
*       NOTE: TT_FontAscender and TT_FontDescender are guarranted to fulfill
*       following formula: ascender - descender = font height.
*
*   INPUTS
*       rastport - attributes associated with this RastPort will be returned.
*       taglist - the list of attributes.
*
*   RESULT
*       counter - the count of recognized tags.
*
*   NOTES
*       Data returned are valid only until TT_CloseFont() call.
*
*   BUGS
*
*   SEE ALSO
*       TT_OpenFontA(), TT_SetAttrsA()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

ULONG VARARGS68K _TTEngine_TT_GetAttrs(struct TTEngineIFace *Self, struct RastPort * rp, ...)
{
	va_list ap;
	struct TagItem *tags;

	va_startlinear(ap, rp);
	tags = va_getlinearva(ap, struct TagItem *);

	return Self->TT_GetAttrsA(rp, tags);
}

ULONG VARARGS68K _TTEngine_TT_GetAttrsA(struct TTEngineIFace *Self, struct RastPort * rp, struct TagItem * taglist)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

ULONG tt_GetAttrsA(void)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
	struct RastPort *rp = (struct RastPort*)REG_A1;
	struct TagItem *taglist = (struct TagItem*)REG_A0;

#else

__ASM__ __SAVE_DS__ ULONG tt_GetAttrsA(__REG__(a6, struct TTEngineBase *ttb), __REG__(a1, struct RastPort *rp), __REG__(a0, struct TagItem *taglist))
{

#endif

	USELIB(UtilityBase, IUtility, UtilityIFace);

	struct TagItem *current_tag, *tag_pointer = taglist;
	struct RenderEnv *re;
	struct LoadedFont *lf;
	ULONG tag_counter = 0;

	LD(kp(""));
	if ((re = envlist_find(ttb, rp)))
	{
		lf = re->re_LoadedFont;

		while ((current_tag = NextTagItem(&tag_pointer)))
		{
			switch (current_tag->ti_Tag)
			{
				case TT_FontAscender:
					if (lf)
					{
						*((ULONG*)current_tag->ti_Data) = lf->lf_Ascender;
						tag_counter++;
					}
				break;

				case TT_FontDescender:
					if (lf)
					{
						*(ULONG*)current_tag->ti_Data = -lf->lf_Descender;
						tag_counter++;
					}
				break;

				case TT_Antialias:
					*(ULONG*)current_tag->ti_Data = re->re_Antialias;
					tag_counter++;
				break;

				case TT_FontName:
					if (lf)
					{
						*(STRPTR*)current_tag->ti_Data = lf->lf_FullName.fn_Text;
						tag_counter++;
					}
				break;

				case TT_FamilyName:
					if (lf)
					{
						*(STRPTR*)current_tag->ti_Data = lf->lf_Family.fn_Text;
						tag_counter++;
					}
				break;

				case TT_SubfamilyName:
					if (lf)
					{
						*(STRPTR*)current_tag->ti_Data = lf->lf_Name.fn_Text;
						tag_counter++;
					}
				break;

				case TT_Transparency:
					*(ULONG*)current_tag->ti_Data = (ULONG)re->re_Transparency;
					tag_counter++;
				break;

				case TT_FontRealAscender:
					if (lf)
					{
						*(ULONG*)current_tag->ti_Data = lf->lf_RealAscender;
						tag_counter++;
					}
				break;

				case TT_FontRealDescender:
					if (lf)
					{
						*(ULONG*)current_tag->ti_Data = lf->lf_RealDescender;
						tag_counter++;
					}
				break;

				case TT_FontAccentedAscender:
					if (lf)
					{
						*(ULONG*)current_tag->ti_Data = lf->lf_AccentedAscender;
						tag_counter++;
					}
				break;

				case TT_FontMaxTop:
					if (lf)
					{
						*(ULONG*)current_tag->ti_Data = lf->lf_MaxTop;
						tag_counter++;
					}
				break;

				case TT_FontMaxBottom:
					if (lf)
					{
						*(ULONG*)current_tag->ti_Data = lf->lf_MaxBottom;
						tag_counter++;
					}
				break;

				case TT_FontDesignHeight:
					if (lf)
					{
						*(ULONG*)current_tag->ti_Data = lf->lf_DesignHeight;
						tag_counter++;
					}
				break;

				case TT_FontHeight:
					if (lf)
					{
						*(ULONG*)current_tag->ti_Data = lf->lf_MaxTop + lf->lf_MaxBottom;
						tag_counter++;
					}
				break;

				case TT_FontFixedWidth:
					if (lf)
					{
						if (FT_IS_FIXED_WIDTH(lf->lf_Face)) *(ULONG*)current_tag->ti_Data = TRUE;
						else *(ULONG*)current_tag->ti_Data = FALSE;
						tag_counter++;
					}
				break;

				case TT_FontWidth:
					if (lf && FT_IS_FIXED_WIDTH(lf->lf_Face))
					{
						*(ULONG*)current_tag->ti_Data = lf->lf_Width;
						tag_counter++;
					}
				break;

				case TT_Gamma:
					*(ULONG*)current_tag->ti_Data = re->re_GammaCf;
					tag_counter++;
				break;
			}
		}
	}
	return tag_counter;
}

///
/// tt_TextLength()

/****** ttengine.library/TT_TextLength **************************************
*
*   NAME
*       TT_TextLength -- Gets string length in pixels (V4).
*
*   SYNOPSIS
*       length = TT_TextLength(rastport, string, count)
*                              A1        A0      D0
*
*       ULONG TT_TextLength (struct RastPort*, APTR, ULONG);
*
*   FUNCTION
*       Calculates the pixel width of given string written with current font.
*       Takes kerning into account. String will be mapped to Unicode using
*       current encoding.
*
*   INPUTS
*       rastport - render engine and font settings for this RastPort will be
*           used for calculations.
*       string - the length of this string will be calculated.
*       count - the string lenght in characters.
*
*   RESULT
*       length - the length of the string in pixels.
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_Text(), TT_OpenFontA()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

ULONG VARARGS68K _TTEngine_TT_TextLength(struct TTEngineIFace *Self,
	struct RastPort * rp,
	APTR string,
	ULONG count)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

ULONG tt_TextLength(void)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
	struct RastPort *rp = (struct RastPort*)REG_A1;
	APTR string = (APTR)REG_A0;
	ULONG count = REG_D0;

#else

__ASM__ __SAVE_DS__ ULONG tt_TextLength(__REG__(a6, struct TTEngineBase *ttb), __REG__(a1, struct RastPort *rp),__REG__(a0, APTR string),__REG__(d0, ULONG count))
{

#endif

	struct RenderEnv *re;

	if ((re = envlist_find(ttb, rp)))
	{
		if (re->re_LoadedFont)
		{
			set_render_mode(ttb, re);
			extent_loop(ttb, re, string, count);
			return re->re_TextExtent.te_Width;
		}
	}
	return 0;
}

///
/// tt_TextExtent()

/****** ttengine.library/TT_TextExtent **************************************
*
*   NAME
*       TT_TextExtent -- Determine raster extent of text data. (V4)
*
*   SYNOPSIS
*       TT_TextExtent (rastport, text, count, extent)
*                      A1        A0    D0:16  A2
*
*       VOID TT_TextExtent (struct RastPort*, APTR, WORD,
*           struct TextExtent*);
*
*   FUNCTION
*       Computes dimensions and coordinates of smallest rectangle containing
*       on the whole given text rendered with current settings. TextExtent
*       structure fields have following meaning:
*
*       te_Width - Horizontal advance of RastPort pen position. The same as
*           TT_TextLength() call result.
*       te_Height - Just current font height.
*       te_Extent.MinX - The horizontal distance from text start point to the
*           leftmost pixel rendered. May be negative, it means that some
*           pixels will be plotted before the start point.
*       te_Extent.MaxX - The horizontal distance from text start point to the
*           rightmost pixel rendered.
*       te_Extent.MinY - The vertical distance from text start point to the
*           topmost pixel rendered. Always negative, do not neccesarily equal
*           to -TTFA_Ascender.
*       te_Extent.MaxY - The vertical distance from text start point to the
*           lowermost pixel rendered. Typically positive if anything is to be
*           drawn below the baseline.
*
*   INPUTS
*       rastport - this rastport settings (font, size, style etc.) will be
*           used for calculations.
*       text - a string (not neccesarily NULL-terminated) to compute the
*           rectangle for.
*       count - length of the string in characters.
*       extent - pointer to TextExtent structure - it will be filled with
*           data.
*
*   RESULT
*       No retrurn value. TextExtent structure is filled with data.
*
*   NOTES
*       This function is almost identical to graphics.library/TextExtent().
*       It even receives parameters in the same registers.
*
*       Do not assume neither horizontal extents sum up to te_Width, nor
*       vertical ones sum up to font height.
*
*   BUGS
*
*   SEE ALSO
*       TT_OpenFontA(), graphics.library/TextExtent()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

VOID VARARGS68K _TTEngine_TT_TextExtent(struct TTEngineIFace *Self,
	struct RastPort * rp,
	APTR string,
	WORD count,
	struct TextExtent * te)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

void tt_TextExtent(void)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
	struct RastPort *rp = (struct RastPort*)REG_A1;
	APTR string = (APTR)REG_A0;
	WORD count = (WORD)REG_D0;
	struct TextExtent *te = (struct TextExtent*)REG_A2;

#else

__ASM__ __SAVE_DS__ VOID tt_TextExtent(__REG__(a6, struct TTEngineBase *ttb),__REG__(a1, struct RastPort* rp),__REG__(a0, APTR string),__REG__(d0, WORD count),
	__REG__(a2, struct TextExtent *te))
{

#endif

	USESYSBASE;
	struct RenderEnv *re;

	te->te_Width = 0;
	te->te_Height = 0;
	te->te_Extent.MinX = 0;
	te->te_Extent.MinY = 0;
	te->te_Extent.MaxX = 0;
	te->te_Extent.MaxY = 0;

	if ((re = envlist_find(ttb, rp)))
	{
		if (re->re_LoadedFont)
		{
			set_render_mode(ttb, re);
			extent_loop(ttb, re, string, count);
			CopyMem(&re->re_TextExtent, te, sizeof(struct TextExtent));
		}
	}
}

///
/// tt_TextFit()

/****** ttengine.library/TT_TextFit *****************************************
*
*   NAME
*       TT_TextFit -- Count characters that will fit in a given extent. (V4)
*
*   SYNOPSIS
*       characters = TT_TextFit (rastport, string, count, extent,
*                                A1        A0      D0:16  A2
*           constr_extent, direction, width, height)
*           A3             D1         D2     D3
*
*       ULONG TT_TextFit (struct RastPort*, APTR, UWORD, struct TextExtent*,
*           struct TextExtent*, WORD, UWORD, UWORD);
*
*   FUNCTION
*       Computes how many characters of given NULL-terminated string rendered
*       with current settings will fit into given extent. Also calculates an
*       extent for this fitting part of the string. Function can fit from the
*       beginning of the string to the right, or from the end of the string
*       to the left. If 'constr_extent' is non NULL, string is fitted only
*       against it, constraining 'width' and 'height' are ignored.
*
*   INPUTS
*       rastport - calculations will be performed using this rastport
*           settings (font, size, styles etc.).
*       string - A string (not neccesarily NULL-terminated) to fit.
*       count - string lenght in characters.
*       extent - Pointer to TextExtent structure - it will be filled with
*           dimensions of fitted part of the string. May be filled with all
*           zeros if no fit is possible at all (e.g. height is to small).
*       constr_extent - Pointer to already filled TextExtent structure. Part
*           of the string will fit into this extent. It is not modified by
*           function. May be NULL, 'width' and 'height' will be used as
*           constraints.
*       direction - When equal to 1 string will be fitted starting from it's
*           beginning to the rigth. When equal to -1, string will be fitted
*           starting from it's end to the left.
*       width - If 'constr_extent' is NULL, the string width will be compared
*           with this parameter. Ignored otherwise. Note that it is exact
*           pixel width of string, not cursor advance.
*       height - If 'constr_extent' is NULL, the string height will be
*           compared with this parameter. Ignored otherwise. Note that it is
*           exact pixel height, not font height (it is equal to font height
*           if TT_DiskfontMetrics is TRUE).
*
*   RESULT
*       characters - the length of fitting string part. May be zero if no fit
*           found.
*
*   NOTES
*       This function is almost identical to graphics.library/TextFit().
*       It even receives parameters in the same registers.
*
*       Do not assume neither horizontal extents sum up to te_Width, nor
*       vertical ones sum up to font height (in returned extent).
*
*   BUGS
*       Fitting in reverse direction (from end to beginning) does not work
*       yet, the 'direction' parameter is currently ignored and fitting
*       is done from beginning to the end always.
*
*   SEE ALSO
*       TT_OpenFontA(), graphics.library/TextFit()
*
*****************************************************************************

*/

static BOOL fits_in(struct TextExtent *te, struct TextExtent *cte, UWORD w, UWORD h)
{
	if (cte)
	{
		if (te->te_Extent.MinX < cte->te_Extent.MinX) return FALSE;
		if (te->te_Extent.MaxX > cte->te_Extent.MaxX) return FALSE;
		if (te->te_Extent.MinY < cte->te_Extent.MinY) return FALSE;
		if (te->te_Extent.MaxY > cte->te_Extent.MaxY) return FALSE;
	}
	else
	{
		if (te->te_Extent.MaxX - te->te_Extent.MinX + 1 > h) return FALSE;
		if (te->te_Extent.MaxY - te->te_Extent.MinY + 1 > w) return FALSE;
	}
	return TRUE;
}

#ifdef __amigaos4__

ULONG VARARGS68K _TTEngine_TT_TextFit(struct TTEngineIFace *Self,
	struct RastPort * rp,
	APTR string,
	UWORD count,
	struct TextExtent * te,
	struct TextExtent * cte,
	WORD dir,
	UWORD width,
	UWORD height)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

ULONG tt_TextFit(void)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
	struct RastPort *rp = (struct RastPort*)REG_A1;
	APTR string = (APTR)REG_A0;
	UWORD count = (UWORD)REG_D0;
	struct TextExtent *te = (struct TextExtent*)REG_A2;
	struct TextExtent *cte = (struct TextExtent*)REG_A3;
	WORD dir = (WORD)REG_D1;
	UWORD width = (UWORD)REG_D2;
	UWORD height = (UWORD)REG_D3;

#else

__ASM__ __SAVE_DS__ ULONG tt_TextFit(__REG__(a6, struct TTEngineBase *ttb), __REG__(a1, struct RastPort *rp),__REG__(a0, APTR string),__REG__(d0, UWORD count),
	__REG__(a2, struct TextExtent *te), __REG__(a3, struct TextExtent *cte),__REG__(d1, WORD dir),__REG__(d2, UWORD width),__REG__(d3, UWORD height))
{

#endif

	USESYSBASE;

	ULONG upper, lower, middle;
	struct RenderEnv *re;
	struct TextExtent fitted;

	/* check for allowed parameter ranges */

	if ((dir != 1) && (dir != -1)) return 0;
	if (!te) return 0;
	if (!string) return 0;
	if (!(re = envlist_find(ttb, rp))) return 0;
	if (!re->re_LoadedFont) return 0;

	/* clear target TextExtent */

	te->te_Width = 0;
	te->te_Height = 0;
	te->te_Extent.MinX = 0;
	te->te_Extent.MaxX = 0;
	te->te_Extent.MinY = 0;
	te->te_Extent.MaxY = 0;

	/*?Maybe the whole string fits into constraining box? Return immediately in the case. */

	extent_loop(ttb, re, string, count);
  
	if (fits_in(&re->re_TextExtent, cte, width, height))
	{
		CopyMem(&re->re_TextExtent, te, sizeof(struct TextExtent));
		return count;
	}

	/* Check if at least one character fits into constraining box. Return zero if no characters can be fitted. */

	extent_loop(ttb, re, string, 1);

	if (!fits_in(&re->re_TextExtent, cte, width, height)) return 0;
	else CopyMem(&re->re_TextExtent, &fitted, sizeof(struct TextExtent));

	/* fitting loop */

	lower = 1;
	upper = count;

	while (upper - lower > 1)
	{
		/* compute center point of string and its bound box */

		middle = (upper + lower) >> 1;
		extent_loop(ttb, re, string, middle);

		/* If middle fits, middle becomes new lower. If middle doesn't fit it */
		/* becomes new upper. */

		if (fits_in(&re->re_TextExtent, cte, width, height))
		{
			lower = middle;
			CopyMem(&re->re_TextExtent, &fitted, sizeof(struct TextExtent));
		}
		else upper = middle;
	}

	/* When loop finishes 'lower' contains maximal number of characters fitting  into constraining box and 'fitted' contains their bounding box. */

	CopyMem(&fitted, te, sizeof(struct TextExtent));
	return lower;
}

///
/// tt_GetPixmapA()

/****** ttengine.library/TT_GetPixmapA **************************************
*
*   NAME
*       TT_GetPixmapA -- Renders string to 8-bit grayscale pixel map. (V5)
*
*   SYNOPSIS
*       pixmap = TT_GetPixmapA (font, string, count, taglist)
*                                   A1    A2      D0     A0
*
*       struct TT_Pixmap *TT_GetPixmapA (APTR, APTR, ULONG, struct TagItem*);
*
*       pixmap = TT_GetPixmap (font, string, count, Tag1, ...)
*
*       ULONG TT_GetPixmap (APTR, APTR, ULONG, Tag, ...);
*
*   FUNCTION
*       Renders a string to 8-bit grayscale pixel map. Function may be useful
*       if the string image is additionally manipulated before rendering into
*       RastPort (e.g. in image processing software). Pixel map is a
*       continuous memory area where one byte represents one gray pixel (0 is
*       black, 255 is white). The first byte is the upper left pixmap corner.
*       Pixels are placed left-to-right in rows, rows are placed
*       top-to-bottom. There is no row padding, it means pixmap modulo is
*       always the same as pixmap width and pixmap data size is equal to width
*       * height. The function accepts following tags:
*
*       TT_Antialias - (BOOL) - controls antialiasing (on, off or
*         automatic):
*         TT_Antialias_Off - turns antialias off
*         TT_Antialias_On - turns antialias on
*         TT_Antialias_Auto (default) - antialias state depends on font
*           size. Typically sizes of 9 or less pixels are antialiased, sizes
*           from 10 to 19 pixels are not antialiased, sizes of 20 of more
*           pixels are antialiased. These settings can be changed in the font
*           database separately for every font face.
*
*       TT_Encoding - selects font encoding table to be used:
*         TT_Encoding_Default - loads encoding table from "ENV:ttfcodepage"
*           file, compatible with ttf.library. If no such file is found,
*           ISO-8859-1 encoding (Amiga default) is used.
*         TT_Encoding_ISO8859_x - where 'x' can be one of 1, 2, 3, 4, 5, 6,
*           7, 8, 9, 10, 11, 13, 14, 15, 16. One of 8-bit encodings defined
*           in ISO-8859 normative.
*         TT_Encoding_UTF16_BE - 16-bit Unicode, big endian (68k/PPC style).
*           Endian mark (0xFFFE) is ignored.
*         TT_Encoding_UTF32_BE - 32-bit Unicode, big endian (68k/PPC style).
*           Endian mark (0x0000FFFE) is ignored.
*         TT_Encoding_UTF16_LE - 16-bit Unicode, little endian (x86 style).
*           Endian mark (0xFFFE) is ignored.
*         TT_Encoding_UTF32_LE - 32-bit Unicode, little endian (x86 style).
*           Endian mark (0xFFFE0000) is ignored.
*         TT_Encoding_UTF16 - 16-bit Unicode, with endian determined by
*           endian mark. If no endian mark encountered big endian is assumed.
*         TT_Encoding_UTF32 - 32-bit Unicode, with endian determined by
*           endian mark. If no endian mark encountered big endian is assumed.
*         TT_Encoding_UTF8 - 8-bit Unicode encapsulation (RFC 2279).
*
*       TT_CustomEncoding - select custom 8-bit encoding table. Tag data
*         should point to 256 UWORDs, every containing one 16-bit big endian
*         Unicode character code. 8-bit character is used as an index of the
*         table. This tag overrides TT_Encoding regardless of its position in
*         the taglist if both are specified in the same taglist.
*
*       TT_ScaleX - a floating point (IEEE single precision) horizontal scale
*         factor. Its absolute value will be limited to <0.01, 100> range. It
*         may be negative (it means horizontal mirror around starting pen
*         position, string will be rendered to the left).
*
*       TT_ScaleY - a floating point (IEEE single precision) vertical scale
*         factor. Its absolute value will be limited to <0.01, 100> range. It
*         may be negative (it means vertical mirror around the baseline,
*         string will be rendered mostly under the baseline).
*
*       TT_DiskFontMetrics - changes TTEngine rendering to be diskfont-like.
*         In this mode pixmap area extends vertically between
*         TT_FontAccentedAscender and TT_FontRealDescender and as such is
*         equal to TT_FontHeight. Also if the left bearing of the first
*         glyph or the right bearing of the last glyph is negative, pixmap
*         area is extended to the left or right accordingly. Note however,
*         glyphs higher than TT_FontAccentedAscender or extending
*         below TT_FontRealDescender will be clipped. This mode is useful
*         especially for text-oriented applications using JAM2 draw mode.
*         Defaults to FALSE.
*
*   INPUTS
*       font - font pointer obtained with TT_OpenFont().
*       string - pointer to string to be rendered to a pixmap.
*       count - length of the string in characters (only equal to bytes in
*         pure 8-bit encodings).
*       taglist - list of rendering attributes.
*
*   RESULT
*       pixmap - structure describing a pixmap defined as follows:
*
*       struct TT_Pixmap
*         {
*           ULONG  ttp_Size;
*           ULONG  ttp_Width;
*           ULONG  ttp_Height;
*           UBYTE *ttp_Data;
*         };
*
*       ttp_Size is the size of the structure (not the pixmap) containing
*       useful informations including ttp_Size itself. For V5 pixmaps the size
*       is 16 bytes. ttp_Width is pixmap width (and modulo) in bytes.
*       ttp_Height is a number of pixmap rows. ttp_Data points to actual
*       pixmap data.
*
*       Function returns NULL if pixmap allocation fails.
*
*   NOTES
*       TT_Pixmap structure is READ ONLY.
*
*   BUGS
*
*   SEE ALSO
*       TT_FreePixmap(), TT_OpenFontA(), TT_SetAtrrsA()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

struct TT_Pixmap * VARARGS68K _TTEngine_TT_GetPixmap(struct TTEngineIFace *Self,
	APTR font,
	APTR string,
	ULONG count,
	...)
{
	va_list ap;
	struct TagItem *tags;

	va_startlinear(ap, count);
	tags = va_getlinearva(ap, struct TagItem *);

	return Self->TT_GetPixmapA(font, string, count, tags);
}

struct TT_Pixmap * VARARGS68K _TTEngine_TT_GetPixmapA(struct TTEngineIFace *Self,
	APTR font,
	APTR string,
	ULONG count,
	struct TagItem * taglist)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

struct TT_Pixmap *tt_GetPixmapA(void)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
	APTR font = (APTR)REG_A1;
	APTR string = (APTR)REG_A2;
	ULONG count = REG_D0;
	struct TagItem *taglist = (struct TagItem*)REG_A0;

#else

__ASM__ __SAVE_DS__ struct TT_Pixmap *tt_GetPixmapA(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a1, APTR font),
	__REG__(a2, APTR string),
	__REG__(d0, ULONG count),
	__REG__(a0, struct TagItem *taglist))
{

#endif

	USESYSBASE;
	USELIB(UtilityBase, IUtility, UtilityIFace);
	struct TT_Pixmap *pm;
	struct RenderEnv lre;
	UWORD *custenc;

	LD(kp(""));
	if ((!font) || (!string) || (!count)) return NULL;

	if ((pm = AllocPooled(ttb->ttb_MemPool, sizeof(struct TT_Pixmap))))
	{
		BOOL diskfont_metrics;

		pm->ttp_Size = sizeof(struct TT_Pixmap);

		lre.re_Flags = 0;
		lre.re_TargetRPort = NULL;
		lre.re_TargetCMap = NULL;
		lre.re_LoadedFont = font;
		lre.re_Antialias = GetTagData(TT_Antialias, TT_Antialias_Auto, taglist);
		lre.re_Encoding = GetTagData(TT_Encoding, TT_Encoding_Default, taglist);

		diskfont_metrics = GetTagData(TT_DiskFontMetrics, FALSE, taglist);

		if (diskfont_metrics) lre.re_Flags |= REF_AMIGAMETRICS;
		else lre.re_Flags &= ~REF_AMIGAMETRICS;

		if ((custenc = (UWORD*)GetTagData(TT_CustomEncoding, 0, taglist)))
		{
			lre.re_CustomEncTable = custenc;
			lre.re_Encoding = TT_Encoding_Custom;
		}

		set_encoding_callback(ttb, &lre);
		lre.re_Transparency = 0;
		lre.re_SoftStyle = GetTagData(TT_SoftStyle, TT_SoftStyle_None, taglist);
		lre.re_Matrix.xx = 0x00010000;
		lre.re_Matrix.xy = 0x00000000;
		lre.re_Matrix.yx = 0x00000000;
		lre.re_Matrix.yy = 0x00010000;
		lre.re_ForceFixed = GetTagData(TT_ForceFixedWidth, 0, taglist);

		/* determine antialias, render mode, bitmap mode */

		antialias_on(&lre);

		switch (lre.re_Antialias)
		{
			case TT_Antialias_Off: antialias_off(&lre); break;

			case TT_Antialias_Auto:
			{
				UWORD smin, smax, fsize;

				smin = ((struct LoadedFont*)font)->lf_NoAntialiasMin;
				smax = ((struct LoadedFont*)font)->lf_NoAntialiasMax;
				fsize = ((struct LoadedFont*)font)->lf_CachedSize->cs_PixelSize;
				if ((fsize >= smin) && (fsize <= smax)) antialias_off(&lre);
			}
		}

		extent_loop(ttb, &lre, string, count);

		if (lre.re_Flags & REF_AMIGAMETRICS)
		{
			pm->ttp_Height = lre.re_LoadedFont->lf_MaxTop + lre.re_LoadedFont->lf_MaxBottom;
			pm->ttp_Width = lre.re_TextExtent.te_Width;
		}
		else
		{
			pm->ttp_Height = lre.re_TextExtent.te_Extent.MaxY - lre.re_TextExtent.te_Extent.MinY + 1;
			pm->ttp_Width = lre.re_TextExtent.te_Extent.MaxX - lre.re_TextExtent.te_Extent.MinX + 1;
		}

		if ((pm->ttp_Data = AllocPooled(ttb->ttb_MemPool, pm->ttp_Width * pm->ttp_Height)))
		{
			get_string_pixmap(ttb, &lre, pm, string, count);
			return pm;
		}
		else FreePooled(ttb->ttb_MemPool, pm, sizeof(struct TT_Pixmap));
	}
	return NULL;
}

///
/// tt_FreePixmap()

/****** ttengine.library/TT_FreePixmap **************************************
*
*   NAME
*       TT_FreePixmap -- Releases grayscale pixmap. (V5)
*
*   SYNOPSIS
*       TT_FreePixmap (pixmap)
*                      A0
*
*       VOID TT_FreePixmap (struct TT_Pixmap*);
*
*   FUNCTION
*       Frees pixmap data and structure allocated by TT_GetPixmap().
*
*   INPUTS
*       pixmap - pointer to TT_Pixmap structure. May be NULL, function does
*         nothing in the case.
*
*   RESULT
*       none
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_GetPixmapA()
*
*****************************************************************************
*
*/

#ifdef __amigaos4__

VOID VARARGS68K _TTEngine_TT_FreePixmap(struct TTEngineIFace *Self, struct TT_Pixmap * pixmap)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

void tt_FreePixmap(VOID)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
	struct TT_Pixmap *pixmap = (struct TT_Pixmap*)REG_A0;

#else

__ASM__ __SAVE_DS__ void tt_FreePixmap(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a0, struct TT_Pixmap *pixmap))
{

#endif

	USESYSBASE;

	if (pixmap)
	{
		if (pixmap->ttp_Data)
		{
			FreePooled(ttb->ttb_MemPool, pixmap->ttp_Data, pixmap->ttp_Width * pixmap->ttp_Height);
		}
		FreePooled(ttb->ttb_MemPool, pixmap, pixmap->ttp_Size);
	}
	return;
}

///
/// tt_DoneRastPort()

/****** ttengine.library/TT_DoneRastPort ************************************
*
*   NAME
*       TT_DoneRastPort -- Releases TTEngine resources associated to a
*         RastPort. (V5)
*
*   SYNOPSIS
*       TT_DoneRastPort (rastport)
*                        A1
*
*       VOID TT_DoneRastPort (struct RastPort*);
*
*   FUNCTION
*       Frees internal TTEngine resources associated with a RastPort.
*       TTEngine creates rendering environment for every RastPort at first
*       call of TT_SetFont() or TT_SetAttrs() for this RastPort. You should
*       free this environment before disposing RastPort (typically before
*       closing a window or screen). Leaving rendering environment not freed
*       has two drawbacks. Firstly it causes memory leak, this memory is
*       recovered however when ttengine.library is expunged. Secondly it is
*       possible that a new RastPort will be created at the same address as
*       disposed one. Then TTEngine will not create new rendering context but
*       use old instead, which can lead to some unexpected behaviour.
*
*   INPUTS
*       rastport - pointer to RastPort. Rendering environment for this
*         RastPort will be disposed. Can be NULL, in the case function does
*         nothing.
*
*   RESULT
*       none
*
*   EXAMPLE
*       font = TT_OpenFont( \* tags *\ );
*       window = OpenWindowTags(NULL, \* tags *\);
*       TT_SetFont(rport, font);   \* environment is created here *\
*       TT_SetAttrs(rport, TT_Window, window, \* ... *\);
*
*       \* you can call TT_SetAttrs() and TT_SetFont() as many times *\
*       \* as you want *\
*
*       TT_DoneRastPort(rport);   \* environment is disposed *\
*       CloseWindow(window);      \* RastPort is disposed *\
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_SetAttrsA(), TT_SetFont()
*
****************************************************************************
*
*/

#ifdef __amigaos4__

VOID VARARGS68K _TTEngine_TT_DoneRastPort(struct TTEngineIFace *Self, struct RastPort *rp)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

void tt_DoneRastPort(void)
{
	struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
	struct RastPort *rp = (struct RastPort*)REG_A1;

#else

__ASM__ __SAVE_DS__ void tt_DoneRastPort(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a1, struct RastPort *rp))
{

#endif

	if (rp) envlist_remove(ttb, rp);
}

///
/// tt_AllocRequest()

/****** ttengine.library/TT_AllocRequest ************************************
*
*   NAME
*       TT_AllocRequest -- Allocates resources for TTEngine font requester.
*         (V6)
*
*   SYNOPSIS
*       requester = TT_AllocRequest ()
*
*       APTR TT_AllocRequest (VOID);
*
*   FUNCTION
*       Allocates resources used by TTEngine font requester. A taglist
*       returned by further TT_RequestA() is allocated here amongst other
*       resources. This call must be followed by TT_FreeRequest() call when
*       the taglist returned by TT_RequestA() is no longer needed.
*
*   INPUTS
*       none
*
*   RESULT
*       requester - pointer to TTEngine internal structure containing all the
*         allocated resources. Private data inside - do not touch please.
*         Returned value should be only used as a parameter to TT_RequestA()
*         and TT_FreeRequest() calls. Function can return NULL if resource
*         allocation fails.
*
*   EXAMPLE
*       see TT_RequestA().
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_RequestA(), TT_FreeRequest()
*
****************************************************************************
*
*/

/* moved to 'lib_req.c' */

///
/// tt_RequestA()

/****** ttengine.library/TT_RequestA ****************************************
*
*   NAME
*       TT_RequestA -- Opens and handles TTEngine font requester window. (V6)
*
*   SYNOPSIS
*       attrlist = TT_RequestA (requester, taglist)
*                               A0         A1
*
*       struct TagItem* TT_RequestA (APTR, struct TagItem*);
*
*       attrlist = TT_Request (requester, Tag1, ...)
*
*       struct TagItem* TT_Request (APTR, Tag, ...);
*
*   FUNCTION
*
*   INPUTS
*       requester - pointer to requester resources returned by
*         TT_AllocRequest(). Can be NULL, in the case no action is performed
*         and function returns NULL.
*
*       taglist - defines requester properties and gadgets. If NULL, empty
*         taglist is assumed (empty taglist is valid, all tags are optional).
*         Following tags are recognized:
*
*         TTRQ_Window (struct Window*) - Parent window of requester. If no
*           TTRQ_Screen tag is specified, the window structure is used to
*           determine on which screen to open the requesting window.
*
*         TTRQ_PubScreenName (STRPTR) - Name of a public screen to open on.
*           This overrides the screen used by TTRQ_Window.
*
*         TTRQ_Screen (struct Screen*) - Screen on which to open the
*           requester. This overrides the screen used by TTRQ_Window or by
*           TTRQ_PubScreenName.
*
*         TTRQ_TitleText (STRPTR) - Title to use for the requesting window.
*           Engilsh default is "Select TrueType font".
*
*         TTRQ_PositiveText (STRPTR) - Label of the positive gadget in the
*           requester. English default is "OK".
*
*         TTRQ_NegativeText (STRPTR) - Label of the negative gadget in the
*           requester. English default is "Cancel".
*
*         TTRQ_InitialLeftEdge (WORD) - Suggested left edge of requesting
*           window. Default is to center the window on the screen.
*
*         TTRQ_InitialTopEdge (WORD) - Suggested top edge of requesting
*           window. Default is to center the window on the screen.
*
*         TTRQ_InitialWidth (WORD) - Suggested width of requesting window.
*           Default is 200 or 25% of screen width whichever is greater.
*
*         TTRQ_InitialHeight (WORD) - Suggested height of requesting window.
*           Default is 200 or 50% of screen height whichever is greater.
*
*         TTRQ_DoSizes (BOOL) - Controls font size listview and string. If
*           FALSE, size listview is not displayed. Defaults to TRUE to be
*           consistent with ASL font requester.
*
*         TTRQ_DoStyle (BOOL) - Controls font style (regular / italic) cycle
*           gadget. Defaults to FALSE (no style cycle gadget).
*
*         TTRQ_DoWeight (BOOL) - Controls font weight cycle gadgets. TTEngine
*           distincts 9 font weights enumerated as 100, 200, ... 900
*           according to Cascading Style Sheets specification. Of course not
*           every font has all the 9 weights, cycle gadget always show only
*           available ones. Defaults to FALSE (no weight cycle gadget).
*
*         TTRQ_Activate (BOOL) - Specifies if requester window is to be
*           activated after opening. Defaults to TRUE.
*
*         TTRQ_FixedWidthOnly (BOOL) - Requester displays only monospaced
*           fonts. Defaults to FALSE.
*
*   RESULT
*       attrlist - a list of font attributes. You should pass it to
*         TT_OpenFontA() and then to TT_SetAttrsA() to have a font open
*         exactly matching user choice. Taglist is valid until you call
*         TT_FreeRequest().
*
*   EXAMPLE
*       APTR font, request;
*       struct RastPort *rp;    \* initialized elsewhere *\
*
*       request = TT_AllocRequest();
*       attrlist = TT_Request(request, TAG_END);
*       if (attrlist)
*         {
*           if (font = TT_OpenFontA(attrlist))
*             {
*                TT_SetAttrsA(rp, attrlist));
*                if (TT_SetFont(rp, font))
*                  {
*                     \* use the font *\
*                  }
*                TT_DoneRastPort(rp);
*             }
*           TT_CloseFont(font);
*         }
*       TT_FreeRequest(request);
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_SetAttrsA(), TT_SetFont(), TT_OpenFontA(), TT_SetAttrsA().
*
****************************************************************************
*
*/

/* moved to 'lib_req.c' */

///
/// tt_FreeRequest()

/****** ttengine.library/TT_FreeRequest *************************************
*
*   NAME
*       TT_FreeRequest -- Frees resources allocated for TTEngine font
*         request. (V6)
*
*   SYNOPSIS
*       TT_FreeRequest (request)
*                       A0
*
*       VOID TT_FreeRequest (APTR);
*
*   FUNCTION
*       Frees internal TTEngine resources allocated for a font request.
*       Taglist returned by TT_AllocRequestA() is also freed by this call,
*       so after it the taglist is no longer valid and must not be used or
*       referenced.
*
*   INPUTS
*       requester - pointer returned by previous call to TT_AllocRequest().
*         NULL is a valid value here, no action will be performed in the
*         case.
*
*   RESULT
*       none
*
*   EXAMPLE
*       See TT_RequestA().
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_AllocRequest(), TT_RequestA()
*
*****************************************************************************
*
*/

/* moved to 'lib_req.c' */

///
/// tt_ObtainFamilyListA()

/****** ttengine.library/TT_ObtainFamilyListA *******************************
*
*   NAME
*       TT_ObtainFamilyListA -- Obtains a list of available font families.
*         (V7)
*
*   SYNOPSIS
*       list = TT_ObtainFamilyListA (taglist)
*                             A0
*
*       STRPTR* TT_ObtainFamilyListA (struct TagItem*);
*
*   FUNCTION
*       Creates a NULL-terminated table of pointers to strings. Every pointer
*       points to a NULL-terminated string being a name of font family. Such
*       a table can be directly passed to MUI cycle or list class. Family
*       table is filtered according to passed tasks. The table is a local
*       copy so can be modified by calling application (but you must not
*       increase the size!).
*
*   INPUTS
*       taglist - a list of filter tags. Currently defined tags are:
*
*         TTRQ_FixedWidthOnly - if TRUE only monospaced families are listed.
*           Defaults to FALSE.
*
*       NULL taglist is valid input and is treated as empty taglist.
*
*   RESULT
*       list - pointer to a NULL-terminated table of pointers to names. The
*         function can return NULL if memory allocation fails or there is no
*         families in the database.
*
*   EXAMPLE
*       Following example prints out all avaliable families to the standard
*       output:
*
*       STRPTR *families;
*
*       if (families = TT_ObtainFamilyListA(NULL))
*       {
*         STRPTR *p = families;
*
*         while (*p) Printf("%s\n", *p++);
*         TT_FreeFamilyList(families);
*       }
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_FreeFamilyList()
*
*****************************************************************************
*
*/

/* moved to 'lib_famlist.c' */

///
/// tt_FreeFamilyList()

/****** ttengine.library/TT_FreeFamilyList **********************************
*
*   NAME
*       TT_FreeFamilyList -- Frees family list allocated with
*         TT_ObtainFamilyListA(). (V7)
*
*   SYNOPSIS
*       TT_FreeFamilyList (list)
*                          A0
*
*       VOID TT_FreeFamilyList (STRPTR*);
*
*   FUNCTION
*       Frees family table obtained by TT_ObtainFamilyListA(). You should
*       call this function when family table is no more needed.
*
*   INPUTS
*       list - pointer to family table, it is the result of
*         TT_ObtainFamilyTable() call. It can be NULL, the function just does
*         nothing in the case.
*
*   RESULT
*       none
*
*   EXAMPLE
*       See TT_ObtainFamilyListA().
*
*   NOTES
*
*   BUGS
*
*   SEE ALSO
*       TT_ObtainFamilyListA()
*
****************************************************************************
*
*/

/* moved to 'lib_famlist.c' */

///
