//  See Copyright Notice in gmMachine.h
#ifndef _timer_h
#define _timer_h

#include <wtypes.h>

class Timer
{
public:

  Timer() : resetTimer(true), dTime(0.0f) {}
  ~Timer() {}

  bool Init();
  void Reset() { resetTimer = true; }

  float Tick();
  float GetDelta() { return dTime; }

private:

  LARGE_INTEGER timerFrequency, lastTime, thisTime;
  float dTime, oof;
  bool resetTimer;
};

#endif //_timer_h
