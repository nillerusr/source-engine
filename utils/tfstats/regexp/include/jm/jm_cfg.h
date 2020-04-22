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
  *   FILE     jm_cfg.h
  *   VERSION  2.12
  */

#ifndef JM_CFG_H
#define JM_CFG_H

/************************************************************************

The purpose of this header is to provide compiler and STL configuration
options.  Options fall into three categaries (namespaces, compiler and STL),
throughout, the defaults assume that the compiler and STL are fully C++ standard
compliant, features that are not supported on your system may be selectively
turned off by defining the appropriate macros.  Borland C++, Borland C++ Builder,
and Microsoft Visual C++ should be auto-recognised and configured. The HP aCC and 
SunPro C++ compiler should also be supported - but run configure for best results.
The SGI, HP, Microsoft and Rogue Wave STL's should be auto-recognised and configured.
Do not change this file unless you really really have to, add options to
<jm_opt.h> instead. See <jm_opt.h> for a full list of macros and their usage.

************************************************************************/

#include <jm/jm_opt.h>
#include <stdlib.h>
#include <stddef.h>

/* this will increase in future versions: */
#define JM_VERSION 212

#ifndef JM_AUTO_CONFIGURE
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
   #define JM_PLATFORM_W32
#endif

#ifdef __BORLANDC__

   #if __BORLANDC__ < 0x500
      #define JM_NO_NAMESPACES
      #define JM_NO_BOOL
      #define JM_NO_MUTABLE
   #endif

   #if __BORLANDC__ < 0x520
      #define JM_NO_WCSTRING
      #define JM_NO_INT64
      // Early versions of Borlands namespace code can't cope with iterators
      // that are in different namespaces from STL code.
      #define __JM std
      #define JM_NO_NOT_EQUAL
   #endif

   #if __BORLANDC__ < 0x530
      #define JM_NO_WCTYPE_H
      #define JM_NO_WCHAR_H
      #define JM_OLD_IOSTREAM
      #define __JM_STDC
      #define JM_NO_TRICKY_DEFAULT_PARAM
      #define JM_NO_EXCEPTION_H
      #ifndef __WIN32__
         #define JM_NO_WCSTRING
      #endif
      #define JM_NO_LOCALE_H
      #define JM_NO_TEMPLATE_RETURNS
      #define JM_TEMPLATE_SPECIALISE
   #endif

   #if __BORLANDC__ < 0x540
      #define JM_NO_MEMBER_TEMPLATES
      // inline contructors exhibit strange behaviour
      // under Builder 3 and C++ 5.x when throwing exceptions
      #define INLINE_EXCEPTION_BUG
      #define JM_NESTED_TEMPLATE_DECL
      #define JM_NO_PARTIAL_FUNC_SPEC
      #define JM_NO_STRING_DEF_ARGS
      #define JM_NO_TYPEINFO    // bad_cast etc not in namespace std.
   #endif
   //
   // Builder 4 seems to have broken template friend support:
   #define JM_NO_TEMPLATE_FRIEND

   #ifndef _CPPUNWIND
      #define JM_NO_EXCEPTIONS
   #endif

   #ifdef _Windows
      #define JM_PLATFORM_WINDOWS
   #else
      #define JM_PLATFORM_DOS
   #endif

   #ifndef __WIN32__
      #define RE_CALL
      #define RE_CCALL
   #else
      #define RE_CALL __fastcall
      #define RE_CCALL __stdcall
   #endif

   #define JM_INT64t __int64
   #define JM_IMM64(val) val##i64
   #define JM_NO_CAT

   #ifdef __MT__
      #define JM_THREADS
   #endif

   //
   // import export options:
   #ifdef _RTLDLL
      #ifdef RE_BUILD_DLL
         #define JM_IX_DECL __declspec( dllexport )
      #else
         #define JM_IX_DECL __declspec( dllimport ) 
      #endif
   #endif
   #include <jm/re_lib.h>
#endif

#ifdef _MSC_VER
   #define RE_CALL __fastcall
   #define RE_CCALL __stdcall

   #if _MSC_VER < 1100
      #define JM_NO_NAMESPACES
      #define JM_NO_DEFAULT_PARAM
      #define JM_NO_BOOL
      #define JM_NO_MUTABLE
      #define JM_NO_WCSTRING
      #define JM_NO_LOCALE_H
      #define JM_NO_TEMPLATE_RETURNS
      #define JM_NO_INT64
   #endif

   #if _MSC_VER < 1200
      #define JM_TEMPLATE_SPECIALISE
      #define JM_NESTED_TEMPLATE_DECL
   #endif

   #ifndef _CPPUNWIND
      #define JM_NO_EXCEPTIONS
   #endif

   #define __JM_STDC
   #define JM_PLATFORM_WINDOWS
   //
   // no support for nested template classes yet....
   // although this part of VC6 is badly documented
   #define JM_NO_MEMBER_TEMPLATES
   #define JM_INT64t __int64
   #define JM_IMM64(val) val##i64
   #define JM_NO_CAT
   #define JM_NO_PARTIAL_FUNC_SPEC
   #define JM_NO_TEMPLATE_FRIEND

   #ifdef _MT
      #define JM_THREADS
   #endif

#pragma warning(disable: 4786)
#pragma warning(disable: 4800)
#pragma warning(disable: 4200)

   //
   // import export options:
   #ifdef _DLL
      #ifdef RE_BUILD_DLL
         #define JM_IX_DECL __declspec( dllexport )
      #else
         #define JM_IX_DECL __declspec( dllimport ) 
      #endif
   #endif
   #include <jm/re_lib.h>
#endif

#ifdef __GNUC__
   #if (__GNUC__ < 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ < 91))
      #define JM_NO_NAMESPACES
      #define JM_NO_MUTABLE
      #define JM_NO_MEMBER_TEMPLATES
      #define JM_NO_PARTIAL_FUNC_SPEC
      #define JM_NO_TEMPLATE_FRIEND
   #endif
   #ifndef __STL_USE_NAMESPACES
      #define JM_NO_EXCEPTION_H
   #endif
   #define JM_INT64t long long
   #define JM_IMM64(val) val##LL

   #ifdef _WIN32
      #define JM_PLATFORM_WINDOWS
      #define JM_NO_WCTYPE_H
      //#define JM_NO_TEMPLATE_SWITCH_MERGE
   #endif
   #define JM_NO_CAT
   #define OLD_IOSTREAM
   #define JM_NESTED_TEMPLATE_DECL
   #define JM_NO_TEMPLATE_TYPENAME

#endif

#ifdef __SUNPRO_CC
   #if (__SUNPRO_CC < 0x500)
      #define JM_NO_NAMESPACES
      #define JM_NO_MUTABLE
      #define JM_NO_MEMBER_TEMPLATES
      #define OLD_IOSTREAM
   #endif
   #ifndef __STL_USE_NAMESPACES
      #define JM_NO_EXCEPTION_H
   #endif
   #define JM_INT64t long long
   #define JM_IMM64(val) val##LL
   #define JM_NESTED_TEMPLATE_DECL
   #define JM_NO_TEMPLATE_TYPENAME
   #define JM_NO_SWPRINTF
   #define JM_NO_TEMPLATE_FRIEND
#endif

#ifdef __HP_aCC
   // putative HP aCC support, run configure for
   // support tailored to your system....
   #define JM_NO_NAMESPACES
   #define JM_NO_MUTABLE
   #define JM_NO_MEMBER_TEMPLATES
   #define OLD_IOSTREAM
   #ifndef __STL_USE_NAMESPACES
      #define JM_NO_EXCEPTION_H
   #endif
   #define JM_INT64t long long
   #define JM_IMM64(val) val##LL
   #define JM_NESTED_TEMPLATE_DECL
   #define JM_NO_TEMPLATE_TYPENAME
   #define JM_NO_TEMPLATE_FRIEND
#endif



#endif  // JM_AUTO_CONFIGURE

#ifndef JM_NO_WCSTRING
#ifndef JM_NO_WCTYPE_H
#include <wctype.h>
#endif
#ifndef JM_NO_WCHAR_H
#include <wchar.h>
#endif
#endif

#ifdef JM_NO_NAMESPACES
#define JM_MAYBE_ACCESS_SPEC ::
#else
#define JM_MAYBE_ACCESS_SPEC __JM::
#endif

#if !defined(JM_INT64t) || !defined(JM_IMM64)
#define JM_NO_INT64
#endif

#ifndef JM_INT32
typedef unsigned int jm_uintfast32_t;
#else
typedef JM_INT32 jm_uintfast32_t;
#endif

#ifndef JM_TEMPLATE_SPECIALISE
#define JM_TEMPLATE_SPECIALISE template <>
#endif

#ifndef JM_NESTED_TEMPLATE_DECL
#define JM_NESTED_TEMPLATE_DECL template
#endif

#ifndef JM_IX_DECL
#define JM_IX_DECL
#endif

#ifndef MB_CUR_MAX
// yuk!
// better make a conservative guess!
#define MB_CUR_MAX 10
#endif


/* everything else is C++: */

#ifdef __cplusplus

/* define macro's to make default parameter declaration easier: */

#ifdef JM_NO_DEFAULT_PARAM
   #define JM_DEFAULT_PARAM(x)
   #define JM_TRICKY_DEFAULT_PARAM(x)
#elif defined(JM_NO_TRICKY_DEFAULT_PARAM)
   #define JM_DEFAULT_PARAM(x) = x
   #define JM_TRICKY_DEFAULT_PARAM(x)
#else
   #define JM_DEFAULT_PARAM(x) = x
   #define JM_TRICKY_DEFAULT_PARAM(x) = x
#endif

/* STL configuration goes here: */

#ifndef JM_AUTO_CONFIGURE
#ifdef JM_NO_STL
   #define JM_NO_EXCEPTION_H
   #define JM_NO_ITERATOR_H
   #define JM_NO_MEMORY_H
   #define JM_NO_LOCALE_H
   #define JM_NO_STRING_H
#endif

#ifndef JM_NO_EXCEPTION_H
   #include <exception>
#endif

#ifndef JM_NO_ITERATOR_H
   #include <iterator>

   #if defined(__SGI_STL_INTERNAL_ITERATOR_H) || defined(__SGI_STL_ITERATOR_H)
      #define JM_NO_LOCALE_H
      #define OLD_IOSTREAM

      /* we are using SGI's STL
       some of these (__JM_STDC)
       may be guesswork: */
      #if !defined(__STL_MEMBER_TEMPLATE_CLASSES) ||  !defined(__STL_MEMBER_TEMPLATES)
         #define JM_NO_MEMBER_TEMPLATES
      #endif

      #if !defined( __JM_STD)
         #if defined (__STL_USE_NAMESPACES)
            #define __JM_STD __STD
         #else
            #define __JM_STD
         #endif
      #endif
      #ifndef __JM_STDC
         #define __JM_STDC
      #endif
      #ifdef __STL_NO_BOOL
         #define JM_NO_BOOL
      #endif
      #ifdef __STL_LIMITED_DEFAULT_TEMPLATES
         #define JM_NO_TRICKY_DEFAULT_PARAM
         #define JM_NO_STRING_DEF_ARGS  
      #endif
      #ifndef __STL_USE_EXCEPTIONS
         #define JM_NO_EXCEPTIONS
      #endif

      #include <algo.h>
      #include <alloc.h>

      #define JM_ALGO_INCLUDED

      #define JM_DISTANCE(i, j, n) __JM_STD::distance(i, j, n)
      #define JM_OUTPUT_ITERATOR(T, D) __JM_STD::output_iterator
      #define JM_INPUT_ITERATOR(T, D) __JM_STD::input_iterator<T, D>
      #define JM_FWD_ITERATOR(T, D) __JM_STD::forward_iterator<T, D>
      #define JM_BIDI_ITERATOR(T, D) __JM_STD::bidirectional_iterator<T, D>
      #define JM_RA_ITERATOR(T, D) __JM_STD::random_access_iterator<T, D>

      #ifdef __STL_USE_STD_ALLOCATORS

         /* new style allocator's with nested template classes */

         #define REBIND_INSTANCE(x, y, inst) y::JM_NESTED_TEMPLATE_DECL rebind<x>::other(inst)
         #define REBIND_TYPE(x, y) y::JM_NESTED_TEMPLATE_DECL rebind<x>::other
         #define JM_DEF_ALLOC_PARAM(x) JM_TRICKY_DEFAULT_PARAM( __JM_STD::allocator<x> )
         #define JM_DEF_ALLOC(x) __JM_STD::allocator<x>

      #else  /* __STL_USE_STD_ALLOCATORS */

         /* old style byte allocator's, no nested templates */
         #define JM_OLD_ALLOCATORS
         #define REBIND_INSTANCE(x, y, inst) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>(inst)
         #define REBIND_TYPE(x, y) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>
         #define JM_DEF_ALLOC_PARAM(x) JM_DEFAULT_PARAM( __JM_STD::alloc )
         #define JM_DEF_ALLOC(x) __JM_STD::alloc 
         #define JM_NEED_BINDER

      #endif /* __STL_USE_STD_ALLOCATORS */

      #define JM_STL_DONE
      #define JM_NO_NOT_EQUAL

   #elif defined(__STD_ITERATOR__)

      /* Rogue Wave STL */

      #if defined(RWSTD_NO_MEMBER_TEMPLATES) || defined(RWSTD_NO_MEM_CLASS_TEMPLATES)
         #define JM_NO_MEMBER_TEMPLATES
      #endif
      #ifdef _RWSTD_NO_TEMPLATE_ON_RETURN_TYPE
         #define JM_NO_TEMPLATE_RETURNS
      #endif

      #ifdef _RWSTD_NO_NAMESPACE
         #define __JM_STD
         #define __JM_STDC
      #else
         #define __JM_STD std
      #endif

      #ifdef RWSTD_NO_EXCEPTIONS
         #define JM_NO_EXCEPTIONS
      #endif

      #ifdef RWSTD_NO_MUTABLE
         #define JM_NO_MUTABLE
      #endif

      #ifdef RWSTD_NO_DEFAULT_TEMPLATES
         #define JM_NO_DEFAULT_PARAM
         #define JM_NO_TRICKY_DEFAULT_PARAM
         #define JM_NO_STRING_DEF_ARGS  
      #endif

      #ifdef _RWSTD_NO_COMPLEX_DEFAULT_TEMPLATES
         #define JM_NO_TRICKY_DEFAULT_PARAM
         #define JM_NO_STRING_DEF_ARGS  
      #endif

      #ifdef RWSTD_NO_BOOL
         #define JM_NO_BOOL
      #endif

      #if _RWSTD_VER > 0x020000
         #ifdef _RWSTD_NO_CLASS_PARTIAL_SPEC
          #define JM_DISTANCE(i, j, n) __JM_STD::distance(i, j, n)
         #else 
          #define JM_DISTANCE(i, j, n) (n = __JM_STD::distance(i, j))
         #endif
         #define JM_OUTPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::output_iterator_tag, T, D, T*, T&>
         #define JM_INPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::input_iterator_tag, T, D, T*, T&>
         #define JM_FWD_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::forward_iterator_tag, T, D, T*, T&>
         #define JM_BIDI_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::bidirectional_iterator_tag, T, D, T*, T&>
         #define JM_RA_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::random_access_iterator_tag, T, D, T*, T&>
      #else 
         #define JM_DISTANCE(i, j, n) __JM_STD::distance(i, j, n)
         #define JM_OUTPUT_ITERATOR(T, D) __JM_STD::output_iterator
         #if _RWSTD_VER >= 0x0200
            #define JM_INPUT_ITERATOR(T, D) __JM_STD::input_iterator<T>
         #else
            #define JM_INPUT_ITERATOR(T, D) __JM_STD::input_iterator<T, D>
         #endif
         #define JM_FWD_ITERATOR(T, D) __JM_STD::forward_iterator<T, D>
         #define JM_BIDI_ITERATOR(T, D) __JM_STD::bidirectional_iterator<T, D>
         #define JM_RA_ITERATOR(T, D) __JM_STD::random_access_iterator<T, D>
      #endif

      #include <memory>

      #ifdef _RWSTD_ALLOCATOR

         /* new style allocator */

         #define REBIND_INSTANCE(x, y, inst) y::JM_NESTED_TEMPLATE_DECL rebind<x>::other(inst)
         #define REBIND_TYPE(x, y) y::JM_NESTED_TEMPLATE_DECL rebind<x>::other
         #define JM_DEF_ALLOC_PARAM(x) JM_TRICKY_DEFAULT_PARAM( __JM_STD::allocator<x> )
         #define JM_DEF_ALLOC(x)  __JM_STD::allocator<x>

      #else
         /*
         // old style allocator
         // this varies a great deal between versions, and there is no way
         // that I can tell of differentiating between them, so use our
         // own default allocator...
         */
         #define JM_OLD_ALLOCATORS
         #define REBIND_INSTANCE(x, y, inst) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>(inst)
         #define REBIND_TYPE(x, y) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>
         #define JM_DEF_ALLOC_PARAM(x) JM_DEFAULT_PARAM( jm_def_alloc )
         #define JM_DEF_ALLOC(x) jm_def_alloc

         #define JM_NEED_BINDER
         #define JM_NEED_ALLOC

      #endif

      #define JM_STL_DONE
      #define JM_NO_OI_ASSIGN

   #elif defined (ITERATOR_H)

      /* HP STL */

      #define __JM_STD
      #define __JM_STDC
      #define JM_NO_LOCALE_H

      #include <algo.h>
      #define JM_ALGO_INCLUDED

      #define JM_DISTANCE(i, j, n) __JM_STD::distance(i, j, n)
      #define JM_OUTPUT_ITERATOR(T, D) __JM_STD::output_iterator
      #define JM_INPUT_ITERATOR(T, D) __JM_STD::input_iterator<T, D>
      #define JM_FWD_ITERATOR(T, D) __JM_STD::forward_iterator<T, D>
      #define JM_BIDI_ITERATOR(T, D) __JM_STD::bidirectional_iterator<T, D>
      #define JM_RA_ITERATOR(T, D) __JM_STD::random_access_iterator<T, D>

      /* old style allocator */
      #define JM_OLD_ALLOCATORS
      #define REBIND_INSTANCE(x, y, inst) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>(inst)
      #define REBIND_TYPE(x, y) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>
      #define JM_DEF_ALLOC_PARAM(x) JM_DEFAULT_PARAM( jm_def_alloc )
      #define JM_DEF_ALLOC(x) jm_def_alloc

      #define JM_NEED_BINDER
      #define JM_NEED_ALLOC
      #define JM_NO_NOT_EQUAL

      #define JM_STL_DONE

   #elif defined (_MSC_VER)

      /* assume we're using MS's own STL (VC++ 5/6) */
      #define __JM_STD std
      #define __JM_STDC
      #define JM_NO_OI_ASSIGN

      #define JM_DISTANCE(i, j, n) n = __JM_STD::distance(i, j)
      #define JM_OUTPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::output_iterator_tag, T, D>
      #define JM_INPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::input_iterator_tag, T, D>
      #define JM_FWD_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::forward_iterator_tag, T, D>
      #define JM_BIDI_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::bidirectional_iterator_tag, T, D>
      #define JM_RA_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::random_access_iterator_tag, T, D>

      /* MS's allocators are rather ambiguous about their properties
      at least as far as MSDN is concerned, so play safe: */
      #define JM_OLD_ALLOCATORS
      #define REBIND_INSTANCE(x, y, inst) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>(inst)
      #define REBIND_TYPE(x, y) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>
      #define JM_DEF_ALLOC_PARAM(x) JM_DEFAULT_PARAM( jm_def_alloc )
      #define JM_DEF_ALLOC(x) jm_def_alloc

      #define JM_NEED_BINDER
      #define JM_NEED_ALLOC

      #define JM_STL_DONE

      #define JM_USE_FACET(l, type) __JM_STD::use_facet(l, (type*)0, true)
      #define JM_HAS_FACET(l, type) __JM_STD::has_facet(l, (type*)0)



   #else

      /* unknown STL version
       try the defaults: */

      #define JM_DISTANCE(i, j, n) __JM_STD::distance(i, j, n)
      /* these may be suspect for older libraries */
      #define JM_OUTPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::output_iterator_tag, T, D, T*, T&>
      #define JM_INPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::input_iterator_tag, T, D, T*, T&>
      #define JM_FWD_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::forward_iterator_tag, T, D, T*, T&>
      #define JM_BIDI_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::bidirectional_iterator_tag, T, D, T*, T&>
      #define JM_RA_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::random_access_iterator_tag, T, D, T*, T&>

   #endif  /* <iterator> config */

#else   /* no <iterator> at all */

   #define JM_DISTANCE(i, j, n) (n = j - i)
   #define JM_OUTPUT_ITERATOR(T, D) dummy_iterator_base<T>
   #define JM_INPUT_ITERATOR(T, D) dummy_iterator_base<T>
   #define JM_FWD_ITERATOR(T, D) dummy_iterator_base<T>
   #define JM_BIDI_ITERATOR(T, D) dummy_iterator_base<T>
   #define JM_RA_ITERATOR(T, D) dummy_iterator_base<T>


#endif

/* now do allocator if not already done */

#ifndef JM_STL_DONE

   #ifdef JM_NO_MEMORY_H

      /* old style allocator */
      
      #define JM_OLD_ALLOCATORS

      #define REBIND_INSTANCE(x, y, inst) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>(inst)
      #define REBIND_TYPE(x, y) JM_MAYBE_ACCESS_SPEC re_alloc_binder<x, y>
      #define JM_DEF_ALLOC_PARAM(x) JM_DEFAULT_PARAM( jm_def_alloc )
      #define JM_DEF_ALLOC(x) jm_def_alloc

      #define JM_NEED_BINDER
      #define JM_NEED_ALLOC

   #else

      /* new style allocator's with nested template classes */

      #define REBIND_INSTANCE(x, y, inst) y::JM_NESTED_TEMPLATE_DECL rebind<x>::other(inst)
      #define REBIND_TYPE(x, y) y::JM_NESTED_TEMPLATE_DECL rebind<x>::other
      #define JM_DEF_ALLOC_PARAM(x) JM_TRICKY_DEFAULT_PARAM( __JM_STD::allocator<x> )
      #define JM_DEF_ALLOC(x) __JM_STD::allocator<x>

   #endif

#endif
#endif // JM_AUTO_CONFIGURE



/* namespace configuration goes here: */
#ifdef JM_NO_NAMESPACES

   #ifdef __JM_STD
      #undef __JM_STD
   #endif

   #ifdef __JM_STDC
      #undef __JM_STDC
   #endif

   #ifdef __JM
      #undef __JM
   #endif

   #define __JM
   #define __JM_STD
   #define __JM_STDC
   #define JM_NAMESPACE(x)
   #define JM_END_NAMESPACE
   #define JM_USING(x)

#else

   #ifndef __JM_STD
      #define __JM_STD std
   #endif

   #ifndef __JM_STDC
      #define __JM_STDC std
   #endif

   #ifndef __JM
      #define __JM jm
   #endif

   #define JM_NAMESPACE(x) namespace x{
   #define JM_END_NAMESPACE };
   #define JM_USING(x) using namespace x;

#endif

/* locale configuration goes here */
#if !defined(JM_NO_LOCALE_H) && defined(RE_LOCALE_CPP)
    #include <locale>
    #define LOCALE_INSTANCE(i) __JM_STD::locale i;
    #define MAYBE_PASS_LOCALE(i) , i
    #ifndef JM_NO_TEMPLATE_RETURNS
      #ifndef JM_USE_FACET
         #define JM_USE_FACET(l, type) __JM_STD::use_facet< type >(l)
      #endif
      #ifndef JM_HAS_FACET
         #define JM_HAS_FACET(l, type) __JM_STD::has_facet< type >(l)
      #endif
    #else
      #ifndef JM_USE_FACET
         #define JM_USE_FACET(l, type) __JM_STD::use_facet(l, (type*)0)
      #endif
      #ifndef JM_HAS_FACET
         #define JM_HAS_FACET(l, type) __JM_STD::has_facet(l, (type*)0)
      #endif
    #endif
#else
    #define LOCALE_INSTANCE(i)
    #define MAYBE_PASS_LOCALE(i)
#endif

/* compiler configuration goes here: */

#ifdef JM_NO_MUTABLE
   #define JM_MUTABLE
#else
   #define JM_MUTABLE mutable
#endif

#if defined( JM_NO_BOOL) && !defined(bool)
   #define bool int
   #define true 1
   #define false 0
#endif

#ifndef RE_CALL
#define RE_CALL
#endif

#ifndef RE_CCALL
#define RE_CCALL
#endif

#ifndef RE_DECL
#define RE_DECL
#endif

#if defined(JM_NO_DEFAULT_PARAM) || defined(JM_NO_TRICKY_DEFAULT_PARAM)
#define JM_NO_STRING_DEF_ARGS  
#endif



/* add our class def's if they are needed: */

JM_NAMESPACE(__JM)

// add our destroy functions:

template <class T>
inline void RE_CALL jm_destroy(T* t)
{
   t->~T();
}

inline void RE_CALL jm_destroy(char* t){}
inline void RE_CALL jm_destroy(short* t){}
inline void RE_CALL jm_destroy(unsigned short* t){}
inline void RE_CALL jm_destroy(int* t){}
inline void RE_CALL jm_destroy(unsigned int* t){}
inline void RE_CALL jm_destroy(long* t){}
inline void RE_CALL jm_destroy(unsigned long* t){}


template <class T>
inline void RE_CALL jm_construct(void* p, const T& t)
{
   new (p) T(t);
}


template<class T, class Allocator>
class re_alloc_binder : public Allocator
{
public:
   typedef T         value_type;
   typedef T*        pointer;
   typedef const T*  const_pointer;
   typedef T&        reference;
   typedef const T&  const_reference;
   typedef size_t    size_type;
   typedef __JM_STDC::ptrdiff_t difference_type;

   re_alloc_binder(const Allocator& i);
   re_alloc_binder(const re_alloc_binder& o) : Allocator(o) {}

   T* RE_CALL allocate(size_t n, size_t /* hint */ = 0)
      { return 0 == n ? 0 : (T*) this->Allocator::allocate(n * sizeof(T)); }
   void RE_CALL deallocate(T *p, size_t n)
             { if (0 != n) this->Allocator::deallocate((char*)p, n * sizeof (T)); }

   pointer RE_CALL address(reference x) const { return &x; }
   const_pointer RE_CALL address(const_reference x) const { return &x; }
   static size_type RE_CALL max_size() { return -1; }
   static void RE_CALL construct(pointer p, const T& val) { jm_construct(p, val); }
   void RE_CALL destroy(pointer p) { jm_destroy(p); }

   const Allocator& RE_CALL instance()const { return *this; }

#ifndef JM_NO_MEMBER_TEMPLATES

   template <class U>
   struct rebind
   {
      typedef re_alloc_binder<U, Allocator> other;
   };
   
   template <class U>
   RE_CALL re_alloc_binder(const re_alloc_binder<U, Allocator>& o) throw()
      : Allocator(o.instance())
   {
   }
#endif
};

template<class T, class Allocator>
inline re_alloc_binder<T, Allocator>::re_alloc_binder(const Allocator &i)
    : Allocator(i)
{}


//
// class jm_def_alloc
// basically a standard allocator that only allocates bytes...
// think of it as allocator<char>, with a non-standard 
// rebind::other typedef.
//
class jm_def_alloc
{
public:
   typedef char         value_type;
   typedef char*        pointer;
   typedef const char*  const_pointer;
   typedef char&        reference;
   typedef const char&  const_reference;
   typedef size_t    size_type;
   typedef __JM_STDC::ptrdiff_t difference_type;

   pointer RE_CALL address(reference x) const { return &x; }
   const_pointer RE_CALL address(const_reference x) const { return &x; }
   static size_type RE_CALL max_size() { return (size_type)-1; }
   static void RE_CALL construct(pointer , const char& ) {  }
   void RE_CALL destroy(pointer ) {  }
   static void * RE_CALL allocate(size_t n, size_t /* hint */ = 0)
   {
      return ::operator new(n);
   }
   static void RE_CALL deallocate(void *p, size_t /*n*/ )
   {
      ::operator delete(p);
   }

#ifndef JM_NO_MEMBER_TEMPLATES
   template <class U>
   struct rebind
   {
      typedef re_alloc_binder<U, jm_def_alloc> other;
   };

   template <class U>
   RE_CALL jm_def_alloc(const re_alloc_binder<U, jm_def_alloc>& ) throw() { }
#endif
   jm_def_alloc(const jm_def_alloc&) {}
   jm_def_alloc() {}
};

template <class T>
struct dummy_iterator_base
{
   typedef T                       value_type;
   typedef __JM_STDC::ptrdiff_t    difference_type;
   typedef T*                      pointer;
   typedef T&                      reference;
   //typedef Category              iterator_category;
};

// we need to absolutely sure that int values are correctly
// translated to bool (true or false) values...
// note that the original HP STL redefines the bool type regardless
// of whether the compiler supports it.... yuk

#if defined(JM_NO_BOOL) || defined(ITERATOR_H) || defined(bool)
#define JM_MAKE_BOOL(x) boolify(x)

template <class I>
inline bool RE_CALL boolify(I val)
{
   return val ? true : false;
}

#else
#define JM_MAKE_BOOL(x) x
#endif

// class auto_array:
//
// encapsulates objects allocated with ::operator new[]()
// interface the same as auto_ptr, but no stream operators
// since we don't know how big the array is.
//
// Usage: auto_array<char> buf = new char[256];
//

template<class X>
class auto_array
{
public: // construct/copy/destroy:

   auto_array(X* p =0)
   { ptr = p; }

   auto_array(const auto_array& ap)
   { ptr = const_cast<auto_array&>(ap).release(); }

   void RE_CALL operator=(const auto_array&);

   ~auto_array();

      //  members:

   X& RE_CALL operator*() const;
   X* RE_CALL operator->() const;
   X* RE_CALL get() const;
   X& RE_CALL operator[](int i);
   X* RE_CALL release();

   // operator not part of the spec:
   RE_CALL operator X*()const
   { return ptr; }

private: //  data:

   X* ptr;
};

template <class X>
inline void RE_CALL auto_array<X>::operator=(const auto_array<X>& ap)
{
   delete[] ptr;
   ptr = const_cast<auto_array<X>&>(ap).release();
}

template <class X>
inline auto_array<X>::~auto_array()
{
   delete[] ptr;
}

template <class X>
inline X& RE_CALL auto_array<X>::operator*() const
{
   return *ptr;
}

template <class X>
inline X* RE_CALL auto_array<X>::operator->() const
{
   return ptr;
}

template <class X>
inline X* RE_CALL auto_array<X>::get() const
{
   return ptr;
}

template <class X>
inline X& RE_CALL auto_array<X>::operator[](int i)
{
   return ptr[i];
}

template <class X>
inline X* RE_CALL auto_array<X>::release()
{
   X* tmp = ptr;
   ptr = NULL;
   return tmp;
}


JM_END_NAMESPACE

#if !defined(INLINE_EXCEPTION_BUG) || defined(JM_NO_TEMPLATE_MERGE)
    #define CONSTRUCTOR_INLINE inline
#else
    #define CONSTRUCTOR_INLINE
#endif

#if defined(JM_PLATFORM_W32) && !defined(RE_LOCALE_W32) && !defined(RE_LOCALE_C) && !defined(RE_LOCALE_CPP) && !defined(JM_NO_W32)
#define RE_LOCALE_W32
#endif

#if !defined(RE_LOCALE_W32) && !defined(RE_LOCALE_C) && !defined(RE_LOCALE_CPP)
#define RE_LOCALE_C
#endif

#if defined(JM_OLD_ALLOCATORS) && defined(JM_NO_TEMPLATE_TYPENAME)
#define JM_MAYBE_TYPENAME
#else
#define JM_MAYBE_TYPENAME typename
#endif

#ifdef RE_LOCALE_W32
#include <windows.h>
#endif


/* now do debugging stuff: */

#ifdef JM_DEBUG

#ifdef OLD_IOSTREAM
#include <iostream.h>
#else
#include <iostream>
using std::cout;
using std::cin;
using std::cerr;
#endif

   #ifndef jm_assert
      #define jm_assert(x) if((x) == 0){ cerr << "Assertion failed: " << #x << " in file " << __FILE__ << "and line " << __LINE__ << endl; exit(-1); }
   #endif
   #ifndef jm_trace
      #define jm_trace(x) cerr << x;
   #endif

   #ifdef __BORLANDC__
      #pragma message "macro __jm_std: " __JM_STD
      #pragma message "macro __jm_stdc: " __JM_STDC
      #pragma message "macro namespace: " JM_NAMESPACE(__JM_STD)
      #pragma message "macro allocator: " JM_DEF_ALLOC_PARAM(wchar_t)
      #pragma message "macro jm_input_iterator: " JM_INPUT_ITERATOR(char, __JM_STDC::ptrdiff_t)
      #pragma message "macro jm_output_iterator: " JM_OUTPUT_ITERATOR(char, __JM_STDC::ptrdiff_t)
      #pragma message "macro jm_fwd_iterator: " JM_FWD_ITERATOR(char, __JM_STDC::ptrdiff_t)
      #pragma message "macro jm_bidi_iterator: " JM_BIDI_ITERATOR(char, __JM_STDC::ptrdiff_t)
      #pragma message "macro jm_ra_iterator: " JM_RA_ITERATOR(char, __JM_STDC::ptrdiff_t)
      #ifdef RE_LOCALE_CPP
         #pragma message "locale support enabled"
      #endif
   #endif

#else

   #define jm_assert(x)
   #define jm_trace(x)

#endif

#endif  /* __cplusplus */


#endif

















