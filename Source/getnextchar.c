#include "lib.h"
#include "ttengine.h"
#include "getnextchar.h"

#if defined(__MORPHOS__) || defined(__amigaos4__)

/* endian conversion macros for PPC */

#define swapw(x) ((x >> 8) | ((x & 0xFF) << 8))
#define swapl(x) ((x >> 24) | ((x & 0xFF0000) >> 8) | ((x & 0xFF00) << 8) | ((x & 0xFF) << 24))

#else

/* endian conversion macros for 68k */

/* swap word */

#define swapw(arg)\
 ({short __arg = (arg);\
  asm ("ROL.W #8,%0"\
    :"=d" (__arg)\
    :"0" (__arg)\
    :"cc");\
    __arg;})

/* swap long */

#define swapl(arg)\
 ({long __arg = (arg);\
  asm ("ROL.W #8,%0;\
        SWAP %0;\
        ROL.W #8,%0"\
    :"=d" (__arg)\
    :"0" (__arg)\
    :"cc");\
    __arg;})

#endif /* #ifdef __MORPHOS__ */

extern UWORD ISO8859_2_Table[128];
extern UWORD ISO8859_3_Table[128];
extern UWORD ISO8859_4_Table[128];
extern UWORD ISO8859_5_Table[128];
extern UWORD ISO8859_6_Table[128];
extern UWORD ISO8859_7_Table[128];
extern UWORD ISO8859_8_Table[128];
extern UWORD ISO8859_9_Table[128];
extern UWORD ISO8859_10_Table[128];
extern UWORD ISO8859_11_Table[128];
extern UWORD ISO8859_13_Table[128];
extern UWORD ISO8859_14_Table[128];
extern UWORD ISO8859_15_Table[128];
extern UWORD ISO8859_16_Table[128];

/* encoding dependant "get next character" callbacks */

BOOL set_encoding_callback(struct TTEngineBase *ttb, struct RenderEnv *re)
{
  BOOL rv = TRUE;

  switch (re->re_Encoding)
  {
    case TT_Encoding_Default:
      if (ttb->ttb_CodePage)
      {
        re->re_GetNextChar = gnc_default_codepage;
      }
      else
      {
        re->re_GetNextChar = gnc_default_no_codepage;
      }
    break;

    case TT_Encoding_Custom: re->re_GetNextChar = gnc_custom_table; break;

    case TT_Encoding_UTF16_BE: re->re_GetNextChar = gnc_utf16be; break;
    case TT_Encoding_UTF32_BE: re->re_GetNextChar = gnc_utf32be; break;
    case TT_Encoding_UTF16_LE: re->re_GetNextChar = gnc_utf16le; break;
    case TT_Encoding_UTF32_LE: re->re_GetNextChar = gnc_utf32le; break;
    case TT_Encoding_UTF16: re->re_GetNextChar = gnc_utf16; break;
    case TT_Encoding_UTF32: re->re_GetNextChar = gnc_utf32; break;
    case TT_Encoding_UTF8: re->re_GetNextChar = gnc_utf8; break;
    case TT_Encoding_ISO8859_1: re->re_GetNextChar = gnc_default_no_codepage; break;
    case TT_Encoding_ISO8859_2: re->re_GetNextChar = gnc_iso8859_2; break;
    case TT_Encoding_ISO8859_3: re->re_GetNextChar = gnc_iso8859_3; break;
    case TT_Encoding_ISO8859_4: re->re_GetNextChar = gnc_iso8859_4; break;
    case TT_Encoding_ISO8859_5: re->re_GetNextChar = gnc_iso8859_5; break;
    case TT_Encoding_ISO8859_6: re->re_GetNextChar = gnc_iso8859_6; break;
    case TT_Encoding_ISO8859_7: re->re_GetNextChar = gnc_iso8859_7; break;
    case TT_Encoding_ISO8859_8: re->re_GetNextChar = gnc_iso8859_8; break;
    case TT_Encoding_ISO8859_9: re->re_GetNextChar = gnc_iso8859_9; break;
    case TT_Encoding_ISO8859_10: re->re_GetNextChar = gnc_iso8859_10; break;
    case TT_Encoding_ISO8859_11: re->re_GetNextChar = gnc_iso8859_11; break;
    case TT_Encoding_ISO8859_13: re->re_GetNextChar = gnc_iso8859_13; break;
    case TT_Encoding_ISO8859_14: re->re_GetNextChar = gnc_iso8859_14; break;
    case TT_Encoding_ISO8859_15: re->re_GetNextChar = gnc_iso8859_15; break;
    case TT_Encoding_ISO8859_16: re->re_GetNextChar = gnc_iso8859_16; break;

    /* not recognized encoding */

    default: re->re_GetNextChar = gnc_default_no_codepage; rv = FALSE;
      break;
  }
  return rv;
}

/* default encoding with no codepage */

ULONG gnc_default_no_codepage(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    ucode32 = (ULONG)**pointer;
    *pointer += 1;
    return ucode32;
  }

/* default encoding with codepage */

ULONG gnc_default_codepage(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    re = re;            // avoid 'unused parameter'
    ucode32 = (ULONG)ttb->ttb_CodePage[**pointer];
    *pointer += 1;
    return ucode32;
  }

/* custom encoding with supplied 8-bit to Unicode table */

ULONG gnc_custom_table(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    ucode32 = (ULONG)re->re_CustomEncTable[**pointer];
    *pointer += 1;
    return ucode32;
  }

/* UTF-32BE */

ULONG gnc_utf32be(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    ucode32 = *((ULONG*)*pointer);
    *pointer += 4;
    if ((ucode32 == 0x0000FEFF) && re->re_UnicodeInitial)
      {
        ucode32 = *((ULONG*)*pointer);
        *pointer += 4;
      }
    re->re_UnicodeInitial = FALSE;
    return ucode32;
  }

/* UTF-16BE */

ULONG gnc_utf16be(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode16;

    ttb = ttb;          // avoid 'unused parameter'
    ucode16 = *((UWORD*)*pointer);
    *pointer += 2;
    if ((ucode16 == 0xFEFF) & re->re_UnicodeInitial)
      {
        ucode16 = *((UWORD*)*pointer);
        *pointer += 2;
      }
    re->re_UnicodeInitial = FALSE;
    return (ULONG)ucode16;
  }

/* UTF-32LE */

ULONG gnc_utf32le(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    ucode32 = swapl(*((ULONG*)*pointer));
    *pointer += 4;
    if ((ucode32 == 0x0000FEFF) && re->re_UnicodeInitial)
      {
        ucode32 = swapl(*((ULONG*)*pointer));
        *pointer += 4;
      }
    re->re_UnicodeInitial = FALSE;
    return ucode32;
  }

/* UTF-16LE */

ULONG gnc_utf16le(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    UWORD ucode16;

    ttb = ttb;          // avoid 'unused parameter'
    ucode16 = swapw(*((UWORD*)*pointer));
    *pointer += 2;
    if ((ucode16 == 0xFEFF) & re->re_UnicodeInitial)
      {
        ucode16 = swapw(*((UWORD*)*pointer));
        *pointer += 2;
      }
    re->re_UnicodeInitial = FALSE;
    return (ULONG)ucode16;
  }

/* UTF-32 */

ULONG gnc_utf32(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    ucode32 = *((ULONG*)*pointer);
    *pointer += 4;
    if (re->re_UnicodeInitial)
      {
        re->re_BigEndian = FALSE;
        if (ucode32 == 0xFFFE0000) re->re_BigEndian = TRUE;
        ucode32 = *((ULONG*)*pointer);
        *pointer += 4;
      }
    re->re_UnicodeInitial = FALSE;
    if (re->re_BigEndian) ucode32 = swapl(ucode32);
    return ucode32;
  }

/* UTF-16 */

ULONG gnc_utf16(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    UWORD ucode16;

    ttb = ttb;          // avoid 'unused parameter'
    ucode16 = *((UWORD*)*pointer);
    *pointer += 2;
    if (re->re_UnicodeInitial)
      {
        re->re_BigEndian = FALSE;
        if (ucode16 == 0xFFFE) re->re_BigEndian = TRUE;
        ucode16 = *((UWORD*)*pointer);
        *pointer += 2;
      }
    re->re_UnicodeInitial = FALSE;
    if (re->re_BigEndian) ucode16 = swapw(ucode16);
    return (ULONG)ucode16;
  }

/* UTF-8 */

ULONG gnc_utf8(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    UBYTE c, d, e, f, g, h;
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'

    for (;;)
      {
        c = **pointer; (*pointer)++;
        if (c < 0x80) return (ULONG)c;
        else if (c < 0xC0);   /* bad byte skip silently */
        else if (c < 0xE0)    /* 2-byte sequence */
          {
            d = **pointer; (*pointer)++;
            if ((d & 0xC0) == 0x80)
              {
                ucode32 = (d & 0x3F) + ((c & 0x1F) << 6);
                if ((ucode32 >= 0x80) && (ucode32 < 0x800)) return ucode32;
              }
          }
        else if (c < 0xF0)  /* 3-byte sequence */
          {
            d = **pointer; (*pointer)++;
            e = **pointer; (*pointer)++;
            if (((d & 0xC0) == 0x80) && ((e & 0xC0) ==0x80))
              {
                ucode32 = (e & 0x3F) + ((d & 0x3F) << 6) + ((c & 0x0F) << 12);
                if (ucode32 >= 0x800 && ucode32 < 0x10000) return ucode32;
              }
          }
        else if (c < 0xF8)  /* 4-byte sequence */
          {
            d = **pointer; (*pointer)++;
            e = **pointer; (*pointer)++;
            f = **pointer; (*pointer)++;
            if (((d & 0xC0) == 0x80) && ((e & 0xC0) == 0x80) && ((f & 0xC0) ==
              0x80))
              {
                ucode32 = (f & 0x3F) + ((e & 0x3F) << 6) + ((d & 0x3F) << 12) +
                  ((c & 0x07) << 18);
                if (ucode32 >= 0x10000 && ucode32 < 0x20000) return ucode32;
              }
          }
        else if (c < 0xFC)  /* 5-byte sequence */
          {
            d = **pointer; (*pointer)++;
            e = **pointer; (*pointer)++;
            f = **pointer; (*pointer)++;
            g = **pointer; (*pointer)++;
            if (((d & 0xC0) == 0x80) && ((e & 0xC0) == 0x80) && ((f & 0xC0) ==
              0x80) && ((g & 0xC0) == 0x80))
              {
                ucode32 = (g & 0x3F) + ((f & 0x3F) << 6) + ((e & 0x3F) << 12) +
                  ((d & 0x3F) << 18) + ((c & 0x03) << 24);
                if (ucode32 >= 0x20000 && ucode32 < 0x400000) return ucode32;
              }
          }
        else if (c < 0xFE)  /* 6-byte sequence */
          {
            d = **pointer; (*pointer)++;
            e = **pointer; (*pointer)++;
            f = **pointer; (*pointer)++;
            g = **pointer; (*pointer)++;
            h = **pointer; (*pointer)++;
            if (((d & 0xC0) == 0x80) && ((e & 0xC0) == 0x80) && ((f & 0xC0) ==
              0x80) && ((g & 0xC0) == 0x80) && ((h &0xC0) == 0x80))
              {
                ucode32 = (h & 0x3F) + ((g & 0x3F) << 6) + ((f & 0x3F) << 12) +
                  ((e & 0x3F) << 18) + ((d & 0x3F) << 24) + ((c & 0x01) << 30);
                if (ucode32 >= 0x400000 && ucode32 < 0x80000000) return ucode32;
              }
          }
      }
  }

/* ISO-8859-1 -> see default encoding with no codepage */

/* ISO-8859-2 */

ULONG gnc_iso8859_2(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_2_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-3 */

ULONG gnc_iso8859_3(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_3_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-4 */

ULONG gnc_iso8859_4(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_4_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-5 */

ULONG gnc_iso8859_5(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_5_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-6 */

ULONG gnc_iso8859_6(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_6_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-7 */

ULONG gnc_iso8859_7(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_7_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-8 */

ULONG gnc_iso8859_8(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_8_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-9 */

ULONG gnc_iso8859_9(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_9_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-10 */

ULONG gnc_iso8859_10(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_10_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-11 */

ULONG gnc_iso8859_11(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_11_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-13 */

ULONG gnc_iso8859_13(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_13_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-14 */

ULONG gnc_iso8859_14(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_14_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-15 */

ULONG gnc_iso8859_15(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_15_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

/* ISO-8859-16 */

ULONG gnc_iso8859_16(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer)
  {
    ULONG ucode32;

    ttb = ttb;          // avoid 'unused parameter'
    re = re;            // avoid 'unused parameter'
    if (**pointer & 0x80) ucode32 = (ULONG)ISO8859_16_Table[**pointer & 0x7F];
    else ucode32 = **pointer;
    *pointer += 1;
    return ucode32;
  }

