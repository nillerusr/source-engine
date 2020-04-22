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
  *   FILE     re_mss.h
  *   VERSION  2.12
  *   This is an internal header file, do not include directly.
  *   Message helper functions, for regular
  *   expression library.
  */
  
#ifndef RE_MSS_H
#define RE_MSS_H

#ifndef JM_CFG_H
#include <jm/jm_cfg.h>
#endif

JM_NAMESPACE(__JM)

//
// re_get_message
// returns required buffer size if len is zero
// otherwise fills in buf.
//

JM_IX_DECL unsigned int RE_CALL re_get_default_message(char* buf, unsigned int len, unsigned int id);

JM_IX_DECL unsigned int RE_CALL __re_get_message(char* buf, unsigned int len, unsigned int id);

template <class charT>
unsigned int RE_CALL re_get_message(charT* buf, unsigned int len, unsigned int id)
{
   unsigned int size = __re_get_message((char*)0, 0, id);
   if(len < size)
      return size;
   auto_array<char> cb(new char[size]);
   __re_get_message((char*)cb, size, id);
   size = re_strwiden(buf, len, (char*)cb);
   return size;
}

inline unsigned int RE_CALL re_get_message(char* buf, unsigned int len, unsigned int id)
{
   return __re_get_message(buf, len, id);
}


//
// declare message initialisers:
//
void RE_CALL re_message_init();
void RE_CALL re_message_update();
void RE_CALL re_message_free();

#ifdef RE_LOCALE_CPP

__JM_STD::messages<char>::string_type RE_CALL re_get_def_message(unsigned int i);

__JM_STD::messages<wchar_t>::string_type RE_CALL re_get_def_message_w(unsigned int i);

extern const char *re_default_error_messages[];

#endif


JM_END_NAMESPACE


#endif

