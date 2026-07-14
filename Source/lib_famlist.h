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

__ASM__ __SAVE_DS__ STRPTR *tt_ObtainFamilyListA(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a0, struct TagItem *taglist));
__ASM__ __SAVE_DS__ void tt_FreeFamilyList(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a0, STRPTR *list));

#endif
