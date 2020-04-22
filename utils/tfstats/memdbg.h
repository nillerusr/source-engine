//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: declarations to track allocations via replacing the global new operator
//	some of this code was written by Paul Andre LeBlanc 
// <paul.a.leblanc@sympatico.ca> I got it off of dejanews.com
//	usage: new TRACKED object-type
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef MEMDBG_H
#define MEMDBG_H
#ifdef WIN32
#pragma once
#endif

#ifdef _DEBUG
#ifdef _MEMDEBUG
#define _MDEBUG
#endif
#endif

#ifdef _MDEBUG
  #define TRACKED (__FILE__, __LINE__)
#else
  #define TRACKED
#endif

#ifdef _MDEBUG 
void *operator new(size_t size, const char *file, const int line);
void *operator new[](size_t size, const char *file, const int line);
void operator delete(void *ptr, const char *file, const int line);
void operator delete[](void *ptr, const char *file, const int line);


//replacing global new for debugging purposes.
//these were written by me, wes cumberland, not paul andre leblanc
void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* v);
void operator delete[](void* v);
#endif

//leave this in, even for release build
int TFStats_win32_new_handler(size_t sz);
void TFStats_linux_new_handler(void);

void TFStats_setNewHandler();

#endif // MEMDBG_H
