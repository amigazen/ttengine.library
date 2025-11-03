/*--------------------------------------------------------------------------*/
/* Resident header written for ttengine.library */
/*--------------------------------------------------------------------------*/

#define __NOLIBBASE__

#define VERSION         7
#define REVISION        2

#define COMPDATE        "22.12.2005"
#define VERS            "7.2"

#include <proto/exec.h>
#include <exec/resident.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include "lib.h"
#include "lib_req.h"
#include "lib_famlist.h"

const UBYTE lib_name[] = "ttengine.library";

#ifdef __amigaos4__

const UBYTE lib_id[] = "$VER: ttengine.library " VERS " (" COMPDATE ") by Grzegorz "
  "\"Krashan\" Kraszewski (AmigaOS 4.0, PPC)\n";

#elif __MORPHOS__

const UBYTE lib_id[] = "$VER: ttengine.library " VERS " (" COMPDATE ") by Grzegorz "
  "\"Krashan\" Kraszewski (MorphOS 1.4, PPC604e)\n";

#else

const UBYTE lib_id[] = "$VER: ttengine.library " VERS " (" COMPDATE ") by Grzegorz "
  "\"Krashan\" Kraszewski (AmigaOS 3.0, 68020)\n";

#endif

#ifdef __amigaos4__

struct TagItem libCreateTags[];

#elif __MORPHOS__

struct TTEngineBase *LibInit(struct Library *unused, APTR seglist, struct Library *sysbase);

#else

__saveds struct TTEngineBase *LibInit (APTR seglist reg(a0), struct Library *sysb reg(a6));

#endif


const struct Resident romtag =
{
  RTC_MATCHWORD,              /* match word */
  (APTR)&romtag,              /* back pointer */
  (APTR)(&romtag + 1),        /* skip pointer */
#ifdef __amigaos4__
  RTF_NATIVE | RTF_AUTOINIT,
#elif __MORPHOS__
  RTF_EXTENDED | RTF_PPC,     /* flags */
#else
  0,                          /* flags */
#endif
  VERSION,                    /* version */
  NT_LIBRARY,                 /* type of module */
  0,                          /* init priority */
  (STRPTR)lib_name,           /* module name */
  (STRPTR)lib_id + 6,
#ifdef __amigaos4__
  libCreateTags,
#else
	(APTR)LibInit,
#endif
#ifdef __MORPHOS__
	REVISION, NULL
#endif
};



#ifndef __amigaos4__

LONG dummy_f(VOID)
  {
    return -1;
  }

APTR FuncTable[] =
{
#ifdef __MORPHOS__
  (APTR)FUNCARRAY_32BIT_NATIVE,
#endif
  (APTR)LibOpen,
  (APTR)LibClose,
  (APTR)LibExpunge,
  (APTR)LibReserved,
  (APTR)tt_OpenFontA,
  (APTR)tt_SetFont,
  (APTR)tt_CloseFont,
  (APTR)tt_Text,
  (APTR)tt_SetAttrsA,
  (APTR)tt_GetAttrsA,
  (APTR)tt_TextLength,
  (APTR)tt_TextExtent,
  (APTR)tt_TextFit,
  (APTR)tt_GetPixmapA,
  (APTR)tt_FreePixmap,
  (APTR)tt_DoneRastPort,
  (APTR)tt_AllocRequest,
  (APTR)tt_RequestA,
  (APTR)tt_FreeRequest,
	(APTR)tt_ObtainFamilyListA,
	(APTR)tt_FreeFamilyList,
  (APTR)-1
};

#endif

#ifdef __amigaos4__

void _start(void)
{

}

struct Library *libInit(struct Library *LibraryBase, APTR seglist, struct Interface *exec)
{
    struct TTEngineBase *ttb = (struct TTEngineBase *)LibraryBase;
    struct ExecIFace *IExec = (struct ExecIFace *)exec;

    ttb->ttb_Lib.lib_Node.ln_Type = NT_LIBRARY;
    ttb->ttb_Lib.lib_Node.ln_Pri  = 0;
    ttb->ttb_Lib.lib_Node.ln_Name = romtag.rt_Name;
    ttb->ttb_Lib.lib_Flags        = LIBF_SUMUSED|LIBF_CHANGED;
    ttb->ttb_Lib.lib_Version      = VERSION;
    ttb->ttb_Lib.lib_Revision     = REVISION;
    ttb->ttb_Lib.lib_IdString     = (STRPTR)lib_id + 6;

    ttb->ttb_IExec = IExec;
    ttb->ttb_SegList = (BPTR)seglist;
    ttb->ttb_MemCacheLimit = 524288;
    ttb->ttb_DefGamma = 2500;
    ttb->ttb_MemPool = NULL;

    NewMinList(&ttb->ttb_MemoryCache);
    NewList(&ttb->ttb_DataBase);
    NewMinList(&ttb->ttb_EnvList);
    NewList(&ttb->ttb_FontList);

    Ttb = ttb;

    return (struct Library *)ttb;
}

/* ------------------- Manager Interface ------------------------ */
/* These are generic. Replace if you need more fancy stuff */
static LONG _manager_Obtain(struct LibraryManagerInterface *Self)
{
    return Self->Data.RefCount++;
}

static ULONG _manager_Release(struct LibraryManagerInterface *Self)
{
    return Self->Data.RefCount--;
}

/* Manager interface vectors */
static void *lib_manager_vectors[] =
{
    (void *)_manager_Obtain,
    (void *)_manager_Release,
    (void *)0,
    (void *)0,
    (void *)LibOpen,
    (void *)LibClose,
    (void *)LibExpunge,
    (void *)0,
    (void *)-1,
};

/* "__library" interface tag list */
static struct TagItem lib_managerTags[] =
{
    {MIT_Name,             (ULONG)"__library"},
    {MIT_VectorTable,      (ULONG)lib_manager_vectors},
    {MIT_Version,          1},
    {TAG_DONE,             0}
};

ULONG _TTEngine_Obtain(struct TTEngineIFace *Self)
{
    return (ULONG)0;
}

ULONG _TTEngine_Release(struct TTEngineIFace *Self)
{
    return (ULONG)0;
}

/* ------------------- Library Interface(s) ------------------------ */

static void *main_vectors[] = {
        (void *)_TTEngine_Obtain,
        (void *)_TTEngine_Release,
        (void *)NULL,
        (void *)NULL,
        (void *)_TTEngine_TT_OpenFontA,
        (void *)_TTEngine_TT_OpenFont,
        (void *)_TTEngine_TT_SetFont,
        (void *)_TTEngine_TT_CloseFont,
        (void *)_TTEngine_TT_Text,
        (void *)_TTEngine_TT_SetAttrsA,
        (void *)_TTEngine_TT_SetAttrs,
        (void *)_TTEngine_TT_GetAttrsA,
        (void *)_TTEngine_TT_GetAttrs,
        (void *)_TTEngine_TT_TextLength,
        (void *)_TTEngine_TT_TextExtent,
        (void *)_TTEngine_TT_TextFit,
        (void *)_TTEngine_TT_GetPixmapA,
        (void *)_TTEngine_TT_GetPixmap,
        (void *)_TTEngine_TT_FreePixmap,
        (void *)_TTEngine_TT_DoneRastPort,
        (void *)_TTEngine_TT_AllocRequest,
        (void *)_TTEngine_TT_RequestA,
        (void *)_TTEngine_TT_Request,
        (void *)_TTEngine_TT_FreeRequest,
        (void *)_TTEngine_TT_ObtainFamilyListA,
        (void *)_TTEngine_TT_ObtainFamilyList,
        (void *)_TTEngine_TT_FreeFamilyList,
        (void *)-1
};

extern ULONG VecTable68K;

static struct TagItem mainTags[] =
{
    {MIT_Name,              (uint32)"main"},
    {MIT_VectorTable,       (uint32)main_vectors},
    {MIT_Version,           1},
    {TAG_DONE,              0}
};

static uint32 libInterfaces[] =
{
    (uint32)lib_managerTags,
    (uint32)mainTags,
    (uint32)0
};

struct TagItem libCreateTags[] =
{
    {CLT_DataSize,         (uint32)(sizeof(struct TTEngineBase))},
    {CLT_InitFunc,         (uint32)libInit},
    {CLT_Interfaces,       (uint32)libInterfaces},
    {CLT_Vector68K,        (uint32)&VecTable68K},
    {TAG_DONE,             0}
};

#elif __MORPHOS__

struct TTEngineBase *InitFunc(struct TTEngineBase *ttb, APTR seglist, struct Library *SysBase)
{
  ttb->ttb_SegList = seglist;
  ttb->ttb_SysBase = SysBase;
  ttb->ttb_MemCacheLimit = 524288;
  ttb->ttb_DefGamma = 2500;
  NEWLIST(&ttb->ttb_MemoryCache);
  NEWLIST(&ttb->ttb_DataBase);
  NEWLIST(&ttb->ttb_EnvList);
  NEWLIST(&ttb->ttb_FontList);
  Ttb = ttb;
  return ttb;
}

struct TTEngineBase *LibInit(struct Library *unused, APTR seglist, struct Library *sysbase)
{
  SysBase = sysbase;
  unused = unused;

  return (struct TTEngineBase*)(NewCreateLibraryTags(
   LIBTAG_FUNCTIONINIT, (ULONG)FuncTable,
   LIBTAG_LIBRARYINIT,  (ULONG)InitFunc,
   LIBTAG_MACHINE,      MACHINE_PPC,
   LIBTAG_BASESIZE,     sizeof(struct TTEngineBase),
   LIBTAG_SEGLIST,      (ULONG)seglist,
   LIBTAG_TYPE,         NT_LIBRARY,
   LIBTAG_NAME,         (ULONG)lib_name,
   LIBTAG_FLAGS,        LIBF_CHANGED | LIBF_SUMUSED,
   LIBTAG_VERSION,      VERSION,
   LIBTAG_REVISION,     REVISION,
   LIBTAG_IDSTRING,     (ULONG)romtag.rt_IdString,
   LIBTAG_PUBLIC,       TRUE,
  TAG_END));
}

#else

__saveds struct TTEngineBase *LibInit (APTR seglist reg(a0),
 struct Library *sysb reg(a6))
{
  struct TTEngineBase *ttb, *rval = NULL;
  struct Library *SysBase = sysb;

  if ((ttb = (struct TTEngineBase*)MakeLibrary (FuncTable, NULL, NULL,
   sizeof(struct TTEngineBase), 0)))
  {
    ttb->ttb_Lib.lib_Node.ln_Type = NT_LIBRARY;
    ttb->ttb_Lib.lib_Node.ln_Name = romtag.rt_Name;
    ttb->ttb_Lib.lib_Flags = LIBF_CHANGED | LIBF_SUMUSED;
    ttb->ttb_Lib.lib_Version = VERSION;
    ttb->ttb_Lib.lib_Revision = REVISION;
    ttb->ttb_Lib.lib_IdString = romtag.rt_IdString;
    ttb->ttb_Lib.lib_OpenCnt = 0;
    ttb->ttb_SegList = seglist;
    ttb->ttb_SysBase = SysBase;
    ttb->ttb_MemPool = NULL;
    ttb->ttb_GfxBase = NULL;
    ttb->ttb_DOSBase = NULL;
    ttb->ttb_CyberGfxBase = NULL;
    ttb->ttb_UtilityBase = NULL;
    ttb->ttb_LocaleBase = NULL;
    ttb->ttb_MathIeeeSingBasBase = NULL;
    ttb->ttb_MathIeeeSingTransBase = NULL;
    ttb->ttb_FTLib = NULL;
    ttb->ttb_MemoryCache.mlh_Head = (struct MinNode*)&ttb->ttb_MemoryCache.mlh_Tail;
    ttb->ttb_MemoryCache.mlh_Tail = NULL;
    ttb->ttb_MemoryCache.mlh_TailPred = (struct MinNode*)&ttb->ttb_MemoryCache.mlh_Head;
    ttb->ttb_MemCacheSize = 0;
    ttb->ttb_MemCacheLimit = 524288;
    ttb->ttb_InitFlag = FALSE;
    ttb->ttb_DataBase.lh_Head = (struct Node*)&ttb->ttb_DataBase.lh_Tail;
    ttb->ttb_DataBase.lh_Tail = NULL;
    ttb->ttb_DataBase.lh_TailPred = (struct Node*)&ttb->ttb_DataBase.lh_Head;
    ttb->ttb_EnvList.mlh_Head = (struct MinNode*)&ttb->ttb_EnvList.mlh_Tail;
    ttb->ttb_EnvList.mlh_Tail = NULL;
    ttb->ttb_EnvList.mlh_TailPred = (struct MinNode*)&ttb->ttb_EnvList.mlh_Head;
    ttb->ttb_FontList.lh_Head = (struct Node*)&ttb->ttb_FontList.lh_Tail;
    ttb->ttb_FontList.lh_Tail = NULL;
    ttb->ttb_FontList.lh_TailPred = (struct Node*)&ttb->ttb_FontList.lh_Head;
    ttb->ttb_DefGamma = 2500;
    AddLibrary ((struct Library*)ttb);
    rval = ttb;
  }
  return rval;
}

#endif

