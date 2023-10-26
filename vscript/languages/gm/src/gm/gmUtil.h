/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMUTIL_H_
#define _GMUTIL_H_

#include "gmConfig.h"

template <class T>
T gmMin(const T &a_a, const T &a_b)
{
  if(a_a < a_b) return a_a;
  return a_b;
}


template <class T>
T gmMax(const T &a_a, const T &a_b)
{
  if(a_a > a_b) return a_a;
  return a_b;
}


/// \brief gmLog2ge() returns the next power of 2, greater than or equal to the given number.
inline unsigned int gmLog2ge(unsigned int n)
{
  --n;

  n |= n >> 16;
  n |= n >> 8;
  n |= n >> 4;
  n |= n >> 2;
  n |= n >> 1;

  return n + 1;
}

/// \brief gmItoa()
char * gmItoa(int a_val, char * a_dst, int a_radix);

/// \brief Is this system LittleEndian?
inline char gmIsLittleEndian()
{
  unsigned int fourBytes = 1; 
  return *((unsigned char*)&fourBytes);
}


#endif
