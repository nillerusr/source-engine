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
  *   FILE     re_str.h
  *   VERSION  2.12
  *   This is an internal header file, do not include directly.
  *   String support and helper functions, for regular
  *   expression library.
  */

#ifndef RE_STR_H
#define RE_STR_H

#ifndef JM_CFG_H
#include <jm/jm_cfg.h>
#endif

#include <string.h>

JM_NAMESPACE(__JM)

//
// start by defining some template function aliases for C API functions:
//

template <class charT>
size_t RE_CALL re_strlen(const charT *s)
{
   size_t len = 0;
   while(*s)
   {
      ++s;
      ++len;
   }
   return len;
}

template <class charT>
int RE_CALL re_strcmp(const charT *s1, const charT *s2)
{
   while(*s1 && *s2)
   {
      if(*s1 != *s2)
         return *s1 - *s2;
      ++s1;
      ++s2;
   }
   return *s1 - *s2;
}

template <class charT>
charT* RE_CALL re_strcpy(charT *s1, const charT *s2)
{
   charT* base = s1;
   while(*s2)
   {
      *s1 = *s2;
      ++s1;
      ++s2;
   }
   *s1 = *s2;
   return base;
}

template <class charT>
unsigned int RE_CALL re_strwiden(charT *s1, unsigned int len, const char *s2)
{
   unsigned int result = 1 + re_strlen(s2);
   if(result > len)
      return result;
   while(*s2)
   {
      *s1 = (unsigned char)*s2;
      ++s2;
      ++s1;
   }
   *s1 = (unsigned char)*s2;
   return result;
}

template <class charT>
unsigned int RE_CALL re_strnarrow(char *s1, unsigned int len, const charT *s2)
{
   unsigned int result = 1 + re_strlen(s2);
   if(result > len)
      return result;
   while(*s2)
   {
      *s1 = (char)(unsigned char)*s2;
      ++s2;
      ++s1;
   }
   *s1 = (char)(unsigned char)*s2;
   return result;
}

inline size_t RE_CALL re_strlen(const char *s)
{
   return strlen(s);
}

inline int RE_CALL re_strcmp(const char *s1, const char *s2)
{
   return strcmp(s1, s2);
}

inline char* RE_CALL re_strcpy(char *s1, const char *s2)
{
   return strcpy(s1, s2);
}

#ifndef JM_NO_WCSTRING

inline size_t RE_CALL re_strlen(const wchar_t *s)
{
   return wcslen(s);
}

inline int RE_CALL re_strcmp(const wchar_t *s1, const wchar_t *s2)
{
   return wcscmp(s1, s2);
}

inline wchar_t* RE_CALL re_strcpy(wchar_t *s1, const wchar_t *s2)
{
   return wcscpy(s1, s2);
}

#endif

#if !defined(JM_NO_WCSTRING) || defined(JM_PLATFORM_W32)

JM_IX_DECL unsigned int RE_CALL _re_strnarrow(char *s1, unsigned int len, const wchar_t *s2);
JM_IX_DECL unsigned int RE_CALL _re_strwiden(wchar_t *s1, unsigned int len, const char *s2);


inline unsigned int RE_CALL re_strnarrow(char *s1, unsigned int len, const wchar_t *s2)
{
   return _re_strnarrow(s1, len, s2);
}

inline unsigned int RE_CALL re_strwiden(wchar_t *s1, unsigned int len, const char *s2)
{
   return _re_strwiden(s1, len, s2);
}

#endif

template <class charT>
charT* RE_CALL re_strdup(const charT* p)
{
   charT* buf = new charT[re_strlen(p) + 1];
   re_strcpy(buf, p);
   return buf;
}

template <class charT>
charT* RE_CALL re_strdup(const charT* p1, const charT* p2)
{
   unsigned int len = p2 - p1 + 1;
   charT* buf = new charT[len];
   memcpy(buf, p1, (len - 1) * sizeof(charT));
   *(buf + len - 1) = 0;
   return buf;
}

template <class charT>
inline void RE_CALL re_strfree(charT* p)
{
   delete[] p;
}

template <class charT>
class re_str
{
   charT* buf;
public:
   re_str() 
   { 
      charT c = 0;
      buf = re_strdup(&c); 
   }
   ~re_str();
   re_str(const re_str& other);
   re_str(const charT* p1);
   re_str(const charT* p1, const charT* p2);
   re_str(charT c);

   re_str& RE_CALL operator=(const re_str& other)
   {
      re_strfree(buf);
      buf = re_strdup(other.buf);
      return *this;
   }
   re_str& RE_CALL operator=(const charT* p)
   {
      re_strfree(buf);
      buf = re_strdup(p);
      return *this;
   }
   re_str& RE_CALL operator=(charT c)
   {
      re_strfree(buf);
      buf = re_strdup(&c, &c+1);
      return *this;
   }
   const charT* RE_CALL c_str()const { return buf; }
   RE_CALL operator const charT*()const { return buf; }
   unsigned int RE_CALL size()const { return re_strlen(buf); }
   charT& RE_CALL operator[](unsigned int i) { return buf[i]; }
   charT RE_CALL operator[](unsigned int i)const { return buf[i]; }

   bool RE_CALL operator==(const re_str& other)const { return re_strcmp(buf, other.buf) == 0; }
   bool RE_CALL operator==(const charT* p)const { return re_strcmp(buf, p) == 0; }
   bool RE_CALL operator==(const charT c)const
   {
      if((*buf) && (*buf == c) && (*(buf+1) == 0))
         return true;
      return false;
   }
   bool RE_CALL operator!=(const re_str& other)const { return re_strcmp(buf, other.buf) != 0; }
   bool RE_CALL operator!=(const charT* p)const { return re_strcmp(buf, p) != 0; }
   bool RE_CALL operator!=(const charT c)const { return !(*this == c); }

   bool RE_CALL operator<(const re_str& other)const { return re_strcmp(buf, other.buf) < 0; }
   bool RE_CALL operator<=(const re_str& other)const { return re_strcmp(buf, other.buf) <= 0; }
   bool RE_CALL operator>(const re_str& other)const { return re_strcmp(buf, other.buf) > 0; }
   bool RE_CALL operator>=(const re_str& other)const { return re_strcmp(buf, other.buf) >= 0; }

   bool RE_CALL operator<(const charT* p)const { return re_strcmp(buf, p) < 0; }
   bool RE_CALL operator<=(const charT* p)const { return re_strcmp(buf, p) <= 0; }
   bool RE_CALL operator>(const charT* p)const { return re_strcmp(buf, p) > 0; }
   bool RE_CALL operator>=(const charT* p)const { return re_strcmp(buf, p) >= 0; }
};

template <class charT>
CONSTRUCTOR_INLINE re_str<charT>::~re_str() { re_strfree(buf); }

template <class charT>
CONSTRUCTOR_INLINE re_str<charT>::re_str(const re_str<charT>& other) { buf = re_strdup(other.buf); }

template <class charT>
CONSTRUCTOR_INLINE re_str<charT>::re_str(const charT* p1) { buf = re_strdup(p1); }

template <class charT>
CONSTRUCTOR_INLINE re_str<charT>::re_str(const charT* p1, const charT* p2) { buf = re_strdup(p1, p2); }

template <class charT>
CONSTRUCTOR_INLINE re_str<charT>::re_str(charT c) { buf = re_strdup(&c, &c+1); }


#ifndef JM_NO_WCSTRING
JM_IX_DECL void RE_CALL re_transform(re_str<wchar_t>& out, const re_str<wchar_t>& in);
#endif
JM_IX_DECL void RE_CALL re_transform(re_str<char>& out, const re_str<char>& in);

template <class charT>
void RE_CALL re_trunc_primary(re_str<charT>& s)
{
   for(unsigned int i = 0; i < s.size(); ++i)
   {
      if(s[i] <= 1)
      {
         s[i] = 0;
         break;
      }
   }
}

#ifdef RE_LOCALE_C
#define TRANSFORM_ERROR (size_t)-1
#else
#define TRANSFORM_ERROR 0
#endif


JM_END_NAMESPACE

#endif


