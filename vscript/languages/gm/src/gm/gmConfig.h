/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMCONFIG_H_
#define _GMCONFIG_H_

// Include the platform config.
// All platform configuration exists in gmconfig_p.h
#include "gmConfig_p.h"

#include <stdlib.h> // atoi, strtoul (binds: rand, srand)
#include <stdio.h> // fprintf, sprintf, _snprintf, _vnsprintf
#include <string.h> // stricmp, strcmp, strcpy, strlen, strcat, memset, memcpy (binds: strlwr, wtrupr, strspn, strcspn, strchr, strstr)
#include <stdarg.h> // va_start, va_end
#include <ctype.h> // isdigit
#include <math.h> // floorf, fmodf


/// \enum gmEndian Endian byte order
enum gmEndian
{
  GM_ENDIAN_BIG = 0,      //!< MOTOROLA (MAC), NINTENDO GC
  GM_ENDIAN_LITTLE = 1    //!< x86, XBOX, PS2
};

//
// Game Monkey Configuration
//

// COMPILE

#define GM_COMPILE_DEBUG            1         // define for compile debugging code, ie, printing code trees, byte code etc.

// COMPILE LOG

#define GMLOG_CHAINSIZE             2048      // memory chunk resolution for the compile log

// COMPILE PARSER

#define GMCODETREE_CHAINSIZE        4096      // memory chunk resolution for compiler code tree nodes.

// COMPILER CODE GENERATOR

#define GM_COMPILE_PASS_THIS_ALWAYS 0         // set to 1 to pass current this to each function call

// RUNTIME THREAD

#define GMTHREAD_INITIALBYTESIZE    512       // initial stack byte size for a single thread
#define GMTHREAD_MAXBYTESIZE        128000    //1024  // max stack byte size for a single thread (Sample scripts like it big)

// MACHINE

#define GMMACHINE_REMOVECOMPILER    0         // Remove compiler code, will only be able to run precompiled libs
#define GMMACHINE_GMCHECKDIVBYZERO  0         // Let GM operator check for divide by zero and possibly cause GM run time exception (rather than OS exception)
#define GMMACHINE_NULL_VAR_CTOR     0         // Nullify gmVariable in constructor.  Not recommended for real-time / time critical applications.
#define GMMACHINE_USERTYPEGROWBY    16        // allocate user types in chunks of this size
#define GMMACHINE_OBJECTCHUNKSIZE   32        // default object chunk allocation size
#define GMMACHINE_TBLCHUNKSIZE      32        // table object chunk allocation size
#define GMMACHINE_STRINGCHUNKSIZE   128       // default object chunk allocation size
#define GMMACHINE_STACKFCHUNKSIZE   128       // stack frame chunk size
#define GMMACHINE_AUTOMEM           true      // automatically decide garbage collection limit
#define GMMACHINE_AUTOMEMMULTIPY    2.5f      // after gc cycle, set limit = current * GMMACHINE_AUTOMEMMULTIPY (This is for atomic GC)
#define GMMACHINE_AUTOMEMALLOWSHRINK 0        // Allow memory liimits to shrink, otherwise memory will grow when needed only
#define GMMACHINE_INITIALGCHARDLIMIT 128*1024  // default gc hard memory limit.
#define GMMACHINE_INITIALGCSOFTLIMIT (GMMACHINE_INITIALGCHARDLIMIT * 9 / 10) // default gc soft memory limit
#define GMMACHINE_STRINGHASHSIZE    8192      // this will be dynamic... todo
#define GMMACHINE_MAXKILLEDTHREADS  16        // max size of the free thread list (don't make too large, ie, < 32)
#define GMMACHINE_GCEVERYALLOC      0         // define this to check garbage collection every allocate.
#define GMMACHINE_SUPERPARANOIDGC   0         // validate references (only for debugging purposes)
#define GMMACHINE_THREEPASSGC       0         // 1 for safe gc of persisting objects that reference other objects, 
                                              // ie, persisting tables.  if you only have persisting simple objects, ie
                                              // strings, set to 0 for faster garbage collection.

// Auto GC Calibration values
#define GMMACHINE_GC_HARD_MEM_INC_FRAC_OF_USED      1.5f   // what to set hard limit to above used mem when growing hard limit
#define GMMACHINE_GC_HARD_MEM_DEC_FRAC_OF_USED      1.5f   // what to set hard limit to above used mem when shinking hard limit
#define GMMACHINE_GC_SOFT_MEM_DEFAULT_FRAC_OF_HARD  (9.0f/10.0f) // what to set soft limit as frac of hard limit by default
#define GMMACHINE_GC_HARD_MEM_SHRINK_THRESH         0.5f   // threshold at which hard limit should shrink
#define GMMACHINE_GC_SOFT_MEM_MIN_FRAC              0.25f  // minimum soft limit as frac of hard limit to shrink soft limit
#define GMMACHINE_GC_SOFT_MEM_DEC_FRAC              0.1f   // amount to shrink soft limit as frac of soft/hard
#define GMMACHINE_GC_MIN_FRAMES_SINCE_RESTART       100    // if gc is restarting within this many frames/calls, it is probably configured bad
#define GM_GC_DEFAULT_WORK_INCREMENT                200    // Desired number of objects to trace per frame
#define GM_GC_DEFAULT_DESTRUCT_INCREMENT            200    // Desired number of old objects to free per frame

#define GMMACHINE_CPPOWNEDGMOBJHASHSIZE 1024  // default hash table size for objects owned by cpp code, necessary for GC.

// DEBUGGING

#define GMDEBUG_SUPPORT             1         // allow use with the gm debugger


// GARBAGE COLLECTOR
#define GM_USE_INCGC                1         // use incremental garbage collector


#define GM_BOOL_OP                  1         // Spport for a bool operator on user types for use in if statements. For full effect, users will want to implement operators [bool, ==, !=, !]

#endif // _GMCONFIG_H_
