/* cliprect hook for BGRA32 pixel format */

#include <exec/types.h>
#include <graphics/rastport.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>
#include <cybergraphx/cybergraphics.h>

#include "lib.h"
#include "lib_blend.h"

void dirblit_bgra32(struct DirBlitData *dbd)
{
	LONG row, col, w, h;
	ULONG lmod, *dstptr;
	UBYTE *srcptr, *gamma, alpha;
	struct Rgb bg, fg;
	ULONG bgdirect, fgdirect;
	struct Library *GfxBase = dbd->ttb->ttb_GfxBase;

	/* prepare to blit */

	lmod = dbd->bitmap_mod >> 2;                                        // bitmap modulo in longwords
	dstptr = (ULONG*)dbd->bitmap_base + dbd->bounds.MinY * lmod;        // we start here, adding lmod for every row
	srcptr = dbd->px->ttp_Data + dbd->srcy * dbd->px->ttp_Width;        // source pixmap pointer
	w = dbd->bounds.MaxX - dbd->bounds.MinX + 1;	                      // rectangle width
	h = dbd->bounds.MaxY - dbd->bounds.MinY + 1;                        // rectangle height
	gamma = dbd->re->re_Gamma;                                          // gamma lookup table
	alpha = dbd->re->re_Transparency;

//	kprintf("TTE: onscr: %ld x %ld at [%ld, %ld], dstptr = $%08lx.\n", w, h, dbd->bounds.MinX, dbd->bounds.MinY, dstptr);

	/* We have 8 draw modes to handle: JAM1/JAM2, transparency/no transparency and xor/no xor */

	ReadPixel(dbd->re->re_TargetRPort, 0, 0);
	if (dbd->background)
	{
		if (dbd->re->re_Transparency == 0)
		{
			/* ======== Mode A: JAM2, no transparency. ======== */

			get_rp_fgcolor(dbd->ttb, dbd->re, &fg);
			get_rp_bgcolor(dbd->ttb, dbd->re, &bg);
			fgdirect = (fg.b << 24) | (fg.g << 16) | (fg.r << 8);
			bgdirect = (bg.b << 24) | (bg.g << 16) | (bg.r << 8);

			for (row = 0; row < h; row++)
			{
				for (col = 0; col < w; col++)
				{
					UBYTE b;

					b = srcptr[col + dbd->srcx];

					if (dbd->xor)
					{
						if (b == 0) dstptr[col + dbd->bounds.MinX] ^= bgdirect;
						else if (b == 0xFF) dstptr[col + dbd->bounds.MinX] ^= fgdirect;
						else dstptr[col + dbd->bounds.MinX] ^= (blend(gamma, bg.b, fg.b, b) << 24) |
							(blend(gamma, bg.g, fg.g, b) << 16) | (blend(gamma, bg.r, fg.r, b) << 8);
					}
					else
					{
						if (b == 0) dstptr[col + dbd->bounds.MinX] = bgdirect;
						else if (b == 0xFF) dstptr[col + dbd->bounds.MinX] = fgdirect;
						else dstptr[col + dbd->bounds.MinX] = (blend(gamma, bg.b, fg.b, b) << 24) |
							(blend(gamma, bg.g, fg.g, b) << 16) | (blend(gamma, bg.r, fg.r, b) << 8);
					}
				}
				dstptr += lmod;
				srcptr += dbd->px->ttp_Width;
			}
		}
		else
		{
			/* ======== Mode C: JAM2, transparency. ======== */

			get_rp_fgcolor(dbd->ttb, dbd->re, &fg);
			get_rp_bgcolor(dbd->ttb, dbd->re, &bg);

			for (row = 0; row < h; row++)
			{
				for (col = 0; col < w; col++)
				{
					UBYTE b;

					b = srcptr[col + dbd->srcx];

					if (b == 0x00)
					{	
						ULONG u = dstptr[col + dbd->bounds.MinX];
						struct Rgb s;

						s.b = ((u & 0xFF000000) >> 24) + ((((WORD)bg.b - (WORD)((u & 0xFF000000) >> 24)) * alpha) >> 8);
						s.g = ((u & 0x00FF0000) >> 16) + ((((WORD)bg.g - (WORD)((u & 0x00FF0000) >> 16)) * alpha) >> 8);
						s.r = ((u & 0x0000FF00) >> 8) + ((((WORD)bg.r - (WORD)((u & 0x0000FF00) >> 8)) * alpha) >> 8);

						if (dbd->xor) dstptr[col + dbd->bounds.MinX] ^= (s.b << 24) | (s.g << 16) | (s.r << 8);
						else dstptr[col + dbd->bounds.MinX] = (s.b << 24) | (s.g << 16) | (s.r << 8);
					}
					else if (b == 0xFF)
					{
						ULONG u = dstptr[col + dbd->bounds.MinX];
						struct Rgb s;

						s.b = ((u & 0xFF000000) >> 24) + ((((WORD)fg.b - (WORD)((u & 0xFF000000) >> 24)) * alpha) >> 8);
						s.g = ((u & 0x00FF0000) >> 16) + ((((WORD)fg.g - (WORD)((u & 0x00FF0000) >> 16)) * alpha) >> 8);
						s.r = ((u & 0x0000FF00) >> 8) + ((((WORD)fg.r - (WORD)((u & 0x0000FF00) >> 8)) * alpha) >> 8);

						if (dbd->xor) dstptr[col + dbd->bounds.MinX] ^= (s.b << 24) | (s.g << 16) | (s.r << 8);
						else dstptr[col + dbd->bounds.MinX] = (s.b << 24) | (s.g << 16) | (s.r << 8);
					}
					else
					{
						ULONG u = dstptr[col + dbd->bounds.MinX];
						struct Rgb s, t;

						s.b = ((u & 0xFF000000) >> 24) + ((((WORD)bg.b - (WORD)((u & 0xFF000000) >> 24)) * alpha) >> 8);
						s.g = ((u & 0x00FF0000) >> 16) + ((((WORD)bg.g - (WORD)((u & 0x00FF0000) >> 16)) * alpha) >> 8);
						s.r = ((u & 0x0000FF00) >> 8) + ((((WORD)bg.r - (WORD)((u & 0x0000FF00) >> 8)) * alpha) >> 8);

						t.b = ((u & 0xFF000000) >> 24) + ((((WORD)fg.b - (WORD)((u & 0xFF000000) >> 24)) * alpha) >> 8);
						t.g = ((u & 0x00FF0000) >> 16) + ((((WORD)fg.g - (WORD)((u & 0x00FF0000) >> 16)) * alpha) >> 8);
						t.r = ((u & 0x0000FF00) >> 8) + ((((WORD)fg.r - (WORD)((u & 0x0000FF00) >> 8)) * alpha) >> 8);

						if (dbd->xor) dstptr[col + dbd->bounds.MinX] ^=
							(blend(gamma, s.b, t.b, b) << 24) |
							(blend(gamma, s.g, t.g, b) << 16) |
							(blend(gamma, s.r, t.r, b) << 8);
						else dstptr[col + dbd->bounds.MinX] =
							(blend(gamma, s.b, t.b, b) << 24) |
							(blend(gamma, s.g, t.g, b) << 16) |
							(blend(gamma, s.r, t.r, b) << 8);
					}
				}
				dstptr += lmod;
				srcptr += dbd->px->ttp_Width;
			}
		}
	}
	else
	{
		if (dbd->re->re_Transparency == 0)
		{
			/* ======== Mode B: JAM1, no transparency, no XOR. ======== */

			get_rp_fgcolor(dbd->ttb, dbd->re, &fg);
			fgdirect = (fg.b << 24) | (fg.g << 16) | (fg.r << 8);

			for (row = 0; row < h; row++)
			{
				for (col = 0; col < w; col++)
				{
					UBYTE b;

					b = srcptr[col + dbd->srcx];

					if (b)
					{
						if (b == 0xFF)
						{
							if (dbd->xor) dstptr[col + dbd->bounds.MinX] ^= fgdirect;
							else dstptr[col + dbd->bounds.MinX] = fgdirect;
						}
						else
						{
							ULONG u = dstptr[col + dbd->bounds.MinX];

							if (dbd->xor)
							{
								dstptr[col + dbd->bounds.MinX] ^=
									(blend(gamma, (u & 0xFF000000) >> 24, fg.b, b) << 24) |
									(blend(gamma, (u & 0x00FF0000) >> 16, fg.g, b) << 16) |
									(blend(gamma, (u & 0x0000FF00) >> 8, fg.r, b) << 8);
							}
							else
							{
								dstptr[col + dbd->bounds.MinX] =
									(blend(gamma, (u & 0xFF000000) >> 24, fg.b, b) << 24) |
									(blend(gamma, (u & 0x00FF0000) >> 16, fg.g, b) << 16) |
									(blend(gamma, (u & 0x0000FF00) >> 8, fg.r, b) << 8);
							}
						}
					}
				}
				dstptr += lmod;
				srcptr += dbd->px->ttp_Width;
			}
		}
		else
		{
			/* ======== Mode D: JAM1, transparency, no XOR. ======== */

			get_rp_fgcolor(dbd->ttb, dbd->re, &fg);
			fgdirect = (fg.b << 24) | (fg.g << 16) | (fg.r << 8);

			for (row = 0; row < h; row++)
			{
				for (col = 0; col < w; col++)
				{
					UBYTE b;

					b = srcptr[col + dbd->srcx];

					if (b == 0xFF)
					{
						ULONG u = dstptr[col + dbd->bounds.MinX];
						struct Rgb s;

						s.b = ((u & 0xFF000000) >> 24) + ((((WORD)fg.b - (WORD)((u & 0xFF000000) >> 24)) * alpha) >> 8);
						s.g = ((u & 0x00FF0000) >> 16) + ((((WORD)fg.g - (WORD)((u & 0x00FF0000) >> 16)) * alpha) >> 8);
						s.r = ((u & 0x0000FF00) >> 8) + ((((WORD)fg.r - (WORD)((u & 0x0000FF00) >> 8)) * alpha) >> 8);

						if (dbd->xor) dstptr[col + dbd->bounds.MinX] ^= (s.b << 24) | (s.g << 16) | (s.r << 8);
						else dstptr[col + dbd->bounds.MinX] = (s.b << 24) | (s.g << 16) | (s.r << 8);
					}
					else if (b != 0x00)
					{
						ULONG u = dstptr[col + dbd->bounds.MinX];
						struct Rgb s;

						s.b = ((u & 0xFF000000) >> 24) + ((((WORD)fg.b - (WORD)((u & 0xFF000000) >> 24)) * alpha) >> 8);
						s.g = ((u & 0x00FF0000) >> 16) + ((((WORD)fg.g - (WORD)((u & 0x00FF0000) >> 16)) * alpha) >> 8);
						s.r = ((u & 0x0000FF00) >> 8) + ((((WORD)fg.r - (WORD)((u & 0x0000FF00) >> 8)) * alpha) >> 8);

						if (dbd->xor) dstptr[col + dbd->bounds.MinX] ^=
							(blend(gamma, ((u & 0xFF000000) >> 24), s.b, b) << 24) |
							(blend(gamma, ((u & 0x00FF0000) >> 16), s.g, b) << 16) |
							(blend(gamma, ((u & 0x0000FF00) >> 8), s.r, b) << 8);
						else dstptr[col + dbd->bounds.MinX] =
							(blend(gamma, ((u & 0xFF000000) >> 24), s.b, b) << 24) |
							(blend(gamma, ((u & 0x00FF0000) >> 16), s.g, b) << 16) |
							(blend(gamma, ((u & 0x0000FF00) >> 8), s.r, b) << 8);
					}
				}
				dstptr += lmod;
				srcptr += dbd->px->ttp_Width;
			}
		}
	}

	return;
}

