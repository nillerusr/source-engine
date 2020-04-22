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
  *   FILE     regcomp.h
  *   VERSION  2.12
  *   This is an internal header file, do not include directly
  */

JM_NAMESPACE(__JM)

template <class traits>
struct kmp_translator
{
   typedef typename traits::char_type char_type;
   bool icase;
   kmp_translator(bool c) : icase(c) {}
   char_type operator()(char_type c
#ifdef RE_LOCALE_CPP
      , const __JM_STD::locale& l
#endif
      )
   {
      return traits::translate(c, icase MAYBE_PASS_LOCALE(l));
   }
};

#if defined(JM_NO_TEMPLATE_SWITCH_MERGE) && !defined(JM_NO_NAMESPACES)
//
// Ugly ugly hack,
// template don't merge if they contain switch statements so declare these
// templates in unnamed namespace (ie with internal linkage), each translation
// unit then gets its own local copy, it works seemlessly but bloats the app.
namespace{
#endif

template <class charT, class traits, class Allocator>
inline bool RE_CALL reg_expression<charT, traits, Allocator>::can_start(charT c, const unsigned char* __map, unsigned char mask, const __wide_type&)
{
   if((traits_size_type)(traits_uchar_type)c >= 256)
      return true;
   return JM_MAKE_BOOL(__map[(traits_uchar_type)c] & mask);
}

template <class charT, class traits, class Allocator>
inline bool RE_CALL reg_expression<charT, traits, Allocator>::can_start(charT c, const unsigned char* __map, unsigned char mask, const __narrow_type&)
{
   return JM_MAKE_BOOL(__map[(traits_uchar_type)c] & mask);
}

template <class charT, class traits, class Allocator>
CONSTRUCTOR_INLINE reg_expression<charT, traits, Allocator>::reg_expression(const Allocator& a)
    : regbase(), data(a), pkmp(0)
{
}

template <class charT, class traits, class Allocator>
CONSTRUCTOR_INLINE reg_expression<charT, traits, Allocator>::reg_expression(const charT* p, jm_uintfast32_t f, const Allocator& a)
    : data(a), pkmp(0)
{
    set_expression(p, f);
}

template <class charT, class traits, class Allocator>
CONSTRUCTOR_INLINE reg_expression<charT, traits, Allocator>::reg_expression(const charT* p1, const charT* p2, jm_uintfast32_t f, const Allocator& a)
    : data(a), pkmp(0)
{
    set_expression(p1, p2, f);
}

template <class charT, class traits, class Allocator>
CONSTRUCTOR_INLINE reg_expression<charT, traits, Allocator>::reg_expression(const charT* p, size_type len, jm_uintfast32_t f, const Allocator& a)
    : data(a), pkmp(0)
{
    set_expression(p, p + len, f);
}

template <class charT, class traits, class Allocator>
reg_expression<charT, traits, Allocator>::reg_expression(const reg_expression<charT, traits, Allocator>& e)
   : regbase(e), data(e.allocator()), pkmp(0)
{
   //
   // we do a deep copy only if e is a valid expression, otherwise fail.
   //
   //_flags = 0;
   //fail(e.error_code());
   if(error_code() == 0)
      set_expression(e.expression(), e.flags());
}

template <class charT, class traits, class Allocator>
reg_expression<charT, traits, Allocator>::~reg_expression()
{
   if(pkmp)
      kmp_free(pkmp, data.allocator());
}

template <class charT, class traits, class Allocator>
reg_expression<charT, traits, Allocator>& RE_CALL reg_expression<charT, traits, Allocator>::operator=(const reg_expression<charT, traits, Allocator>& e)
{
   //
   // we do a deep copy only if e is a valid expression, otherwise fail.
   //
   if(this == &e) return *this;
   _flags = 0;
   fail(e.error_code());
   if(error_code() == 0)
      set_expression(e.expression(), e.flags());
   return *this;
}

template <class charT, class traits, class Allocator>
inline bool RE_CALL reg_expression<charT, traits, Allocator>::operator==(const reg_expression<charT, traits, Allocator>& e)
{
   return (_flags == e.flags()) && (re_strcmp(expression(), e.expression()) == 0);
}

template <class charT, class traits, class Allocator>
bool RE_CALL reg_expression<charT, traits, Allocator>::operator<(const reg_expression<charT, traits, Allocator>& e)
{
   int i = re_strcmp(expression(), e.expression());
   if(i == 0)
      return _flags < e.flags();
   return i < 0;
}

template <class charT, class traits, class Allocator>
Allocator RE_CALL reg_expression<charT, traits, Allocator>::allocator()const
{
    return data.allocator();
}

template <class charT, class traits, class Allocator>
unsigned int RE_CALL reg_expression<charT, traits, Allocator>::parse_inner_set(const charT*& first, const charT* last)
{
   //
   // we have an inner [...] construct
   //
   jm_assert(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*first MAYBE_PASS_LOCALE(locale_inst)) == syntax_open_set);
   const charT* base = first;
   while( (first != last)
      && (traits_type::syntax_type((traits_size_type)(traits_uchar_type)*first MAYBE_PASS_LOCALE(locale_inst)) != syntax_close_set) )
         ++first;
   if(first == last)
      return 0;
   ++first;
   if((first-base) < 5)
      return 0;
   if(*(base+1) != *(first-2))
      return 0;
   unsigned int result = traits_type::syntax_type((traits_size_type)(traits_uchar_type)*(base+1) MAYBE_PASS_LOCALE(locale_inst));
   if((result == syntax_colon) && ((first-base) == 5))
   {
      return traits_type::syntax_type((traits_size_type)(traits_uchar_type)*(base+2) MAYBE_PASS_LOCALE(locale_inst));
   }
   return ((result == syntax_colon) || (result == syntax_dot) || (result == syntax_equal)) ? result : 0;
}


template <class charT, class traits, class Allocator>
bool RE_CALL reg_expression<charT, traits, Allocator>::skip_space(const charT*& first, const charT* last)
{
   //
   // returns true if we get to last:
   //
   while((first != last) && (traits_type::is_class(*first, char_class_space MAYBE_PASS_LOCALE(locale_inst)) == true))
   {
      ++first;
   }
   return first == last;
}

template <class charT, class traits, class Allocator>
void RE_CALL reg_expression<charT, traits, Allocator>::parse_range(const charT*& ptr, const charT* end, unsigned& min, unsigned& max)
{
   //
   // we have {x} or {x,} or {x,y} NB no spaces inside braces
   // anything else is illegal
   // On input ptr points to "{"
   //
   ++ptr;
   if(skip_space(ptr, end))
   {
      fail(REG_EBRACE);
      return;
   }
   if(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*ptr MAYBE_PASS_LOCALE(locale_inst)) != syntax_digit)
   {
      fail(REG_BADBR);
      return;
   }
   min = traits_type::toi(ptr, end, 10 MAYBE_PASS_LOCALE(locale_inst));
   if(skip_space(ptr, end))
   {
      fail(REG_EBRACE);
      return;
   }
   if(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*ptr MAYBE_PASS_LOCALE(locale_inst)) == syntax_comma)
   {
      //we have a second interval:
      ++ptr;
      if(skip_space(ptr, end))
      {
         fail(REG_EBRACE);
         return;
      }
      if(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*ptr MAYBE_PASS_LOCALE(locale_inst)) == syntax_digit)
         max = traits_type::toi(ptr, end, 10 MAYBE_PASS_LOCALE(locale_inst));
      else
         max = (unsigned)-1;
   }
   else
      max = min;

   // validate input:
   if(skip_space(ptr, end))
   {
      fail(REG_EBRACE);
      return;
   }
   if(max < min)
   {
      fail(REG_ERANGE);
      return;
   }
   if(_flags & bk_braces)
   {
      if(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*ptr MAYBE_PASS_LOCALE(locale_inst)) != syntax_slash)
      {
         fail(REG_BADBR);
         return;
      }
      else
      {
         // back\ is OK now check the }
         ++ptr;
         if((ptr == end) || (traits_type::syntax_type((traits_size_type)(traits_uchar_type)*ptr MAYBE_PASS_LOCALE(locale_inst)) != syntax_close_brace))
         {
            fail(REG_BADBR);
            return;
         }
      }
   }
   else if(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*ptr MAYBE_PASS_LOCALE(locale_inst)) != syntax_close_brace)
   {
      fail(REG_BADBR);
      return;
   }
}

template <class charT, class traits, class Allocator>
charT RE_CALL reg_expression<charT, traits, Allocator>::parse_escape(const charT*& first, const charT* last)
{
   charT c;
   switch(traits_type::syntax_type(*first MAYBE_PASS_LOCALE(locale_inst)))
   {
   case syntax_a:
      c = '\a';
      ++first;
      break;
   case syntax_f:
      c = '\f';
      ++first;
      break;
   case syntax_n:
      c = '\n';
      ++first;
      break;
   case syntax_r:
      c = '\r';
      ++first;
      break;
   case syntax_t:
      c = '\t';
      ++first;
      break;
   case syntax_v:
      c = '\v';
      ++first;
      break;
   case syntax_x:
      ++first;
      if(first == last)
      {
         fail(REG_EESCAPE);
         break;
      }
      // maybe have \x{ddd}
      if(traits_type::syntax_type(*first MAYBE_PASS_LOCALE(locale_inst)) == syntax_open_brace)
      {
         ++first;
         if(first == last)
         {
            fail(REG_EESCAPE);
            break;
         }
         if(traits_type::is_class(*first, char_class_xdigit MAYBE_PASS_LOCALE(locale_inst)) == false)
         {
            fail(REG_BADBR);
            break;
         }
         c = (charT)traits_type::toi(first, last, -16 MAYBE_PASS_LOCALE(locale_inst));
         if((first == last) || (traits_type::syntax_type(*first MAYBE_PASS_LOCALE(locale_inst)) != syntax_close_brace))
         {
            fail(REG_BADBR);
         }
         ++first;
         break;
      }
      else
      {
         if(traits_type::is_class(*first, char_class_xdigit MAYBE_PASS_LOCALE(locale_inst)) == false)
         {
            fail(REG_BADBR);
            break;
         }
         c = (charT)traits_type::toi(first, last, -16 MAYBE_PASS_LOCALE(locale_inst));
      }
      break;
   case syntax_c:
      ++first;
      if(first == last)
      {
         fail(REG_EESCAPE);
         break;
      }
      if(((traits_uchar_type)(*first) < (traits_uchar_type)'@')
            || ((traits_uchar_type)(*first) > (traits_uchar_type)127) )
      {
         fail(REG_EESCAPE);
         return (charT)0;
      }
      c = (charT)((traits_uchar_type)(*first) - (traits_uchar_type)'@');
      ++first;
      break;
   case syntax_e:
      c = (charT)27;
      ++first;
      break;
   case syntax_digit:
      c = (charT)traits_type::toi(first, last, -8 MAYBE_PASS_LOCALE(locale_inst));
      break;
   default:
      c = *first;
      ++first;
   }
   return c;
}

template <class charT, class traits, class Allocator>
void RE_CALL reg_expression<charT, traits, Allocator>::compile_maps()
{
   re_syntax_base* record = (re_syntax_base*)data.data();
   // always compile the first __map:
   memset(startmap, 0, 256);
   record->can_be_null = 0;
   compile_map(record, startmap, NULL, mask_all);

   while(record->type != syntax_element_match)
   {
      if((record->type == syntax_element_alt) || (record->type == syntax_element_rep))
      {
         memset(&(((re_jump*)record)->__map), 0, 256);
         record->can_be_null = 0;
         compile_map(record->next.p, ((re_jump*)record)->__map, &(record->can_be_null), mask_take, ((re_jump*)record)->alt.p);
         compile_map(((re_jump*)record)->alt.p, ((re_jump*)record)->__map, &(record->can_be_null), mask_skip);
      }
      else
      {
         record->can_be_null = 0;
         compile_map(record, NULL, &(record->can_be_null), mask_all);
      }
      record = record->next.p;
   }
   record->can_be_null = mask_all;
}

template <class charT, class traits_type, class Allocator>
bool RE_CALL re_maybe_set_member(charT c,
                                 re_set_long* set,
                                 const reg_expression<charT, traits_type, Allocator>& e)
{
   const charT* p = (const charT*)(set+1);
   bool icase = e.flags() & regbase::icase;
   charT col = traits_type::translate(c, icase MAYBE_PASS_LOCALE(e.locale()));
   for(unsigned int i = 0; i < set->csingles; ++i)
   {
      if(col == *p)
         return set->isnot ? false : true;

      while(*p)++p;
      ++p;     // skip null
   }
   return set->isnot ? true : false;
}

template <class charT, class traits, class Allocator>
bool RE_CALL reg_expression<charT, traits, Allocator>::probe_start(
                        re_syntax_base* node, charT cc, re_syntax_base* terminal) const
{
   unsigned int c;

   switch(node->type)
   {
   case syntax_element_startmark:
   case syntax_element_endmark:
   case syntax_element_start_line:
   case syntax_element_word_boundary:
   case syntax_element_buffer_start:
   case syntax_element_restart_continue:
      // doesn't tell us anything about the next character, so:
      return probe_start(node->next.p, cc, terminal);
   case syntax_element_literal:
      // only the first character of the literal can match:
      // note these have already been translated:
      if(*(charT*)(((re_literal*)node)+1) == traits_type::translate(cc, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst)))
         return true;
      return false;
   case syntax_element_end_line:
      // next character (if there is one!) must be a newline:
      if(traits_type::is_separator(traits_type::translate(cc, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst))))
         return true;
      return false;
   case syntax_element_wild:
      return true;
   case syntax_element_match:
      return true;
   case syntax_element_within_word:
   case syntax_element_word_start:
      return traits_type::is_class(traits_type::translate(cc, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst)), char_class_word MAYBE_PASS_LOCALE(locale_inst));
   case syntax_element_word_end:
      // what follows must not be a word character,
      return traits_type::is_class(traits_type::translate(cc, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst)), char_class_word MAYBE_PASS_LOCALE(locale_inst)) ? false : true;
   case syntax_element_buffer_end:
      // we can be null, nothing must follow,
      // NB we assume that this is followed by
      // syntax_element_match, if its not then we can
      // never match anything anyway!!
      return false;
   case syntax_element_soft_buffer_end:
      // we can be null, only newlines must follow,
      // NB we assume that this is followed by
      // syntax_element_match, if its not then we can
      // never match anything anyway!!
      return traits_type::is_separator(traits_type::translate(cc, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst)));
   case syntax_element_backref:
      // there's no easy way to determine this
      // which is not to say it can't be done!
      // for now:
      return true;
   case syntax_element_long_set:
      // we can not be null,
      // we need to add already translated values in the set
      // to values in the __map
      return re_maybe_set_member(cc, (re_set_long*)node, *this) || re_is_set_member((const charT*)&cc, (const charT*)(&cc+1), (re_set_long*)node, *this) != &cc;
   case syntax_element_set:
      // set all the elements that are set in corresponding set:
      c = (traits_size_type)(traits_uchar_type)traits_type::translate(cc, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst));
      return ((re_set*)node)->__map[c] != 0;
   case syntax_element_jump:
      if(((re_jump*)node)->alt.p < node)
      {
         // backwards jump,
         // caused only by end of repeat section, we'll treat this
         // the same as a match, because the sub-expression has matched.
         // this is only caused by NULL repeats as in "(a*)*" or "(\<)*"
         // these are really nonsensence and make the matching code much
         // harder, it would be nice to get rid of them altogether.
         if(node->next.p == terminal)
            return true;
         else
            return probe_start(((re_jump*)node)->alt.p, cc, terminal);
      }
      else
         // take the jump and compile:
         return probe_start(((re_jump*)node)->alt.p, cc, terminal);
   case syntax_element_alt:
      // we need to take the OR of the two alternatives:
      return probe_start(((re_jump*)node)->alt.p, cc, terminal) || probe_start(node->next.p, cc, terminal);
   case syntax_element_rep:
      // we need to take the OR of the two alternatives
      if(((re_repeat*)node)->min == 0)
         return probe_start(node->next.p, cc, ((re_jump*)node)->alt.p) || probe_start(((re_jump*)node)->alt.p, cc, terminal);
      else
         return probe_start(node->next.p, cc, ((re_jump*)node)->alt.p);
   case syntax_element_combining:
      return !traits_type::is_combining(traits_type::translate(cc, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst)));
   }
   return false;
}

template <class charT, class traits, class Allocator>
bool RE_CALL reg_expression<charT, traits, Allocator>::probe_start_null(re_syntax_base* node, re_syntax_base* terminal)const
{
   switch(node->type)
   {
   case syntax_element_startmark:
   case syntax_element_endmark:
   case syntax_element_start_line:
   case syntax_element_word_boundary:
   case syntax_element_buffer_start:
   case syntax_element_restart_continue:
   case syntax_element_end_line:
   case syntax_element_word_end:
      // doesn't tell us anything about the next character, so:
      return probe_start_null(node->next.p, terminal);
   case syntax_element_match:
   case syntax_element_buffer_end:
   case syntax_element_soft_buffer_end:
   case syntax_element_backref:
      return true;
   case syntax_element_jump:
      if(((re_jump*)node)->alt.p < node)
      {
         // backwards jump,
         // caused only by end of repeat section, we'll treat this
         // the same as a match, because the sub-expression has matched.
         // this is only caused by NULL repeats as in "(a*)*" or "(\<)*"
         // these are really nonsensence and make the matching code much
         // harder, it would be nice to get rid of them altogether.
         if(node->next.p == terminal)
            return true;
         else
            return probe_start_null(((re_jump*)node)->alt.p, terminal);
      }
      else
         // take the jump and compile:
         return probe_start_null(((re_jump*)node)->alt.p, terminal);
   case syntax_element_alt:
      // we need to take the OR of the two alternatives:
      return probe_start_null(((re_jump*)node)->alt.p, terminal) || probe_start_null(node->next.p, terminal);
   case syntax_element_rep:
      // only need to consider skipping the repeat:
      return probe_start_null(((re_jump*)node)->alt.p, terminal);
   }
   return false;
}

template <class charT, class traits, class Allocator>
void RE_CALL reg_expression<charT, traits, Allocator>::compile_map(
                        re_syntax_base* node, unsigned char* __map,
                        unsigned int* pnull, unsigned char mask, re_syntax_base* terminal)const
{
   if(__map)
   {
      for(unsigned int i = 0; i < 256; ++i)
      {
         if(probe_start(node, (charT)i, terminal))
            __map[i] |= mask;
      }
   }
   if(pnull && probe_start_null(node, terminal))
      *pnull |= mask;
}
  
template <class charT, class traits, class Allocator>
void RE_CALL reg_expression<charT, traits, Allocator>::move_offsets(re_syntax_base* j, unsigned size)
{
   // move all offsets starting with j->link forward by size
   // called after an insert:
   j = (re_syntax_base*)((const char*)data.data() + j->next.i);
   while(true)
   {
      switch(j->type)
      {
      case syntax_element_rep:
         ((re_jump*)j)->alt.i += size;
         j->next.i += size;
         break;
      case syntax_element_jump:
      case syntax_element_alt:
         ((re_jump*)j)->alt.i += size;
         j->next.i += size;
         break;
      default:
         j->next.i += size;
         break;
      }
      if(j->next.i == size)
         break;
      j = (re_syntax_base*)((const char*)data.data() + j->next.i);
   }
}

template <class charT, class traits, class Allocator>
re_syntax_base* RE_CALL reg_expression<charT, traits, Allocator>::compile_set_simple(re_syntax_base* dat, unsigned long cls, bool isnot)
{
   jstack<re_str<charT>, Allocator> singles(64, data.allocator());
   jstack<re_str<charT>, Allocator> ranges(64, data.allocator());
   jstack<jm_uintfast32_t, Allocator> classes(64, data.allocator());
   jstack<re_str<charT>, Allocator> equivalents(64, data.allocator());
   classes.push(cls);
   if(dat)
   {
      data.align();
      dat->next.i = data.size();
   }
   return compile_set_aux(singles, ranges, classes, equivalents, isnot, is_byte<charT>::width_type());
}

template <class charT, class traits, class Allocator>
re_syntax_base* RE_CALL reg_expression<charT, traits, Allocator>::compile_set(const charT*& first, const charT* last)
{
   jstack<re_str<charT>, Allocator> singles(64, data.allocator());
   jstack<re_str<charT>, Allocator> ranges(64, data.allocator());
   jstack<jm_uintfast32_t, Allocator> classes(64, data.allocator());
   jstack<re_str<charT>, Allocator> equivalents(64, data.allocator());
   bool has_digraphs = false;
   jm_assert(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*first MAYBE_PASS_LOCALE(locale_inst)) == syntax_open_set);
   ++first;
   bool started = false;
   bool done = false;
   bool isnot = false;

   enum last_type
   {
      last_single,
      last_none,
      last_dash
   };

   unsigned l = last_none;
   re_str<charT> s;

   while((first != last) && !done)
   {
      traits_size_type c = (traits_size_type)(traits_uchar_type)*first;
      switch(traits_type::syntax_type(c MAYBE_PASS_LOCALE(locale_inst)))
      {
      case syntax_caret:
         if(!started && !isnot)
         {
            isnot = true;
         }
         else
         {
            s = (charT)c;
            goto char_set_literal;
         }
         break;
      case syntax_open_set:
      {
         if((_flags & char_classes) == 0)
         {
            s = (charT)c;
            goto char_set_literal;
         }
         // check to see if we really have a class:
         const charT* base = first;
         switch(parse_inner_set(first, last))
         {
         case syntax_colon:
            {
               if(l == last_dash)
               {
                  fail(REG_ERANGE);
                  return NULL;
               }
               jm_uintfast32_t id = traits_type::lookup_classname(base+2, first-2 MAYBE_PASS_LOCALE(locale_inst));
               if(_flags & regbase::icase)
               {
                  if((id == char_class_upper) || (id == char_class_lower))
                  {
                     id = char_class_alpha;
                  }
               }
               if(id == 0)
               {
                  fail(REG_ECTYPE);
                  return NULL;
               }
               classes.push(id);
               started = true;
               l = last_none;
            }
            break;
         case syntax_dot:
            //
            // we have a collating element [.collating-name.]
            //
            if(traits_type::lookup_collatename(s, base+2, first-2 MAYBE_PASS_LOCALE(locale_inst)))
            {
               --first;
               if(s.size() > 1)
                  has_digraphs = true;
               goto char_set_literal;
            }
            fail(REG_ECOLLATE);
            return NULL;
         case syntax_equal:
            //
            // we have an equivalence class [=collating-name=]
            //
            if(traits_type::lookup_collatename(s, base+2, first-2 MAYBE_PASS_LOCALE(locale_inst)))
            {
               unsigned i = 0;
               while(s[i])
               {
                  s[i] = traits_type::translate(s[i], (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst));
                  ++i;
               }
               re_str<charT> s2;
               traits_type::transform_primary(s2, s MAYBE_PASS_LOCALE(locale_inst));
               equivalents.push(s2);
               started = true;
               l = last_none;
               break;
            }
            fail(REG_ECOLLATE);
            return NULL;
         case syntax_left_word:
            if((started == false) && (traits_type::syntax_type((traits_size_type)(traits_uchar_type)*first MAYBE_PASS_LOCALE(locale_inst)) == syntax_close_set))
            {
               ++first;
               return add_simple(0, syntax_element_word_start);
            }
            fail(REG_EBRACK);
            return NULL;
         case syntax_right_word:
            if((started == false) && (traits_type::syntax_type((traits_size_type)(traits_uchar_type)*first MAYBE_PASS_LOCALE(locale_inst)) == syntax_close_set))
            {
               ++first;
               return add_simple(0, syntax_element_word_end);
            }
            fail(REG_EBRACK);
            return NULL;
         default:
            if(started == false)
            {
               unsigned int t = traits_type::syntax_type((traits_size_type)(traits_uchar_type)*(base+1) MAYBE_PASS_LOCALE(locale_inst));
               if((t != syntax_colon) && (t != syntax_dot) && (t != syntax_equal))
               {
                  first = base;
                  s = (charT)c;
                  goto char_set_literal;
               }
            }
            fail(REG_EBRACK);
            return NULL;
         }
         if(first == last)
         {
            fail(REG_EBRACK);
            return NULL;
         }
         continue;
      }
      case syntax_close_set:
         if(started == false)
         {
            s = (charT)c;
            goto char_set_literal;
         }
         done = true;
         break;
      case syntax_dash:
         if(!started)
         {
            s = (charT)c;
            goto char_set_literal;
         }
         ++first;
         if(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*first MAYBE_PASS_LOCALE(locale_inst)) == syntax_close_set)
         {
            --first;
            s = (charT)c;
            goto char_set_literal;
         }
         if((singles.empty() == true) || (l != last_single))
         {
            fail(REG_ERANGE);
            return NULL;
         }
         ranges.push(singles.peek());
         if(singles.peek().size() <= 1)  // leave digraphs and ligatures in place
            singles.pop();
         l = last_dash;
         continue;
      case syntax_slash:
         if(_flags & regbase::escape_in_lists)
         {
            ++first;
            if(first == last)
               continue;
            switch(traits_type::syntax_type(*first MAYBE_PASS_LOCALE(locale_inst)))
            {
            case syntax_w:
               if(l == last_dash)
               {
                  fail(REG_ERANGE);
                  return NULL;
               }
               classes.push(char_class_word);
               started = true;
               l = last_none;
               ++first;
               continue;
            case syntax_d:
               if(l == last_dash)
               {
                  fail(REG_ERANGE);
                  return NULL;
               }
               classes.push(char_class_digit);
               started = true;
               l = last_none;
               ++first;
               continue;
            case syntax_s:
               if(l == last_dash)
               {
                  fail(REG_ERANGE);
                  return NULL;
               }
               classes.push(char_class_space);
               started = true;
               l = last_none;
               ++first;
               continue;
            case syntax_l:
               if(l == last_dash)
               {
                  fail(REG_ERANGE);
                  return NULL;
               }
               classes.push(char_class_lower);
               started = true;
               l = last_none;
               ++first;
               continue;
            case syntax_u:
               if(l == last_dash)
               {
                  fail(REG_ERANGE);
                  return NULL;
               }
               classes.push(char_class_upper);
               started = true;
               l = last_none;
               ++first;
               continue;
            case syntax_W:
            case syntax_D:
            case syntax_S:
            case syntax_U:
            case syntax_L:
               fail(REG_EESCAPE);
               return NULL;
            default:
               c = parse_escape(first, last);
               --first;
               s = (charT)c;
               goto char_set_literal;
            }
         }
         else
         {
            s = (charT)c;
            goto char_set_literal;
         }
      default:
         s = (charT)c;
         char_set_literal:
         unsigned i = 0;
         while(s[i])
         {
            s[i] = traits_type::translate(s[i], (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst));
            ++i;
         }
         started = true;
         if(l == last_dash)
         {
            ranges.push(s);
            l = last_none;
            if(s.size() > 1)   // add ligatures to singles list as well
               singles.push(s);
         }
         else
         {
            singles.push(s);
            l = last_single;
         }
      }
      ++first;
   }
   if(!done)
      return NULL;

   re_syntax_base* result;
   if(has_digraphs)
      result = compile_set_aux(singles, ranges, classes, equivalents, isnot, __wide_type());
   else
      result = compile_set_aux(singles, ranges, classes, equivalents, isnot, is_byte<charT>::width_type());
   #ifdef __BORLANDC__
   // delayed throw:
   if((result == 0) && (_flags & regbase::use_except))
      fail(code);
   #endif
   return result;
}

template <class charT, class traits, class Allocator>
re_syntax_base* RE_CALL reg_expression<charT, traits, Allocator>::compile_set_aux(jstack<re_str<charT>, Allocator>& singles, jstack<re_str<charT>, Allocator>& ranges, jstack<jm_uintfast32_t, Allocator>& classes, jstack<re_str<charT>, Allocator>& equivalents, bool isnot, const __wide_type&)
{
   size_type base = data.size();
   data.extend(sizeof(re_set_long));
   unsigned int csingles = 0;
   unsigned int cranges = 0;
   jm_uintfast32_t cclasses = 0;
   unsigned int cequivalents = 0;
   bool nocollate_state = flags() & regbase::nocollate;

   while(singles.empty() == false)
   {
      ++csingles;
      const re_str<charT>& s = singles.peek();
      unsigned len = (re_strlen(s.c_str()) + 1) * sizeof(charT);
      memcpy((charT*)data.extend(len), s.c_str(), len);
      //*(charT*)data.extend(sizeof(charT)) = charT(singles.peek());
      singles.pop();
   }
   while(ranges.empty() == false)
   {
      re_str<charT> c1, c2;
      if(nocollate_state)
         c1 = ranges.peek();
      else
         traits_type::transform(c1, ranges.peek() MAYBE_PASS_LOCALE(locale_inst));
      ranges.pop();
      if(nocollate_state)
         c2 = ranges.peek();
      else
         traits_type::transform(c2, ranges.peek() MAYBE_PASS_LOCALE(locale_inst));
      ranges.pop();
      if(c1 < c2)
      {
         // for some reason bc5 crashes when throwing exceptions
         // from here - probably an EH-compiler bug, but hard to
         // be sure...
         // delay throw to later:
         #ifdef __BORLANDC__
         jm_uintfast32_t f = _flags;
         _flags &= ~regbase::use_except;
         #endif
         fail(REG_ERANGE);
         #ifdef __BORLANDC__
         _flags = f;
         #endif
         return NULL;
      }
      ++cranges;
      unsigned len = (re_strlen(c1.c_str()) + 1) * sizeof(charT);
      memcpy(data.extend(len), c1.c_str(), len);
      len = (re_strlen(c2.c_str()) + 1) * sizeof(charT);
      memcpy(data.extend(len), c2.c_str(), len);
   }
   while(classes.empty() == false)
   {
      cclasses |= classes.peek();
      classes.pop();
   }
   while(equivalents.empty() == false)
   {
      ++cequivalents;
      const re_str<charT>& s = equivalents.peek();
      unsigned len = (re_strlen(s.c_str()) + 1) * sizeof(charT);
      memcpy((charT*)data.extend(len), s.c_str(), len);
      equivalents.pop();
   }

   re_set_long* dat = (re_set_long*)((unsigned char*)data.data() + base);
   dat->type = syntax_element_long_set;
   dat->csingles = csingles;
   dat->cranges = cranges;
   dat->cclasses = cclasses;
   dat->cequivalents = cequivalents;
   dat->isnot = isnot;
   dat->next.i = -1;
   return dat;
}

template <class charT, class traits, class Allocator>
re_syntax_base* RE_CALL reg_expression<charT, traits, Allocator>::compile_set_aux(jstack<re_str<charT>, Allocator>& singles, jstack<re_str<charT>, Allocator>& ranges, jstack<jm_uintfast32_t, Allocator>& classes, jstack<re_str<charT>, Allocator>& equivalents, bool isnot, const __narrow_type&)
{
   re_set* dat = (re_set*)data.extend(sizeof(re_set));
   memset(dat, 0, sizeof(re_set));

   while(singles.empty() == false)
   {
      dat->__map[(traits_size_type)(traits_uchar_type)*(singles.peek().c_str())] = mask_all;
      singles.pop();
   }
   while(ranges.empty() == false)
   {
      re_str<charT> c1, c2, c3, c4;

      if(flags() & regbase::nocollate)
         c1 = ranges.peek();
      else
         traits_type::transform(c1, ranges.peek() MAYBE_PASS_LOCALE(locale_inst));
      ranges.pop();
      if(flags() & regbase::nocollate)
         c2 = ranges.peek();
      else
         traits_type::transform(c2, ranges.peek() MAYBE_PASS_LOCALE(locale_inst));
      ranges.pop();

      if(c1 < c2)
      {
         // for some reason bc5 crashes when throwing exceptions
         // from here - probably an EH-compiler bug, but hard to
         // be sure...
         // delay throw to later:
         #ifdef __BORLANDC__
         jm_uintfast32_t f = _flags;
         _flags &= ~regbase::use_except;
         #endif
         fail(REG_ERANGE);
         #ifdef __BORLANDC__
         _flags = f;
         #endif
         return NULL;
      }
      for(unsigned int i = 0; i < 256; ++i)
      {
         c4 = (charT)i;
         if(flags() & regbase::nocollate)
            c3 = c4;
         else
            traits_type::transform(c3, c4 MAYBE_PASS_LOCALE(locale_inst));
         if((c3 <= c1) && (c3 >= c2))
            dat->__map[i] = mask_all;
      }
   }
   while(equivalents.empty() == false)
   {
      re_str<charT> c1, c2;
      for(unsigned int i = 0; i < 256; ++i)
      {
         c2 = (charT)i;
         traits_type::transform_primary(c1, c2 MAYBE_PASS_LOCALE(locale_inst));
         if(c1 == equivalents.peek())
            dat->__map[i] = mask_all;
      }
      equivalents.pop();
   }

   jm_uintfast32_t flags = 0;
   while(classes.empty() == false)
   {
      flags |= classes.peek();
      classes.pop();
   }
   if(flags)
   {
      for(unsigned int i = 0; i < 256; ++i)
      {
         if(traits_type::is_class(charT(i), flags MAYBE_PASS_LOCALE(locale_inst)))
            dat->__map[(traits_uchar_type)traits_type::translate((charT)i, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst))] = mask_all;
      }
   }

   if(isnot)
   {
      for(unsigned int i = 0; i < 256; ++i)
      {
         dat->__map[i] = !dat->__map[i];
      }
   }

   dat->type = syntax_element_set;
   dat->next.i = -1;
   return dat;
}


template <class charT, class traits, class Allocator>
void RE_CALL reg_expression<charT, traits, Allocator>::fixup_apply(re_syntax_base* b, unsigned cbraces)
{
   typedef JM_MAYBE_TYPENAME REBIND_TYPE(bool, Allocator) b_alloc;
   
   register unsigned char* base = (unsigned char*)b;
   register re_syntax_base* ptr = b;
   bool* pb = 0;
   b_alloc a(data.allocator());
#ifndef JM_NO_EXCEPTIONS
   try
   {
#endif
      pb = a.allocate(cbraces);
      for(unsigned i = 0; i < cbraces; ++i)
         pb[i] = false;

      repeats = 0;

      while(ptr->next.i)
      {
         switch(ptr->type)
         {
         case syntax_element_rep:
            ((re_jump*)ptr)->alt.p = (re_syntax_base*)(base + ((re_jump*)ptr)->alt.i);
            ((re_repeat*)ptr)->id = repeats;
            ++repeats;
            goto rebase;
         case syntax_element_jump:
         case syntax_element_alt:
            ((re_jump*)ptr)->alt.p = (re_syntax_base*)(base + ((re_jump*)ptr)->alt.i);
            goto rebase;
         case syntax_element_backref:
            if((((re_brace*)ptr)->index >= cbraces) || (pb[((re_brace*)ptr)->index] == false) )
            {
               fail(REG_ESUBREG);
               a.deallocate(pb, cbraces);
               return;
            }
            goto rebase;
         case syntax_element_endmark:
            pb[((re_brace*)ptr)->index] = true;
            goto rebase;
         default:
            rebase:
            ptr->next.p = (re_syntax_base*)(base + ptr->next.i);
            ptr = ptr->next.p;
         }
      }
      a.deallocate(pb, cbraces);
      pb = 0;
#ifndef JM_NO_EXCEPTIONS
   }
   catch(...)
   {
      if(pb)
         a.deallocate(pb, cbraces);
      throw;
   }
#endif
}


template <class charT, class traits, class Allocator>
unsigned int RE_CALL reg_expression<charT, traits, Allocator>::set_expression(const charT* p, const charT* end, jm_uintfast32_t f)
{
   if(p == expression())
   {
      re_str<charT> s(p, end);
      return set_expression(s.c_str(), f);
   }
#if defined(RE_LOCALE_C) || defined(RE_LOCALE_W32)
   locale_initialiser.update();
#else
   if(JM_HAS_FACET(locale_inst, regfacet<charT>) == false)
   {
#ifdef _MSC_VER
      locale_inst = __JM_STD::_ADDFAC(locale_inst, new regfacet<charT>());
#else
      locale_inst = __JM_STD::locale(locale_inst, new regfacet<charT>());
#endif
   }
   JM_USE_FACET(locale_inst, regfacet<charT>).update(locale_inst);
#endif
   const charT* base = p;
   data.clear();
   _flags = f;
   fail(REG_NOERROR);  // clear any error

   if(p >= end)
   {
      fail(REG_EMPTY);
      return code;
   }

   const charT* ptr = p;
   marks = 0;
   jstack<unsigned int, Allocator> mark(64, data.allocator());
   jstack<unsigned int, Allocator> markid(64, data.allocator());
   unsigned int last_mark_popped = 0;
   register traits_size_type c;
   register re_syntax_base* dat;

   unsigned rep_min, rep_max;

   //
   // set up header:
   //
   ++marks;
   dat = 0;

   if(_flags & regbase::literal)
   {
      while(ptr != end)
      {
         dat = add_literal(dat, traits::translate(*ptr, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst)));
         ++ptr;
      }
   }

   while (ptr < end)
   {
      c = (traits_size_type)(traits_uchar_type)*ptr;
      switch(traits_type::syntax_type(c MAYBE_PASS_LOCALE(locale_inst)))
      {
      case syntax_open_bracket:
         if(_flags & bk_parens)
         {
            dat = add_literal(dat, (charT)c);
            ++ptr;
            continue;
         }
         open_bracked_jump:
         // extend:
         dat = add_simple(dat, syntax_element_startmark, sizeof(re_brace));
         markid.push(marks);
         ((re_brace*)dat)->index = marks++;
         mark.push(data.index(dat));
         ++ptr;
         break;
      case syntax_close_bracket:
         if(_flags & bk_parens)
         {
            dat = add_literal(dat, (charT)c);
            ++ptr;
            continue;
         }
         
         close_bracked_jump:
         if(dat)
         {
            data.align();
            dat->next.i = data.size();
         }

         if(mark.empty())
         {
            fail(REG_EPAREN);
            return code;
         }
         // see if we have an empty alternative:
         if(mark.peek() == data.index(dat) )
         {
            re_syntax_base* para = (re_syntax_base*)((char*)data.data() + mark.peek());
            if(para->type == syntax_element_jump)
            {
               fail(REG_EMPTY);
               return code;
            }
         }

         // pop any pushed alternatives and set the target end destination:
         dat = (re_syntax_base*)((unsigned char*)data.data() + mark.peek());
         while(dat->type == syntax_element_jump)
         {
            ((re_jump*)dat)->alt.i = data.size();
            mark.pop();
            dat = (re_jump*)((unsigned char*)data.data() + mark.peek());
            if(mark.empty())
            {
               fail(REG_EPAREN);
               return code;
            }
         }

         dat = add_simple(0, syntax_element_endmark, sizeof(re_brace));
         ((re_brace*)dat)->index = markid.peek();
         markid.pop();
         last_mark_popped = mark.peek();
         mark.pop();
         ++ptr;
         break;
      case syntax_char:
         dat = add_literal(dat, (charT)c);
         ++ptr;
         break;
      case syntax_slash:
         if(++ptr == end)
         {
            fail(REG_EESCAPE);
            return code;
         }
         c = (traits_size_type)(traits_uchar_type)*ptr;
         switch(traits_type::syntax_type(c MAYBE_PASS_LOCALE(locale_inst)))
         {
         case syntax_open_bracket:
            if(_flags & bk_parens)
               goto open_bracked_jump;
            break;
         case syntax_close_bracket:
            if(_flags & bk_parens)
               goto close_bracked_jump;
            break;
         case syntax_plus:
            if((_flags & bk_plus_qm) && ((_flags & limited_ops) == 0))
            {
               rep_min = 1;
               rep_max = (unsigned)-1;
               goto repeat_jump;
            }
            break;
         case syntax_question:
            if((_flags & bk_plus_qm) && ((_flags & limited_ops) == 0))
            {
               rep_min = 0;
               rep_max = 1;
               goto repeat_jump;
            }
            break;
         case syntax_or:
            if(((_flags & bk_vbar) == 0) || (_flags & limited_ops))
               break;
            goto alt_string_jump;
         case syntax_open_brace:
            if( ((_flags & bk_braces) == 0) || ((_flags & intervals) == 0))
               break;

            // we have {x} or {x,} or {x,y}:
            parse_range(ptr, end, rep_min, rep_max);
            goto repeat_jump;

         case syntax_digit:
            if(_flags & bk_refs)
            {
               // update previous:
               int i = traits_type::toi((charT)c MAYBE_PASS_LOCALE(locale_inst));
               if(i == 0)
               {
                  // we can have \025 which means take char whose
                  // code is 25 (octal), so parse string:
                  c = traits_type::toi(ptr, end, -8 MAYBE_PASS_LOCALE(locale_inst));
                  --ptr;
                  break;
               }
               dat = add_simple(dat, syntax_element_backref, sizeof(re_brace));
               ((re_brace*)dat)->index = i;
               ++ptr;
               continue;
            }
            break;
         case syntax_b:     // syntax_element_word_boundary
            dat = add_simple(dat, syntax_element_word_boundary);
            ++ptr;
            continue;
         case syntax_B:
            dat = add_simple(dat, syntax_element_within_word);
            ++ptr;
            continue;
         case syntax_left_word:
            dat = add_simple(dat, syntax_element_word_start);
            ++ptr;
            continue;
         case syntax_right_word:
            dat = add_simple(dat, syntax_element_word_end);
            ++ptr;
            continue;
         case syntax_w:     //syntax_element_word_char
            dat = compile_set_simple(dat, char_class_word);
            ++ptr;
            continue;
         case syntax_W:
            dat = compile_set_simple(dat, char_class_word, true);
            ++ptr;
            continue;
         case syntax_d:     //syntax_element_word_char
            dat = compile_set_simple(dat, char_class_digit);
            ++ptr;
            continue;
         case syntax_D:
            dat = compile_set_simple(dat, char_class_digit, true);
            ++ptr;
            continue;
         case syntax_s:     //syntax_element_word_char
            dat = compile_set_simple(dat, char_class_space);
            ++ptr;
            continue;
         case syntax_S:
            dat = compile_set_simple(dat, char_class_space, true);
            ++ptr;
            continue;
         case syntax_l:     //syntax_element_word_char
            dat = compile_set_simple(dat, char_class_lower);
            ++ptr;
            continue;
         case syntax_L:
            dat = compile_set_simple(dat, char_class_lower, true);
            ++ptr;
            continue;
         case syntax_u:     //syntax_element_word_char
            dat = compile_set_simple(dat, char_class_upper);
            ++ptr;
            continue;
         case syntax_U:
            dat = compile_set_simple(dat, char_class_upper, true);
            ++ptr;
            continue;
         case syntax_Q:
            ++ptr;
            while(true)
            {
               if(ptr == end)
               {
                  fail(REG_EESCAPE);
                  return code;
               }
               if(traits_type::syntax_type((traits_size_type)(traits_uchar_type)*ptr MAYBE_PASS_LOCALE(locale_inst)) == syntax_slash)
               {
                  ++ptr;
                  if((ptr != end) && (traits_type::syntax_type((traits_size_type)(traits_uchar_type)*ptr MAYBE_PASS_LOCALE(locale_inst)) == syntax_E))
                     break;
                  else
                  {
                     dat = add_literal(dat, *(ptr-1));
                     continue;
                  }
               }
               dat = add_literal(dat, *ptr);
               ++ptr;
            }
            ++ptr;
            continue;
         case syntax_C:
            dat = add_simple(dat, syntax_element_wild);
            ++ptr;
            continue;
         case syntax_X:
            dat = add_simple(dat, syntax_element_combining);
            ++ptr;
            continue;
         case syntax_Z:
            dat = add_simple(dat, syntax_element_soft_buffer_end);
            ++ptr;
            continue;
         case syntax_G:
            dat = add_simple(dat, syntax_element_restart_continue);
            ++ptr;
            continue;
         case syntax_start_buffer:
            dat = add_simple(dat, syntax_element_buffer_start);
            ++ptr;
            continue;
         case syntax_end_buffer:
            dat = add_simple(dat, syntax_element_buffer_end);
            ++ptr;
            continue;
         default:
            c = (traits_size_type)(traits_uchar_type)parse_escape(ptr, end);
            dat = add_literal(dat, (charT)c);
            continue;
         }
         dat = add_literal(dat, (charT)c);
         ++ptr;
         break;
      case syntax_dollar:
         dat = add_simple(dat, syntax_element_end_line, sizeof(re_syntax_base));
         ++ptr;
         continue;
      case syntax_caret:
         dat = add_simple(dat, syntax_element_start_line, sizeof(re_syntax_base));
         ++ptr;
         continue;
      case syntax_dot:
         dat = add_simple(dat, syntax_element_wild, sizeof(re_syntax_base));
         ++ptr;
         continue;
      case syntax_star:
         rep_min = 0;
         rep_max = (unsigned)-1;

         repeat_jump:
         {
            unsigned offset;
            if(dat == 0)
            {
               fail(REG_BADRPT);
               return code;
            }
            switch(dat->type)
            {
            case syntax_element_endmark:
               offset = last_mark_popped;
               break;
            case syntax_element_literal:
               if(((re_literal*)dat)->length > 1)
               {
                  // update previous:
                  charT lit = *(charT*)((char*)dat + sizeof(re_literal) + ((((re_literal*)dat)->length-1)*sizeof(charT)));
                  --((re_literal*)dat)->length;
                  dat = add_simple(dat, syntax_element_literal, sizeof(re_literal) + sizeof(charT));
                  ((re_literal*)dat)->length = 1;
                  *((charT*)(((re_literal*)dat)+1)) = lit;
               }
               offset = (char*)dat - (char*)data.data();
               break;
            case syntax_element_backref:
            case syntax_element_long_set:
            case syntax_element_set:
            case syntax_element_wild:
            case syntax_element_combining:
               // we're repeating a single item:
               offset = (char*)dat - (char*)data.data();
               break;
            default:
               fail(REG_BADRPT);
               return code;
            }
            data.align();
            dat->next.i = data.size();
            //unsigned pos = (char*)dat - (char*)data.data();

            // add the trailing jump:
            add_simple(dat, syntax_element_jump, re_jump_size);

            // now insert the leading repeater:
            dat = (re_syntax_base*)data.insert(offset, re_repeater_size);
            dat->next.i = ((char*)dat - (char*)data.data()) + re_repeater_size;
            dat->type = syntax_element_rep;
            ((re_repeat*)dat)->alt.i = data.size();
            ((re_repeat*)dat)->min = rep_min;
            ((re_repeat*)dat)->max = rep_max;
            ((re_repeat*)dat)->leading = false;
            move_offsets(dat, re_repeater_size);
            dat = (re_syntax_base*)((char*)data.data() + data.size() - re_jump_size);
            ((re_repeat*)dat)->alt.i = offset;
            ++ptr;
            continue;
         }
      case syntax_plus:
         if(_flags & (bk_plus_qm | limited_ops))
         {
            dat = add_literal(dat, (charT)c);
            ++ptr;
            continue;
         }
         rep_min = 1;
         rep_max = (unsigned)-1;
         goto repeat_jump;
      case syntax_question:
         if(_flags & (bk_plus_qm | limited_ops))
         {
            dat = add_literal(dat, (charT)c);
            ++ptr;
            continue;
         }
         rep_min = 0;
         rep_max = 1;
         goto repeat_jump;
      case syntax_open_set:
         // update previous:
         if(dat)
         {
            data.align();
            dat->next.i = data.size();
         }
         // extend:
         dat = compile_set(ptr, end);
         if(dat == 0)
         {
            if((_flags & regbase::failbit) == 0)
               fail(REG_EBRACK);
            return code;
         }
         break;
      case syntax_or:
      {
         if(_flags & (bk_vbar | limited_ops))
         {
            dat = add_literal(dat, (charT)c);
            ++ptr;
            continue;
         }

         alt_string_jump:

         // update previous:
         if(dat == 0)
         {
            // start of pattern can't have empty "|"
            fail(REG_EMPTY);
            return code;
         }
         // see if we have an empty alternative:
         if(mark.empty() == false)
            if(mark.peek() == data.index(dat))
            {
               fail(REG_EMPTY);
               return code;
            }
         // extend:
         /*dat = */add_simple(dat, syntax_element_jump, re_jump_size);
         data.align();

         // now work out where to insert:
         unsigned int offset = 0;
         if(mark.empty() == false)
         {
            // we have a '(' or '|' to go back to:
            offset = mark.peek();
            re_syntax_base* base = (re_syntax_base*)((unsigned char*)data.data() + offset);
            offset = base->next.i;
         }
         re_jump* j = (re_jump*)data.insert(offset, re_jump_size);
         j->type = syntax_element_alt;
         j->next.i = offset + re_jump_size;
         j->alt.i = data.size();
         move_offsets(j, re_jump_size);
         dat = (re_syntax_base*)((unsigned char*)data.data() + data.size() - re_jump_size);
         mark.push(data.size() - re_jump_size);
         ++ptr;
         break;
      }
      case syntax_open_brace:
         if((_flags & bk_braces) || ((_flags & intervals) == 0))
         {
            dat = add_literal(dat, (charT)c);
            ++ptr;
            continue;
         }
         // we have {x} or {x,} or {x,y}:
         parse_range(ptr, end, rep_min, rep_max);
         goto repeat_jump;
      case syntax_newline:
         if(_flags & newline_alt)
            goto alt_string_jump;
         dat = add_literal(dat, (charT)c);
         ++ptr;
         continue;
      case syntax_close_brace:
         if(_flags & bk_braces)
         {
            dat = add_literal(dat, (charT)c);
            ++ptr;
            continue;
         }
         fail(REG_BADPAT);
         return code;
      default:
         dat = add_literal(dat, (charT)c);
         ++ptr;
         break;
      }  // switch
   }     // while

   //
   // update previous:
   if(dat)
   {
      data.align();
      dat->next.i = data.size();
   }

   // see if we have an empty alternative:
   if(mark.empty() == false)
      if(mark.peek() == data.index(dat) )
      {
         re_syntax_base* para = (re_syntax_base*)((char*)data.data() + mark.peek());
         if(para->type == syntax_element_jump)
         {
            fail(REG_EMPTY);
            return code;
         }
      }
   //
   // set up tail:
   //
   if(mark.empty() == false)
   {
      // pop any pushed alternatives and set the target end destination:
      dat = (re_syntax_base*)((unsigned char*)data.data() + mark.peek());
      while(dat->type == syntax_element_jump)
      {
         ((re_jump*)dat)->alt.i = data.size();
         mark.pop();
         if(mark.empty() == true)
            break;
         dat = (re_jump*)((unsigned char*)data.data() + mark.peek());
      }
   }

   dat = (re_brace*)data.extend(sizeof(re_syntax_base));
   dat->type = syntax_element_match;
   dat->next.i = 0;

   if(mark.empty() == false)
   {
      fail(REG_EPAREN);
      return code;
   }

   //
   // allocate space for start __map:
   startmap = (unsigned char*)data.extend(256 + ((end - base + 1) * sizeof(charT)));
   //
   // and copy the expression we just compiled:
   _expression = (charT*)((const char*)startmap + 256);
   memcpy(_expression, base, (end - base) * sizeof(charT));
   *(_expression + (end - base)) = charT(0);

   //
   // now we need to apply fixups to the array
   // so that we can use pointers and not indexes
   fixup_apply((re_syntax_base*)data.data(), marks);

   // check for error during fixup:
   if(_flags & regbase::failbit)
      return code;

   //
   // finally compile the maps so that we can make intelligent choices
   // whenever we encounter an alternative:
   compile_maps();
   if(pkmp)
   {
      kmp_free(pkmp, data.allocator());
      pkmp = 0;
   }
   re_syntax_base* sbase = (re_syntax_base*)data.data();
   _restart_type = probe_restart(sbase);
   _leading_len = fixup_leading_rep(sbase, 0);
   if((sbase->type == syntax_element_literal) && (sbase->next.p->type == syntax_element_match))
   {
      _restart_type = restart_fixed_lit;
      if(0 == pkmp)
      {
         charT* p1 = (charT*)((char*)sbase + sizeof(re_literal));
         charT* p2 = p1 + ((re_literal*)sbase)->length;
         pkmp = kmp_compile(p1, p2, charT(), kmp_translator<traits>(_flags&regbase::icase), data.allocator() MAYBE_PASS_LOCALE(locale_inst));
      }
   }
   return code;
}

template <class charT, class traits, class Allocator>
re_syntax_base* RE_CALL reg_expression<charT, traits, Allocator>::add_simple(re_syntax_base* dat, syntax_element_type type, unsigned int size)
{
   if(dat)
   {
      data.align();
      dat->next.i = data.size();
   }
   if(size < sizeof(re_syntax_base))
      size = sizeof(re_syntax_base);
   dat = (re_syntax_base*)data.extend(size);
   dat->type = type;
   dat->next.i = 0;
   return dat;
}

template <class charT, class traits, class Allocator>
re_syntax_base* RE_CALL reg_expression<charT, traits, Allocator>::add_literal(re_syntax_base* dat, charT c)
{
   if(dat && (dat->type == syntax_element_literal))
   {
      // add another charT to the list:
      __JM_STDC::ptrdiff_t pos = (unsigned char*)dat - (unsigned char*)data.data();
      *(charT*)data.extend(sizeof(charT)) = traits::translate(c, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst));
      dat = (re_syntax_base*)((unsigned char*)data.data() + pos);
      ++(((re_literal*)dat)->length);
   }
   else
   {
      // extend:
      dat = add_simple(dat, syntax_element_literal, sizeof(re_literal) + sizeof(charT));
      ((re_literal*)dat)->length = 1;
      *((charT*)(((re_literal*)dat)+1)) = traits::translate(c, (_flags & regbase::icase) MAYBE_PASS_LOCALE(locale_inst));
   }
   return dat;
}

template <class charT, class traits, class Allocator>
unsigned int RE_CALL reg_expression<charT, traits, Allocator>::probe_restart(re_syntax_base* dat)
{
   switch(dat->type)
   {
   case syntax_element_startmark:
   case syntax_element_endmark:
      return probe_restart(dat->next.p);
   case syntax_element_start_line:
      return regbase::restart_line;
   case syntax_element_word_start:
      return regbase::restart_word;
   case syntax_element_buffer_start:
      return regbase::restart_buf;
   case syntax_element_restart_continue:
      return regbase::restart_continue;
   default:
      return regbase::restart_any;
   }
}

template <class charT, class traits, class Allocator>
unsigned int RE_CALL reg_expression<charT, traits, Allocator>::fixup_leading_rep(re_syntax_base* dat, re_syntax_base* end)
{
   unsigned int len = 0;
   bool leading_lit = end ? false : true;
   while(dat != end)
   {
      switch(dat->type)
      {
      case syntax_element_literal:
         len += ((re_literal*)dat)->length;
         if((leading_lit) && (((re_literal*)dat)->length > 2))
         {
            // we can do a literal search for the leading literal string
            // using Knuth-Morris-Pratt (or whatever), and only then check for 
            // matches.  We need a decent length string though to make it
            // worth while.
            _leading_string = (charT*)((char*)dat + sizeof(re_literal));
            _leading_string_len = ((re_literal*)dat)->length;
            _restart_type = restart_lit;
            leading_lit = false;
            const charT* p1 = _leading_string;
            const charT* p2 = _leading_string + _leading_string_len;
            pkmp = kmp_compile(p1, p2, charT(), kmp_translator<traits>(_flags&regbase::icase), data.allocator() MAYBE_PASS_LOCALE(locale_inst));
         }
         break;
      case syntax_element_wild:
         ++len;
         leading_lit = false;
         break;
      case syntax_element_match:
         return len;
      case syntax_element_backref:
      //case syntax_element_jump:
      case syntax_element_alt:
      case syntax_element_combining:
         return 0;
      case syntax_element_long_set:
      {
         // we need to verify that there are no multi-character
         // collating elements inside the repeat:
         const charT* p = (const charT*)((const char*)dat + sizeof(re_set_long));
         unsigned int csingles = ((re_set_long*)dat)->csingles;
         for(unsigned int i = 0; i < csingles; ++i)
         {
            if(re_strlen(p) > 1)
               return 0;
            while(*p)++p;
            ++p;
         }
         ++len;
         leading_lit = false;
         break;
      }
      case syntax_element_set:
         ++len;
         leading_lit = false;
         break;
      case syntax_element_rep:
         if(1 == fixup_leading_rep(dat->next.p, ((re_repeat*)dat)->alt.p) )
         {
            ((re_repeat*)dat)->leading = true;
            return len;
         }
         return 0;
      }
      dat = dat->next.p;
   }
   return len;
}

#if defined(JM_NO_TEMPLATE_SWITCH_MERGE) && !defined(JM_NO_NAMESPACES)
} // namespace
#endif

JM_END_NAMESPACE







