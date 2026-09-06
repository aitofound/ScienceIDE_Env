#include "TimeCtr.h"

using namespace amrex;

EventCtr::EventCtr(TimeCtr *tcIn, const Real dtIn, const int dnIn,
                   const int multipleIn) {
  tc = tcIn;
  multiple = multipleIn;
  init(dtIn, dnIn);
}

void EventCtr::init(const Real dtIn, const int dnIn) {
  useDt = dtIn > 0;
  useDn = dnIn > 0;

  if (useDt && useDn) {
    Abort("Error: The event can not be controlled by dt and dn at the "
          "same time");
  }

  if (useDt) {
    dtEvent = dtIn;
    nCount = floor(tc->get_time_si() / dtEvent);
    nextTime = nCount * dtEvent;
    lastTime = (nCount - 1) * dtEvent;
  }

  if (useDn) {
    dnEvent = dnIn;
    nNext = (floor((Real)(tc->get_cycle()) / dnEvent)) * dnEvent;
    nLast = nNext - dnEvent;
  }
}

bool EventCtr::is_time_to(bool doForce) {

  bool isTime = false;

  // Q: Why use 'while' instead of 'if'?
  // A: Make sure 'nextTime' would be larger than the curren time.
  while (useDt && (tc->get_time_si() >= nextTime - dtEvent * 1e-6)) {
    nCount++;
    lastTime = tc->get_time_si();
    nextTime = nCount * dtEvent;
    isTime = true;
  }

  if (useDn && tc->get_cycle() >= nNext) {
    nLast = tc->get_cycle();
    nNext = nLast + dnEvent;

    // Example: assume the test particle save frequency is dn=7, and it writes
    // to disk for every 20 records, so dnEvent = 140. If it restarts from
    // ncycle=400, nNext is 540 so far. But 540 can not be divided by dn=7. With
    // the following line, test particles will be flushed to disk at cycle=539.
    nNext = floor(float(nNext) / multiple) * multiple;

    isTime = true;
  }

  if (doForce && !isTime) {
    // If doForce, return true unless the time/cycle has not changed since the
    // last action, to avoid repeating the same action.
    if (useDn) {
      isTime = (nLast != tc->get_cycle());
      nLast = tc->get_cycle();
    }

    if (useDt) {
      if (fabs(lastTime - tc->get_time_si()) > dtEvent * 1e-6) {
        isTime = true;
        lastTime = tc->get_time_si();
      }
    }
  } // doForce

  return isTime;
}
