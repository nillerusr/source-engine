/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMSCANNER_H_
#define _GMSCANNER_H_

#include "gmConfig.h"

//
// gmscanner.cpp, gmparser.cpp.h and gmparser.cpp are created by flex and bison
// see fontend.bat for more details.
//

// fwd decl for flex
typedef struct yy_buffer_state * YY_BUFFER_STATE;
YY_BUFFER_STATE gm_scan_string(const char * str);
YY_BUFFER_STATE gm_scan_bytes(const char *bytes, int len);
void gm_delete_buffer(YY_BUFFER_STATE b);
int gmlex();
extern char * gmtext;
extern int gmlineno;

#endif // _GMSCANNER_H_

