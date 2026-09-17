#ifndef _TESTPARTICLES_H_
#define _TESTPARTICLES_H_

#include "BitArray.h"
#include "Particles.h"

class TestParticles : public PTParticles {
public:
  static constexpr int iTPt_ = 0;
  static constexpr int iTPx_ = 1;
  static constexpr int iTPy_ = 2;
  static constexpr int iTPz_ = 3;
  static constexpr int iTPu_ = 4;
  static constexpr int iTPv_ = 5;
  static constexpr int iTPw_ = 6;
  static constexpr int iTPBx_ = 7;
  static constexpr int iTPBy_ = 8;
  static constexpr int iTPBz_ = 9;
  static constexpr int iTPEx_ = 10;
  static constexpr int iTPEy_ = 11;
  static constexpr int iTPEz_ = 12;
  static constexpr int iTPdBxdx_ = 13;
  static constexpr int iTPdBxdy_ = 14;
  static constexpr int iTPdBxdz_ = 15;
  static constexpr int iTPdBydx_ = 16;
  static constexpr int iTPdBydy_ = 17;
  static constexpr int iTPdBydz_ = 18;
  static constexpr int iTPdBzdx_ = 19;
  static constexpr int iTPdBzdy_ = 20;
  static constexpr int iTPdBzdz_ = 21;

private:
  static constexpr int iRegionBoundary_ = 1;
  static constexpr int iRegionUniform_ = 2;
  static constexpr int iRegionSideXp_ = 3;
  static constexpr int iRegionUser_ = 4;

public:
  TestParticles(Grid* gridIn, FluidInterface* const fluidIn,
                TimeCtr* const tcIn, const int speciesID,
                const amrex::Real charge, const amrex::Real mass, int id = 0);

  ~TestParticles() = default;

  inline int record_var_index(int iPart, int iVar = 0) {
    return nPicPartReal + ptRecordSize * iPart + iVar;
  }

  void move_and_save_particles(const amrex::MultiFab& nodeEMF,
                               const amrex::MultiFab& nodeBMF, amrex::Real dt,
                               amrex::Real dtNext, amrex::Real tNow,
                               bool doSave);

  void move_and_save_particles_cell_centered(const amrex::MultiFab& centerEMF,
                                             const amrex::MultiFab& centerBMF,
                                             amrex::Real dt, amrex::Real dtNext,
                                             amrex::Real tNow, bool doSave);

  void move_and_save_charged_particles(const amrex::MultiFab& nodeEMF,
                                       const amrex::MultiFab& nodeBMF,
                                       amrex::Real dt, amrex::Real dtNext,
                                       amrex::Real tNow, bool doSave);

  void move_and_save_charged_particles_cell_centered(
      const amrex::MultiFab& centerEMF, const amrex::MultiFab& centerBMF,
      amrex::Real dt, amrex::Real dtNext, amrex::Real tNow, bool doSave);

  void move_and_save_neutrals(amrex::Real dt, amrex::Real tNow, bool doSave);

  void read_test_particle_list(const amrex::Vector<std::string>& listFiles);

  void add_test_particles_from_fluid(
      const amrex::Vector<Vel>& tpStates = amrex::Vector<Vel>());

  void add_test_particles_from_pic(PicParticles* pts);

  void reset_record_counter();

  bool write_particles(int cycle);

  unsigned long long int loop_particles(
      std::string action = "count_record_size", char* buff = nullptr,
      unsigned long long int sizeLimit = 0, unsigned long long int shift = 0);

  void print_record_buffer(char* buffer, unsigned long long int nBuffer);

  void set_IO_units(amrex::Real no2outLIn, amrex::Real no2outVIn,
                    amrex::Real no2outMIn, amrex::Real no2outBIn,
                    amrex::Real no2outEIn) {
    no2outL = no2outLIn;
    no2outV = no2outVIn;
    no2outM = no2outMIn;
    no2outB = no2outBIn;
    no2outE = no2outEIn;
  }

  void set_interval(amrex::IntVect in) { nIntervalCell = in; };

  void set_particle_region(std::string& sRegion,
                           amrex::Vector<std::shared_ptr<Shape> >& shapes) {
    if (!sRegion.empty()) {
      // TODO: std::string::starts_with (C++20)
      if (sRegion[0] == '+' || sRegion[0] == '-') {
        iPartRegion = iRegionUser_;
        tpRegions = Regions(shapes, sRegion);
      } else if (sRegion == "boundary") {
        iPartRegion = iRegionBoundary_;
      } else if (sRegion == "uniform") {
        iPartRegion = iRegionUniform_;
      } else if (sRegion == "sideX+") {
        iPartRegion = iRegionSideXp_;
      } else {
        amrex::Abort("Error:Unknown test particle region!");
      }
    }
  }

  template <typename T> void gather_accumulate_and_scatter(T& local, T& ahead) {
    int nProc = amrex::ParallelDescriptor::NProcs();

    // The following two vectors are only useful on the root processor. They are
    // allocated on all processors to avoid memory leak since they are passed to
    // functions like Gather()
    amrex::Vector<T> perProc, accumulated;
    perProc.resize(nProc, 0);
    accumulated.resize(nProc, 0);

    amrex::ParallelDescriptor::Gather(
        &local, 1, &perProc[0], 1,
        amrex::ParallelDescriptor::IOProcessorNumber());

    if (amrex::ParallelDescriptor::IOProcessor()) {
      for (int i = 1; i < accumulated.size(); ++i) {
        accumulated[i] = accumulated[i - 1] + perProc[i - 1];
      }
    }

    amrex::ParallelDescriptor::Scatter(
        &ahead, 1, &accumulated[0], 1,
        amrex::ParallelDescriptor::IOProcessorNumber());
  }

  void update_initial_particle_number() {
    nInitPart = TotalNumberOfParticles(false, false);
  }

  unsigned long int init_particle_number() const { return nInitPart; }
  void init_particle_number(unsigned long int in) { nInitPart = in; }

private:
  std::string outputDir;

  std::string printPrefix;
  std::string gridName;
  int gridID;

  amrex::IntVect nIntervalCell = { AMREX_D_DECL(1, 1, 1) };

  int iPartRegion = iRegionBoundary_;

  amrex::Real no2outL, no2outV, no2outM, no2outB, no2outE;

  unsigned long int nInitPart;

  std::vector<PID> vIDs;

  Regions tpRegions;
};

#endif
