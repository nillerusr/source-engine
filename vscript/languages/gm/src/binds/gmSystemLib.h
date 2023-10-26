/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMSYSTEMLIB_H_
#define _GMSYSTEMLIB_H_

#include "gmConfig.h"

class gmMachine;

#define GM_SYSTEM_LIB 1
#define GM_SYSTEM_LIB_MAX_LINE    1024      // maximum file line length

#if GM_SYSTEM_LIB

void gmBindSystemLib(gmMachine * a_machine);

#endif // GM_SYSTEM_LIB

#endif // _GMSYSTEMLIB_H_
