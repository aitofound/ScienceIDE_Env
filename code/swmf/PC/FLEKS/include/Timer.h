#ifndef _TIMER_H_
#define _TIMER_H_

#include <string>

#include <AMReX_BLProfiler.H>

#include "Timing_c.h"

class Timer {
private:
  std::string name;
  bool isTiming = true;

public:
  Timer(std::string nameIn) {
    name = nameIn;
    timing_start(name);
  }

  ~Timer() { stop(); }

  void stop() {
    if (isTiming)
      timing_stop(name);
    isTiming = false;
  }
};

#define timing_func(name)                                                      \
  Timer funcTimer(name);                                                       \
  BL_PROFILE(name);

#endif
