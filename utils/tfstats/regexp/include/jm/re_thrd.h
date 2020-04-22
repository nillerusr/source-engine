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
  *   FILE     re_thrd.h
  *   VERSION  2.12
  *   Thread synch helper functions, for regular
  *   expression library.
  */

#ifndef RE_THRD_H
#define RE_THRD_H

#ifndef JM_CFG_H
#include <jm/jm_cfg.h>
#endif

#if defined(JM_PLATFORM_W32) && defined(JM_THREADS)
//#include <windows.h>
#endif

#if !defined(JM_PLATFORM_W32) && defined(JM_THREADS)
#include <pthread.h>
#endif


JM_NAMESPACE(__JM)

void RE_CALL re_init_threads();
void RE_CALL re_free_threads();

#ifdef JM_THREADS

#ifndef JM_PLATFORM_W32

typedef pthread_mutex_t CRITICAL_SECTION;

inline void RE_CALL InitializeCriticalSection(CRITICAL_SECTION* ps)
{
   pthread_mutex_init(ps, NULL);
}

inline void RE_CALL DeleteCriticalSection(CRITICAL_SECTION* ps)
{
   pthread_mutex_destroy(ps);
}

inline void RE_CALL EnterCriticalSection(CRITICAL_SECTION* ps)
{
   pthread_mutex_lock(ps);
}

inline void RE_CALL LeaveCriticalSection(CRITICAL_SECTION* ps)
{
   pthread_mutex_unlock(ps);
}

#endif

template <class Lock>
class lock_guard
{
   typedef Lock lock_type;
public:
   lock_guard(lock_type& m, bool aq = true)
      : mut(m), owned(false){ acquire(aq); }

   ~lock_guard()
   { acquire(false); }

   void RE_CALL acquire(bool aq = true, DWORD timeout = INFINITE)
   {
      if(aq && !owned)
      {
         mut.acquire(true, timeout);
         owned = true;
      }
      else if(!aq && owned)
      {
         mut.acquire(false);
         owned = false;
      }
   }
private:
   lock_type& mut;
   bool owned;
};


class critical_section
{
public:
   critical_section()
   { InitializeCriticalSection(&hmutex);}

   critical_section(const critical_section&)
   { InitializeCriticalSection(&hmutex);}

   const critical_section& RE_CALL operator=(const critical_section&)
   {return *this;}

   ~critical_section()
   {DeleteCriticalSection(&hmutex);}

private:

   void RE_CALL acquire(bool aq, DWORD unused = INFINITE)
   { if(aq) EnterCriticalSection(&hmutex);
      else LeaveCriticalSection(&hmutex);
   }

   CRITICAL_SECTION hmutex;

public:
   typedef lock_guard<critical_section> ro_guard;
   typedef lock_guard<critical_section> rw_guard;

   friend lock_guard<critical_section>;
};

inline bool RE_CALL operator==(const critical_section&, const critical_section&)
{
   return false;
}

inline bool RE_CALL operator<(const critical_section&, const critical_section&)
{
   return true;
}

typedef lock_guard<critical_section> cs_guard;

JM_IX_DECL extern critical_section* p_re_lock;
JM_IX_DECL extern unsigned int re_lock_count;

#define JM_GUARD(inst) __JM::critical_section::rw_guard g(inst);

#else  // JM_THREADS

#define JM_GUARD(inst)

#endif // JM_THREADS

JM_END_NAMESPACE

#endif // sentry




