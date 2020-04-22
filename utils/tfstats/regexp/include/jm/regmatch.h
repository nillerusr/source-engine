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
  *   FILE     regmatch.h
  *   VERSION  2.12
  *   regular expression matching algorithms
  */


#ifndef __REGMATCH_H
#define __REGMATCH_H


JM_NAMESPACE(__JM)

template <class iterator, class charT, class traits_type, class Allocator>
iterator RE_CALL re_is_set_member(iterator next, 
                          iterator last, 
                          re_set_long* set, 
                          const reg_expression<charT, traits_type, Allocator>& e)
{   
   const charT* p = (const charT*)(set+1);
   iterator ptr;
   unsigned int i;
   bool icase = e.flags() & regbase::icase;

   // try and match a single character, could be a multi-character
   // collating element...
   for(i = 0; i < set->csingles; ++i)
   {
      ptr = next;
      while(*p && (ptr != last))
      {
         if(traits_type::translate(*ptr, icase MAYBE_PASS_LOCALE(e.locale())) != *p)
            break;
         ++p;
         ++ptr;
      }
      if(*p == 0) // if null we've matched
         return set->isnot ? next : (ptr == next) ? ++next : ptr;

      while(*p)++p;
      ++p;     // skip null
   }

   charT col = traits_type::translate(*next, icase MAYBE_PASS_LOCALE(e.locale()));


   if(set->cranges || set->cequivalents)
   {
      re_str<charT> s2(col);
      re_str<charT> s1;
      //
      // try and match a range, NB only a single character can match
      if(set->cranges)
      {
         if(e.flags() & regbase::nocollate)
            s1 = s2;
         else
            traits_type::transform(s1, s2 MAYBE_PASS_LOCALE(e.locale()));
         for(i = 0; i < set->cranges; ++i)
         {
            if(s1 <= p)
            {
               while(*p)++p;
               ++p;
               if(s1 >= p)
                  return set->isnot ? next : ++next;
            }
            else
            {
               // skip first string
               while(*p)++p;
               ++p;
            }
            // skip second string
            while(*p)++p;
            ++p;
         }
      }
      //
      // try and match an equivalence class, NB only a single character can match
      if(set->cequivalents)
      {
         traits_type::transform_primary(s1, s2 MAYBE_PASS_LOCALE(e.locale()));
         for(i = 0; i < set->cequivalents; ++i)
         {
            if(s1 == p)
               return set->isnot ? next : ++next;
            // skip string
            while(*p)++p;
            ++p;
         }
      }
   }

   if(traits_type::is_class(col, set->cclasses MAYBE_PASS_LOCALE(e.locale())) == true)
      return set->isnot ? next : ++next;
   return set->isnot ? ++next : next;
}

template <class iterator, class Allocator>
class __priv_match_data
{
public:
   typedef JM_MAYBE_TYPENAME REBIND_TYPE(int, Allocator) i_alloc;
   typedef JM_MAYBE_TYPENAME REBIND_TYPE(iterator, Allocator) it_alloc;

   reg_match_base<iterator, Allocator> temp_match;
   // failure stacks:
   jstack<reg_match_base<iterator, Allocator>, Allocator> matches;
   jstack<iterator, Allocator> prev_pos;
   jstack<const re_syntax_base*, Allocator> prev_record;
   jstack<int, Allocator> prev_acc;
   int* accumulators;
   unsigned int caccumulators;
   iterator* loop_starts;

   __priv_match_data(const reg_match_base<iterator, Allocator>&);
   
   ~__priv_match_data()
   {
      free();
   }
   void free();
   void set_accumulator_size(unsigned int size);
   int* get_accumulators()
   {
      return accumulators;
   }
   iterator* get_loop_starts()
   {
      return loop_starts;
   }
};

template <class iterator, class Allocator>
__priv_match_data<iterator, Allocator>::__priv_match_data(const reg_match_base<iterator, Allocator>& m)
  : temp_match(m), matches(64, m.allocator()), prev_pos(64, m.allocator()), prev_record(64, m.allocator())
{
  accumulators = 0;
  caccumulators = 0;
  loop_starts = 0;
}

template <class iterator, class Allocator>
void __priv_match_data<iterator, Allocator>::set_accumulator_size(unsigned int size)
{
   if(size > caccumulators)
   {
      free();
      caccumulators = size;
      accumulators = i_alloc(temp_match.allocator()).allocate(caccumulators);
      loop_starts = it_alloc(temp_match.allocator()).allocate(caccumulators);
      for(unsigned i = 0; i < caccumulators; ++i)
         new (loop_starts + i) iterator();
   }
}

template <class iterator, class Allocator>
void __priv_match_data<iterator, Allocator>::free()
{
   if(caccumulators)
   {
      //REBIND_INSTANCE(int, Allocator, temp_match.allocator()).deallocate(accumulators, caccumulators);
      i_alloc temp1(temp_match.allocator());
      temp1.deallocate(accumulators, caccumulators);
      for(unsigned i = 0; i < caccumulators; ++i)
         jm_destroy(loop_starts + i);
      //REBIND_INSTANCE(iterator, Allocator, temp_match.allocator()).deallocate(loop_starts, caccumulators);
      it_alloc temp2(temp_match.allocator());
      temp2.deallocate(loop_starts, caccumulators);
   }
}

//
// proc query_match
// returns true if the specified regular expression matches
// at position first.  Fills in what matched in m.
//
template <class iterator, class Allocator, class charT, class traits, class Allocator2>
bool query_match(iterator first, iterator last, reg_match<iterator, Allocator>& m, const reg_expression<charT, traits, Allocator2>& e, unsigned flags = match_default)
{
   // prepare m for failure:
   if((flags & match_init) == 0)
   {
      m.set_size(e.mark_count(), first, last);
   }
   __priv_match_data<iterator, Allocator> pd(m);
   iterator restart;
   return query_match_aux(first, last, m, e, flags, pd, &restart);
}

//
// query_match convenience interfaces:
#ifndef JM_NO_PARTIAL_FUNC_SPEC
//
// this isn't really a partial specialisation, but template function
// overloading - if the compiler doesn't support partial specialisation
// then it really won't support this either:
template <class charT, class Allocator, class traits, class Allocator2>
inline bool query_match(const charT* str, 
                        reg_match<const charT*, Allocator>& m, 
                        const reg_expression<charT, traits, Allocator2>& e, 
                        unsigned flags = match_default)
{
   return query_match(str, str + traits::length(str), m, e, flags);
}

#ifndef JM_NO_STRING_H
template <class ST, class SA, class Allocator, class charT, class traits, class Allocator2>
inline bool query_match(const __JM_STD::basic_string<charT, ST, SA>& s, 
                 reg_match<typename __JM_STD::basic_string<charT, ST, SA>::const_iterator, Allocator>& m, 
                 const reg_expression<charT, traits, Allocator2>& e, 
                 unsigned flags = match_default)
{
   return query_match(s.begin(), s.end(), m, e, flags);
}
#endif
#else  // partial specialisation
inline bool query_match(const char* str, 
                        cmatch& m, 
                        const regex& e, 
                        unsigned flags = match_default)
{
   return query_match(str, str + regex::traits_type::length(str), m, e, flags);
}
#ifndef JM_NO_WCSTRING
inline bool query_match(const wchar_t* str, 
                        wcmatch& m, 
                        const wregex& e, 
                        unsigned flags = match_default)
{
   return query_match(str, str + wregex::traits_type::length(str), m, e, flags);
}
#endif
#ifndef JM_NO_STRING_H
inline bool query_match(const __JM_STD::string& s, 
                        reg_match<__JM_STD::string::const_iterator, regex::alloc_type>& m, 
                        const regex& e, 
                        unsigned flags = match_default)
{
   return query_match(s.begin(), s.end(), m, e, flags);
}
#if !defined(JM_NO_STRING_DEF_ARGS) && !defined(JM_NO_WCSTRING)
inline bool query_match(const __JM_STD::basic_string<wchar_t>& s, 
                        reg_match<__JM_STD::basic_string<wchar_t>::const_iterator, wregex::alloc_type>& m, 
                        const wregex& e, 
                        unsigned flags = match_default)
{
   return query_match(s.begin(), s.end(), m, e, flags);
}
#endif

#endif

#endif


#if defined(JM_NO_TEMPLATE_SWITCH_MERGE) && !defined(JM_NO_NAMESPACES)
//
// Ugly ugly hack,
// template don't merge if they contain switch statements so declare these
// templates in unnamed namespace (ie with internal linkage), each translation
// unit then gets its own local copy, it works seemlessly but bloats the app.
namespace{
#endif

template <class iterator, class Allocator, class charT, class traits, class Allocator2>
bool query_match_aux(iterator first, 
                     iterator last, 
                     reg_match<iterator, Allocator>& m, 
                     const reg_expression<charT, traits, Allocator2>& e, 
                     unsigned flags, 
                     __priv_match_data<iterator, Allocator>& pd,
                     iterator* restart)
{
   if(e.flags() & regbase::failbit)
      return false;

   typedef typename traits::size_type traits_size_type;
   typedef typename traits::uchar_type traits_uchar_type;
   typedef typename is_byte<charT>::width_type width_type;
   
   #ifdef RE_LOCALE_CPP
   const __JM_STD::locale& locale_inst = e.locale();
   #endif

   // declare some local aliases to reduce pointer loads
   // good optimising compilers should make this unnecessary!!
   jstack<reg_match_base<iterator, Allocator>, Allocator>& matches = pd.matches;
   jstack<iterator, Allocator>& prev_pos = pd.prev_pos;
   jstack<const re_syntax_base*, Allocator>& prev_record = pd.prev_record;
   jstack<int, Allocator>& prev_acc = pd.prev_acc;
   reg_match_base<iterator, Allocator>& temp_match = pd.temp_match;
   temp_match.set_first(first);

   //temp_match.set_size(e.mark_count(), first, last);
   register const re_syntax_base* ptr = e.first();
   bool match_found = false;
   bool need_push_match = (e.mark_count() > 1);
   int cur_acc = -1;    // no active accumulator
   pd.set_accumulator_size(e.repeat_count());
   int* accumulators = pd.get_accumulators();
   iterator* start_loop = pd.get_loop_starts();
   int k; // for loops
   bool icase = e.flags() & regbase::icase;
   *restart = first;
   iterator base = first;

   // prepare m for failure:
   /*
   if((flags & match_init) == 0)
   {
      m.init_fail(first, last);
   } */

   retry:

   while(first != last)
   {
      jm_assert(ptr);
      switch(ptr->type)
      {
      case syntax_element_match:
         match_jump:
         {
            // match found, save then fallback in case we missed a
            // longer one.
            if((flags & match_not_null) && (first == temp_match[0].first))
               goto failure;
            temp_match.set_second(first);
            m.maybe_assign(temp_match);
            match_found = true;
            if((flags & match_any) || ((first == last) && (need_push_match == false)))
            {
               // either we don't care what we match or we've matched
               // the whole string and can't match anything longer.
               while(matches.empty() == false)
                  matches.pop();
               while(prev_pos.empty() == false)
                  prev_pos.pop();
               while(prev_record.empty() == false)
                  prev_record.pop();
               while(prev_acc.empty() == false)
                  prev_acc.pop();
               return true;
            }
         }
         goto failure;
      case syntax_element_startmark:
         temp_match.set_first(first, ((re_brace*)ptr)->index);
         ptr = ptr->next.p;
         break;
      case syntax_element_endmark:
         temp_match.set_second(first, ((re_brace*)ptr)->index);
         ptr = ptr->next.p;
         break;
      case syntax_element_literal:
      {
         unsigned int len = ((re_literal*)ptr)->length;
         charT* what = (charT*)(((re_literal*)ptr) + 1);
         //
         // compare string with what we stored in
         // our records:
         for(unsigned int i = 0; i < len; ++i, ++first)
         {
            if((first == last) || (traits::translate(*first, icase MAYBE_PASS_LOCALE(locale_inst)) != what[i]))
               goto failure;
         }
         ptr = ptr->next.p;
         break;
      }
      case syntax_element_start_line:
         outer_line_check:
         if(first == temp_match[0].first)
         {
            // we're at the start of the buffer
            if(flags & match_prev_avail)
            {
               inner_line_check:
               // check the previous value even though its before
               // the start of our "buffer".
               iterator t(first);
               --t;
               if(traits::is_separator(*t) && !((*t == '\r') && (*first == '\n')) )
               {
                  ptr = ptr->next.p;
                  continue;
               }
               goto failure;
            }
            if((flags & match_not_bol) == 0)
            {
               ptr = ptr->next.p;
               continue;
            }
            goto failure;
         }
         // we're in the middle of the string
         goto inner_line_check;
      case syntax_element_end_line:
         // we're not yet at the end so *first is always valid:
         if(traits::is_separator(*first))
         {
            if((first != base) || (flags & match_prev_avail))
            {
               // check that we're not in the middle of \r\n sequence
               iterator t(first);
               --t;
               if((*t == '\r') && (*first == '\n'))
               {
                  goto failure;
               }
            }
            ptr = ptr->next.p;
            continue;
         }
         goto failure;
      case syntax_element_wild:
         // anything except possibly NULL or \n:
         if(traits::is_separator(*first))
         {
            if(flags & match_not_dot_newline)
               goto failure;
            ptr = ptr->next.p;
            ++first;
            continue;
         }
         if(*first == charT(0))
         {
            if(flags & match_not_dot_null)
               goto failure;
            ptr = ptr->next.p;
            ++first;
            continue;
         }
         ptr = ptr->next.p;
         ++first;
         break;
      case syntax_element_word_boundary:
      {
         // prev and this character must be opposites:
         bool b = traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst));
         if((first == temp_match[0].first)  && ((flags & match_prev_avail) == 0))
         {
            if(flags & match_not_bow)
               b ^= true;
            else
               b ^= false;
         }
         else
         {
            --first;
            b ^= traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst));
            ++first;
         }
         if(b)
         {
            ptr = ptr->next.p;
            continue;
         }
         goto failure;
      }
      case syntax_element_within_word:
         // both prev and this character must be char_class_word:
         if(traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst)))
         {
            bool b;
            if((first == temp_match[0].first) && ((flags & match_prev_avail) == 0))
               b = false;
            else
            {
               --first;
               b = traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst));
               ++first;
            }
            if(b)
            {
               ptr = ptr->next.p;
               continue;
            }
         }
         goto failure;
      case syntax_element_word_start:
         if((first == temp_match[0].first) && ((flags & match_prev_avail) == 0))
         {
            // start of buffer:
            if(flags & match_not_bow)
               goto failure;
            if(traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst)))
            {
               ptr = ptr->next.p;
               continue;
            }
            goto failure;
         }
         // otherwise inside buffer:
         if(traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst)))
         {
            iterator t(first);
            --t;
            if(traits::is_class(*t, char_class_word MAYBE_PASS_LOCALE(locale_inst)) == false)
            {
               ptr = ptr->next.p;
               continue;
            }
         }
         goto failure;      // if we fall through to here then we've failed
      case syntax_element_word_end:
         if((first == temp_match[0].first) && ((flags & match_prev_avail) == 0))
            goto failure;  // start of buffer can't be end of word

         // otherwise inside buffer:
         if(traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst)) == false)
         {
            iterator t(first);
            --t;
            if(traits::is_class(*t, char_class_word MAYBE_PASS_LOCALE(locale_inst)))
            {
               ptr = ptr->next.p;
               continue;
            }
         }
         goto failure;      // if we fall through to here then we've failed
      case syntax_element_buffer_start:
         if((first != temp_match[0].first) || (flags & match_not_bob))
            goto failure;
         // OK match:
         ptr = ptr->next.p;
         break;
      case syntax_element_buffer_end:
         if((first != last) || (flags & match_not_eob))
            goto failure;
         // OK match:
         ptr = ptr->next.p;
         break;
      case syntax_element_backref:
      {
         // compare with what we previously matched:
         iterator i = temp_match[((re_brace*)ptr)->index].first;
         iterator j = temp_match[((re_brace*)ptr)->index].second;
         while(i != j)
         {
            if((first == last) || (traits::translate(*first, icase MAYBE_PASS_LOCALE(locale_inst)) != traits::translate(*i, icase MAYBE_PASS_LOCALE(locale_inst))))
               goto failure;
            ++i;
            ++first;
         }
         ptr = ptr->next.p;
         break;
      }
      case syntax_element_long_set:
      {
         // let the traits class do the work:
         iterator t = re_is_set_member(first, last, (re_set_long*)ptr, e);
         if(t != first)
         {
            ptr = ptr->next.p;
            first = t;
            continue;
         }
         goto failure;
      }
      case syntax_element_set:
         // lookup character in table:
         if(((re_set*)ptr)->__map[(traits_uchar_type)traits::translate(*first, icase MAYBE_PASS_LOCALE(locale_inst))])
         {
            ptr = ptr->next.p;
            ++first;
            continue;
         }
         goto failure;
      case syntax_element_jump:
         ptr = ((re_jump*)ptr)->alt.p;
         continue;
      case syntax_element_alt:
      {
         // alt_jump:
         if(reg_expression<charT, traits, Allocator2>::can_start(*first, ((re_jump*)ptr)->__map, (unsigned char)mask_take, width_type()))
         {
            // we can take the first alternative,
            // see if we need to push next alternative:
            if(reg_expression<charT, traits, Allocator2>::can_start(*first, ((re_jump*)ptr)->__map, mask_skip, width_type()))
            {
               if(need_push_match)
                  matches.push(temp_match);
               for(k = 0; k <= cur_acc; ++k)
                  prev_pos.push(start_loop[k]);
               prev_pos.push(first);
               prev_record.push(ptr);
               for(k = 0; k <= cur_acc; ++k)
                  prev_acc.push(accumulators[k]);
               prev_acc.push(cur_acc);
            }
            ptr = ptr->next.p;
            continue;
         }
         if(reg_expression<charT, traits, Allocator2>::can_start(*first, ((re_jump*)ptr)->__map, mask_skip, width_type()))
         {
            ptr = ((re_jump*)ptr)->alt.p;
            continue;
         }
         goto failure;  // neither option is possible
      }
      case syntax_element_rep:
      {
         // repeater_jump:
         // if we're moving to a higher id (nested repeats etc)
         // zero out our accumualtors:
         if(cur_acc < ((re_repeat*)ptr)->id)
         {
            cur_acc = ((re_repeat*)ptr)->id;
            accumulators[cur_acc] = 0;
            start_loop[cur_acc] = iterator();
         }

         cur_acc = ((re_repeat*)ptr)->id;

         if(((re_repeat*)ptr)->leading)
            *restart = first;

         //charT c = traits::translate(*first MAYBE_PASS_LOCALE(locale_inst));

         // first of all test for special case where this is last element,
         // if that is the case then repeat as many times as possible:

         if(((re_repeat*)ptr)->alt.p->type == syntax_element_match)
         {
            // see if we can take the repeat:
            if(((unsigned int)accumulators[cur_acc] < ((re_repeat*)ptr)->max)
                  && reg_expression<charT, traits, Allocator2>::can_start(*first, ((re_repeat*)ptr)->__map, mask_take, width_type()))
            {
               // push terminating match as fallback:
               if((unsigned int)accumulators[cur_acc] >= ((re_repeat*)ptr)->min)
               {
                  if((prev_record.empty() == false) && (prev_record.peek() == ((re_repeat*)ptr)->alt.p))
                  {
                     // we already have the required fallback
                     // don't add any more, just update this one:
                     if(need_push_match)
                        matches.peek() = temp_match;
                     prev_pos.peek() = first;
                  }
                  else
                  {
                     if(need_push_match)
                        matches.push(temp_match);
                     prev_pos.push(first);
                     prev_record.push(((re_repeat*)ptr)->alt.p);
                  }
               }
               // move to next item in list:
               if(first != start_loop[cur_acc])
               {
                  ++accumulators[cur_acc];
                  ptr = ptr->next.p;
                  start_loop[cur_acc] = first;
                  continue;
               }
               goto failure;
            }
            // see if we can skip the repeat:
            if(((unsigned int)accumulators[cur_acc] >= ((re_repeat*)ptr)->min)
               && reg_expression<charT, traits, Allocator2>::can_start(*first, ((re_repeat*)ptr)->__map, mask_skip, width_type()))
            {
               ptr = ((re_repeat*)ptr)->alt.p;
               continue;
            }
            // otherwise fail:
            goto failure;
         }

         // see if we can skip the repeat:
         if(((unsigned int)accumulators[cur_acc] >= ((re_repeat*)ptr)->min)
            && reg_expression<charT, traits, Allocator2>::can_start(*first, ((re_repeat*)ptr)->__map, mask_skip, width_type()))
         {
            // see if we can push failure info:
            if(((unsigned int)accumulators[cur_acc] < ((re_repeat*)ptr)->max)
               && reg_expression<charT, traits, Allocator2>::can_start(*first, ((re_repeat*)ptr)->__map, mask_take, width_type()))
            {
               // check to see if the last loop matched a NULL string
               // if so then we really don't want to loop again:
               if(((unsigned int)accumulators[cur_acc] == ((re_repeat*)ptr)->min)
                  || (first != start_loop[cur_acc]))
               {
                  if(need_push_match)
                     matches.push(temp_match);
                  prev_pos.push(first);
                  prev_record.push(ptr);
                  for(k = 0; k <= cur_acc; ++k)
                     prev_acc.push(accumulators[k]);
                  //prev_acc.push(cur_acc);
               }
            }
            ptr = ((re_repeat*)ptr)->alt.p;
            continue;
         }

         // otherwise see if we can take the repeat:
         if(((unsigned int)accumulators[cur_acc] < ((re_repeat*)ptr)->max)
               && reg_expression<charT, traits, Allocator2>::can_start(*first, ((re_repeat*)ptr)->__map, mask_take, width_type()) &&
               (first != start_loop[cur_acc]))
         {
            // move to next item in list:
            ++accumulators[cur_acc];
            ptr = ptr->next.p;
            start_loop[cur_acc] = first;
            continue;
         }

         // if we get here then neither option is allowed so fail:
         goto failure;

      }
      case syntax_element_combining:
         if(traits::is_combining(traits::translate(*first, icase MAYBE_PASS_LOCALE(locale_inst))))
            goto failure;
         ++first;
         while((first != last) && traits::is_combining(traits::translate(*first, icase MAYBE_PASS_LOCALE(locale_inst))))++first;
         ptr = ptr->next.p;
         continue;
      case syntax_element_soft_buffer_end:
         {
            if(flags & match_not_eob)
               goto failure;
            iterator p(first);
            while((p != last) && traits::is_separator(traits::translate(*first, icase MAYBE_PASS_LOCALE(locale_inst))))++p;
            if(p != last)
               goto failure;
            ptr = ptr->next.p;
            continue;
         }
      case syntax_element_restart_continue:
         if(first != temp_match[-1].first)
            goto failure;
         ptr = ptr->next.p;
         continue;
      default:
         jm_assert(0); // should never get to here!!
         return false;
      }
   }

   //
   // if we get to here then we've run out of characters to match against,
   // we could however still have non-character regex items left
   if(ptr->can_be_null == 0)
      goto failure;
   while(true)
   {
      jm_assert(ptr);
      switch(ptr->type)
      {
      case syntax_element_match:
         goto match_jump;
      case syntax_element_startmark:
         temp_match.set_first(first, ((re_brace*)ptr)->index);
         ptr = ptr->next.p;
         break;
      case syntax_element_endmark:
         temp_match.set_second(first, ((re_brace*)ptr)->index);
         ptr = ptr->next.p;
         break;
      case syntax_element_start_line:
         goto outer_line_check;
      case syntax_element_end_line:
         // we're at the end so *first is never valid:
         if((flags & match_not_eol) == 0)
         {
            ptr = ptr->next.p;
            continue;
         }
         goto failure;
      case syntax_element_word_boundary:
      case syntax_element_word_end:
         if(((flags & match_not_eow) == 0) && (first != temp_match[0].first))
         {
            iterator t(first);
            --t;
            if(traits::is_class(*t, char_class_word MAYBE_PASS_LOCALE(locale_inst)))
            {
               ptr = ptr->next.p;
               continue;
            }
         }
         goto failure;
      case syntax_element_buffer_end:
      case syntax_element_soft_buffer_end:
         if(flags & match_not_eob)
            goto failure;
         // OK match:
         ptr = ptr->next.p;
         break;
      case syntax_element_jump:
         ptr = ((re_jump*)ptr)->alt.p;
         continue;
      case syntax_element_alt:
         if(ptr->can_be_null & mask_take)
         {
            // we can test the first alternative,
            // see if we need to push next alternative:
            if(ptr->can_be_null & mask_skip)
            {
               if(need_push_match)
                  matches.push(temp_match);
               for(k = 0; k <= cur_acc; ++k)
                  prev_pos.push(start_loop[k]);
               prev_pos.push(first);
               prev_record.push(ptr);
               for(k = 0; k <= cur_acc; ++k)
                  prev_acc.push(accumulators[k]);
               prev_acc.push(cur_acc);
            }
            ptr = ptr->next.p;
            continue;
         }
         if(ptr->can_be_null & mask_skip)
         {
            ptr = ((re_jump*)ptr)->alt.p;
            continue;
         }
         goto failure;  // neither option is possible
      case syntax_element_rep:
         // if we're moving to a higher id (nested repeats etc)
         // zero out our accumualtors:
         if(cur_acc < ((re_repeat*)ptr)->id)
         {
            cur_acc = ((re_repeat*)ptr)->id;
            accumulators[cur_acc] = 0;
            start_loop[cur_acc] = first;
         }

         cur_acc = ((re_repeat*)ptr)->id;

         // see if we can skip the repeat:
         if(((unsigned int)accumulators[cur_acc] >= ((re_repeat*)ptr)->min)
            && (ptr->can_be_null & mask_skip))
         {
            // don't push failure info, there's no point:
            ptr = ((re_repeat*)ptr)->alt.p;
            continue;
         }

         // otherwise see if we can take the repeat:
         if(((unsigned int)accumulators[cur_acc] < ((re_repeat*)ptr)->max)
               && ((ptr->can_be_null & (mask_take | mask_skip)) == (mask_take | mask_skip)))
         {
            // move to next item in list:
            ++accumulators[cur_acc];
            ptr = ptr->next.p;
            start_loop[cur_acc] = first;
            continue;
         }

         // if we get here then neither option is allowed so fail:
         goto failure;
      case syntax_element_restart_continue:
         if(first != temp_match[-1].first)
            goto failure;
         ptr = ptr->next.p;
         continue;
      default:
         goto failure;
      }
   }

   failure:

   if(prev_record.empty() == false)
   {
      ptr = prev_record.peek();
      switch(ptr->type)
      {
      case syntax_element_alt:
         // get next alternative:
         ptr = ((re_jump*)ptr)->alt.p;
         if(need_push_match)
            matches.pop(temp_match);
         prev_acc.pop(cur_acc);
         for(k = cur_acc; k >= 0; --k)
            prev_acc.pop(accumulators[k]);
         prev_pos.pop(first);
         for(k = cur_acc; k >= 0; --k)
            prev_pos.pop(start_loop[k]);
         prev_record.pop();
         goto retry;
      case syntax_element_rep:
         // we're doing least number of repeats first,
         // increment count and repeat again:
         if(need_push_match)
            matches.pop(temp_match);
         prev_pos.pop(first);
         cur_acc = ((re_repeat*)ptr)->id;
         for(k = cur_acc; k >= 0; --k)
            prev_acc.pop(accumulators[k]);
         prev_record.pop();
         if((unsigned int)++accumulators[cur_acc] > ((re_repeat*)ptr)->max)
            goto failure;  // repetions exhausted.
         ptr = ptr->next.p;
         start_loop[cur_acc] = first;
         goto retry;
      case syntax_element_match:
         if(need_push_match)
            matches.pop(temp_match);
         prev_pos.pop(first);
         prev_record.pop();
         goto retry;
     default:
         jm_assert(0);
         // mustn't get here!!
      }
   }

   if(match_found)
      return true;

   // if we get to here then everything has failed
   // and no match was found:
   return false;
}
#if defined(JM_NO_TEMPLATE_SWITCH_MERGE) && !defined(JM_NO_NAMESPACES)
} // namespace
#endif


template <class iterator>
void __skip_and_inc(unsigned int& clines, iterator& last_line, iterator& first, const iterator last)
{
   while(first != last)
   {
      if(*first == '\n')
      {
         last_line = ++first;
         ++clines;
      }
      else
         ++first;
   }
}

template <class iterator>
void __skip_and_dec(unsigned int& clines, iterator& last_line, iterator& first, iterator base, unsigned int len)
{
   bool need_line = false;
   for(unsigned int i = 0; i < len; ++i)
   {
      --first;
      if(*first == '\n')
      {
         need_line = true;
         --clines;
      }
   }

   if(need_line)
   {
      last_line = first;

      if(last_line != base)
         --last_line;
      else
         return;

      while((last_line != base) && (*last_line != '\n'))
         --last_line;
      if(*last_line == '\n')
         ++last_line;
   }
}

template <class iterator>
inline void __inc_one(unsigned int& clines, iterator& last_line, iterator& first)
{
   if(*first == '\n')
   {
      last_line = ++first;
      ++clines;
   }
   else
      ++first;
}

template <class iterator, class Allocator>
struct grep_search_predicate
{
   reg_match<iterator, Allocator>* pm;
   grep_search_predicate(reg_match<iterator, Allocator>* p) : pm(p) {}
   bool operator()(const reg_match<iterator, Allocator>& m) 
   {
      *pm = static_cast<const reg_match_base<iterator, Allocator>&>(m);
      return false;
   }
};

#if !defined(JM_NO_TEMPLATE_RETURNS) && !defined(JM_NO_PARTIAL_FUNC_SPEC)

template <class iterator, class Allocator>
inline const reg_match_base<iterator, Allocator>& grep_out_type(const grep_search_predicate<iterator, Allocator>& o, const Allocator&)
{
   return *(o.pm);
}

#endif

template <class T, class Allocator>
inline const Allocator& grep_out_type(const T&, const Allocator& a)
{
   return a;
}

#if defined(JM_NO_TEMPLATE_SWITCH_MERGE) && !defined(JM_NO_NAMESPACES)
//
// Ugly ugly hack,
// template don't merge if they contain switch statements so declare these
// templates in unnamed namespace (ie with internal linkage), each translation
// unit then gets its own local copy, it works seemlessly but bloats the app.
namespace{
#endif

//
// reg_grep2:
// find all non-overlapping matches within the sequence first last:
//
template <class Predicate, class I, class charT, class traits, class A, class A2>
unsigned int reg_grep2(Predicate foo, I first, I last, const reg_expression<charT, traits, A>& e, unsigned flags, A2 a)
{
   if(e.flags() & regbase::failbit)
      return 0;

   typedef typename traits::size_type traits_size_type;
   typedef typename traits::uchar_type traits_uchar_type;
   typedef typename is_byte<charT>::width_type width_type;

   reg_match<I, A2> m(grep_out_type(foo, a));
   I restart;
   m.set_size(e.mark_count(), first, last);
   m.set_line(1, first);

   #ifdef RE_LOCALE_CPP
   const __JM_STD::locale& locale_inst = e.locale();
   #endif

   unsigned int clines = 1;
   unsigned int cmatches = 0;
   I last_line = first;
   I next_base;
   I base = first;
   bool need_init;

   flags |= match_init;

   __priv_match_data<I, A2> pd(m);

   const unsigned char* __map = e.get_map();
   unsigned int type;

   if(first == last)
   {
      // special case, only test if can_be_null,
      // don't dereference any pointers!!
      if(e.first()->can_be_null)
         if(query_match_aux(first, last, m, e, flags, pd, &restart))
         {
            foo(m);
            ++cmatches;
         }
      return cmatches;
   }

   // try one time whatever:
   if( reg_expression<charT, traits, A>::can_start(*first, __map, (unsigned char)mask_any, width_type() ) )
   {
      if(query_match_aux(first, last, m, e, flags, pd, &restart))
      {
         ++cmatches;
         if(foo(m) == false)
            return cmatches;
         // update to end of what matched
         // trying to match again with match_not_null set if this 
         // is a null match...
         need_init = true;
         if(first == m[0].second)
         {
            next_base = m[0].second;
            pd.temp_match.init_fail(next_base, last);
            m.init_fail(next_base, last);
            if(query_match_aux(first, last, m, e, flags | match_not_null, pd, &restart))
            {
               ++cmatches;
               if(foo(m) == false)
                  return cmatches;
            }
            else
            {
               need_init = false;
               for(unsigned int i = 0; (restart != first) && (i < e.leading_length()); ++i, --restart);
               if(restart != last)
                  ++restart;
               __skip_and_inc(clines, last_line, first, restart);
            }
         }
         if(need_init)
         {
            __skip_and_inc(clines, last_line, first, m[0].second);
            next_base = m[0].second;
            pd.temp_match.init_fail(next_base, last);
            m.init_fail(next_base, last);
         }
      }
      else
      {
         for(unsigned int i = 0; (restart != first) && (i < e.leading_length()); ++i, --restart);
         if(restart != last)
            ++restart;
         __skip_and_inc(clines, last_line, first, restart);
      }
   }
   else
      __inc_one(clines, last_line, first);
   flags |= match_prev_avail | match_not_bob;

   
   // depending on what the first record is we may be able to
   // optimise the search:
   type = (flags & match_continuous) ? regbase::restart_continue : e.restart_type();

   if(type == regbase::restart_buf)
      return cmatches;

   switch(type)
   {
   case regbase::restart_lit: 
   case regbase::restart_fixed_lit:
   {
      const kmp_info<charT>* info = e.get_kmp();
      int len = info->len;
      const charT* x = info->pstr;
      int j = 0; 
      bool icase = e.flags() & regbase::icase;
      while (first != last) 
      {
         while((j > -1) && (x[j] != traits::translate(*first, icase MAYBE_PASS_LOCALE(locale_inst)))) 
            j = info->kmp_next[j];
         __inc_one(clines, last_line, first);
         ++j;
         if(j >= len) 
         {
            if(type == regbase::restart_fixed_lit)
            {
               __skip_and_dec(clines, last_line, first, base, j);
               restart = first;
               restart += len;
               m.set_first(first);
               m.set_second(restart);
               m.set_line(clines, last_line);
               ++cmatches;
               if(foo(m) == false)
                  return cmatches;
               __skip_and_inc(clines, last_line, first, restart);
               next_base = m[0].second;
               pd.temp_match.init_fail(next_base, last);
               m.init_fail(next_base, last);
               j = 0;
            }
            else
            {
               restart = first;
               __skip_and_dec(clines, last_line, first, base, j);
               if(query_match_aux(first, last, m, e, flags, pd, &restart))
               {

                  m.set_line(clines, last_line);
                  ++cmatches;
                  if(foo(m) == false)
                     return cmatches;
                  // update to end of what matched
                 __skip_and_inc(clines, last_line, first, m[0].second);
                  next_base = m[0].second;
                  pd.temp_match.init_fail(next_base, last);
                  m.init_fail(next_base, last);
                  j = 0;
               }
               else
               {
                  for(int k = 0; (restart != first) && (k < j); ++k, --restart);
                  if(restart != last)
                     ++restart;
                  __skip_and_inc(clines, last_line, first, restart);
                  j = 0;  //we could do better than this...
               }
            }
         }
      }
      break;
   }
   case regbase::restart_any:
   {
      while(first != last)
      {
         if( reg_expression<charT, traits, A>::can_start(*first, __map, (unsigned char)mask_any, width_type()) )
         {
            if(query_match_aux(first, last, m, e, flags, pd, &restart))
            {

               m.set_line(clines, last_line);
               ++cmatches;
               if(foo(m) == false)
                  return cmatches;
               // update to end of what matched
               // trying to match again with match_not_null set if this 
               // is a null match...
               need_init = true;
               if(first == m[0].second)
               {
                  next_base = m[0].second;
                  pd.temp_match.init_fail(next_base, last);
                  m.init_fail(next_base, last);
                  if(query_match_aux(first, last, m, e, flags | match_not_null, pd, &restart))
                  {
                     m.set_line(clines, last_line);
                     ++cmatches;
                     if(foo(m) == false)
                        return cmatches;
                  }
                  else
                  {
                     need_init = false;
                     for(unsigned int i = 0; (restart != first) && (i < e.leading_length()); ++i, --restart);
                     if(restart != last)
                        ++restart;
                     __skip_and_inc(clines, last_line, first, restart);
                  }
               }
               if(need_init)
               {
                 __skip_and_inc(clines, last_line, first, m[0].second);
                  next_base = m[0].second;
                  pd.temp_match.init_fail(next_base, last);
                  m.init_fail(next_base, last);
               }
               continue;
            }
            else
            {
               for(unsigned int i = 0; (restart != first) && (i < e.leading_length()); ++i, --restart);
               if(restart != last)
                  ++restart;
               __skip_and_inc(clines, last_line, first, restart);
            }
         }
         else
            __inc_one(clines, last_line, first);
      }
   }
   break;
   case regbase::restart_word:
   {
      // do search optimised for word starts:
      while(first != last)
      {
         --first;
         if(*first == '\n')
            --clines;
         // skip the word characters:
         while((first != last) && traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst)))
            ++first;
         // now skip the white space:
         while((first != last) && (traits::is_class(*first, char_class_word MAYBE_PASS_LOCALE(locale_inst)) == false))
            __inc_one(clines, last_line, first);
         if(first == last)
            break;

         if( reg_expression<charT, traits, A>::can_start(*first, __map, (unsigned char)mask_any, width_type()) )
         {
            if(query_match_aux(first, last, m, e, flags, pd, &restart))
            {
               m.set_line(clines, last_line);
               ++cmatches;
               if(foo(m) == false)
                  return cmatches;
               // update to end of what matched
               // trying to match again with match_not_null set if this
               // is a null match...
               need_init = true;
               if(first == m[0].second)
               {
                  next_base = m[0].second;
                  pd.temp_match.init_fail(next_base, last);
                  m.init_fail(next_base, last);
                  if(query_match_aux(first, last, m, e, flags | match_not_null, pd, &restart))
                  {
                     m.set_line(clines, last_line);
                     ++cmatches;
                     if(foo(m) == false)
                        return cmatches;
                  }
                  else
                  {
                     need_init = false;
                     for(unsigned int i = 0; (restart != first) && (i < e.leading_length()); ++i, --restart);
                     if(restart != last)
                        ++restart;
                     __skip_and_inc(clines, last_line, first, restart);
                  }
               }
               if(need_init)
               {
                  __skip_and_inc(clines, last_line, first, m[0].second);
                  next_base = m[0].second;
                  pd.temp_match.init_fail(next_base, last);
                  m.init_fail(next_base, last);
               }
            }
            else
            {
               for(unsigned int i = 0; (restart != first) && (i < e.leading_length()); ++i, --restart);
               if(restart != last)
                  ++restart;
               __skip_and_inc(clines, last_line, first, restart);
            }
         }
         else
            __inc_one(clines, last_line, first);
      }
   }
   break;
   case regbase::restart_line:
   {
      // do search optimised for line starts:
      while(first != last)
      {
         // find first charcter after a line break:
         --first;
         if(*first == '\n')
            --clines;
         while((first != last) && (*first != '\n'))
            ++first;
         if(first == last)
            break;
         ++first;
         if(first == last)
            break;

         ++clines;
         last_line = first;

         if( reg_expression<charT, traits, A>::can_start(*first, __map, (unsigned char)mask_any, width_type()) )
         {
            if(query_match_aux(first, last, m, e, flags, pd, &restart))
            {
               m.set_line(clines, last_line);
               ++cmatches;
               if(foo(m) == false)
                  return cmatches;
               // update to end of what matched
               // trying to match again with match_not_null set if this
               // is a null match...
               need_init = true;
               if(first == m[0].second)
               {
                  next_base = m[0].second;
                  pd.temp_match.init_fail(next_base, last);
                  m.init_fail(next_base, last);
                  if(query_match_aux(first, last, m, e, flags | match_not_null, pd, &restart))
                  {
                     m.set_line(clines, last_line);
                     ++cmatches;
                     if(foo(m) == false)
                        return cmatches;
                  }
                  else
                  {
                     need_init = false;
                     for(unsigned int i = 0; (restart != first) && (i < e.leading_length()); ++i, --restart);
                     if(restart != last)
                        ++restart;
                     __skip_and_inc(clines, last_line, first, restart);
                  }
               }
               if(need_init)
               {
                  __skip_and_inc(clines, last_line, first, m[0].second);
                  next_base = m[0].second;
                  pd.temp_match.init_fail(next_base, last);
                  m.init_fail(next_base, last);
               }
            }
            else
            {
               for(unsigned int i = 0; (restart != first) && (i < e.leading_length()); ++i, --restart);
               if(restart != last)
                  ++restart;
               __skip_and_inc(clines, last_line, first, restart);
            }
         }
         else
            __inc_one(clines, last_line, first);
      }
   }
   break;
   case regbase::restart_continue:
   {
      while(first != last)
      {
         if( reg_expression<charT, traits, A>::can_start(*first, __map, (unsigned char)mask_any, width_type()) )
         {
            if(query_match_aux(first, last, m, e, flags, pd, &restart))
            {
               m.set_line(clines, last_line);
               ++cmatches;
               if(foo(m) == false)
                  return cmatches;
               // update to end of what matched
               // trying to match again with match_not_null set if this
               // is a null match...
               if(first == m[0].second)
               {
                  next_base = m[0].second;
                  pd.temp_match.init_fail(next_base, last);
                  m.init_fail(next_base, last);
                  if(query_match_aux(first, last, m, e, flags | match_not_null, pd, &restart))
                  {
                     m.set_line(clines, last_line);
                     ++cmatches;
                     if(foo(m) == false)
                        return cmatches;
                  }
                  else
                     return cmatches;  // can't continue from null match
               }
               __skip_and_inc(clines, last_line, first, m[0].second);
               next_base = m[0].second;
               pd.temp_match.init_fail(next_base, last);
               m.init_fail(next_base, last);
               continue;
            }
         }
         return cmatches;
      }
   }
   break;
   }


   // finally check trailing null string:
   if(e.first()->can_be_null)
   {
      if(query_match_aux(first, last, m, e, flags, pd, &restart))
      {
         m.set_line(clines, last_line);
         ++cmatches;
         if(foo(m) == false)
            return cmatches;
      }
   }

   return cmatches;
}
#if defined(JM_NO_TEMPLATE_SWITCH_MERGE) && !defined(JM_NO_NAMESPACES)
} // namespace
#endif


template <class iterator, class Allocator, class charT, class traits, class Allocator2>
bool reg_search(iterator first, iterator last, reg_match<iterator, Allocator>& m, const reg_expression<charT, traits, Allocator2>& e, unsigned flags = match_default)
{
   if(e.flags() & regbase::failbit)
      return false;

   typedef typename traits::size_type traits_size_type;
   typedef typename traits::uchar_type traits_uchar_type;

   // prepare m for failure:
   if((flags & match_init) == 0)
   {
      m.set_size(e.mark_count(), first, last);
   }

   flags |= match_init;
   return reg_grep2(grep_search_predicate<iterator, Allocator>(&m), first, last, e, flags, m.allocator());
}

//
// reg_search convenience interfaces:
#ifndef JM_NO_PARTIAL_FUNC_SPEC
//
// this isn't really a partial specialisation, but template function
// overloading - if the compiler doesn't support partial specialisation
// then it really won't support this either:
template <class charT, class Allocator, class traits, class Allocator2>
inline bool reg_search(const charT* str, 
                        reg_match<const charT*, Allocator>& m, 
                        const reg_expression<charT, traits, Allocator2>& e, 
                        unsigned flags = match_default)
{
   return reg_search(str, str + traits::length(str), m, e, flags);
}

#ifndef JM_NO_STRING_H
template <class ST, class SA, class Allocator, class charT, class traits, class Allocator2>
inline bool reg_search(const __JM_STD::basic_string<charT, ST, SA>& s, 
                 reg_match<typename __JM_STD::basic_string<charT, ST, SA>::const_iterator, Allocator>& m, 
                 const reg_expression<charT, traits, Allocator2>& e, 
                 unsigned flags = match_default)
{
   return reg_search(s.begin(), s.end(), m, e, flags);
}
#endif
#else  // partial specialisation
inline bool reg_search(const char* str, 
                        cmatch& m, 
                        const regex& e, 
                        unsigned flags = match_default)
{
   return reg_search(str, str + regex::traits_type::length(str), m, e, flags);
}
#ifndef JM_NO_WCSTRING
inline bool reg_search(const wchar_t* str, 
                        wcmatch& m, 
                        const wregex& e, 
                        unsigned flags = match_default)
{
   return reg_search(str, str + wregex::traits_type::length(str), m, e, flags);
}
#endif
#ifndef JM_NO_STRING_H
inline bool reg_search(const __JM_STD::string& s, 
                        reg_match<__JM_STD::string::const_iterator, regex::alloc_type>& m, 
                        const regex& e, 
                        unsigned flags = match_default)
{
   return reg_search(s.begin(), s.end(), m, e, flags);
}
#if !defined(JM_NO_STRING_DEF_ARGS) && !defined(JM_NO_WCSTRING)
inline bool reg_search(const __JM_STD::basic_string<wchar_t>& s, 
                        reg_match<__JM_STD::basic_string<wchar_t>::const_iterator, wregex::alloc_type>& m, 
                        const wregex& e, 
                        unsigned flags = match_default)
{
   return reg_search(s.begin(), s.end(), m, e, flags);
}
#endif

#endif

#endif


//
// reg_grep:
// find all non-overlapping matches within the sequence first last:
//
template <class Predicate, class iterator, class charT, class traits, class Allocator>
inline unsigned int reg_grep(Predicate foo, iterator first, iterator last, const reg_expression<charT, traits, Allocator>& e, unsigned flags = match_default)
{
   return reg_grep2(foo, first, last, e, flags, e.allocator());
}

//
// reg_grep convenience interfaces:
#ifndef JM_NO_PARTIAL_FUNC_SPEC
//
// this isn't really a partial specialisation, but template function
// overloading - if the compiler doesn't support partial specialisation
// then it really won't support this either:
template <class Predicate, class charT, class Allocator, class traits>
inline bool reg_grep(Predicate foo, const charT* str, 
                        const reg_expression<charT, traits, Allocator>& e, 
                        unsigned flags = match_default)
{
   return reg_grep(foo, str, str + traits::length(str), e, flags);
}

#ifndef JM_NO_STRING_H
template <class Predicate, class ST, class SA, class Allocator, class charT, class traits>
inline bool reg_grep(Predicate foo, const __JM_STD::basic_string<charT, ST, SA>& s, 
                 const reg_expression<charT, traits, Allocator>& e, 
                 unsigned flags = match_default)
{
   return reg_grep(foo, s.begin(), s.end(), e, flags);
}
#endif
#else  // partial specialisation
inline bool reg_grep(bool (*foo)(const cmatch&), const char* str, 
                        const regex& e, 
                        unsigned flags = match_default)
{
   return reg_grep(foo, str, str + regex::traits_type::length(str), e, flags);
}
#ifndef JM_NO_WCSTRING
inline bool reg_grep(bool (*foo)(const wcmatch&), const wchar_t* str, 
                        const wregex& e, 
                        unsigned flags = match_default)
{
   return reg_grep(foo, str, str + wregex::traits_type::length(str), e, flags);
}
#endif
#ifndef JM_NO_STRING_H
inline bool reg_grep(bool (*foo)(const reg_match<__JM_STD::string::const_iterator, regex::alloc_type>&), const __JM_STD::string& s, 
                        const regex& e, 
                        unsigned flags = match_default)
{
   return reg_grep(foo, s.begin(), s.end(), e, flags);
}
#if !defined(JM_NO_STRING_DEF_ARGS) && !defined(JM_NO_WCSTRING)
inline bool reg_grep(bool (*foo)(const reg_match<__JM_STD::basic_string<wchar_t>::const_iterator, wregex::alloc_type>&), 
                     const __JM_STD::basic_string<wchar_t>& s, 
                        const wregex& e, 
                        unsigned flags = match_default)
{
   return reg_grep(foo, s.begin(), s.end(), e, flags);
}
#endif

#endif

#endif


//
// finally for compatablity with version 1.x of the library
// we need a form of reg_grep that takes an output iterator
// as its first argument:
//

//
// struct grep_match:
// stores what matched during a reg_grep,
// the output iterator type passed to reg_grep must have an
// operator*() that returns a type with an
// operator=(const grep_match<iterator, Allocator>&);
//
template <class iterator, class Allocator>
struct grep_match
{
   unsigned int line;
   iterator line_start;
   reg_match<iterator, Allocator> what;

   grep_match(Allocator a = Allocator()) : what(a) {}

   grep_match(unsigned int l, iterator p1, const reg_match<iterator, Allocator>& m)
      : what(m) { line = l; line_start = p1; }

   bool operator == (const grep_match& )
   { return false; }

   bool operator < (const grep_match&)
   { return false; }
};

template <class O, class I, class A>
struct grep_adaptor
{
   O oi;
   reg_match<I, A> m;
   grep_adaptor(O i, A a) : m(a), oi(i) {}
   bool operator()(const reg_match_base<I, A>& w)
   {
      m.what = w;
      m.line = w.line();
      m.line_start = w.line_start();
      *oi = m;
      ++oi;
      return true;
   }
};

template <class Out, class iterator, class charT, class traits, class Allocator>
inline unsigned int reg_grep_old(Out oi, iterator first, iterator last, const reg_expression<charT, traits, Allocator>& e, unsigned flags = match_default)
{
   return reg_grep2(grep_adaptor<Out, iterator, Allocator>(oi, e.allocator()), first, last, e, flags, e.allocator());
}



JM_END_NAMESPACE   // namespace regex

#endif   // __REGMATCH_H







