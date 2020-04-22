//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*
 *
 * Copyright (c) 1998-9
 * Dr John Maddock
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Dr John Maddock makes no representations
 * about the suitability of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
 *
 */
 
 /*
  *   FILE     re_cls.h
  *   VERSION  2.12
  *   This is an internal header file, do not include directly.
  *   character class lookup, for regular
  *   expression library.
  */

#ifndef RE_CLS_H
#define RE_CLS_H

#ifndef JM_CFG_H
#include <jm/jm_cfg.h>
#endif

#ifndef RE_STR_H
#include <jm/re_str.h>
#endif

JM_NAMESPACE(__JM)

#define re_classes_max 14

void RE_CALL re_init_classes();
void RE_CALL re_free_classes();
void RE_CALL re_update_classes();
JM_IX_DECL jm_uintfast32_t RE_CALL __re_lookup_class(const char* p);

inline jm_uintfast32_t RE_CALL re_lookup_class(const char* first, const char* last)
{
   re_str<char> s(first, last);
   return __re_lookup_class(s.c_str());
}

#ifndef JM_NO_WCSTRING
inline jm_uintfast32_t RE_CALL re_lookup_class(const wchar_t* first, const wchar_t* last)
{
   re_str<wchar_t> s(first, last);
   unsigned int len = re_strnarrow((char*)NULL, 0, s.c_str());
   auto_array<char> buf(new char[len]);
   re_strnarrow((char*)buf, len, s.c_str());
   len =  __re_lookup_class((char*)buf);
   return len;
}
#endif

#ifdef RE_LOCALE_CPP

extern jm_uintfast32_t re_char_class_id[];
extern const char* re_char_class_names[];

#endif

JM_END_NAMESPACE

#endif

