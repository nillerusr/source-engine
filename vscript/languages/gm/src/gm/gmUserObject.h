/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMUSEROBJECT_H_
#define _GMUSEROBJECT_H_

#include "gmConfig.h"
#include "gmVariable.h"

/// \class gmUserObject
/// \brief
class gmUserObject : public gmObject
{
public:

  virtual int GetType() const { return m_userType; }

  virtual void Destruct(gmMachine * a_machine);
#if GM_USE_INCGC
  virtual bool Trace(gmMachine * a_machine, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone);
#else //GM_USE_INCGC
  virtual void Mark(gmMachine * a_machine, gmuint32 a_mark);
#endif //GM_USE_INCGC

  int m_userType;
  void * m_user;
};

#endif // _GMUSEROBJECT_H_
