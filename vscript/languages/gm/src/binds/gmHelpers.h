/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMHELPERS_H_
#define _GMHELPERS_H_

#include "gmConfig.h"
#include "gmThread.h"
#include <math.h>

//
// Helpers
//

#define GM_PI_VALUE 3.1415927f

// Clamp value between to range
template <class TYPE>
GM_FORCEINLINE TYPE gmClamp(const TYPE a_min, const TYPE a_value, const TYPE a_max)
{
  if(a_value < a_min)
  {
    return a_min;
  }
  else if(a_value > a_max)
  {
    return a_max;
  }
  return a_value;
}

template <class TYPE>
GM_FORCEINLINE TYPE gmMin3(const TYPE a_x, const TYPE a_y, const TYPE a_z)
{
  if(a_y < a_x)
  {
    if(a_z <  a_y)
    {
      return a_z;
    }
    else
    {
      return a_y;
    }
  }
  else
  { 
    if (a_z < a_x)
    {
      return a_z;
    }
    else
    {
      return a_x;
    }
  }
}

template <class TYPE>
GM_FORCEINLINE TYPE gmMax3(const TYPE a_x, const TYPE a_y, const TYPE a_z)
{
  if(a_y > a_x)
  {
    if(a_z >  a_y)
    {
      return a_z;
    }
    else
    {
      return a_y;
    }
  }
  else
  { 
    if (a_z > a_x)
    {
      return a_z;
    }
    else
    {
      return a_x;
    }
  }
}

GM_FORCEINLINE float gmGetFloatOrIntParamAsFloat(gmThread * a_thread, int a_paramIndex)
{
  if(a_thread->ParamType(a_paramIndex) == GM_INT)
  {
    return (float)a_thread->Param(a_paramIndex).m_value.m_int;
  }
  else
  {
    return a_thread->Param(a_paramIndex).m_value.m_float;
  }
}


GM_FORCEINLINE bool gmGetFloatOrIntParamAsFloat(gmThread * a_thread, int a_paramIndex, float& a_retValue)
{
  if(a_thread->ParamType(a_paramIndex) == GM_INT)
  {
    a_retValue = (float)a_thread->Param(a_paramIndex).m_value.m_int;
    return true;
  }
  else if(a_thread->ParamType(a_paramIndex) == GM_FLOAT)
  {
    a_retValue = a_thread->Param(a_paramIndex).m_value.m_float;
    return true;
  }
  else
  {
    return false;
  }
}

GM_FORCEINLINE int gmGetFloatOrIntParamAsInt(gmThread * a_thread, int a_paramIndex)
{
  if(a_thread->ParamType(a_paramIndex) == GM_INT)
  {
    return a_thread->Param(a_paramIndex).m_value.m_int;
  }
  else
  {
    return (int)a_thread->Param(a_paramIndex).m_value.m_float;
  }
}

GM_FORCEINLINE bool gmGetFloatOrIntParamAsInt(gmThread * a_thread, int a_paramIndex, int& a_retValue)
{
  if(a_thread->ParamType(a_paramIndex) == GM_INT)
  {
    a_retValue = a_thread->Param(a_paramIndex).m_value.m_int;
    return true;
  }
  else if(a_thread->ParamType(a_paramIndex) == GM_FLOAT)
  {
    a_retValue = (int)a_thread->Param(a_paramIndex).m_value.m_float;
    return true;
  }
  else
  {
    return false;
  }
}

/*!
  \brief gmRandomInt() returns a random int b/n two values
         Beware of overflow since ints are only 32bit on Intel
  \return number is >= min and < max (exclusive of max)
*/
int gmRandomInt(int a_min, int a_max);

/*!
  \brief gmRandomInt() returns a random int b/n two values
         Note this is a low precision random value since it is generated from an int.
  \return number is >= min and < max (exclusive of max)
*/
float gmRandomFloat(float a_min, float a_max);

/*!
  \brief Returns the sine and cosine of values
         Note should make platform specific version
*/
GM_FORCEINLINE void gmSinCos(const float a_angle, float& a_sin, float& a_cos)
{
  a_sin = (float)sinf(a_angle);
  a_cos = (float)cosf(a_angle);
}

#endif // _GMHELPERS_H_
