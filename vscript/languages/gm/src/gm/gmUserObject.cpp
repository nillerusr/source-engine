/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmUserObject.h"
#include "gmMachine.h"

// Must be last header
#include "memdbgon.h"

#if GM_USE_INCGC

void gmUserObject::Destruct(gmMachine * a_machine)
{
  gmGCDestructCallback gcDestruct = a_machine->GetUserGCDestructCallback((gmType) m_userType);
  if(gcDestruct)
  {
    gcDestruct(a_machine, this);
  }

#if GM_USE_INCGC
  a_machine->DestructDeleteObject(this);
#endif //GM_USE_INCGC
}

bool gmUserObject::Trace(gmMachine * a_machine, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone)
{
  gmGCTraceCallback gcTrace = a_machine->GetUserGCTraceCallback((gmType) m_userType);
  if(gcTrace)
  {
    return gcTrace(a_machine, this, a_gc, a_workLeftToGo, a_workDone);
  }
  else
  {
    ++a_workDone;
    return true;
  }
}

#else //GM_USE_INCGC

void gmUserObject::Destruct(gmMachine * a_machine)
{
  gmGarbageCollectCallback gc = a_machine->GetUserGCCallback((gmType) m_userType);
  if(gc) gc(a_machine, this, GM_MARK_PERSIST);
}

void gmUserObject::Mark(gmMachine * a_machine, gmuint32 a_mark)
{
  if(m_mark != GM_MARK_PERSIST) m_mark = a_mark;
  gmGarbageCollectCallback mark = a_machine->GetUserMarkCallback((gmType) m_userType);
  if(mark) mark(a_machine, this, a_mark);
}
#endif //GM_USE_INCGC

