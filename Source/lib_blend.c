/* TTEngine alphablending and monochrome imposing functions */

#define __NOLIBBASE__

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>

#include "lib.h"
#include <math.h>

//==========================================================================================================================================================
// get_rp_fgcolor()
//==========================================================================================================================================================

void get_rp_fgcolor(struct TTEngineBase *ttb, struct RenderEnv *re, struct Rgb *dest)
{
	USELIB(GfxBase, IGraphics, GraphicsIFace);

	ULONG tcolor[3];
	struct RastPort *rp = re->re_TargetRPort;
	struct ColorMap *cm = re->re_TargetCMap;

	if ((re->re_DestBitmapType == DEST_LUT) || (re->re_Foreground == TT_Foreground_UseRastPort))
	{
		GetRGB32(cm, GetAPen(rp), 1, tcolor);
		dest->r = tcolor[0] >> 24;
		dest->g = tcolor[1] >> 24;
		dest->b = tcolor[2] >> 24;
//		kprintf("TTE: get_rp_fgcolor() from APen of RPort $%08lx: active pen %ld, rgb $00%02lx%02lx%02lx.\n", rp, GetAPen(rp), dest->r, dest->g, dest->b);
	}
	else
	{
		dest->r = (re->re_Foreground & 0x00FF0000) >> 16;
		dest->g = (re->re_Foreground & 0x0000FF00) >> 8;
		dest->b = (re->re_Foreground & 0x000000FF);
//		kprintf("TTE: get_rp_fgcolor() from REnv: rgb $%08lx.\n", re->re_Foreground);
	}
}

//==========================================================================================================================================================
// get_rp_bgcolor()
//==========================================================================================================================================================

void get_rp_bgcolor(struct TTEngineBase *ttb, struct RenderEnv *re, struct Rgb *dest)
{
	USELIB(GfxBase, IGraphics, GraphicsIFace);

	ULONG tcolor[3];
	struct RastPort *rp = re->re_TargetRPort;
	struct ColorMap *cm = re->re_TargetCMap;

	if (re->re_DestBitmapType == DEST_LUT || re->re_Background == TT_Background_UseRastPort)
	{
		GetRGB32(cm, GetBPen(rp), 1, tcolor);
		dest->r = tcolor[0] >> 24;
		dest->g = tcolor[1] >> 24;
		dest->b = tcolor[2] >> 24;
	}
	else
	{
		dest->r = (re->re_Background & 0x00FF0000) >> 16;
		dest->g = (re->re_Background & 0x0000FF00) >> 8;
		dest->b = (re->re_Background & 0x000000FF);
	}
}

//==========================================================================================================================================================
// expand_render_buffer()
//==========================================================================================================================================================

static BOOL expand_render_buffer(struct TTEngineBase *ttb, struct RenderEnv *re, ULONG new_size)
{
	USESYSBASE;

	if (re->re_RenderBuffer)
	{
		FreePooled(ttb->ttb_MemPool, re->re_RenderBuffer, re->re_RendBufSize);
	}

	re->re_RenderBuffer = NULL;
	re->re_RendBufSize = 0;

	if ((re->re_RenderBuffer = AllocPooled(ttb->ttb_MemPool, new_size)))
	{
		re->re_RendBufSize = new_size;
		return TRUE;
	}
	return FALSE;
}

//==========================================================================================================================================================
// blend()
//==========================================================================================================================================================

UBYTE blend(UBYTE *g, UBYTE bg, UBYTE fg, UBYTE a)
{
	if (bg < fg) return (bg + (((fg - bg) * g[a]) >> 8));
	else return (fg + (((bg - fg) * g[255-a]) >> 8));
}

//==========================================================================================================================================================
// blend_jam1()
//==========================================================================================================================================================

void blend_jam1(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re, LONG x, LONG y)
{
	USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
	ULONG ix, iy;
	ULONG bufsize;
	UBYTE *src, *dst, *g, *ag;
	UBYTE pix;
	struct Rgb fg;
	struct RastPort *rp = re->re_TargetRPort;

	g = re->re_Gamma;
	ag = re->re_AntiGamma;

	/* get RastPort foreground color */

	get_rp_fgcolor(ttb, re, &fg);

	/* compute render buffer size and expand if too small */

	bufsize = px->ttp_Width * px->ttp_Height * sizeof(ULONG);

	if (bufsize > re->re_RendBufSize)
	{
		if (!expand_render_buffer(ttb, re, bufsize)) return;
	}

	/* load background from RastPort to render buffer */

	ReadPixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG), rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);

	/* alphablend pixmap using RastPort foreground color */

	src = px->ttp_Data;
	dst = re->re_RenderBuffer;

	for (ix = 0; ix < px->ttp_Width; ix++)
	{
		for (iy = 0; iy < px->ttp_Height; iy++)
		{
			*dst++ = 0x00;
			pix = *src++;

			if (pix == 0)           /* background untouched */
			{
				dst += 3;
			}
			else if (pix == 0xFF)   /* full foreground colour */
			{
				*dst++ = fg.r;
				*dst++ = fg.g;
				*dst++ = fg.b;
			}
			else
			{
				*dst++ = blend(g, *dst, fg.r, pix);
				*dst++ = blend(g, *dst, fg.g, pix);
				*dst++ = blend(g, *dst, fg.b, pix);
			}
		}
	}

	/* write out alphablended output */

	WritePixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG), rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);
}

//==========================================================================================================================================================
// blend_jam2()
//==========================================================================================================================================================

VOID blend_jam2(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
    ULONG ix, iy;
    ULONG bufsize;
    UBYTE *src, *dst, *g, *ag;
    UBYTE pix;
    struct Rgb fg, bg;
    struct RastPort *rp = re->re_TargetRPort;

    g = re->re_Gamma;
    ag = re->re_AntiGamma;

    /* get RastPort foreground  and background colors */

    get_rp_fgcolor(ttb, re, &fg);
    get_rp_bgcolor(ttb, re, &bg);

    /* compute render buffer size and expand if too small */

    bufsize = px->ttp_Width * px->ttp_Height * sizeof(ULONG);
    if (bufsize > re->re_RendBufSize)
      {
        if (!expand_render_buffer(ttb, re, bufsize)) return;
      }

    /* alphablend pixmap using RastPort foreground and background colors */

    src = px->ttp_Data;
    dst = re->re_RenderBuffer;

    for (ix = 0; ix < px->ttp_Width; ix++)
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            *dst++ = 0x00;
            pix = *src++;

            if (pix == 0)   /* background colour */
              {
                *dst++ = bg.r;
                *dst++ = bg.g;
                *dst++ = bg.b;
              }
            else if (pix == 0xFF)   /* foreground colour */
              {
                *dst++ = fg.r;
                *dst++ = fg.g;
                *dst++ = fg.b;
              }
            else
              {
                *dst++ = blend (g, bg.r, fg.r, pix);
                *dst++ = blend (g, bg.g, fg.g, pix);
                *dst++ = blend (g, bg.b, fg.b, pix);
              }
          }
      }

    /* write out alphablended output */

    WritePixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);
  }

///
/// blend_jam1_xor()

VOID blend_jam1_xor(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
    ULONG ix, iy;
    ULONG bufsize;
    UBYTE *src, *dst, *g, *ag;
    UBYTE pix;
    struct Rgb fg;
    struct RastPort *rp = re->re_TargetRPort;
    UBYTE g1[256];

    for (ix = 0; ix < 256; ix++) g1[ix] = ix;

    g = g1; /*re->re_Gamma;*/
    ag = g1; /*re->re_AntiGamma; */

    /* get RastPort foreground color */

    get_rp_fgcolor(ttb, re, &fg);

    /* compute render buffer size and expand if too small */

    bufsize = px->ttp_Width * px->ttp_Height * sizeof(ULONG);
    if (bufsize > re->re_RendBufSize)
      {
        if (!expand_render_buffer(ttb, re, bufsize)) return;
      }

    /* load background from RastPort to render buffer */

    ReadPixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);

    /* alphablend pixmap using RastPort foreground color */

    src = px->ttp_Data;
    dst = re->re_RenderBuffer;

    for (ix = 0; ix < px->ttp_Width; ix++)
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            *dst++ = 0x00;
            pix = *src++;

            if (pix == 0x00) dst += 3;
            else if (pix == 0xFF)
              {
                *dst++ ^= fg.r;
                *dst++ ^= fg.g;
                *dst++ ^= fg.b;
              }
            else
              {
                *dst++ ^= blend (g, 0, fg.r, pix);
                *dst++ ^= blend (g, 0, fg.g, pix);
                *dst++ ^= blend (g, 0, fg.b, pix);
              }
          }
      }

    /* write out alphablended output */

    WritePixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);
  }

///
/// blend_jam2_xor()

VOID blend_jam2_xor(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
    ULONG ix, iy;
    ULONG bufsize;
    UBYTE *src, *dst, *g, *ag;
    UBYTE pix;
    struct Rgb fg, bg;
    struct RastPort *rp = re->re_TargetRPort;

    g = re->re_Gamma;
    ag = re->re_AntiGamma;

    /* get RastPort foreground  and background colors */

    get_rp_fgcolor(ttb, re, &fg);
    get_rp_bgcolor(ttb, re, &bg);

    /* compute render buffer size and expand if too small */

    bufsize = px->ttp_Width * px->ttp_Height * sizeof(ULONG);
    if (bufsize > re->re_RendBufSize)
      {
        if (!expand_render_buffer(ttb, re, bufsize)) return;
      }

    /* load background from RastPort to render buffer */

    ReadPixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);

    /* alphablend pixmap using RastPort foreground and background colors */

    src = px->ttp_Data;
    dst = re->re_RenderBuffer;

    for (ix = 0; ix < px->ttp_Width; ix++)
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            *dst++ = 0x00;
            pix = *src++;

            if (pix == 0x00)
              {
                *dst++ ^= bg.r;
                *dst++ ^= bg.g;
                *dst++ ^= bg.b;
              }
            else if (pix == 0xFF)
              {
                *dst++ ^= fg.r;
                *dst++ ^= fg.g;
                *dst++ ^= fg.b;
              }
            else
              {
                *dst++ ^= blend (g, bg.r, fg.r, pix);
                *dst++ ^= blend (g, bg.g, fg.g, pix);
                *dst++ ^= blend (g, bg.b, fg.b, pix);
              }
          }
      }

    /* write out alphablended output */

    WritePixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);
  }

///
/// draw_jam1()

VOID draw_jam1(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(GfxBase, IGraphics, GraphicsIFace);
    ULONG ix, iy;
    UBYTE *src;
    struct RastPort *rp = re->re_TargetRPort;
    ULONG mode;

    src = px->ttp_Data;

    if ((mode = GetDrMd(rp)) & INVERSVID)
      {
        SetDrMd(rp, mode & ~INVERSVID);
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            for (ix = 0; ix < px->ttp_Width; ix++)
              {
                if (!*src++) WritePixel(rp, x + ix, y + iy);
              }
          }
        SetDrMd(rp, mode);
      }
    else
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            for (ix = 0; ix < px->ttp_Width; ix++)
              {

                if (*src++) WritePixel(rp, x + ix, y + iy);
              }
          }
      }
  }

///
/// draw_jam2()

VOID draw_jam2(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(GfxBase, IGraphics, GraphicsIFace);
    ULONG pen, mode;
    ULONG ix, iy;
    UBYTE *src;
    struct RastPort *rp = re->re_TargetRPort;

    /* draw background first */

    pen = GetAPen(rp);
    SetAPen(rp, GetBPen(rp));
    RectFill(rp, x, y, x + px->ttp_Width - 1, y + px->ttp_Height - 1);
    SetAPen(rp, pen);

    /* plotting pixmap using RastPort foreground color */

    src = px->ttp_Data;

    if ((mode = GetDrMd(rp)) & INVERSVID)
      {
        SetDrMd(rp, mode & ~INVERSVID);
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            for (ix = 0; ix < px->ttp_Width; ix++)
              {
                if (!*src++) WritePixel(rp, x + ix, y + iy);
              }
          }
        SetDrMd(rp, mode);
      }
    else
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            for (ix = 0; ix < px->ttp_Width; ix++)
              {

                if (*src++) WritePixel(rp, x + ix, y + iy);
              }
          }
      }
  }

///
/// transp_jam1()

VOID transp_jam1(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
    ULONG ix, iy;
    ULONG bufsize;
    UBYTE *src, *dst, *g, *ag;
    UBYTE pix, alpha = ~re->re_Transparency;
    struct Rgb fg;
    struct RastPort *rp = re->re_TargetRPort;

    g = re->re_Gamma;
    ag = re->re_AntiGamma;

    /* get RastPort foreground color */

    get_rp_fgcolor(ttb, re, &fg);

    /* compute render buffer size and expand if too small */

    bufsize = px->ttp_Width * px->ttp_Height * sizeof(ULONG);
    if (bufsize > re->re_RendBufSize)
      {
        if (!expand_render_buffer(ttb, re, bufsize)) return;
      }

    /* load background from RastPort to render buffer */

    ReadPixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);

    /* alphablend pixmap using RastPort foreground color */

    src = px->ttp_Data;
    dst = re->re_RenderBuffer;

    for (ix = 0; ix < px->ttp_Width; ix++)
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            *dst++ = 0x00;
            pix = *src++;

            if (pix == 0x00) dst += 3;
            else if (pix == 0xFF)
              {
                *dst++ += (((WORD)fg.r - (WORD)*dst) * alpha) >> 8;
                *dst++ += (((WORD)fg.g - (WORD)*dst) * alpha) >> 8;
                *dst++ += (((WORD)fg.b - (WORD)*dst) * alpha) >> 8;
              }
            else
              {
                struct Rgb f;

                f.r = dst[0] + ((((WORD)fg.r - (WORD)dst[0]) * alpha) >> 8);
                f.g = dst[1] + ((((WORD)fg.g - (WORD)dst[1]) * alpha) >> 8);
                f.b = dst[2] + ((((WORD)fg.b - (WORD)dst[2]) * alpha) >> 8);
                *dst++ = blend (g, *dst, f.r, pix);
                *dst++ = blend (g, *dst, f.g, pix);
                *dst++ = blend (g, *dst, f.b, pix);
              }
          }
      }

    /* write out alphablended output */

    WritePixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);
  }

///
/// transp_jam2()

VOID transp_jam2(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
    ULONG ix, iy;
    ULONG bufsize;
    UBYTE *src, *dst, *g, *ag;
    UBYTE pix, alpha = ~re->re_Transparency;
    struct Rgb fg, bg;
    struct RastPort *rp = re->re_TargetRPort;

    g = re->re_Gamma;
    ag = re->re_AntiGamma;

    /* get RastPort foreground  and background colors */

    get_rp_fgcolor(ttb, re, &fg);
    get_rp_bgcolor(ttb, re, &bg);

    /* compute render buffer size and expand if too small */

    bufsize = px->ttp_Width * px->ttp_Height * sizeof(ULONG);
    if (bufsize > re->re_RendBufSize)
      {
        if (!expand_render_buffer(ttb, re, bufsize)) return;
      }

    /* load background from RastPort to render buffer */

    ReadPixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);

    /* alphablend pixmap using RastPort foreground and background colors */

    src = px->ttp_Data;
    dst = re->re_RenderBuffer;

    for (ix = 0; ix < px->ttp_Width; ix++)
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            *dst++ = 0x00;
            pix = *src++;

            if (pix == 0x00)
              {
                *dst++ += (((WORD)bg.r - (WORD)*dst) * alpha) >> 8;
                *dst++ += (((WORD)bg.g - (WORD)*dst) * alpha) >> 8;
                *dst++ += (((WORD)bg.b - (WORD)*dst) * alpha) >> 8;
              }
            else if (pix == 0xFF)
              {
                *dst++ += (((WORD)fg.r - (WORD)*dst) * alpha) >> 8;
                *dst++ += (((WORD)fg.g - (WORD)*dst) * alpha) >> 8;
                *dst++ += (((WORD)fg.b - (WORD)*dst) * alpha) >> 8;
              }
            else
              {
                struct Rgb bg2, fg2;

                bg2.r = dst[0] + ((((WORD)bg.r - (WORD)dst[0]) * alpha) >> 8);
                bg2.g = dst[1] + ((((WORD)bg.g - (WORD)dst[1]) * alpha) >> 8);
                bg2.b = dst[2] + ((((WORD)bg.b - (WORD)dst[2]) * alpha) >> 8);

                fg2.r = dst[0] + ((((WORD)fg.r - (WORD)dst[0]) * alpha) >> 8);
                fg2.g = dst[1] + ((((WORD)fg.g - (WORD)dst[1]) * alpha) >> 8);
                fg2.b = dst[2] + ((((WORD)fg.b - (WORD)dst[2]) * alpha) >> 8);

                *dst++ = blend (g, bg2.r, fg2.r, pix);
                *dst++ = blend (g, bg2.g, fg2.g, pix);
                *dst++ = blend (g, bg2.b, fg2.b, pix);
              }
          }
      }

    /* write out alphablended output */

    WritePixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG), rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);
  }

///
/// transp_jam1_xor()

VOID transp_jam1_xor(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
    ULONG ix, iy;
    ULONG bufsize;
    UBYTE *src, *dst, *g, *ag;
    UBYTE pix, alpha = ~re->re_Transparency;
    struct Rgb fg;
    struct RastPort *rp = re->re_TargetRPort;

    g = re->re_Gamma;
    ag = re->re_AntiGamma;

    /* get RastPort foreground color */

    get_rp_fgcolor(ttb, re, &fg);

    /* compute render buffer size and expand if too small */

    bufsize = px->ttp_Width * px->ttp_Height * sizeof(ULONG);
    if (bufsize > re->re_RendBufSize)
      {
        if (!expand_render_buffer(ttb, re, bufsize)) return;
      }

    /* load background from RastPort to render buffer */

    ReadPixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);

    /* alphablend pixmap using RastPort foreground color */

    src = px->ttp_Data;
    dst = re->re_RenderBuffer;

    for (ix = 0; ix < px->ttp_Width; ix++)
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            *dst++ = 0x00;
            pix = *src++;

            if (pix == 0x00) dst += 3;
            else if (pix == 0xFF)
              {
                *dst++ ^= ((WORD)fg.r * alpha) >> 8;
                *dst++ ^= ((WORD)fg.g * alpha) >> 8;
                *dst++ ^= ((WORD)fg.b * alpha) >> 8;
              }
            else
              {
                struct Rgb f;

                f.r = ((WORD)fg.r * alpha) >> 8;
                f.g = ((WORD)fg.g * alpha) >> 8;
                f.b = ((WORD)fg.b * alpha) >> 8;
                *dst++ ^= blend (g, 0, f.r, pix);
                *dst++ ^= blend (g, 0, f.g, pix);
                *dst++ ^= blend (g, 0, f.b, pix);
              }
          }
      }

    /* write out alphablended output */

    WritePixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);
  }

///
/// transp_jam2_xor()

VOID transp_jam2_xor(struct TTEngineBase *ttb, struct TT_Pixmap *px, struct RenderEnv *re,
 LONG x, LONG y)
  {
    USELIB(CyberGfxBase, ICyberGfx, CyberGfxIFace);
    ULONG ix, iy;
    ULONG bufsize;
    UBYTE *src, *dst, *g, *ag;
    UBYTE pix, alpha = ~re->re_Transparency;
    struct Rgb fg, bg;
    struct RastPort *rp = re->re_TargetRPort;

    g = re->re_Gamma;
    ag = re->re_AntiGamma;

    /* get RastPort foreground  and background colors */

    get_rp_fgcolor(ttb, re, &fg);
    get_rp_bgcolor(ttb, re, &bg);

    /* compute render buffer size and expand if too small */

    bufsize = px->ttp_Width * px->ttp_Height * sizeof(ULONG);
    if (bufsize > re->re_RendBufSize)
      {
        if (!expand_render_buffer(ttb, re, bufsize)) return;
      }

    /* load background from RastPort to render buffer */

    ReadPixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);

    /* alphablend pixmap using RastPort foreground and background colors */

    src = px->ttp_Data;
    dst = re->re_RenderBuffer;

    for (ix = 0; ix < px->ttp_Width; ix++)
      {
        for (iy = 0; iy < px->ttp_Height; iy++)
          {
            *dst++ = 0x00;
            pix = *src++;

            if (pix == 0x00)
              {
                *dst++ ^= ((WORD)bg.r * alpha) >> 8;
                *dst++ ^= ((WORD)bg.g * alpha) >> 8;
                *dst++ ^= ((WORD)bg.b * alpha) >> 8;
              }
            else if (pix == 0xFF)
              {
                *dst++ ^= ((WORD)fg.r * alpha) >> 8;
                *dst++ ^= ((WORD)fg.g * alpha) >> 8;
                *dst++ ^= ((WORD)fg.b * alpha) >> 8;
              }
            else
              {
                struct Rgb bg2, fg2;

                bg2.r = ((WORD)bg.r * alpha) >> 8;
                bg2.g = ((WORD)bg.g * alpha) >> 8;
                bg2.b = ((WORD)bg.b * alpha) >> 8;

                fg2.r = ((WORD)fg.r * alpha) >> 8;
                fg2.g = ((WORD)fg.g * alpha) >> 8;
                fg2.b = ((WORD)fg.b * alpha) >> 8;

                *dst++ ^= blend (g, bg2.r, fg2.r, pix);
                *dst++ ^= blend (g, bg2.g, fg2.g, pix);
                *dst++ ^= blend (g, bg2.b, fg2.b, pix);
              }
          }
      }

    /* write out alphablended output */

    WritePixelArray(re->re_RenderBuffer, 0, 0, px->ttp_Width * sizeof(ULONG),
     rp, x, y, px->ttp_Width, px->ttp_Height, RECTFMT_ARGB);
  }

///

