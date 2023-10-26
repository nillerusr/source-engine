/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMMACHINELIB_H_
#define _GMMACHINELIB_H_

class gmMachine;

#define GM_STATE_NUM_PARAMS 1 // requried for gmThread::PushStackFrame() for state(fp) implementation

void gmMachineLib(gmMachine * a_machine);

#endif // _GMMACHINELIB_H_
