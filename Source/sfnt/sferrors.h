/***************************************************************************/
/*                                                                         */
/*  sferrors.h                                                             */
/*                                                                         */
/*    SFNT error codes (specification only).                               */
/*                                                                         */
/***************************************************************************/

#ifndef __SFERRORS_H__
#define __SFERRORS_H__

#include FT_MODULE_ERRORS_H

#if defined(__SASC) || defined(__SASC__) || defined(FT_AMIGA_SASC)

  /* Module-error offsets are disabled in this build, so SFNT_Err_* has
     the same values as FT_Err_*.  Emit one enum constant per code; avoid
     FT_ERR_PREFIX##e (SAS/C will not expand the prefix before ##). */
#define FT_NOERRORDEF_( e, v, s )  enum { SFNT_Err_##e = FT_Err_##e };
#define FT_ERRORDEF_( e, v, s )    enum { SFNT_Err_##e = FT_Err_##e };
#include FT_ERROR_DEFINITIONS_H
#undef FT_ERRORDEF_
#undef FT_NOERRORDEF_

#else

#undef __FTERRORS_H__

#undef  FT_ERR_PREFIX
#define FT_ERR_PREFIX  SFNT_Err_
#undef  FT_ERR_BASE
#define FT_ERR_BASE    FT_Mod_Err_SFNT

#include FT_ERRORS_H

#endif

#endif /* __SFERRORS_H__ */

/* END */
