/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmStringObject.h"
#include "gmMachine.h"

// Must be last header
#include "memdbgon.h"

void gmStringObject::Destruct(gmMachine * a_machine) 
{
  a_machine->Sys_FreeUniqueString(m_string);
#if GM_USE_INCGC
  a_machine->DestructDeleteObject(this);
#endif //GM_USE_INCGC
}

