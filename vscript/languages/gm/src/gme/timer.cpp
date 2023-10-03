//  See Copyright Notice in gmMachine.h
#include "timer.h"

bool Timer::Init()
{
  resetTimer = true;
  dTime = 0.0f;
  bool result = QueryPerformanceFrequency(&timerFrequency) > 0;
  oof = 1.0f / ((float)timerFrequency.QuadPart);

  return result;
}

float Timer::Tick()
{
  QueryPerformanceCounter(&thisTime);

  if (resetTimer)
  {
    dTime = 0;
    resetTimer = false;
  }
  else
  {
    dTime = ((float)(thisTime.QuadPart - lastTime.QuadPart)) * oof;
  }
  lastTime = thisTime;

  return dTime;
}
