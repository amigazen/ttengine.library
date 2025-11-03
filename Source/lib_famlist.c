/* family list functions */

#define __NOLIBBASE__

#include <proto/exec.h>
#include <proto/utility.h>

#include "lib_famlist.h"

#if !defined(__MORPHOS__) && !defined(__amigaos4__)
APTR AllocVecPooled(APTR poolHeader, ULONG memSize);    // from "ftsystem.c"
void FreeVecPooled(APTR poolHeader, APTR memory);
#endif

/// strcpy2()

// strcpy2() copies string and returns address of the first free byte in dest after
// 0x00 byte (just adderss of the next destination).

STRPTR strcpy2(STRPTR dest, STRPTR src)
{
        while (*dest++ = *src++);
        return dest;
}

///
/// all_fixed()

// Returns TRUE if all faces of the family are fixedwidth.

BOOL all_fixed(struct TTFamily *ttf)
{
        struct TTFont *tta;

        for (tta = (struct TTFont*)ttf->faces.lh_Head; tta->node.ln_Succ; tta = (struct TTFont*)tta->node.ln_Succ)
        {
                if (!tta->fixed) return FALSE;
        }
        return TRUE;
}

///
/// calculate_buffer_size()

// Calculates buffer size. Takes account of used filtering. The buffer should be:
// [strlen(entry) + 1] for all entries + (sizeof STRPTR) * (entries + 1).

ULONG calculate_buffer_size(struct TTEngineBase *ttb, ULONG *countvar, ULONG fixed)
{
        ULONG size = 0, famcount = 0;
        struct TTFamily *ttf;

        for (ttf = (struct TTFamily*)ttb->ttb_DataBase.lh_Head; ttf->node.ln_Succ; ttf = (struct TTFamily*)ttf->node.ln_Succ)
        {
                if (!fixed || all_fixed(ttf))
                {
                        size += strlen(ttf->node.ln_Name) + 1;
                        famcount++;
                }
        }
        size += (famcount + 1) * sizeof(STRPTR);
        *countvar = famcount;
        return size;
}

///
/// tt_ObtainFamilyListA()

#ifdef __amigaos4__

STRPTR * VARARGS68K _TTEngine_TT_ObtainFamilyList(struct TTEngineIFace *Self,
       ...)
{
    va_list ap;
    struct TagItem *tags;

    va_startlinear(ap, Self);
    tags = va_getlinearva(ap, struct TagItem *);

    return Self->TT_ObtainFamilyListA(
        tags);

}

STRPTR * VARARGS68K _TTEngine_TT_ObtainFamilyListA(struct TTEngineIFace *Self,
       struct TagItem * taglist)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

STRPTR *tt_ObtainFamilyListA(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
        struct TagItem *taglist = (struct TagItem*)REG_A0;

#else

STRPTR *tt_ObtainFamilyListA(struct TTEngineBase *ttb reg(a6), struct TagItem *taglist reg(a0))
{

#endif

  USESYSBASE;
        USELIB(UtilityBase, IUtility, UtilityIFace);
        ULONG bufsize, entries;
        UBYTE *buffer = NULL;
        ULONG fixed_only = FALSE;

        if (taglist) fixed_only = GetTagData(TTRQ_FixedWidthOnly, FALSE, taglist);
        ObtainSemaphoreShared(ttb->ttb_Semaphore);

        if (bufsize = calculate_buffer_size(ttb, &entries, fixed_only))
        {
                // for MorphOS use exec.library AllocVecPooled(), for 68k function defined in
                // "ftsystem.c" is used.

                if (buffer = AllocVecPooled(ttb->ttb_MemPool, bufsize))
                {
                        // The table of pointers to strings goes first. Then strings themselves.
                        // Both areas are filled simultaneously.

                        STRPTR *ptrptr;
                        UBYTE *strptr;
                        struct TTFamily *ttf;

                        ptrptr = (STRPTR*)buffer;
                        strptr = buffer + (entries + 1) * sizeof(STRPTR);

                        for (ttf = (struct TTFamily*)ttb->ttb_DataBase.lh_Head; ttf->node.ln_Succ; ttf = (struct TTFamily*)ttf->node.ln_Succ)
                        {
                                if (!fixed_only || all_fixed(ttf))
                                {
                                        *ptrptr++ = strptr;
                                        strptr = strcpy2(strptr, ttf->node.ln_Name);
                                }
                        }
                        *ptrptr = NULL;
                }
        }

        ReleaseSemaphore(ttb->ttb_Semaphore);
        return (STRPTR*)buffer;
}

///
/// tt_FreeFamilyList()

#ifdef __amigaos4__

VOID VARARGS68K _TTEngine_TT_FreeFamilyList(struct TTEngineIFace *Self,
       STRPTR * list)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

void tt_FreeFamilyList()
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
        STRPTR *list = (STRPTR*)REG_A0;

#else

void tt_FreeFamilyList(struct TTEngineBase *ttb reg(a6), STRPTR *list reg(a0))
{

#endif
        USESYSBASE;

        if (list)
        {
                // for MorphOS use exec.library FreeVecPooled(), for 68k function defined in
                // "ftsystem.c" is used.

                FreeVecPooled(ttb->ttb_MemPool, list);
        }

        return;
}

///


