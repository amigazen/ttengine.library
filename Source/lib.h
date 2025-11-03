/* $VER: lib.h 7.2 (12.1.2005) */

#ifndef TTE_LIB_H
#define TTE_LIB_H

#define __NOLIBBASE__

#include <ft2build.h>
#include FT_FREETYPE_H
#include <graphics/text.h>
#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <dos/dos.h>

#ifdef __MORPHOS__
#include <clib/debug_protos.h>
#endif

#ifdef __amigaos4__
#include <interfaces/ttengine.h>
#endif

#include "ttengine.h"

#define reg(x) __asm(#x)

#ifndef __amigaos4__
#define USELIB(b, i, s) struct Library * b = ttb->ttb_##b
#define USELIB_G(b, i, s) struct Library * b = Ttb->ttb_##b
#define USESYSBASE struct Library *SysBase = ttb->ttb_SysBase
#define USESYSBASE_G struct Library *SysBase = Ttb->ttb_SysBase
#else
#define USELIB(b, i, s) struct s * i = ttb->ttb_##i
#define USELIB_G(b, i, s) struct s * i = Ttb->ttb_##i
#define USESYSBASE struct ExecIFace *IExec = ttb->ttb_IExec
#define USESYSBASE_G struct ExecIFace *IExec = Ttb->ttb_IExec
#endif

extern struct TTEngineBase *Ttb;

#ifndef __amigaos4__
extern struct Library *SysBase;
#endif

/* === DEBUG STUFF === */

#define LD(x)
#define kp(a) kprintf("TTEngine: [" __FILE__ "/%ld] " __FUNCTION__ "() " a "\n", __LINE__)
#define kp1(a, b) kprintf("TTEngine: [" __FILE__ "/%ld] " __FUNCTION__ "() " a "\n", __LINE__, (ULONG)b)
#define kp2(a, b, c) kprintf("TTEngine: [" __FILE__ "/%ld] " __FUNCTION__ "() " a "\n", __LINE__, (ULONG)b, (ULONG)c)

/* destination bitmap types */

#define DEST_LUT       0
#define DEST_RGB       1


/* ====== PRIVATE STRUCTURES, DO NOT TOUCH! ====== */

struct FontName
{
	STRPTR fn_Text;
	ULONG fn_Length;
};

/* LoadedFont flags */

#define LFF_SOFTITALIC     0x00000001           /* font is sheared right to generate italic style */


struct LoadedFont
{
	struct Node            lf_Node;
	struct SignalSemaphore lf_Lock;             /* lf_Face operations lock */
	FT_Face                lf_Face;
	struct CachedSize     *lf_CachedSize;
	LONG                   lf_Ascender;
	LONG                   lf_Descender;
	UWORD                  lf_NoAntialiasMin;
	UWORD                  lf_NoAntialiasMax;
	WORD                   lf_UnderPos;         /* underline position (scaled) */
	WORD                   lf_UnderThk;         /* underline thickness (scaled) */
	WORD                   lf_StrikePos;        /* overstrike position (scaled) */
	struct FontName        lf_Name;
	struct FontName        lf_Family;
	struct FontName        lf_FullName;
	ULONG                  lf_OpenCnt;
	ULONG                  lf_Flags;
	LONG                   lf_DesignHeight;
	LONG                   lf_MaxTop;
	LONG                   lf_MaxBottom;
	LONG                   lf_RealAscender;
	LONG                   lf_RealDescender;
	LONG                   lf_AccentedAscender;
	LONG                   lf_Width;
};

struct TTEngineBase
{
	struct Library             ttb_Lib;
	struct Library            *ttb_SysBase;
	struct Library            *ttb_DOSBase;
	struct Library            *ttb_GfxBase;
	struct Library            *ttb_CyberGfxBase;
	struct Library            *ttb_P96Base;
	struct Library            *ttb_UtilityBase;
	struct Library            *ttb_LocaleBase;
	struct Library            *ttb_LayersBase;

	#ifdef __amigaos4__
	struct ExecIFace          *ttb_IExec;
	struct DOSIFace           *ttb_IDOS;
	struct GraphicsIFace      *ttb_IGraphics;
	struct CyberGfxIFace      *ttb_ICyberGfx;
	struct UtilityIFace       *ttb_IUtility;
	struct LocaleIFace        *ttb_ILocale;
	struct LayersIFace        *ttb_ILayers;
	#else
	struct Library            *ttb_MathIeeeSingBasBase;
	struct Library            *ttb_MathIeeeSingTransBase;
	#endif

	APTR                       ttb_SegList;
	FT_Library                 ttb_FTLib;
	struct List                ttb_DataBase;          /* system font database */
	struct MinList             ttb_MemoryCache;       /* list of CachedFont structures */
	struct SignalSemaphore    *ttb_Semaphore;
	struct MinList             ttb_EnvList;           /* list of environments (RastPorts) */
	struct List                ttb_FontList;
	UWORD                     *ttb_CodePage;          /* default encoding table */
	ULONG                      ttb_MemCacheSize;
	ULONG                      ttb_MemCacheLimit;
	APTR                       ttb_MemPool;
	BOOL                       ttb_InitFlag;
	ULONG                      ttb_DefGamma;          /* default gamma correction coefficent */

	/* debug only */

	#ifdef TTR_DEBUG
	BPTR debug;
	#endif
};

/* RenderEnv flags */

#define REF_TRANSFORMED        0x0001
#define REF_AMIGAMETRICS       0x0002

#define TT_Encoding_Custom     0xFFFFFFFF    /* INTERNAL ONLY */

struct RenderEnv
{
	struct MinNode         re_Node;
	struct RastPort       *re_TargetRPort;
	struct ColorMap       *re_TargetCMap;
	struct LoadedFont     *re_LoadedFont;
	ULONG                  re_Antialias;
	ULONG                  re_Encoding;         /* XXX to Unicode encoding */
	ULONG (*re_GetNextChar)(struct TTEngineBase*, struct RenderEnv*, STRPTR*);
	UWORD                 *re_CustomEncTable;
	struct TextExtent      re_TextExtent;       /* for computing metrics */
	BYTE                   re_RenderMode;
	BYTE                   re_BitmapMode;
	BYTE                   re_DestBitmapType;
	UBYTE                  re_Transparency;
	UWORD                  re_Flags;
	UWORD                  re_SoftStyle;
	FT_Matrix              re_Matrix;
	ULONG                  re_Foreground;
	ULONG                  re_Background;
	BOOL                   re_UnicodeInitial;
	BOOL                   re_BigEndian;
	ULONG                  re_ForceFixed;
	ULONG                  re_GammaCf;
	UBYTE                  re_Gamma[256];       /* gamma correction lookup table */
	UBYTE                  re_AntiGamma[256];
	APTR                   re_Pixmap;
	ULONG                  re_PixmapSize;
	APTR                   re_RenderBuffer;
	ULONG                  re_RendBufSize;
};


struct Parser
{
	LONG params[6];
	struct RDArgs *args;
	struct TTFamily *current_family;
};


struct TTFamily
{
	struct Node node;
	struct TTFamily *alias;
	struct List faces;
};


struct TTFont
{
	struct Node node;
	STRPTR filepath;
	WORD weight;
	WORD style;
	WORD smoothsmall;
	WORD smoothbig;
	BOOL fixed;
};

/* cached one glyph bitmap */

struct CachedBitmap
{
	struct MinNode   cb_Node;
	LONG             cb_AdvanceX;
	LONG             cb_AdvanceY;
	WORD             cb_OffsetX;
	WORD             cb_OffsetY;
	LONG             cb_LeftBearing;
	LONG             cb_RightBearing;
	LONG             cb_TopBearing;
	LONG             cb_BottomBearing;
	FT_Bitmap        cb_Bitmap;
	ULONG            cb_Index;
};

/* cached typeface of given size */

struct CachedSize
{
	struct MinNode   cs_Node;
	ULONG            cs_Users;
	UWORD            cs_PixelSize;
	struct MinList   cs_Bitmaps;    /* list of CachedBitmap structures */
};

/* cached multiple sizes of one face */

struct CachedFont
{
	struct MinNode   cf_Node;
	UBYTE           *cf_Name;
	struct MinList   cf_Sizes;      /* list of CachedSize structures */
};

/* Memory cache functions ("lib_cache.c")*/

struct CachedBitmap *cache_add_bitmap(struct TTEngineBase *ttb, struct CachedSize *cs, FT_GlyphSlot glyph, ULONG index);
struct CachedBitmap *cache_find_bitmap(struct TTEngineBase *ttb, struct CachedSize *cs, ULONG index, char mode);
void cache_flush_bitmap(struct TTEngineBase *ttb, struct CachedBitmap *cb);

struct CachedSize *cache_add_size(struct TTEngineBase *ttb, struct CachedFont *cf, UWORD size);
struct CachedSize *cache_find_size(struct TTEngineBase *ttb, struct CachedFont *cf, UWORD size);
void cache_flush_size(struct TTEngineBase *ttb, struct CachedSize *cs);

struct CachedFont *cache_add_font(struct TTEngineBase *ttb, char *name);
struct CachedFont *cache_find_font(struct TTEngineBase *ttb, char *name);
void cache_flush_font(struct TTEngineBase *ttb, struct CachedFont *cf);

void cache_flush_all(struct TTEngineBase *ttb);

/* cliprects hook message */

struct CRHookMsg
{
	struct Layer *layer;
	struct Rectangle bounds;
	LONG offsetx;
	LONG offsety;
};

/* RastPort alphablending functions ("lib_blend.c") */

struct Rgb
{
	UBYTE r;
	UBYTE g;
	UBYTE b;
};

void get_rp_fgcolor(struct TTEngineBase *ttb, struct RenderEnv *re, struct Rgb *dest);
void get_rp_bgcolor(struct TTEngineBase *ttb, struct RenderEnv *re, struct Rgb *dest);
UBYTE blend(UBYTE *g, UBYTE bg, UBYTE fg, UBYTE a);

void draw_jam1(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void draw_jam2(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void blend_jam1(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void blend_jam2(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void blend_jam1_xor(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void blend_jam2_xor(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void transp_jam1(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void transp_jam2(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void transp_jam1_xor(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);
void transp_jam2_xor(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y);


/* structure passed to dirblit_xxxxx() functions */

struct DirBlitData
{
	struct TTEngineBase *ttb;
	struct TT_Pixmap *px;
	struct RenderEnv *re;
	LONG srcx;                  // source coordinates
	LONG srcy;                  // source coordinates
	APTR bitmap_base;
	LONG bitmap_mod;            // in bytes
	struct Rectangle bounds;    // screen coordinates
	BOOL background;            // JAM2 if true
	BOOL xor;                   // COMPLEMENT if true
};


/* user data structure passed to cliprects hook */

struct CrhUData
{
	struct TTEngineBase *ttb; // the library base
	struct TT_Pixmap *px;     // the pixmap with string to blit
	struct RenderEnv *re;     // rendering environment for blit mode
	LONG srcx;								// left edge of the string rectangle in RastPort
	LONG srcy;                // top edge of the string rectangle in RastPort
	void(*blitfunc)(struct DirBlitData*);
};


/* cliprects hook functions */

void dirblit_bgra32(struct DirBlitData*);


/* API functions ("lib.c") */

#ifdef __amigaos4__

struct TTEngineBase *LibOpen(struct LibraryManagerInterface *Self, ULONG version);
APTR LibClose(struct LibraryManagerInterface *Self);
APTR LibExpunge(struct LibraryManagerInterface *Self);

APTR                 VARARGS68K _TTEngine_TT_OpenFontA(struct TTEngineIFace *, struct TagItem * taglist);
APTR                 VARARGS68K _TTEngine_TT_OpenFont(struct TTEngineIFace *, ...);
BOOL                 VARARGS68K _TTEngine_TT_SetFont(struct TTEngineIFace *, struct RastPort * rp, APTR font);
VOID                 VARARGS68K _TTEngine_TT_CloseFont(struct TTEngineIFace *, APTR font);
VOID                 VARARGS68K _TTEngine_TT_Text(struct TTEngineIFace *, struct RastPort * rp, APTR string, ULONG count);
ULONG                VARARGS68K _TTEngine_TT_SetAttrsA(struct TTEngineIFace *, struct RastPort * rp, struct TagItem * taglist);
ULONG                VARARGS68K _TTEngine_TT_SetAttrs(struct TTEngineIFace *, struct RastPort * rp, ...);
ULONG                VARARGS68K _TTEngine_TT_GetAttrsA(struct TTEngineIFace *, struct RastPort * rp, struct TagItem * taglist);
ULONG                VARARGS68K _TTEngine_TT_GetAttrs(struct TTEngineIFace *, struct RastPort * rp, ...);
ULONG                VARARGS68K _TTEngine_TT_TextLength(struct TTEngineIFace *, struct RastPort * rp, APTR string, ULONG count);
VOID                 VARARGS68K _TTEngine_TT_TextExtent(struct TTEngineIFace *, struct RastPort * rp, APTR string, WORD count, struct TextExtent * te);
ULONG                VARARGS68K _TTEngine_TT_TextFit(struct TTEngineIFace *, struct RastPort * rp, APTR string, UWORD count, struct TextExtent * te, struct TextExtent * tec, WORD dir, UWORD cwidth, UWORD cheight);
struct TT_Pixmap *   VARARGS68K _TTEngine_TT_GetPixmapA(struct TTEngineIFace *, APTR font, APTR string, ULONG count, struct TagItem * taglist);
struct TT_Pixmap *   VARARGS68K _TTEngine_TT_GetPixmap(struct TTEngineIFace *, APTR font, APTR string, ULONG count, ...);
VOID                 VARARGS68K _TTEngine_TT_FreePixmap(struct TTEngineIFace *, struct TT_Pixmap * pixmap);
VOID                 VARARGS68K _TTEngine_TT_DoneRastPort(struct TTEngineIFace *, struct RastPort * rp);

#elif __MORPHOS__

struct TTEngineBase *LibOpen(void);
LONG LibClose(void);
APTR LibExpunge(void);
APTR MyLibExpunge(struct TTEngineBase *ttb);
LONG LibReserved(void);

APTR tt_OpenFontA(void);
BOOL tt_SetFont(void);
void tt_CloseFont(void);
void tt_Text(void);
ULONG tt_SetAttrsA(void);
ULONG tt_GetAttrsA(void);
ULONG tt_TextLength(void);
void tt_TextExtent(void);
ULONG tt_TextFit(void);
struct TT_Pixmap *tt_GetPixmapA(void);
void tt_FreePixmap(void);
void tt_DoneRastPort(void);

#else

struct TTEngineBase *LibInit(void *seglist reg(a0), struct Library *sysb reg(a6));
struct TTEngineBase *LibOpen(struct TTEngineBase *ttb reg(a6));
LONG LibClose(struct TTEngineBase *ttb reg(a6));
APTR LibExpunge(struct TTEngineBase *ttb reg(a6));
LONG LibReserved(void);

APTR tt_OpenFontA(struct TTEngineBase *ttb reg(a6), struct TagItem *taglist reg(a0));
BOOL tt_SetFont(struct TTEngineBase *ttb reg(a6), struct RastPort *rp reg(a1), APTR font reg(a0));
void tt_CloseFont(struct TTEngineBase *ttb reg(a6), APTR font reg(a0));
void tt_Text(struct TTEngineBase *ttb reg(a6), struct RastPort *rp reg(a1), APTR string reg(a0), ULONG count reg(d0));
ULONG tt_SetAttrsA(struct TTEngineBase *ttb reg(a6), struct RastPort *rp reg(a1), struct TagItem *taglist reg(a0));
ULONG tt_GetAttrsA(struct TTEngineBase *ttb reg(a6), struct RastPort *rp reg(a1), struct TagItem *taglist reg(a0));
ULONG tt_TextLength(struct TTEngineBase *ttb reg(a6), struct RastPort *rp reg(a1), APTR string reg(a0), ULONG count reg(d0));
void tt_TextExtent(struct TTEngineBase *ttb reg(a6), struct RastPort *rp reg(a1), APTR string reg(a0), WORD count reg(d0), struct TextExtent *te reg(a2));
ULONG tt_TextFit(struct TTEngineBase *ttb reg(a6), struct RastPort *rp reg(a1), APTR string reg(a0), UWORD count reg(d0), struct TextExtent *te reg(a2), struct TextExtent *tec reg(a3), WORD dir reg(d1), UWORD cwidth reg(d2), UWORD cheight reg(d3));
struct TT_Pixmap *tt_GetPixmapA(struct TTEngineBase *ttb reg(a6), APTR font reg(a1), APTR string reg(a2), ULONG count reg(d0), struct TagItem *taglist reg(a0));
void tt_FreePixmap(struct TTEngineBase *ttb reg(a6), struct TT_Pixmap *pixmap reg(a0));
void tt_DoneRastPort(struct TTEngineBase *ttb reg(a6), struct RastPort *rp reg(a1));

#endif /* #ifdef __MORPHOS__ */

#endif
