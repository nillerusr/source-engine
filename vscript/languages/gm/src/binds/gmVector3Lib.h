/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMVECTOR3LIB_H_
#define _GMVECTOR3LIB_H_

#include "gmConfig.h"
#include "gmVariable.h"

class gmMachine;
class gmThread;
class gmUserObject;

// Bind the Vector3 Library.
void gmBindVector3Lib(gmMachine * a_machine);

// Push a Vector3 onto the stack
void gmVector3_Push(gmThread* a_thread, const float* a_vec);

// Create a Vector3 user object and fill it
gmUserObject* gmVector3_Create(gmMachine* a_machine, const float* a_vec);

// The Vector3 type Id.
extern gmType GM_VECTOR3;

// Example of getting Vector3 from parameter
// GM_CHECK_USER_PARAM(float*, GM_VECTOR3, vec1, 0);

#endif // _GMVECTOR3LIB_H_
