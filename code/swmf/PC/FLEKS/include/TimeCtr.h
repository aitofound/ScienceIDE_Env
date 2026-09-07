#ifndef _TIMECTR_H_
#define _TIMECTR_H_

#include <math.h>
#include <string>

#include <AMReX.H>
#include <AMReX_Print.H>
#include <AMReX_REAL.H>
#include <AMReX_Vector.H>

#include "PlotWriter.h"

class TimeCtr;

class EventCtr {
private:
  TimeCtr *tc;

  // In SI unit
  amrex::Real dtEvent;
  amrex::Real lastTime;
  amrex::Real nextTime;
  bool useDt;
  int nCount;

  int dnEvent;
  int nLast;
  int nNext;
  bool useDn;
  int multiple;

public:
  EventCtr(TimeCtr *tcIn, const amrex::Real dtIn = -1, const int dnIn = -1,
           const int dnMultipleIn = 1);

  void init(const amrex::Real dtIn, const int dnIn = -1);

  bool is_time_to(bool doForce = false);

  void set_multiple(const int in) { multiple = in; }
};

class PlotCtr : public EventCtr {
public:
  PlotWriter writer;

public:
  PlotCtr(const MPI_Comm &ic, TimeCtr *tcIn, const int idIn = 0,
          const amrex::Real dtIn = -1, const int dnIn = -1,
          const std::string plotStringIN = "", const double dxIn = 1,
          const std::string plotVarIn = "",
          const amrex::RealVect plotMinIn_D = { AMREX_D_DECL(1, 1, 1) },
          const amrex::RealVect plotMaxIn_D = { AMREX_D_DECL(-1, -1, -1) },
          const int nSpeciesIn = 2)
      : EventCtr(tcIn, dtIn, dnIn),
        writer(ic, idIn, plotStringIN, dxIn, plotVarIn, plotMinIn_D,
               plotMaxIn_D, nSpeciesIn) {}
};

class TimeCtr {
private:
  amrex::Real timeNowSI;
  amrex::Real dtSI, dtMaxSI;
  amrex::Real nextDtSI;

  // The time step when the grid is empty.
  amrex::Real dummyDtSI;

  amrex::Real cfl;

  amrex::Real si2no;
  amrex::Real no2si;
  long int cycle;

  // public member variables.
public:
  amrex::Vector<PlotCtr> plots;

  EventCtr picLog;

  EventCtr ptLog;

  EventCtr loadBalance;

  EventCtr monitor;

  // public methods
public:
  TimeCtr()
      : timeNowSI(0),
        dtSI(-1),
        dtMaxSI(-1),
        nextDtSI(-1),
        dummyDtSI(1),
        cfl(0.2),
        si2no(1),
        no2si(1),
        cycle(0),
        picLog(this, -1, 10),
        ptLog(this, -1, 10),
        loadBalance(this, -1, -1),
        monitor(this, -1, 10) {}

  void set_si2no(const amrex::Real si2noIn) {
    si2no = si2noIn;
    no2si = 1. / si2no;
  }
  void set_no2si(const amrex::Real no2siIn) {
    no2si = no2siIn;
    si2no = 1. / no2si;
  }

  void set_cycle(long int cycleIn) { cycle = cycleIn; }
  long int get_cycle() const { return cycle; }

  void set_time(const amrex::Real timeIn) { timeNowSI = timeIn * no2si; }
  void set_time_si(const amrex::Real timeIn) { timeNowSI = timeIn; }
  amrex::Real get_time() const { return timeNowSI * si2no; }
  amrex::Real get_time_si() const { return timeNowSI; }

  void set_dt(const amrex::Real dtIn) { dtSI = dtIn * no2si; }
  void set_dt_si(const amrex::Real dtIn) { dtSI = dtIn; }
  amrex::Real get_dt() const { return dtSI * si2no; }
  amrex::Real get_dt_si() const { return dtSI; }

  void set_dt_max(const amrex::Real dtIn) { dtMaxSI = dtIn * no2si; }
  void set_dt_max_si(const amrex::Real dtIn) { dtMaxSI = dtIn; }
  amrex::Real get_dt_max() const { return dtMaxSI * si2no; }
  amrex::Real get_dt_max_si() const { return dtMaxSI; }

  void set_next_dt(amrex::Real in) { nextDtSI = in * no2si; }
  void set_next_dt_si(amrex::Real in) { nextDtSI = in; }
  amrex::Real get_next_dt() { return nextDtSI * si2no; }
  amrex::Real get_next_dt_si() { return nextDtSI; }

  void set_dummy_dt(amrex::Real in) { dummyDtSI = in * no2si; }
  void set_dummy_dt_si(amrex::Real in) { dummyDtSI = in; }
  amrex::Real get_dummy_dt() { return dummyDtSI * si2no; }
  amrex::Real get_dummy_dt_si() { return dummyDtSI; }

  void set_cfl(amrex::Real in) { cfl = in; }
  amrex::Real get_cfl() { return cfl; }

  void update() {
    timeNowSI += dtSI;
    cycle++;

    if (dtSI > 0) {
      set_dummy_dt_si(dtSI);
    }
  }
};

#endif
