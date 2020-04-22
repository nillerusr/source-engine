/*
     File:       CGBase.h
 
     Contains:   CoreGraphics base types
 
     Version:    QuickTime 7.3
 
     Copyright:  (c) 2007 (c) 2000-2001 by Apple Computer, Inc., all rights reserved.
 
     Bugs?:      For bug reports, consult the following page on
                 the World Wide Web:
 
                     http://developer.apple.com/bugreporter/
 
*/
#ifndef CGBASE_H_
#define CGBASE_H_

#ifndef __CONDITIONALMACROS__
#include <ConditionalMacros.h>
#endif

#include <stddef.h>
#if __MWERKS__ > 0x2300 
#include <stdint.h>
#endif


#if PRAGMA_ONCE
#pragma once
#endif

#if PRAGMA_IMPORT
#pragma import on
#endif

#include <stdint.h>

#if !defined(CG_INLINE)
#  if defined(__GNUC__)
#    define CG_INLINE static __inline__
#  elif defined(__MWERKS__)
#    define CG_INLINE static inline
#  else
#    define CG_INLINE static    
#  endif
#endif


#ifdef PRAGMA_IMPORT_OFF
#pragma import off
#elif PRAGMA_IMPORT
#pragma import reset
#endif


#endif /* CGBASE_H_ */

