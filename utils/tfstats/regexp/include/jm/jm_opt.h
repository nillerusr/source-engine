//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef JM_OPT_H
#define JM_OPT_H

/* #define JM_AUTO_CONFIGURE */
#ifdef JM_AUTO_CONFIGURE

/* Namespace Options: */

/* JM_NO_NAMESPACES        Define if your compiler does not support namespaces */
/* #define JM_NO_NAMESPACES */

/* __JM                    Defines the namespace used for this library,
                           defaults to "jm", but can be changed by defining
                           __JM on the command line. */
/* #define __JM */

/* __JM_STD               Defines the namespace used by the underlying STL
                          (if any), defaults to "std", can be changed by
                          defining __JM_STD on the command line. */
/* #define __JM_STD */


/* __JM_STDC              Defines the namespace used by the C Library defs.
                          Defaults to "std" as recomended by the latest
                          draft standard, can be redefined by defining
                          __JM_STDC on the command line. */
/* #define __JM_STDC */



/* Compiler options: */

/* JM_NO_EXCEPTIONS           Disables exception handling support. */
/* #define JM_NO_EXCEPTIONS */

/* JM_NO_MUTABLE             Disables use of mutable keyword. */
/* #define JM_NO_MUTABLE */

/* JM_INT32                  The type for 32-bit integers - what C calls intfast32_t */
/* #define JM_INT32 */

/* JM_NO_DEFAULT_PARAM       If templates can not have default parameters. */
/* #define JM_NO_DEFAULT_PARAM */

/* JM_NO_TRICKY_DEFAULT_PARAM If templates can not have derived default parameters. */
/* #define JM_NO_TRICKY_DEFAULT_PARAM */

/* JM_NO_TEMPLATE_TYPENAME   If class scope typedefs of the form:
                             typedef typename X<T> Y;
                      where T is a template parameter to this,
                      do not compile unless the typename is omitted. */
/* #define JM_NO_TEMPLATE_TYPENAME */

/* JM_NO_TEMPLATE_FRIEND     If template friend declarations are not supported */
/* #define JM_NO_TEMPLATE_FRIEND */

/* JM_PLATFORM_WINDOWS       Platform is MS Windows. */
/* #define JM_PLATFORM_WINDOWS */

/* JM_PLATFORM_DOS           Platform if MSDOS. */
/* #define JM_PLATFORM_DOS */

/* JM_PLATFORM_W32           Platform is MS Win32 */
/* #define JM_PLATFORM_W32 */

/* JM_NO_WIN32               Disable Win32 support even when present */
/* #define JM_NO_WIN32 */

/* JM_NO_BOOL                If bool is not a distict type. */
/* #define JM_NO_BOOL */

/* JM_NO_WCHAR_H             If there is no <wchar.h> */
/* #define JM_NO_WCHAR_H */

/* JM_NO_WCTYPE_H            If there is no <wctype.h> */
/* #define JM_NO_WCTYPE_H */

/* JM_NO_WCSTRING            If there are no wcslen and wcsncmp functions available. */
/* #define JM_NO_WCSTRING */

/* JM_NO_SWPRINTF            If there is no swprintf available. */
/* #define JM_NO_SWPRINTF */

/* JM_NO_WSPRINTF            If there is no wsprintf available. */
/* #define JM_NO_WSPRINTF */

/* JM_NO_MEMBER_TEMPLATES    If member function templates or nested template classes are not allowed. */
/* #define JM_NO_MEMBER_TEMPLATES */

/* JM_NO_TEMPLATE_RETURNS    If template functions based on return type are not supported. */
/* #define JM_NO_TEMPLATE_RETURNS */

/* JM_NO_PARTIAL_FUNC_SPEC   If partial template function specialisation is not supported */
/* #define JM_NO_PARTIAL_FUNC_SPEC */

/* JM_NO_INT64               If 64bit integers are not supported. */
/* JM_INT64t                 The type of a 64-bit signed integer if available. */
/* JM_IMM64(val)             Declares a 64-bit immediate value by appending any
                             necessary suffix to val. */
/* JM_INT64_T                0 = NA
                             1 = short
                      2 = int
                      3 = long
                      4 = int64_t
                      5 = long long
                      6 = __int64 */
/* #define JM_INT64_T */

/* JM_NO_CAT                 Define if the compiler does not support POSIX style
                             message categories (catopen catgets catclose). */
/* #define JM_NO_CAT */

/* JM_THREADS                Define if the compiler supports multiple threads in
                             the current translation mode. */
/* #define JM_THREADS */

/* JM_TEMPLATE_SPECIALISE    Defaults to template<> , ie the template specialisation
                             prefix, can be redefined to nothing for older compilers. */
/* #define JM_TEMPLATE_SPECIALISE */

/* JM_NESTED_TEMPLATE_DECL   Defaults to template, the standard prefix when accessing
                             nested template classes, can be redefined to nothing if
                             the compiler does not support this. */
/* #define JM_NESTED_TEMPLATE_DECL */

/* JM_NO_TEMPLATE_INST       If explicit template instantiation with the "template class X<T>"
                             syntax is not supported */
/* #define JM_NO_TEMPLATE_INST */

/* JM_NO_TEMPLATE_MERGE      If template in separate translation units don't merge at link time */
/* #define JM_NO_TEMPLATE_MERGE */

/* JM_NO_TEMPLATE_MERGE_A    If template merging from library archives is not supported */
/* #define JM_NO_TEMPLATE_MERGE_A */

/* JM_NO_TEMPLATE_SWITCH_MERGE If merging of templates containing switch statements is not supported */
/* #define JM_NO_TEMPLATE_SWITCH_MERGE */

/* RE_CALL                   Optionally define a calling convention for C++ functions */
/* #define RE_CALL */

/* RE_CCALL                  Optionally define a calling convention for C functions */
/* #define RE_CCALL */

/* JM_SIZEOF_SHORT           sizeof(short) */
/* #define JM_SIZEOF_SHORT */

/* JM_SIZEOF_INT             sizeof(int) */
/* #define JM_SIZEOF_INT */

/* JM_SIZEOF_LONG            sizeof(long) */
/* #define JM_SIZEOF_LONG */

/* JM_SIZEOF_WCHAR_T         sizeof(wchar_t) */
/* #define JM_SIZEOF_WCHAR_T */


/* STL options: */

/* JM_NO_EXCEPTION_H         Define if you do not a compliant <exception>
                             header file. */
/* #define JM_NO_EXCEPTION_H */

/* JM_NO_ITERATOR_H          Define if you do not have a version of <iterator>. */
/* #define JM_NO_ITERATOR_H */

/* JM_NO_MEMORY_H            Define if <memory> does not fully comply with the
                             latest standard, and is not auto-recognised,
                             that means nested template classes
                             which hardly any compilers support at present. */
/* #define JM_NO_MEMORY_H */

/* JM_NO_LOCALE_H            Define if there is no verion of the standard
                             <locale> header available. */
/* #define JM_NO_LOCALE_H */

/* JM_NO_STL                 Disables the use of any supporting STL code. */
/* #define JM_NO_STL */

/* JM_NO_NOT_EQUAL           Disables the generation of operator!= if this
                             clashes with the STL version. */

/* JM_NO_STRING_H            Define if <string> not available */
/* #define JM_NO_STRING_H */

/* JM_NO_STRING_DEF_ARGS     Define if std::basic_string<charT> not allowed - in
                             other words if the template is missing its required
                      default arguments. */
/* #define JM_NO_STRING_DEF_ARGS */

/* JM_NO_TYPEINFO            Define if <typeinfo> is absent or non-standard */
/* #define JM_NO_TYPEINFO */

/* JM_USE_ALGO               If <algo.h> not <algorithm> is present */
/* #define JM_USE_ALGO */

/* JM_OLD_IOSTREAM           If the new iostreamm classes are not available */
/* #define JM_OLD_IOSTREAM */

/* JM_DISTANCE_T             For std::distance:
                             0 = NA
                      1 = std::distance(i, j, n)
                      2 = n = std::distance(i, j) */
/* #define JM_DISTANCE_T */

/* JM_ITERATOR_T             Defines generic standard iterator type if available, use this as
                             a shortcut to define all the other iterator types.
                             1 = __JM_STD::iterator<__JM_STD::tag_type, T, D, T*, T&>
                      2 = __JM_STD::iterator<__JM_STD::tag_type, T, D> */
/* #define JM_ITERATOR_T */

/* JM_OI_T                   For output iterators:
                             0 = NA
                      1 = __JM_STD::iterator<__JM_STD::output_iterator_tag, T, D, T*, T&>
                      2 = __JM_STD::iterator<__JM_STD::output_iterator_tag, T, D>
                      3 = __JM_STD::output_iterator */
/* #define JM_OI_T */

/* JM_II_T                   For input iterators:
                             0 = NA
                      1 = __JM_STD::iterator<__JM_STD::input_iterator_tag, T, D, T*, T&>
                      2 = __JM_STD::iterator<__JM_STD::input_iterator_tag, T, D>
                      3 = __JM_STD::input_iterator<T, D>
                      4 = __JM_STD::input_iterator<T> */
/* #define JM_II_T */

/* JM_FI_T                   For forward iterators:
                             0 = NA
                      1 = __JM_STD::iterator<__JM_STD::forward_iterator_tag, T, D, T*, T&>
                      2 = __JM_STD::iterator<__JM_STD::forward_iterator_tag, T, D>
                      3 = __JM_STD::forward_iterator<T, D> */
/* #define JM_FI_T */

/* JM_BI_T                   For bidirectional iterators:
                             0 = NA
                      1 = __JM_STD::iterator<__JM_STD::bidirectional_iterator_tag, T, D, T*, T&>
                      2 = __JM_STD::iterator<__JM_STD::bidirectional_iterator_tag, T, D>
                      3 = __JM_STD::bidirectional_iterator<T, D> */
/* #define JM_BI_T */

/* JM_RI_T                   For random access iterators:
                             0 = NA
                             1 = __JM_STD::iterator<__JM_STD::random_access_iterator_tag, T, D, T*, T&>
                      2 = __JM_STD::iterator<__JM_STD::random_access_iterator_tag, T, D>
                      3 = __JM_STD::random_access_iterator<T, D> */
/* #define JM_RI_T */

/* JM_NO_OI_ASSIGN           If output iterators ostream_iterator<>, back_insert_iterator<> and 
                             front_insert_iterator<> do not have assignment operators */
/* #define JM_NO_OI_ASSIGN */


#if JM_INT64_T == 0
#define JM_NO_INT64
#elif JM_INT64_T == 1
#define JM_INT64t short
#define JM_IMM64(val) val
#elif JM_INT64_T == 2
#define JM_INT64t int
#define JM_IMM64(val) val
#elif JM_INT64_T == 3
#define JM_INT64t long
#define JM_IMM64(val) val##L
#elif JM_INT64_T == 4
#define JM_INT64t int64_t
#define JM_IMM64(val) INT64_C(val)
#elif JM_INT64_T == 5
#define JM_INT64t long long
#define JM_IMM64(val) val##LL
#elif JM_INT64_T == 6
#define JM_INT64t __int64
#define JM_IMM64(val) val##i64
#else
syntax error: unknown value for JM_INT64_T
#endif

#if JM_DISTANCE_T == 0
#  define JM_DISTANCE(i, j, n) n = j - i
#elif JM_DISTANCE_T == 1
#  define JM_DISTANCE(i, j, n) n = __JM_STD::distance(i, j)
#elif JM_DISTANCE_T == 2
#  define JM_DISTANCE(i, j, n) (n = 0, __JM_STD::distance(i, j, n))
#else
syntax erorr
#endif

#ifdef JM_ITERATOR_T
#ifndef JM_OI_T
#define JM_OI_T JM_ITERATOR_T
#endif
#ifndef JM_II_T
#define JM_II_T JM_ITERATOR_T
#endif
#ifndef JM_FI_T
#define JM_FI_T JM_ITERATOR_T
#endif
#ifndef JM_BI_T
#define JM_BI_T JM_ITERATOR_T
#endif
#ifndef JM_RI_T
#define JM_RI_T JM_ITERATOR_T
#endif
#endif

#if JM_OI_T == 0
# define JM_OUTPUT_ITERATOR(T, D) dummy_iterator_base<T>
#elif JM_OI_T == 1
# define JM_OUTPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::output_iterator_tag, T, D, T*, T&>
#elif JM_OI_T == 2
# define JM_OUTPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::output_iterator_tag, T, D>
#elif JM_OI_T == 3
# define JM_OUTPUT_ITERATOR(T, D) __JM_STD::output_iterator
#else
syntax error
#endif

#if JM_II_T == 0
# define JM_INPUT_ITERATOR(T, D) dummy_iterator_base<T>
#elif JM_II_T == 1
#define JM_INPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::input_iterator_tag, T, D, T*, T&>
#elif JM_II_T == 2
#define JM_INPUT_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::input_iterator_tag, T, D>
#elif JM_II_T == 3
# define JM_INPUT_ITERATOR(T, D) __JM_STD::input_iterator<T, D>
#elif JM_II_T == 4
# define JM_INPUT_ITERATOR(T, D) __JM_STD::input_iterator<T>
#else
syntax error
#endif

#if JM_FI_T == 0
# define JM_FWD_ITERATOR(T, D) dummy_iterator_base<T>
#elif JM_FI_T == 1
# define JM_FWD_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::forward_iterator_tag, T, D, T*, T&>
#elif JM_FI_T == 2
# define JM_FWD_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::forward_iterator_tag, T, D>
#elif JM_FI_T == 3
# define JM_FWD_ITERATOR(T, D) __JM_STD::forward_iterator<T, D>
#else
syntax error
#endif

#if JM_BI_T == 0
# define JM_BIDI_ITERATOR(T, D) dummy_iterator_base<T>
#elif JM_BI_T == 1
# define JM_BIDI_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::bidirectional_iterator_tag, T, D, T*, T&>
#elif JM_BI_T == 2
# define JM_BIDI_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::bidirectional_iterator_tag, T, D>
#elif JM_BI_T == 3
# define JM_BIDI_ITERATOR(T, D) __JM_STD::bidirectional_iterator<T, D>
#else
syntax error
#endif

#if JM_RI_T == 0
# define JM_RA_ITERATOR(T, D) dummy_iterator_base<T>
#elif JM_RI_T == 1
# define JM_RA_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::random_access_iterator_tag, T, D, T*, T&>
#elif JM_RI_T == 2
# define JM_RA_ITERATOR(T, D) __JM_STD::iterator<__JM_STD::random_access_iterator_tag, T, D>
#elif JM_RI_T == 3
# define JM_RA_ITERATOR(T, D) __JM_STD::random_access_iterator<T, D>
#else
syntax error
#endif


#ifndef JM_NO_EXCEPTION_H
#include <exception>
#endif

#ifndef JM_NO_ITERATOR_H
#include <iterator>
#ifdef JM_USE_ALGO
#include <algo.h>
#else
#include <algorithm>
#endif
#endif

#ifdef JM_NO_MEMORY_H
 #define JM_OLD_ALLOCATORS
 #define REBIND_INSTANCE(x, y, inst) re_alloc_binder<x, y>(inst)
 #define REBIND_TYPE(x, y) re_alloc_binder<x, y>
 #define JM_DEF_ALLOC_PARAM(x) JM_DEFAULT_PARAM( jm_def_alloc )
 #define JM_DEF_ALLOC(x) jm_def_alloc

 #define JM_NEED_BINDER
 #define JM_NEED_ALLOC
#else
#include <memory>
 #define REBIND_INSTANCE(x, y, inst) y::JM_NESTED_TEMPLATE_DECL rebind<x>::other(inst)
 #define REBIND_TYPE(x, y) y::JM_NESTED_TEMPLATE_DECL rebind<x>::other
 #define JM_DEF_ALLOC_PARAM(x) JM_TRICKY_DEFAULT_PARAM( __JM_STD::allocator<x> )
 #define JM_DEF_ALLOC(x) __JM_STD::allocator<x>
#endif


#endif // JM_AUTO_CONFIGURE


#endif /* JM_OPT_H */




