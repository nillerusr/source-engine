/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmHelpers.h"


// Must be last header
#include "memdbgon.h"


/*!
  \brief gmRandomInt() returns a random int b/n two values
  \return number is >= min and < max (exclusive of max)
*/
int gmRandomInt(int a_min, int a_max)
{
  if(a_min > a_max)
  {
    int temp = a_max;
    a_max = a_min;
    a_min = temp;
  }
  else if(a_min == a_max)
  {
    return a_min; // hmmm, not good.
  }
 
  int randVal = rand();
  int val = (randVal % (a_max - a_min)) + a_min;
 
  return val;
}
 

/*!
  \brief gmRandomInt() returns a random int b/n two values
  \return number is >= min and < max (exclusive of max)
*/
float gmRandomFloat(float a_min, float a_max)
{
  if(a_min > a_max)
  {
    float temp = a_max;
    a_max = a_min;
    a_min = temp;
  }
  else if(a_min == a_max)
  {
    return a_min; // hmmm, not good.
  }
  
  int randVal = rand() % RAND_MAX;
  float frandVal = (float)randVal / (float)RAND_MAX;
  return a_min + (frandVal * (a_max - a_min));
}
