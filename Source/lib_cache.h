/* TTEngine glyph bitmap cache header */

#include <exec/types.h>

/* memory cache functions declarations */

struct CachedBitmap *cache_add_bitmap(struct TTEngineBase *ttb, struct CachedSize *cs,
  FT_GlyphSlot glyph, ULONG index);
struct CachedBitmap *cache_find_bitmap(struct CachedSize *cs, ULONG index, char mode);
void cache_flush_bitmap(struct TTEngineBase *ttb, struct CachedBitmap *cb);

struct CachedSize *cache_add_size(struct TTEngineBase *ttb, struct CachedFont *cf, UWORD size);
struct CachedSize *cache_find_size(struct CachedFont *cf, UWORD size);
void cache_flush_size(struct TTEngineBase *ttb, struct CachedSize *cs);

struct CachedFont *cache_add_font(struct TTEngineBase *ttb, char *name);
struct CachedFont *cache_find_font(struct TTEngineBase *ttb, char *name);
void cache_flush_font(struct TTEngineBase *ttb, struct CachedFont *cf);

void cache_flush_all(struct TTEngineBase *ttb);


