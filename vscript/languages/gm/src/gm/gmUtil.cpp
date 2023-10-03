/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmUtil.h"

// Must be last header
#include "memdbgon.h"


static char s_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

char * gmItoa(int a_val, char * a_dst, int a_radix)
{
  char * dst = a_dst;
  char buffer[65];
  char *p;
  int new_val;

  if (a_radix < 0) {
    if (a_radix < -36 || a_radix > -2) return NULL;
    if (a_val < 0) {
      *dst++ = '-';
      a_val = -a_val;
    }
    a_radix = -a_radix;
  } 
  else 
  {
    if (a_radix > 36 || a_radix < 2) return NULL;
  }
  p = &buffer[sizeof(buffer)-1];
  *p = '\0';
  new_val = (gmuint32) a_val / (gmuint32) a_radix;
  *--p = s_digits[(gmuint8) ((gmuint32) a_val- (gmuint32) new_val*(gmuint32) a_radix)];
  a_val = new_val;
  while (a_val != 0)
  {
    new_val=a_val/a_radix;
    *--p = s_digits[(gmuint8) (a_val-new_val*a_radix)];
    a_val= new_val;
  }

  while ((*dst++ = *p++) != 0) ;
  return a_dst;
}

