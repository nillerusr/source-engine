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
  *   FILE     re_lib.h
  *   VERSION  2.12
  *   Automatic library file inclusion.
  */


#ifndef RE_LIB_H
#define RE_LIB_H

#if defined(_MSC_VER) && !defined(RE_BUILD_DLL)
   #ifdef _DLL
      #ifdef _DEBUG
         #pragma comment(lib, "mre200dl.lib")
      #else // DEBUG
         #pragma comment(lib, "mre200l.lib")
      #endif // _DEBUG
   #else // _DLL
      #ifdef _MT
         #ifdef _DEBUG
            #pragma comment(lib, "mre200dm.lib")
         #else //_DEBUG
            #pragma comment(lib, "mre200m.lib")
         #endif //_DEBUG
      #else //_MT
         #ifdef _DEBUG
            #pragma comment(lib, "mre200d.lib")
         #else //_DEBUG
            #pragma comment(lib, "mre200.lib")
         #endif //_DEBUG
      #endif //_MT
   #endif //_DLL
#endif //_MSC_VER


#if defined(__BORLANDC__) && !defined(RE_BUILD_DLL)
   #if (__BORLANDC__ > 0x520) && !defined(_NO_VCL)
      #define JM_USE_VCL
   #endif
   
   #if __BORLANDC__ <= 0x520
   
   #ifdef JM_USE_VCL
   
      #ifdef _RTLDLL
         #pragma comment(lib, "b2re200lv.lib")
      #else
         #pragma comment(lib, "b2re200v.lib")
      #endif
   
   #else // VCL
   
   #ifdef _RTLDLL
      #ifdef __MT__
         #pragma comment(lib, "b2re200lm.lib")
      #else // __MT__
         #pragma comment(lib, "b2re200l.lib")
      #endif // __MT__
   #else //_RTLDLL
      #ifdef __MT__
         #pragma comment(lib, "b2re200m.lib")
      #else // __MT__
         #pragma comment(lib, "b2re200.lib")
      #endif // __MT__
   #endif // _RTLDLL
   
   #endif // VCL
   
   #elif __BORLANDC__ <= 0x530
   
   #ifdef JM_USE_VCL
   
      #ifdef _RTLDLL
         #pragma comment(lib, "b3re200lv.lib")
      #else
         #pragma comment(lib, "b3re200v.lib")
      #endif
   
   #else // VCL
   
   #ifdef _RTLDLL
      #ifdef __MT__
         #pragma comment(lib, "b3re200lm.lib")
      #else // __MT__
         #pragma comment(lib, "b3re200l.lib")
      #endif // __MT__
   #else //_RTLDLL
      #ifdef __MT__
         #pragma comment(lib, "b3re200m.lib")
      #else // __MT__
         #pragma comment(lib, "b3re200.lib")
      #endif // __MT__
   #endif // _RTLDLL
   
   #endif // VCL
   
   #else // Version: 0x540
   
   #ifdef JM_USE_VCL
   
      #ifdef _RTLDLL
         #pragma comment(lib, "b4re200lv.lib")
      #else
         #pragma comment(lib, "b4re200v.lib")
      #endif
   
   #else // VCL
   
   #ifdef _RTLDLL
      #ifdef __MT__
         #pragma comment(lib, "b4re200lm.lib")
      #else // __MT__
         #pragma comment(lib, "b4re200l.lib")
      #endif // __MT__
   #else //_RTLDLL
      #ifdef __MT__
         #pragma comment(lib, "b4re200m.lib")
      #else // __MT__
         #pragma comment(lib, "b4re200.lib")
      #endif // __MT__
   #endif // _RTLDLL
   
   #endif // VCL
   
   #endif   
   
#endif //__BORLANDC__


#endif // RE_LIB_H



