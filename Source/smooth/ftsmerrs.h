/***************************************************************************/
/*                                                                         */
/*  ftsmerrs.h                                                             */
/*                                                                         */
/*    smooth renderer error codes (specification only).                    */
/*                                                                         */
/***************************************************************************/

#ifndef __FTSMERRS_H__
#define __FTSMERRS_H__

#include FT_MODULE_ERRORS_H

#if defined(__SASC) || defined(__SASC__) || defined(FT_AMIGA_SASC)

#define FT_NOERRORDEF_( e, v, s )  enum { Smooth_Err_##e = FT_Err_##e };
#define FT_ERRORDEF_( e, v, s )    enum { Smooth_Err_##e = FT_Err_##e };
#include FT_ERROR_DEFINITIONS_H
#undef FT_ERRORDEF_
#undef FT_NOERRORDEF_

#else

#undef __FTERRORS_H__

#undef  FT_ERR_PREFIX
#define FT_ERR_PREFIX  Smooth_Err_
#undef  FT_ERR_BASE
#define FT_ERR_BASE    FT_Mod_Err_Smooth

#include FT_ERRORS_H

#endif

#endif /* __FTSMERRS_H__ */

/* END */
