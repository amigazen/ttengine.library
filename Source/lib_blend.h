/* lib_blend.h */

UBYTE blend(UBYTE *g, UBYTE bg, UBYTE fg, UBYTE a);
void get_rp_bgcolor(struct TTEngineBase *ttb, struct RenderEnv *re, struct Rgb *dest);
void get_rp_fgcolor(struct TTEngineBase *ttb, struct RenderEnv *re, struct Rgb *dest);
