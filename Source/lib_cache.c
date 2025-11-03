/* TTEngine glyph bitmap cache code */

#define __NOLIBBASE__
#include <proto/exec.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "lib.h"

/* ========== BITMAP CACHE FUNCTIONS ========== */

//==========================================================================================================================================================
// check_cache_size()
//==========================================================================================================================================================

void check_cache_size(struct TTEngineBase *ttb)
{
	if (ttb->ttb_MemCacheSize > ttb->ttb_MemCacheLimit)
	{
		struct Node *last_font, *last_size;

		last_font = (struct Node*)ttb->ttb_MemoryCache.mlh_TailPred;
		last_size = (struct Node*)((struct CachedFont*)last_font)->cf_Sizes.mlh_TailPred;
        
		if (((struct CachedSize*)last_size)->cs_Users == 0)
		{
			cache_flush_size(ttb, (struct CachedSize*)last_size);

			if (((struct CachedFont*)last_font)->cf_Sizes.mlh_Head == (struct MinNode*)&((struct CachedFont*)last_font)->cf_Sizes.mlh_Tail)
			{
				cache_flush_font(ttb, (struct CachedFont*)last_font);
			}
		}
	}
}

//==========================================================================================================================================================
// cache_add_bitmap()
//==========================================================================================================================================================

struct CachedBitmap *cache_add_bitmap(struct TTEngineBase *ttb, struct CachedSize *cs, FT_GlyphSlot glyph, ULONG index)
{
	USESYSBASE;
	struct CachedBitmap *cb;
	ULONG bmsize;

	if ((cb = AllocPooled(ttb->ttb_MemPool, sizeof(struct CachedBitmap))))
	{
		cb->cb_Index = index;
		cb->cb_AdvanceX = glyph->advance.x;
		cb->cb_AdvanceY = glyph->advance.y;
		cb->cb_OffsetX = glyph->bitmap_left;
		cb->cb_OffsetY = glyph->bitmap_top;
		cb->cb_LeftBearing = glyph->metrics.horiBearingX;
		cb->cb_RightBearing = glyph->advance.x - glyph->metrics.width - cb->cb_LeftBearing;
		cb->cb_TopBearing = -glyph->metrics.horiBearingY;
		cb->cb_BottomBearing = glyph->metrics.height - glyph->metrics.horiBearingY;

		CopyMem(&glyph->bitmap, &cb->cb_Bitmap, sizeof(FT_Bitmap));
		bmsize = cb->cb_Bitmap.rows * cb->cb_Bitmap.pitch;

		if (bmsize)
		{
			if ((cb->cb_Bitmap.buffer = AllocPooled(ttb->ttb_MemPool, bmsize)))
			{
				CopyMem(glyph->bitmap.buffer, cb->cb_Bitmap.buffer, bmsize);
			}
		}
		else cb->cb_Bitmap.buffer = NULL;

		if (cb->cb_Bitmap.buffer || !bmsize)
		{
			ObtainSemaphore(ttb->ttb_Semaphore);
			ttb->ttb_MemCacheSize += sizeof(struct CachedBitmap) + bmsize;
			AddHead((struct List*)&cs->cs_Bitmaps, (struct Node*)cb);
			ReleaseSemaphore(ttb->ttb_Semaphore);
			check_cache_size(ttb);
			return cb;
		}
		FreePooled(ttb->ttb_MemPool, cb, sizeof(struct CachedBitmap));
	}
	return NULL;
}

//==========================================================================================================================================================
// cache_find_bitmap()
//==========================================================================================================================================================

struct CachedBitmap *cache_find_bitmap(struct TTEngineBase *ttb, struct CachedSize *cs, ULONG index, char mode)
{
	USESYSBASE;
	struct MinNode *n, *res = NULL;

	ObtainSemaphore(ttb->ttb_Semaphore);

	for(n = cs->cs_Bitmaps.mlh_Head; n->mln_Succ; n = n->mln_Succ)
	{
		if (((struct CachedBitmap*)n)->cb_Index == index && ((struct CachedBitmap*)n)->cb_Bitmap.pixel_mode == mode)
		{
			if (n != (struct MinNode*)cs->cs_Bitmaps.mlh_Head)
			{
				Remove((struct Node*)n);
				AddHead((struct List*)&cs->cs_Bitmaps, (struct Node*)n);
			}
			res = n;
			break;
		}
	}
	ReleaseSemaphore(ttb->ttb_Semaphore);
	return (struct CachedBitmap*)res;
}

//==========================================================================================================================================================
// cache_flush_bitmap()
//==========================================================================================================================================================

void cache_flush_bitmap(struct TTEngineBase *ttb, struct CachedBitmap *cb)
{
	USESYSBASE;
	ULONG bsize;

	bsize = cb->cb_Bitmap.rows * cb->cb_Bitmap.pitch;
	ObtainSemaphore(ttb->ttb_Semaphore);
	Remove((struct Node*)cb);

	if (cb->cb_Bitmap.buffer) FreePooled(ttb->ttb_MemPool, cb->cb_Bitmap.buffer, bsize);
	FreePooled(ttb->ttb_MemPool, cb, sizeof(struct CachedBitmap));
	ttb->ttb_MemCacheSize -= sizeof(struct CachedBitmap) + bsize;
	ReleaseSemaphore(ttb->ttb_Semaphore);
}

//==========================================================================================================================================================
// cache_new_size()
//==========================================================================================================================================================

struct CachedSize *cache_new_size(struct TTEngineBase *ttb, UWORD size)
{
	USESYSBASE;

	struct CachedSize *cs;

	if ((cs = AllocPooled(ttb->ttb_MemPool, sizeof(struct CachedSize))))
	{
		cs->cs_Users = 0;
		cs->cs_PixelSize = size;
		cs->cs_Bitmaps.mlh_Head = (struct MinNode*)&cs->cs_Bitmaps.mlh_Tail;
		cs->cs_Bitmaps.mlh_Tail = NULL;
		cs->cs_Bitmaps.mlh_TailPred = (struct MinNode*)&cs->cs_Bitmaps.mlh_Head;
		return cs;
	}
	return NULL;
}

//==========================================================================================================================================================
// cache_add_size()
//==========================================================================================================================================================

struct CachedSize *cache_add_size(struct TTEngineBase *ttb, struct CachedFont *cf, UWORD size)
{
	USESYSBASE;
	struct CachedSize *cs;

	if ((cs = cache_new_size(ttb, size)))
	{
		ObtainSemaphore(ttb->ttb_Semaphore);
		AddHead((struct List*)&cf->cf_Sizes, (struct Node*)cs);
		ttb->ttb_MemCacheSize += sizeof(struct CachedSize);
		ReleaseSemaphore(ttb->ttb_Semaphore);
		check_cache_size(ttb);
		return cs;
	}
	return NULL;
}

//==========================================================================================================================================================
// cache_find_size()
//==========================================================================================================================================================

struct CachedSize *cache_find_size(struct TTEngineBase *ttb, struct CachedFont *cf, UWORD size)
{
	USESYSBASE;
	struct MinNode *n, *res = NULL;

	ObtainSemaphore(ttb->ttb_Semaphore);

	for(n = cf->cf_Sizes.mlh_Head; n->mln_Succ; n = n->mln_Succ)
	{
		if (((struct CachedSize*)n)->cs_PixelSize == size)
		{
			if (n != (struct MinNode*)cf->cf_Sizes.mlh_Head)
			{
				Remove((struct Node*)n);
				AddHead((struct List*)&cf->cf_Sizes, (struct Node*)n);
			}
			res = n;
			break;
		}
	}
	ReleaseSemaphore(ttb->ttb_Semaphore);
	return (struct CachedSize*)res;
}

//==========================================================================================================================================================
// cache_remove_size()
//==========================================================================================================================================================

void cache_remove_size(struct TTEngineBase *ttb, struct CachedSize *cs)
{
	USESYSBASE;
	struct MinNode *n, *n2;

	ObtainSemaphore(ttb->ttb_Semaphore);
	n = cs->cs_Bitmaps.mlh_Head;

	while ((n2 = n->mln_Succ))
	{
		cache_flush_bitmap(ttb, (struct CachedBitmap*)n);
		n = n2;
	}

	FreePooled(ttb->ttb_MemPool, cs, sizeof(struct CachedSize));
	ttb->ttb_MemCacheSize -= sizeof(struct CachedSize);
	ReleaseSemaphore(ttb->ttb_Semaphore);
}

//==========================================================================================================================================================
// cache_flush_size()
//==========================================================================================================================================================

void cache_flush_size(struct TTEngineBase *ttb, struct CachedSize *cs)
{
	USESYSBASE;

	ObtainSemaphore(ttb->ttb_Semaphore);
	Remove((struct Node*)cs);
	cache_remove_size(ttb, cs);
	ReleaseSemaphore(ttb->ttb_Semaphore);
}

//==========================================================================================================================================================
// cache_add_font()
//==========================================================================================================================================================

struct CachedFont *cache_add_font(struct TTEngineBase *ttb, char *name)
{
	USESYSBASE;
	struct CachedFont *cf;
	ULONG namelen;

	namelen = strlen(name) + 1;

	if ((cf = AllocPooled(ttb->ttb_MemPool, sizeof(struct CachedFont))))
	{
		if ((cf->cf_Name = AllocPooled(ttb->ttb_MemPool, namelen)))
		{
			strcpy(cf->cf_Name, name);
			cf->cf_Sizes.mlh_Head = (struct MinNode*)&cf->cf_Sizes.mlh_Tail;
			cf->cf_Sizes.mlh_Tail = NULL;
			cf->cf_Sizes.mlh_TailPred = (struct MinNode*)&cf->cf_Sizes.mlh_Head;
			ObtainSemaphore(ttb->ttb_Semaphore);
			ttb->ttb_MemCacheSize += sizeof(struct CachedFont) + namelen;
			AddHead((struct List*)&ttb->ttb_MemoryCache, (struct Node*)cf);
			ReleaseSemaphore(ttb->ttb_Semaphore);
			check_cache_size(ttb);
			return cf;
		}
		FreePooled(ttb->ttb_MemPool, cf, sizeof(struct CachedFont));
	}
	return NULL;
}

//==========================================================================================================================================================
// cache_flush_font()
//==========================================================================================================================================================

void cache_flush_font(struct TTEngineBase *ttb, struct CachedFont *cf)
{
	USESYSBASE;
	struct MinNode *n, *n2;
	ULONG namelen;

	ObtainSemaphore(ttb->ttb_Semaphore);
	namelen = strlen(cf->cf_Name) + 1;
	Remove((struct Node*)cf);
	n = cf->cf_Sizes.mlh_Head;
    
	while((n2 = n->mln_Succ))
	{
		cache_flush_size(ttb, (struct CachedSize*)n);
		n = n2;
	}

	FreePooled(ttb->ttb_MemPool, cf->cf_Name, namelen);
	FreePooled(ttb->ttb_MemPool, cf, sizeof(struct CachedFont));
	ttb->ttb_MemCacheSize -= namelen + sizeof(struct CachedFont);
	ReleaseSemaphore(ttb->ttb_Semaphore);
}

//==========================================================================================================================================================
// cache_find_font()
//==========================================================================================================================================================

struct CachedFont *cache_find_font(struct TTEngineBase *ttb, char *name)
{
	USESYSBASE;

	struct MinNode *n, *res = NULL;

	ObtainSemaphore(ttb->ttb_Semaphore);

	for(n = ttb->ttb_MemoryCache.mlh_Head; n->mln_Succ; n = n->mln_Succ)
	{
		if (strcmp(((struct CachedFont*)n)->cf_Name, name) == 0)
		{
			if (n != (struct MinNode*)ttb->ttb_MemoryCache.mlh_Head)
			{
				Remove((struct Node*)n);
				AddHead((struct List*)&ttb->ttb_MemoryCache, (struct Node*)n);
			}
			res = n;
			break;
		}
	}
	ReleaseSemaphore(ttb->ttb_Semaphore);
	return (struct CachedFont*)res;
}

//==========================================================================================================================================================
// cache_flush_all()
//==========================================================================================================================================================

void cache_flush_all(struct TTEngineBase *ttb)
{
	USESYSBASE;
	struct MinNode *n, *n2;

	ObtainSemaphore(ttb->ttb_Semaphore);
	n = ttb->ttb_MemoryCache.mlh_Head;

	while((n2 = n->mln_Succ))
	{
		cache_flush_font(ttb, (struct CachedFont*)n);
		n = n2;
	}
	ReleaseSemaphore(ttb->ttb_Semaphore);
}
