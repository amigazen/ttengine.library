/* requester functions prototypes */

#include "lib.h"

/* API functions */

#ifdef __amigaos4__

STRPTR *             VARARGS68K _TTEngine_TT_ObtainFamilyListA(struct TTEngineIFace *, struct TagItem * taglist);
STRPTR *             VARARGS68K _TTEngine_TT_ObtainFamilyList(struct TTEngineIFace *, ...);
VOID                 VARARGS68K _TTEngine_TT_FreeFamilyList(struct TTEngineIFace *, STRPTR * list);

#elif __MORPHOS__

STRPTR *tt_ObtainFamilyListA(void);
void tt_FreeFamilyList(void);

#else

STRPTR *tt_ObtainFamilyListA(struct TTEngineBase *ttb reg(a6), struct TagItem *taglist reg(a0));
void tt_FreeFamilyList(struct TTEngineBase *ttb reg(a6), STRPTR *list reg(a0));

#endif
