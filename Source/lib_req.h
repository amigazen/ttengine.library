/* requester functions prototypes */

#include "lib.h"

/* font requester */

#define TAGLIST_LENGTH  5

struct TTRequest
  {
    struct TagItem     ttrq_Result[TAGLIST_LENGTH];
    STRPTR             ttrq_FamilyTable[2];
    STRPTR             ttrq_NameBuffer;
    LONG               ttrq_InitialSize;
    STRPTR             ttrq_InitialName;
    LONG               ttrq_InitialStyle;
    LONG               ttrq_InitialLeft;
    LONG               ttrq_InitialTop;
    LONG               ttrq_InitialWidth;
    LONG               ttrq_InitialHeight;
  };

/* font requester flags */

#define TTRQF_DO_SIZES        0x00000001
#define TTRQF_DO_STYLE        0x00000002
#define TTRQF_DO_WEIGHT       0x00000004
#define TTRQF_ACTIVATE        0x00000008
#define TTRQF_DO_PREVIEW      0x00000010

/* API functions */

#ifdef __amigaos4__

APTR                 VARARGS68K _TTEngine_TT_AllocRequest(struct TTEngineIFace *);
struct TagItem *     VARARGS68K _TTEngine_TT_RequestA(struct TTEngineIFace *, APTR request1, struct TagItem * taglist);
struct TagItem *     VARARGS68K _TTEngine_TT_Request(struct TTEngineIFace *, APTR request, ...);
VOID                 VARARGS68K _TTEngine_TT_FreeRequest(struct TTEngineIFace *, APTR request);

#elif __MORPHOS__

struct TTRequest *tt_AllocRequest(void);
void tt_FreeRequest(void);
struct TagItem *tt_RequestA(void);

#else

__ASM__ __SAVE_DS__ struct TTRequest *tt_AllocRequest(
	__REG__(a6, struct TTEngineBase *ttb));
__ASM__ __SAVE_DS__ void tt_FreeRequest(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a0, struct TTRequest *ttreq));
__ASM__ __SAVE_DS__ struct TagItem *tt_RequestA(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a0, struct TTRequest *ttreq),
	__REG__(a1, struct TagItem *taglist));

#endif
