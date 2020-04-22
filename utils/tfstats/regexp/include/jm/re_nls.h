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
  *   FILE     re_nls.h
  *   VERSION  2.12
  *   This is an internal header file, do not include directly
  */
  
#ifndef RE_NLS_H
#define RE_NLS_H

#ifndef JM_CFG_H
#include <jm/jm_cfg.h>
#endif

#ifdef RE_LOCALE_CPP
#include <jm/regfac.h>
#endif

#include <limits.h>

JM_NAMESPACE(__JM)

enum char_class_type
{
#ifdef RE_LOCALE_CPP
   char_class_none = 0,
   char_class_alnum = __JM_STD::ctype_base::alnum,
   char_class_alpha = __JM_STD::ctype_base::alpha,
   char_class_cntrl = __JM_STD::ctype_base::cntrl,
   char_class_digit = __JM_STD::ctype_base::digit,
   char_class_graph = __JM_STD::ctype_base::graph,
   char_class_lower = __JM_STD::ctype_base::lower,
   char_class_print = __JM_STD::ctype_base::print,
   char_class_punct = __JM_STD::ctype_base::punct,
   char_class_space = __JM_STD::ctype_base::space,
   char_class_upper = __JM_STD::ctype_base::upper,
   char_class_xdigit = __JM_STD::ctype_base::xdigit,
   char_class_blank = 1<<12,
   char_class_underscore = 1<<13,
   char_class_word = __JM_STD::ctype_base::alnum | char_class_underscore,
   char_class_unicode = 1<<14,
   char_class_all_base = char_class_alnum | char_class_alpha | char_class_cntrl
                      | char_class_digit | char_class_graph | char_class_lower
                      | char_class_print | char_class_punct | char_class_space
                      | char_class_upper | char_class_xdigit

#elif defined(RE_LOCALE_W32)
   char_class_none = 0,
   char_class_alnum = C1_ALPHA | C1_DIGIT,
   char_class_alpha = C1_ALPHA,
   char_class_cntrl = C1_CNTRL,
   char_class_digit = C1_DIGIT,
   char_class_graph = C1_UPPER | C1_LOWER | C1_DIGIT | C1_PUNCT | C1_ALPHA,
   char_class_lower = C1_LOWER,
   char_class_print = C1_UPPER | C1_LOWER | C1_DIGIT | C1_PUNCT | C1_BLANK | C1_ALPHA,
   char_class_punct = C1_PUNCT,
   char_class_space = C1_SPACE,
   char_class_upper = C1_UPPER,
   char_class_xdigit = C1_XDIGIT,
   char_class_blank = C1_BLANK,
   char_class_underscore = 0x0200,
   char_class_word = C1_ALPHA | C1_DIGIT | char_class_underscore,
   char_class_unicode = 0x0400
#else
   char_class_none = 0,
   char_class_alpha = 1,
   char_class_cntrl = char_class_alpha << 1,
   char_class_digit = char_class_cntrl << 1,
   char_class_lower = char_class_digit << 1,
   char_class_punct = char_class_lower << 1,
   char_class_space = char_class_punct << 1,
   char_class_upper = char_class_space << 1,
   char_class_xdigit = char_class_upper << 1,
   char_class_blank = char_class_xdigit << 1,
   char_class_unicode = char_class_blank << 1,
   char_class_underscore = char_class_unicode << 1,

   char_class_alnum = char_class_alpha | char_class_digit,
   char_class_graph = char_class_alpha | char_class_digit | char_class_punct | char_class_underscore,
   char_class_print = char_class_alpha | char_class_digit | char_class_punct | char_class_underscore | char_class_blank,
   char_class_word = char_class_alpha | char_class_digit | char_class_underscore
#endif
};

//
// declare our initialise class and functions:
//

template <class charT>
class re_initialiser
{
public:
   void update();
};

JM_IX_DECL void RE_CALL re_init();
JM_IX_DECL void RE_CALL re_update();
JM_IX_DECL void RE_CALL re_free();
JM_IX_DECL void RE_CALL re_init_w();
JM_IX_DECL void RE_CALL re_update_w();
JM_IX_DECL void RE_CALL re_free_w();

JM_TEMPLATE_SPECIALISE
class re_initialiser<char>
{
public:
   re_initialiser() { re_init(); }
   ~re_initialiser() { re_free(); }
   void RE_CALL update() { re_update(); }
};

#ifndef JM_NO_WCSTRING
JM_TEMPLATE_SPECIALISE
class re_initialiser<wchar_t>
{
public:
   re_initialiser() { re_init_w(); }
   ~re_initialiser() { re_free_w(); }
   void RE_CALL update() { re_update_w(); }
};
#endif

//
// start by declaring externals for RE_LOCALE_C
// and RE_LOCALE_W32:
//

JM_IX_DECL extern unsigned char re_syntax_map[];
JM_IX_DECL extern unsigned short re_class_map[];
JM_IX_DECL extern char re_lower_case_map[];
JM_IX_DECL extern char re_zero;
JM_IX_DECL extern char re_ten;

#ifndef JM_NO_WCSTRING
JM_IX_DECL extern unsigned short re_unicode_classes[];
JM_IX_DECL extern const wchar_t* re_lower_case_map_w;
JM_IX_DECL extern wchar_t re_zero_w;
JM_IX_DECL extern wchar_t re_ten_w;

JM_IX_DECL wchar_t RE_CALL re_wtolower(wchar_t c);
JM_IX_DECL bool RE_CALL re_iswclass(wchar_t c, jm_uintfast32_t f);
#endif

JM_IX_DECL const char* RE_CALL re_get_error_str(unsigned int id);
JM_IX_DECL unsigned int RE_CALL re_get_syntax_type(wchar_t c);

#ifdef RE_LOCALE_CPP
__JM_STD::string RE_CALL re_get_error_str(unsigned int id, const __JM_STD::locale&);
#endif

//
// add some API's for character manipulation:
//
inline char RE_CALL re_tolower(char c
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
#ifdef RE_LOCALE_CPP
   return JM_USE_FACET(l, __JM_STD::ctype<char>).tolower(c);
#else
   return re_lower_case_map[(unsigned char)c];
#endif
}

#ifndef JM_NO_WCSTRING
inline wchar_t RE_CALL re_tolower(wchar_t c
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
#ifdef RE_LOCALE_CPP
   return JM_USE_FACET(l, __JM_STD::ctype<wchar_t>).tolower(c);
#else
   return c < 256 ? re_lower_case_map_w[c] : re_wtolower(c);
#endif
}
#endif

inline bool RE_CALL re_istype(char c, jm_uintfast32_t f
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
#ifdef RE_LOCALE_CPP
   if(JM_USE_FACET(l, __JM_STD::ctype<char>).is((__JM_STD::ctype<char>::mask)(f & char_class_all_base), c))
      return true;
   if((f & char_class_underscore) && (c == '_'))
      return true;
   if((f & char_class_blank) && ((c == ' ') || (c == '\t')))
      return true;
   return false;
#else
   return re_class_map[(unsigned char)c] & f;
#endif
}

#ifndef JM_NO_WCSTRING
inline bool RE_CALL re_istype(wchar_t c, jm_uintfast32_t f
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
#ifdef RE_LOCALE_CPP
   if(JM_USE_FACET(l, __JM_STD::ctype<wchar_t>).is((__JM_STD::ctype<wchar_t>::mask)(f & char_class_all_base), c))
      return true;
   if((f & char_class_underscore) && (c == '_'))
      return true;
   if((f & char_class_blank) && ((c == ' ') || (c == '\t')))
      return true;
   return false;
#else
   return c < 256 ? re_unicode_classes[c] & f : re_iswclass(c, f);
#endif
}
#endif

inline char RE_CALL re_get_zero(char
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
#ifdef RE_LOCALE_CPP
   return JM_USE_FACET(l, regfacet<char>).zero();
#else
   return re_zero;
#endif
}

#ifndef JM_NO_WCSTRING
inline wchar_t RE_CALL re_get_zero(wchar_t
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
#ifdef RE_LOCALE_CPP
   return JM_USE_FACET(l, regfacet<wchar_t>).zero();
#else
   return re_zero_w;
#endif
}
#endif

inline char RE_CALL re_get_ten(char
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
#ifdef RE_LOCALE_CPP
   return JM_USE_FACET(l, regfacet<char>).ten();
#else
   return re_ten;
#endif
}

#ifndef JM_NO_WCSTRING
inline wchar_t RE_CALL re_get_ten(wchar_t
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
#ifdef RE_LOCALE_CPP
   return JM_USE_FACET(l, regfacet<wchar_t>).ten();
#else
   return re_ten_w;
#endif
}
#endif

//
// re_toi:
// convert a single character to the int it represents:
//
template <class charT>
unsigned int RE_CALL re_toi(charT c
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
   if(re_istype(c, char_class_digit MAYBE_PASS_LOCALE(l)))
      return c - re_get_zero(c MAYBE_PASS_LOCALE(l));
   if(re_istype(c, char_class_xdigit MAYBE_PASS_LOCALE(l)))
      return 10 + re_tolower(c MAYBE_PASS_LOCALE(l)) - re_tolower(re_get_ten(c MAYBE_PASS_LOCALE(l)) MAYBE_PASS_LOCALE(l));
   return -1; // error!!
}

//
// re_toi:
// parse an int from the input string
// update first to point to end of int
// on exit.
//
template <class charT>
unsigned int RE_CALL re_toi(const charT*& first, const charT*const last, int radix
#ifdef RE_LOCALE_CPP
, const __JM_STD::locale& l
#endif
)
{
   unsigned int maxval;
   if(radix < 0)
   {
      // if radix is less than zero, then restrict
      // return value to charT. NB assumes sizeof(charT) <= sizeof(int)
      radix *= -1;
      maxval = 1 << (sizeof(charT) * CHAR_BIT - 1);
      maxval /= radix;
      maxval *= 2;
      maxval -= 1;
   }
   else
   {
      maxval = (unsigned int)-1;
      maxval /= radix;
   }

   unsigned int result = 0;
   unsigned int type = (radix > 10) ? char_class_xdigit : char_class_digit;
   while((first != last) && re_istype(*first, type MAYBE_PASS_LOCALE(l)) && (result <= maxval))
   {
      result *= radix;
      result += re_toi(*first MAYBE_PASS_LOCALE(l));
      ++first;
   }
   return result;
}


#ifndef JM_NO_WCSTRING
JM_IX_DECL bool RE_CALL re_is_combining(wchar_t c);
#endif

extern const char* regex_message_catalogue;

JM_IX_DECL const char* RE_CALL get_global_locale_name(int);


JM_END_NAMESPACE

#endif

