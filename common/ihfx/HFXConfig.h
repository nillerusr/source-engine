#ifndef NOVINT_HFX_CONFIG_H
#define NOVINT_HFX_CONFIG_H
// COMPILER VERSION
#if _MSC_VER
#if _MSC_VER >= 1500
#define HFX_MSVC9
#define HFX_MSVC 9 // Microsoft Visual C++ 2008
 
#elif _MSC_VER >= 1400
#define HFX_MSVC8
#define HFX_MSVC 8 // Microsoft Visual C++ 2005
 
#elif _MSC_VER >= 1300
#define HFX_MSVC7
#define HFX_MSVC 7 // Microsoft Visual C++ 2003
 
#else
#define HFX_MSVC6
#define HFX_MSVC 6 // Microsoft Visual C++ 6
 
#endif
#endif
 
//HFX_PURE_INTERFACE
#if HFX_MSVC >= 7
#define HFX_PURE_INTERFACE __declspec(novtable) 
#else
#define HFX_PURE_INTERFACE
#endif
 
//HFX_ABSTRACT
#if HFX_MSVC >= 8
#define HFX_ABSTRACT abstract
#else
#define HFX_ABSTRACT
#endif
 
#define HFX_EXPLICIT explicit
 
//HFX_INLINE
#if _MSC_VER>=1000
#define HFX_INLINE __forceinline
 
#else
#define HFX_INLINE inline
#endif
 
//HFX_ALIGN
#if HFX_MSVC > 6
#define HFX_ALIGN(nBits) __declspec(align(nBits)) 
#else
#define HFX_ALIGN(nBits) 
#endif
 
#define HFX_MEMSET memset
#define HFX_MEMCPY memcpy
 
#endif
 