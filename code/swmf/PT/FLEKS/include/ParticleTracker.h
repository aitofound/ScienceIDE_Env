#ifndef _PARTICLETRACKER_H_
#define _PARTICLETRACKER_H_

#include <AMReX_Vector.H>

#include "Grid.h"
#include "Particles.h"
#include "Pic.h"
#include "TestParticles.h"

class ParticleTracker : public Grid {
public:
  ParticleTracker(amrex::Geometry const &gm, amrex::AmrInfo const &amrInfo,
                  int nGst, FluidInterface *fluidIn, TimeCtr *tcIn, int id,
                  ParticleTrackerInfo &info, const DomainParameters &dp)
      : Grid(gm, amrInfo, nGst, id, "pt"),
        tc(tcIn),
        fi(fluidIn),
        domainParameters(dp),
        pInfo(&info) {}

  ~ParticleTracker() {
    if (isNewGrid)
      return;

    bool doSave = savectr->is_time_to(true);
    for (auto &tps : parts) {
      if (doSave) {
        tps->write_particles(tc->get_cycle());
      }
    }
  };

  void post_process_param();

  void pre_regrid() override;
  void post_regrid() override;

  void update_field(Pic &pic);
  void set_ic(Pic &pic);
  void update(Pic &pic, bool doReport = false);

  void complete_parameters();

  void save_restart_data();
  void save_restart_header(std::ofstream &headerFile);
  void read_restart();
  void write_log(bool doForce = false, bool doCreateFile = false);

  void set_tp_init_shapes(amrex::Vector<std::shared_ptr<Shape> > &shapes);

private:
  TimeCtr *tc = nullptr;
  FluidInterface *fi = nullptr;

  const DomainParameters &domainParameters;

  // Parameter container populated by Domain during read_param and resolved in
  // ParticleTrackerInfo::post_process_param (after fi is fully processed).
  ParticleTrackerInfo *pInfo = nullptr;

  amrex::Vector<std::unique_ptr<TestParticles> > parts;
  amrex::Vector<amrex::MultiFab> nodeE;

  amrex::Vector<amrex::MultiFab> nodeB;

  // Cell-centred EM mirrors for the hybrid test-particle gather.
  amrex::Vector<amrex::MultiFab> centerE;
  amrex::Vector<amrex::MultiFab> centerB;

  std::unique_ptr<PlotCtr> savectr;

  std::string logFile;

  // Test Particle initialization regions (set from the domain #REGION blocks).
  amrex::Vector<std::shared_ptr<Shape> > tpShapes;
};

#endif
