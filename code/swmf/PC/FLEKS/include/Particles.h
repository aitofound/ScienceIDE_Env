#ifndef _PARTICLES_H_
#define _PARTICLES_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>

#include <AMReX_AmrCore.H>
#include <AMReX_AmrParticles.H>
#include <AMReX_CoordSys.H>

#include "Array1D.h"
#include "BC.h"
#include "Bit.h"
#include "Constants.h"
#include "FluidInterface.h"
#include "GridUtility.h"
#include "RandNum.h"
#include "SourceInterface.h"
#include "TimeCtr.h"
#include "UMultiFab.h"

class InitialCondition; // forward: Particles only stores a non-owning pointer

enum class CrossSection { LS = 0, MT };

enum class PartMode { PIC = 0, Neutral, SEP };

struct PID {
  int cpu;
  int id;
  bool flag;

  // This function is used by c++ STL algorithms.
  bool operator<(const PID& t) const {
    bool lt = cpu < t.cpu;

    if (cpu == t.cpu)
      lt = id < t.id;

    return lt;
  }

  bool operator==(const PID& t) const { return cpu == t.cpu && id == t.id; }
};

struct Vel {
  amrex::Real vth = 0.0;
  amrex::Real vx = 0.0;
  amrex::Real vy = 0.0;
  amrex::Real vz = 0.0;
  amrex::Real nDens = -1.0; // >= 0 overrides fluid density
  int tag = -1;             // species ID (-1 = unset)

  Vel() = default;
};

struct OHIon {
  amrex::Real rAnalytic = 0;
  amrex::Real rCutoff = 0;
  amrex::Real swRho = 0;
  amrex::Real swT = 0;
  amrex::Real swU = 0;
  bool doGetFromOH = false;
};

struct IDs {
  int id;
  int supID;
};

class ParticlesInfo {
public:
  amrex::IntVect nPartPerCell = { AMREX_D_DECL(6, 6, 6) };

  bool isParticleLocationRandom = true;
  bool isPPVconstant = false;
  bool doPreSplitting = false;

  bool fastMerge = false;
  bool mergeLight = false;

  int nPartCombine = 6;
  int nPartNew = 5;
  int nMergeTry = 3;

  amrex::Real mergeThresholdDistance = 0.6;
  amrex::Real velBinBufferSize = 0.125;

  amrex::Real mergeRatioMax = 1.5;
  amrex::Real pLevRatio = 1.2;

  amrex::Real mergePartRatioMax = 10;

  // [amu/cc]
  amrex::Real vacuumIO = 0;

  OHIon ionOH;

  // Particle boundary conditions, one entry per species.
  amrex::Vector<BoxBC<ParticleBC::Type> > pBCs;

  // Parallel to pBCs: 1 when the species had its own #PARTICLEBOXBOUNDARY
  // block, 0 when the entry is just the default.  Lets the periodic auto-fill
  // tell "the user asked for X" apart from "nothing was specified".
  amrex::Vector<char> pBCsSet;

  amrex::Vector<int> supIDs;

  int initial_sup_id(const int speciesID) const {
    if (speciesID >= 0 && speciesID < static_cast<int>(supIDs.size()))
      return supIDs[speciesID];
    return 1;
  }

  // pBCs is sized from nSpecies in Pic::post_process_param(), but Particles
  // objects can be constructed before that: TestParticles is built with a
  // throwaway default ParticlesInfo (see src/TestParticles.cpp), so its pBCs
  // is always empty.  Return the default (all-`coupled`) entry instead of
  // reading out of bounds.
  const BoxBC<ParticleBC::Type>& particle_bc(const int speciesID) const {
    static const BoxBC<ParticleBC::Type> bcDefault;
    if (speciesID < 0 || speciesID >= static_cast<int>(pBCs.size()))
      return bcDefault;
    return pBCs[speciesID];
  }
};

//===========================================================================
/// Parameter container for the test-particle (ParticleTracker) component.
///
/// Holds all test particle settings parsed by Domain during read_param();
/// ParticleTracker reads its configuration back from this object.
/// Species-dependent quantities are resolved in post_process_param().
//===========================================================================
class ParticleTrackerInfo {
public:
  amrex::IntVect nTPPerCell = { AMREX_D_DECL(1, 1, 1) };
  amrex::IntVect nTPIntervalCell = { AMREX_D_DECL(1, 1, 1) };

  std::string sIOUnit = "planet";
  bool isRelativistic = false;

  amrex::Vector<std::string> listFiles;
  bool doInitFromPIC = false;

  // Test-particle velocity states.  Optionally given in SI units and
  // converted to normalized units during read_param() via fi->get_Si2NoV().
  amrex::Vector<Vel> tpStates;

  std::string sRegion = "";

  // Sized to the number of species in post_process_param(); filled by #TPSAVE.
  amrex::Vector<int> dnSave;
  amrex::Vector<amrex::Real> launchThreshold;

  // Per-species particle counts used by restart (#TESTPARTICLENUMBER).
  amrex::Vector<unsigned long int> initPartNumber;

  void set_fluid_interface(FluidInterface* in) { fi = in; }

  void read_param(const std::string& command, ReadParam& param) {
    if (command == "#TPPARTICLES") {
      param.read_var("npcelx", nTPPerCell[ix_]);
      param.read_var("npcely", nTPPerCell[iy_]);
      if (nDim == 3)
        param.read_var("npcelz", nTPPerCell[iz_]);
    } else if (command == "#TPCELLINTERVAL") {
      param.read_var("nIntervalX", nTPIntervalCell[ix_]);
      param.read_var("nIntervalY", nTPIntervalCell[iy_]);
      if (nDim == 3)
        param.read_var("nIntervalZ", nTPIntervalCell[iz_]);
    } else if (command == "#TPREGION") {
      param.read_var("region", sRegion);
    } else if (command == "#TPSAVE") {
      int iSpecies;
      param.read_var("iSpecies", iSpecies);
      if (iSpecies < 0)
        amrex::Abort(
            "Error [ParticleTrackerInfo]: iSpecies must be >= 0 in #TPSAVE.");
      // Defer the final bounds check against nSpecies to post_process_param();
      // grow the vectors here so #TPSAVE may appear before #PLASMA.
      if (iSpecies >= (int)dnSave.size())
        dnSave.resize(iSpecies + 1, 10);
      if (iSpecies >= (int)launchThreshold.size())
        launchThreshold.resize(iSpecies + 1, 0.5);
      param.read_var("IOUnit", sIOUnit);
      param.read_var("dnSave", dnSave[iSpecies]);
      param.read_var("launchThreshold", launchThreshold[iSpecies]);
    } else if (command == "#TPRELATIVISTIC") {
      param.read_var("isRelativistic", isRelativistic);
    } else if (command == "#TPSTATESI") {
      if (!fi)
        amrex::Abort("Error [ParticleTrackerInfo]: #TPSTATESI requires a "
                     "FluidInterface to be set.");
      double si2noV = fi->get_Si2NoV();
      int nState;
      param.read_var("nState", nState);
      tpStates.clear();
      tpStates.reserve(nState);
      for (int i = 0; i < nState; ++i) {
        Vel state;
        param.read_var("iSpecies", state.tag);
        param.read_var("vth", state.vth);
        param.read_var("vx", state.vx);
        param.read_var("vy", state.vy);
        param.read_var("vz", state.vz);
        state.vth *= si2noV;
        state.vx *= si2noV;
        state.vy *= si2noV;
        state.vz *= si2noV;
        tpStates.push_back(state);
      }
    } else if (command == "#TPINITFROMPIC") {
      param.read_var("doInitFromPIC", doInitFromPIC);
      if (doInitFromPIC) {
        int nList;
        param.read_var("nList", nList);
        listFiles.clear();
        listFiles.reserve(nList);
        for (int i = 0; i < nList; ++i) {
          std::string s;
          param.read_var("list", s);
          listFiles.push_back(s);
        }
      }
    } else if (command == "#TESTPARTICLENUMBER") {
      initPartNumber.clear();
      int nS = fi ? fi->get_nS() : 0;
      for (int iPart = 0; iPart < nS; iPart++) {
        unsigned long int num;
        param.read_var("Number", num);
        initPartNumber.push_back(num);
      }
    }
  }

  // Resolve species-dependent quantities.  MUST be called after fi has been
  // fully processed (fi->post_process_param) so fi->get_nS() is final.
  void post_process_param() {
    if (!fi)
      amrex::Abort("Error [ParticleTrackerInfo]: post_process_param called "
                   "before the FluidInterface was set.");
    const int nS = fi->get_nS();

    if (dnSave.empty()) {
      dnSave.assign(nS, 10);
    } else if ((int)dnSave.size() < nS) {
      dnSave.resize(nS, 10);
    } else if ((int)dnSave.size() > nS) {
      amrex::Abort("Error [ParticleTrackerInfo]: #TPSAVE iSpecies exceeds the "
                   "number of species.");
    }

    if (launchThreshold.empty()) {
      launchThreshold.assign(nS, 0.5);
    } else if ((int)launchThreshold.size() < nS) {
      launchThreshold.resize(nS, 0.5);
    } else if ((int)launchThreshold.size() > nS) {
      amrex::Abort("Error [ParticleTrackerInfo]: #TPSAVE iSpecies exceeds the "
                   "number of species.");
    }
  }

private:
  FluidInterface* fi = nullptr;
};

template <int NStructReal, int NStructInt>
class ParticlesIter : public amrex::ParIter<NStructReal, NStructInt> {
public:
  using amrex::ParIter<NStructReal, NStructInt>::ParIter;
};

/*
Q: How it the Grid* gridIn variable used inside the particle container?
A: The following codes show how gridIn is passed into the particle container.

  1.
    AmrParticleContainer (AmrCore* amr_core)
        : ParticleContainer<NStructReal, NStructInt, NArrayReal, NArrayInt,
Allocator>(amr_core->GetParGDB()){ }

  2.
      ParticleContainer (ParGDBBase* gdb):
        ParticleContainerBase(gdb)

  3.
      ParticleContainerBase (ParGDBBase* gdb)
        :
        m_verbose(0),
        m_gdb(gdb)
    {}

But, what is amr_core->GetParGDB()? It returns a AmrParGDB pointer that is
pointing to AmrCore::m_gdb. AmrParGDB class (object AmrCore::m_gdb) contains a
member variable  AmrCore* m_amrcore, which is a copy of amr_core pointer. So,
the Geometries, DistributionMaps and BoxArrays of Grid can be accessed by the
particle container through m_gdb pointer.

In short, once the grids or distributions maps of Pic or ParticleTracker change,
the particle contains aware of the changes through the m_gdb pointer.
*/

// Forward declaration.
template <int NStructReal, int NStructInt> class Particles;

using PicParticles = Particles<nPicPartReal, nPicPartInt>;
using PTParticles = Particles<nPTPartReal, nPTPartInt>;

template <int NStructReal, int NStructInt>
class Particles : public amrex::AmrParticleContainer<NStructReal, NStructInt> {
public:
  // Since this is a template, the compiler will not search names in the base
  // class by default, and the following 'using ' statements are required.
  using ParticleType = amrex::Particle<NStructReal, NStructInt>;
  using ParticleTileType = amrex::ParticleTile<ParticleType, 0, 0>;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::Geom;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::do_tiling;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::tile_size;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::SetUseUnlink;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::GetParticles;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::MakeMFIter;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::Redistribute;
  using amrex::AmrParticleContainer<NStructReal,
                                    NStructInt>::NumberOfParticlesAtLevel;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::Checkpoint;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::Index;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::ParticlesAt;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::maxLevel;
  using amrex::AmrParticleContainer<NStructReal, NStructInt>::GetParGDB;
  using amrex::AmrParticleContainer<NStructReal,
                                    NStructInt>::CreateGhostParticles;
  using amrex::AmrParticleContainer<NStructReal,
                                    NStructInt>::CreateVirtualParticles;
  using amrex::AmrParticleContainer<NStructReal,
                                    NStructInt>::AddParticlesAtLevel;

  using AoS = amrex::ArrayOfStructs<ParticleType>;

  using PIter = ParticlesIter<NStructReal, NStructInt>;

protected:
  Grid* grid = nullptr;

  FluidInterface* fi = nullptr;
  TimeCtr* tc = nullptr;

  PartMode pMode = PartMode::PIC;

  int speciesID;
  RandNum randNum;
  amrex::Real charge;
  amrex::Real mass;

  amrex::Real qom;
  int qomSign;

  amrex::IntVect nPartPerCell;

  // Fractional-particle accumulators for the inflow flux injector (see
  // inject_flux_at_inflow_faces). Key packs (iLev, iDim, side, j, k) of the
  // boundary-transverse cell; the value is the not-yet-injected fractional
  // macroparticle count carried between steps.
  std::map<int64_t, amrex::Real> injectFluxAcc;

  amrex::Vector<amrex::RealVect> plo, phi, dx, invDx;
  amrex::Vector<amrex::Real> invVol;

  // ------- Particle resampling begin -------
  amrex::Real mergeThresholdDistance = 0.6;
  amrex::Real velBinBufferSize = 0.125;

  // If fastMerge == false: find the particle pair that is closest to each other
  // in the phase space and try to delete the lighter one.
  // If fastMerge == true: merge nPartCombineMax particles into nPartNew with
  // Lagrange multiplier method.
  bool fastMerge = false;
  int nPartCombine = 6;
  int nPartNew = 5;
  int nMergeTry = 3;
  amrex::Real mergeRatioMax = 1.5;

  bool mergeLight = false;
  amrex::Real mergePartRatioMax = 10;
  // ------- Particle resampling end -------

  amrex::Real pLevRatio = 1.2;

  amrex::Real vacuum = 0;

  bool isRelativistic = false;

  bool isParticleLocationRandom = true;

  bool isPPVconstant = false;

  bool doPreSplitting = false;

  bool isTargetPPCDefined = false;

  BoxBC<ParticleBC::Type> bc; // particle boundary condition

  // Faces carrying a FieldBC::wave *field* boundary, indexed 2*d + {0=lo,1=hi}.
  // Set by Pic so the wave velocity kick is driven by the field boundary
  // instead of a particle-side spelling -- the particle domain has no `wave`
  // type of its own.
  bool isWaveFace[6] = { false, false, false, false, false, false };

  // Absorbing-BC tallies per face (2*d + {0=lo,1=hi}).
  amrex::Real absorbTallyCount[6] = { 0, 0, 0, 0, 0, 0 };
  amrex::Real absorbTallyCharge[6] = { 0, 0, 0, 0, 0, 0 };
  amrex::Real absorbTallyMass[6] = { 0, 0, 0, 0, 0, 0 };

  // AMREX uses 40 bits(it is 40! Not a typo. See AMReX_Particle.H) to store
  // p.id(), but it is converted to a 32-bit integer when saving to disk. To
  // avoid the mismatch, FLEKS set the maximum value of p.id() to 2^31-1, and
  // introduce a new integer 'supID' to avoid the overflow of p.id(). See
  // set_ids() below. In short, a FLEKS particle is identified by p.cpu(),
  // p.id() and p.idata(iSupID_).
  int supID = 1;

  OHIon ionOH;

  bool isFake2D;

public:
  static constexpr int iup_ = 0;
  static constexpr int ivp_ = 1;
  static constexpr int iwp_ = 2;
  static constexpr int iqp_ = 3;

  // mu = cos(theta), theta is the pitch angle.
  static constexpr int imu_ = 4;

  // Non-owning pointer to the active initial condition (owned by Pic).
  const InitialCondition* ic_ = nullptr;

  // Wave bulk-velocity kick (pos, t -> dvx,dvy,dvz), set by Pic; null = off.
  std::function<void(const amrex::Real*, amrex::Real, amrex::Real&,
                     amrex::Real&, amrex::Real&)>
      waveVelocityKick = nullptr;

  // Index of the integer data.
  static constexpr int iRecordCount_ = 1;

  Particles(Grid* gridIn, FluidInterface* fluidIn, TimeCtr* tcIn,
            const int speciesIDIn, const amrex::Real chargeIn,
            const amrex::Real massIn, const ParticlesInfo& pInfo,
            const PartMode pModeIn, const InitialCondition* icIn = nullptr);

  int n_lev() const { return GetParGDB()->finestLevel() + 1; }

  int n_lev_max() const { return maxLevel() + 1; }

  void add_particles_domain();
  void add_particles_cell(const int iLev, const amrex::MFIter& mfi,
                          const amrex::IntVect ijk,
                          const FluidInterface* interface, bool doVacuumLimit,
                          amrex::IntVect ppc = amrex::IntVect(),
                          const Vel& tpVel = Vel(), amrex::Real dt = -1);
  void inject_particles_at_boundary();

  // Inflow (ParticleBC::inflow) particle boundary
  // New macroparticles are created AT the physical boundary face, their
  // normal velocity is drawn from the flux-weighted (half-space) drifting
  // Maxwellian, the transverse velocities from the corresponding Gaussians,
  // and the number injected per step matches the analytic influx of the
  // prescribed #INFLOW state.
  void inject_flux_at_inflow_faces(amrex::Real dt);

  void add_particles_source(const FluidInterface* interface,
                            const FluidInterface* const stateOH = nullptr,
                            amrex::Real dt = -1,
                            amrex::IntVect ppc = amrex::IntVect(),
                            const bool doSelectRegion = false,
                            const bool adaptivePPC = false);

  // Copy particles from (ip,jp,kp) to (ig, jg, kg) and shift boundary
  // particle's coordinates accordingly.
  void outflow_bc(const amrex::MFIter& mfi, const amrex::IntVect ijkGst,
                  const amrex::IntVect ijkPhy);

  // 1) Only inject particles ONCE for one ghost cells. This function decides
  // which block injects particles. 2) bx should be a valid box 3) The cell
  // (i,j,k) can NOT be the outmost ghost cell layer!!!!
  bool do_inject_particles_for_this_cell(const amrex::Box& bx,
                                         const amrex::Array4<const int>& status,
                                         const amrex::IntVect ijk,
                                         amrex::IntVect& ijksrc);

  amrex::Real sum_moments(amrex::Vector<amrex::MultiFab>& momentsMF,
                          amrex::Vector<amrex::MultiFab>& nodeBMF,
                          amrex::Real dt);

  // Cell-centred moment deposit: scatter rho/rhoU/Pi from particles to the
  // momentsMF (centerPlasma[iSpecies]) with trilinear weights.
  amrex::Real sum_moments_cell_centered(
      amrex::Vector<amrex::MultiFab>& momentsMF);

  std::array<amrex::Real, 5> total_moments(bool localOnly = false);

  void calc_mass_matrix(amrex::UMultiFab<RealMM>& nodeMM, amrex::MultiFab& jHat,
                        amrex::MultiFab& nodeBMF, amrex::MultiFab& u0MF,
                        amrex::Real dt, int iLev, bool solveInCoMov);

  void calc_mass_matrix_amr(
      amrex::UMultiFab<RealMM>& nodeMM,
      amrex::Vector<amrex::Vector<amrex::UMultiFab<RealMM> > >& nmmc,
      amrex::Vector<amrex::UMultiFab<RealMM> >& nmmf, amrex::MultiFab& jHat,
      amrex::Vector<amrex::Vector<amrex::MultiFab> >& jhc,
      amrex::Vector<amrex::MultiFab>& jhf, amrex::MultiFab& nodeBMF,
      amrex::MultiFab& u0MF, amrex::Real dt, int iLev, bool solveInCoMov,
      amrex::Vector<amrex::iMultiFab>& cellstatus);

  void calc_jhat(amrex::MultiFab& jHat, amrex::MultiFab& nodeBMF,
                 amrex::Real dt);

  void apply_jhat_mirror(amrex::MultiFab& jHat, int iLev = 0);

  // It is real 'thermal velocity'. It is sqrt(sum(q*v2)/sum(q)).
  amrex::Real calc_max_thermal_velocity(amrex::MultiFab& momentsMF);

  void sum_to_center(amrex::MultiFab& netChargeMF,
                     amrex::UMultiFab<RealCMM>& centerMM, bool doNetChargeOnly,
                     int iLev);

  void sum_to_center_amr(amrex::MultiFab& netChargeMF, amrex::MultiFab& jc,
                         amrex::MultiFab& jf,
                         amrex::UMultiFab<RealCMM>& centerMM,
                         bool doNetChargeOnly, int iLev);

  void charge_exchange(
      amrex::Real dt, FluidInterface* stateOH, FluidInterface* sourcePT2OH,
      SourceInterface* source, bool kineticSource,
      amrex::Vector<std::unique_ptr<PicParticles> >& sourceParts,
      bool doSelectRegion, int nppc, amrex::Real& maxExchangeRatio);

  /// Apply chemical loss (recombination, etc.) by reducing particle
  /// weights proportionally.  Reads per-ion loss rates from
  /// source->nodeLossFluid and reduces each particle's weight by
  /// fraction = min(lossRate * dt / rhoExisting, 1).
  void apply_loss(const SourceInterface* source, amrex::Real dt);

  void accumulate_mass_matrix_contribution(int iLev,
                                           const amrex::IntVect& loIdx,
                                           const amrex::RealVect& dShift,
                                           amrex::Real qp,
                                           amrex::Array4<RealCMM> const& mmArr);

  void get_ion_fluid(FluidInterface* stateOH, PIter& pti, const int iLev,
                     const int iFluid, const amrex::RealVect xyz,
                     amrex::Real& rhoIon, amrex::Real& cs2Ion,
                     amrex::Real (&uIon)[nDim3]);

  // Input:
  // xyz in NO units.
  // Output:
  // rhoIon: amu/m^3
  // cs2Ion: (m/s)^2
  // uIon: m/s
  void get_analytic_ion_fluid(const amrex::RealVect xyz, amrex::Real& rhoIon,
                              amrex::Real& cs2Ion, amrex::Real (&uIon)[nDim3]);

  void add_source_particles(std::unique_ptr<PicParticles>& sourcePart,
                            amrex::IntVect ppc, const bool adaptivePPC);

  void mover(const amrex::Vector<amrex::MultiFab>& nodeE,
             const amrex::Vector<amrex::MultiFab>& nodeB,
             const amrex::Vector<amrex::MultiFab>& eBg,
             const amrex::Vector<amrex::MultiFab>& uBg, amrex::Real dt,
             amrex::Real dtNext);

  // Cell-centred mover dispatch: gathers from the centerE/centerB
  // (rather than the node fields) using a trilinear gather.
  void mover_cell_centered(const amrex::Vector<amrex::MultiFab>& centerE,
                           const amrex::Vector<amrex::MultiFab>& centerB,
                           const amrex::Vector<amrex::MultiFab>& eBg,
                           const amrex::Vector<amrex::MultiFab>& uBg,
                           amrex::Real dt, amrex::Real dtNext);

  void charged_particle_mover(const amrex::Vector<amrex::MultiFab>& nodeE,
                              const amrex::Vector<amrex::MultiFab>& nodeB,
                              const amrex::Vector<amrex::MultiFab>& eBg,
                              const amrex::Vector<amrex::MultiFab>& uBg,
                              amrex::Real dt, amrex::Real dtNext);

  // Cell-centred particle gather + Boris push, reading E and B from the
  // fields (centerE / centerB) using a trilinear gather.
  void charged_particle_mover_cell_centered(
      const amrex::Vector<amrex::MultiFab>& centerE,
      const amrex::Vector<amrex::MultiFab>& centerB,
      const amrex::Vector<amrex::MultiFab>& eBg,
      const amrex::Vector<amrex::MultiFab>& uBg, amrex::Real dt,
      amrex::Real dtNext);

  // select particles based on input supid and id
  void select_particle(amrex::Vector<std::array<int, 3> >& selectParticleIn);

  // Both the input are in the SI unit: m/s
  amrex::Real charge_exchange_dis(amrex::Real* vp, amrex::Real* vh,
                                  amrex::Real* up, amrex::Real vth,
                                  CrossSection cs);

  void sample_charge_exchange(amrex::Real* vp, amrex::Real* vh, amrex::Real* up,
                              amrex::Real vth, CrossSection cs);

  void neutral_mover(amrex::Real dt);

  // Returns true if a pushed particle should be deleted.  `absorb` removes and
  // tallies; `reflect` mirrors.  Only acts at iLev == 0.
  inline bool reflect_or_delete_particle(ParticleType& p,
                                         amrex::Array4<int const> const& status,
                                         const amrex::IntVect& low,
                                         const amrex::IntVect& high, int iLev) {
    if (iLev > 0)
      return is_outside_active_region(p, status, low, high, iLev);

    const amrex::Real* plo = Geom(iLev).ProbLo();
    const amrex::Real* phi = Geom(iLev).ProbHi();
    for (int d = 0; d < nDim; ++d) {
      const int bcLo = bc.lo[d];
      const int bcHi = bc.hi[d];
      // Absorbing and inflow faces remove particles that cross outward,
      // tallying the lost charge/mass per face.
      if ((bcLo == ParticleBC::absorb || bcLo == ParticleBC::inflow) &&
          p.pos(d) < plo[d]) {
        absorb_tally(2 * d, p.rdata(iwp_));
        return true;
      }
      if ((bcHi == ParticleBC::absorb || bcHi == ParticleBC::inflow) &&
          p.pos(d) > phi[d]) {
        absorb_tally(2 * d + 1, p.rdata(iwp_));
        return true;
      }
      // Specular reflection: mirror position and normal velocity.
      if (bcLo == ParticleBC::reflect && p.pos(d) < plo[d]) {
        p.pos(d) = 2.0 * plo[d] - p.pos(d);
        p.rdata(iup_ + d) = -p.rdata(iup_ + d);
      } else if (bcHi == ParticleBC::reflect && p.pos(d) > phi[d]) {
        p.pos(d) = 2.0 * phi[d] - p.pos(d);
        p.rdata(iup_ + d) = -p.rdata(iup_ + d);
      }
    }
    return is_outside_active_region(p, status, low, high, iLev);
  }

  // Tally an absorbed particle per face (2*d + {0=lo,1=hi}).
  inline void absorb_tally(int face, amrex::Real weight) {
    absorbTallyCount[face] += 1.0;
    absorbTallyCharge[face] += weight * charge;
    absorbTallyMass[face] += weight * mass;
  }

  void update_position_to_half_stage(const amrex::MultiFab& nodeEMF,
                                     const amrex::MultiFab& nodeBMF,
                                     amrex::Real dt);

  void convert_to_fluid_moments(amrex::Vector<amrex::MultiFab>& momentsMF);

  PartMode part_mode() const { return pMode; }

  static inline bool compare_two_parts(const ParticleType& pl,
                                       const ParticleType& pr) {
    // It is non-trivial to compare floating point numbers. If there is
    // significant difference between the two floating point numbers, the
    // comparison is based on the floating point numbers. Otherwise, the
    // comparison is based on the integer numbers (particle ids). However,
    // different number of processors may have different results for id
    // comparison.
    if (fabs(pl.pos(ix_) - pr.pos(ix_)) >
        1e-9 * (fabs(pl.pos(ix_)) + fabs(pr.pos(ix_)))) {
      return pl.pos(ix_) > pr.pos(ix_);
    }

    if (fabs(pl.rdata(iup_) - pr.rdata(iup_)) >
        1e-9 * (fabs(pl.rdata(iup_)) + fabs(pr.rdata(iup_)))) {
      return pl.rdata(iup_) > pr.rdata(iup_);
    }

    return false;
  }

  amrex::Real cosine(ParticleType& p, amrex::Real (&bIn)[nDim3]) {
    amrex::Real u[nDim3];
    amrex::Real b[nDim3];
    for (int i = 0; i < nDim3; ++i) {
      u[i] = p.rdata(iup_ + i);
      b[i] = bIn[i];
    }
    amrex::Real mu = 0;
    for (int i = 0; i < nDim; ++i)
      mu += u[i] * b[i];

    const amrex::Real bNorm = l2_norm(b, nDim3);
    const amrex::Real uNorm = l2_norm(u, nDim3);

    const amrex::Real invB = bNorm > 1e-99 ? 1.0 / bNorm : 0;
    const amrex::Real invU = uNorm > 1e-99 ? 1.0 / uNorm : 0;

    return mu * invB * invU;
  }

  IDs get_next_ids() {
    constexpr long idMax = 2147483647L;
    long id = ParticleType::NextID();
    if (id > idMax) {
      id = 1;
      ParticleType::NextID(id);
      supID++;
    }

    IDs ids = { static_cast<int>(id), supID };
    return ids;
  }

  /**
   * @brief Creates a particle with a fully initialized object representation.
   *
   * AMReX serializes the complete particle object during redistribution,
   * including any alignment padding. Value initialization prevents undefined
   * padding bytes from entering MPI send buffers.
   */
  [[nodiscard]] static ParticleType make_particle() noexcept {
    return ParticleType{};
  }

  /**
   * @brief Sets the IDs for a particle.
   *
   * This function sets the unique ID, supplementary ID, and CPU ID for the
   * given particle.
   *
   * @param p The particle for which the IDs are to be set.
   */
  void set_ids(ParticleType& p) {
    auto ids = get_next_ids();
    p.id() = ids.id;
    p.idata(iSupID_) = ids.supID;
    p.cpu() = amrex::ParallelDescriptor::MyProc();
  }

  int sup_id() const { return supID; }
  void set_sup_id(int in) { supID = in; }

  amrex::IntVect get_ref_ratio(const int iLev) const {
    const amrex::ParGDBBase* gdb = GetParGDB();
    return gdb->refRatio(iLev);
  }

  // This function distributes particles to proper processors and apply
  // periodic boundary conditions if needed.
  void redistribute_particles() {
    const amrex::ParGDBBase* gdb = GetParGDB();

    if (!gdb->boxArray(0).empty()) {
      // It will crash if there is no active cells.
      Redistribute();
    }
  }

  long calc_random_seed(const int iLev, const amrex::IntVect ijk,
                        const amrex::IntVect nPPC) {
    amrex::IntVect nCell = Geom(iLev).Domain().size();

    int nRandom = 7;

    int nxcg = nCell[ix_];
    int nycg = nCell[iy_];
    int nzcg = 1;
    if (nDim > 2)
      nzcg = nCell[iz_];

    int iCycle = tc->get_cycle();

    int i = ijk[0];
    int j = ijk[1];
    int k = nDim > 2 ? ijk[2] : 0;

    const long nCellOffset = static_cast<long>(nxcg) * nycg * nzcg * iCycle +
                             static_cast<long>(nycg) * nzcg * i +
                             static_cast<long>(nzcg) * j + k;
    const long seed = static_cast<long>(speciesID + 3) * nRandom *
                      product(nPPC) * nCellOffset;
    return seed;
  }

  long set_random_seed(const int iLev, const amrex::IntVect ijk,
                       const amrex::IntVect nPPC) {
    long seed = calc_random_seed(iLev, ijk, nPPC);
    randNum.set_seed(seed);
    return seed;
  }

  const amrex::iMultiFab& cell_status(int iLev) const {
    return grid->cell_status(iLev);
  }

  const amrex::iMultiFab& node_status(int iLev) const {
    return grid->node_status(iLev);
  }

  const amrex::iMultiFab& target_PPC(int iLev) const {
    return grid->target_PPC(iLev);
  }

  ParticleTileType& get_particle_tile(int iLev, const amrex::MFIter& mfi,
                                      const amrex::IntVect& iv) {
    amrex::Box tileBox;
    const int tileIdx =
        getTileIndex(iv, mfi.validbox(), do_tiling, tile_size, tileBox);
    return GetParticles(iLev)[std::make_pair(mfi.index(), tileIdx)];
  }

  ParticleTileType& get_particle_tile(int iLev, const amrex::MFIter& mfi) {
    return GetParticles(
        iLev)[std::make_pair(mfi.index(), mfi.LocalTileIndex())];
  }

  void set_ppc(amrex::IntVect& in) { nPartPerCell = in; };

  void set_bc(const BoxBC<ParticleBC::Type>& bcIn) { bc = bcIn; }

  // Mark face `side` (0 = lo, 1 = hi) of dimension `d` as carrying a
  // FieldBC::wave field boundary; drives the wave velocity kick.
  void set_wave_face(const int d, const int side, const bool on) {
    if (d >= 0 && d < 3 && side >= 0 && side < 2)
      isWaveFace[2 * d + side] = on;
  }

  void set_is_target_ppc_defined(bool in) { isTargetPPCDefined = in; }

  inline bool is_outside_active_region(const ParticleType& p, int iLev) {
    if (iLev > 0) {
      return false;
    }
    amrex::RealVect loc;
    for (int iDim = 0; iDim < nDim; iDim++) {
      loc[iDim] = p.pos(iDim);
      if (Geom(iLev).isPeriodic(iDim)) {
        // Fix index/loc for periodic BC.
        while (loc[iDim] > phi[iLev][iDim])
          loc[iDim] -= phi[iLev][iDim] - plo[iLev][iDim];
        while (loc[iDim] < plo[iLev][iDim])
          loc[iDim] += phi[iLev][iDim] - plo[iLev][iDim];
      }
    }

    return !grid->is_inside_domain(loc.begin());
  }

  inline bool is_outside_level(const ParticleType& p, int iLev,
                               amrex::Array4<int const> const& status,
                               const amrex::IntVect cellIdx) {
    bool isOutsideLevel = false;
    if (bit::is_refined(status(cellIdx)) ||
        bit::is_lev_boundary(status(cellIdx))) {
      isOutsideLevel = true;
    }

    return isOutsideLevel;
  }

  /**
   * @brief Checks if a particle is outside the active region at a given level.
   *
   * This function determines whether a particle is outside the active region
   * at a specified level (`iLev`). It takes into account periodic boundary
   * conditions and adjusts the particle's position accordingly.
   *
   * @param p The particle to check.
   * @param iLev The level at which to check the particle's position.
   * @return True if the particle is outside the active region, false otherwise.
   */
  inline bool is_outside_active_region(const ParticleType& p,
                                       amrex::Array4<int const> const& status,
                                       const amrex::IntVect& low,
                                       const amrex::IntVect& high, int iLev) {

    // TODO: It does not work with AMR.
    // Contains ghost cells.
    if (iLev > 0) {
      return false;
    }
    bool isInsideBox = true;
    amrex::IntVect cellIdx;
    amrex::RealVect dShift;
    for (int i = 0; i < nDim; ++i) {
      dShift[i] = (p.pos(i) - plo[iLev][i]) * invDx[iLev][i];
      cellIdx[i] = fastfloor(dShift[i]);
      if (cellIdx[i] > high[i] || cellIdx[i] < low[i]) {
        isInsideBox = false;
        break;
      }
    }

    if (isInsideBox) {
      return bit::is_domain_boundary(status(cellIdx));
    } else {
      return is_outside_active_region(p, iLev);
    }
  }

  inline void label_particles_outside_active_region() {
    for (int iLev = 0; iLev < n_lev(); iLev++)
      if (NumberOfParticlesAtLevel(iLev, true, true) > 0) {
        for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
          AoS& particles = pti.GetArrayOfStructs();
          if (cell_status(iLev).empty()) {
            for (auto& p : particles) {
              p.id() = -1;
            }
          } else {
            const amrex::Box& bx = cell_status(iLev)[pti].box();
            const amrex::Array4<int const>& status =
                cell_status(iLev)[pti].array();

            const amrex::IntVect lowCorner = bx.smallEnd();
            const amrex::IntVect highCorner = bx.bigEnd();

            for (auto& p : particles) {
              if (is_outside_active_region(p, status, lowCorner, highCorner,
                                           iLev)) {
                p.id() = -1;
              }
            }
          }
        }
      }
  }

  inline void label_particles_outside_active_region_general() {
    for (int iLev = 0; iLev < n_lev(); iLev++)
      if (NumberOfParticlesAtLevel(iLev, true, true) > 0) {
        for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
          AoS& particles = pti.GetArrayOfStructs();
          for (auto& p : particles) {
            if (is_outside_active_region(p, iLev)) {
              p.id() = -1;
            }
          }
        }
      }
  }

  void limit_weight(amrex::Real maxRatio, bool seperateVelocity = false);
  void limit_weight_new(amrex::Real maxRatio, bool seperateVelocity = false);
  void split(amrex::Real limit, bool seperateVelocity = false);
  void split_new(amrex::Real limit, bool seperateVelocity = false);
  void split_particles_by_velocity(amrex::Vector<ParticleType*>& plist,
                                   amrex::Vector<ParticleType>& newparticles);
  bool split_by_seperate_velocity(ParticleType& p1, ParticleType& p2,
                                  ParticleType& p3, ParticleType& p4);
  void merge(amrex::Real limit);
  void merge_new(amrex::Real limit);

  // Generic tool: add a circularly-polarized velocity perturbation to every
  // particle already in the container:
  //   dv_y += ampY * cos(kx * x),   dv_z += ampZ * sin(kx * x).
  // Names no test case; an InitialCondition plug-in that needs to perturb an
  // existing particle population (rather than seed it at creation time via the
  // per-particle hooks) can call this.
  void add_velocity_perturbation(amrex::Real ampY, amrex::Real ampZ,
                                 amrex::Real kx);
  bool merge_particles_fast(int iLev, AoS& particles,
                            amrex::Vector<int>& partIdx,
                            amrex::Vector<int>& idx_I, int nPartCombine,
                            int nPartNew, amrex::Vector<amrex::Real>& x,
                            long seed);

  bool merge_particles_accurate(int iLev, AoS& particles,
                                amrex::Vector<int>& partIdx,
                                amrex::Vector<int>& idx_I, int nPartCombine,
                                int nPartNew, amrex::Vector<amrex::Real>& x,
                                amrex::Real velNorm);

  void divE_correct_position(const amrex::Vector<amrex::MultiFab>& phiMF,
                             int iLev);

  bool is_neutral() const { return charge == 0; };

  int get_speciesID() const { return speciesID; }
  amrex::Real get_charge() const { return charge; }
  amrex::Real get_mass() const { return mass; }

  // Absorbing-BC diagnostics (per face, 2*d + {0=lo,1=hi}).
  amrex::Real get_absorb_count(int face) const {
    return absorbTallyCount[face];
  }
  amrex::Real get_absorb_charge(int face) const {
    return absorbTallyCharge[face];
  }
  amrex::Real get_absorb_mass(int face) const { return absorbTallyMass[face]; }

  void set_relativistic(const bool& in) { isRelativistic = in; }

  void Write_Paraview(std::string folder = "Particles",
                      std::string particletype = "1") {
    // redistribute_particles();
    std::string command = "python "
                          "../util/AMREX/Tools/Py_util/amrex_particles_to_vtp/"
                          "amrex_binary_particles_to_vtp.py";
    Checkpoint(folder, particletype);
    command = command + " " + folder + " " + particletype;
    if (amrex::ParallelDescriptor::IOProcessor()) {
      int result = std::system(command.c_str());
      if (result != 0) {
        std::cerr << "Error executing command: " << command << std::endl;
      }
    }
    command = "mv";
    command = command + " " + folder + ".vtp" + " " + folder + "_" +
              particletype + ".vtp";
    if (amrex::ParallelDescriptor::IOProcessor()) {
      int result = std::system(command.c_str());
      if (result != 0) {
        std::cerr << "Error executing command: " << command << std::endl;
      }
    }
    command = "rm -rf";
    command = command + " " + folder;
    if (amrex::ParallelDescriptor::IOProcessor()) {
      int result = std::system(command.c_str());
      if (result != 0) {
        std::cerr << "Error executing command: " << command << std::endl;
      }
    }
  }

  void Write_Binary(std::string folder = "Particles",
                    std::string particletype = "1") {
    Checkpoint(folder, particletype);
  }

  void Generate_GhostParticles(int iLev, int nGhost) {
    ParticleTileType ptile;
    CreateGhostParticles(iLev - 1, nGhost, ptile);
    AddParticlesAtLevel(ptile, iLev, nGhost);
  }

  void Generate_VirtualParticles(int iLev) {
    ParticleTileType ptile;
    CreateVirtualParticles(iLev + 1, ptile);
    AddParticlesAtLevel(ptile, iLev);
  }

  void Exchange_VirtualParticles(int iLev) {
    ParticleTileType ptile;
    ParticleTileType ptile2;
    CreateVirtualParticles(iLev + 1, ptile);
    CreateGhostParticles(iLev, 1, ptile2);
    AddParticlesAtLevel(ptile, iLev);
    AddParticlesAtLevel(ptile2, iLev + 1, 1);
  }

  void delete_particles_from_refined_region(int iLev) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      AoS& particles = pti.GetArrayOfStructs();
      const auto& status = cell_status(iLev)[pti].array();
      for (auto& p : particles) {
        amrex::IntVect loIdx;
        amrex::RealVect dShift;
        find_cell_index_exp(p.pos(), Geom(iLev).ProbLo(),
                            Geom(iLev).InvCellSize(), loIdx, dShift);
        if (bit::is_refined(status(loIdx))) {
          p.id() = -1;
        }
      }
    }
  }

  void delete_particles_from_ghost_cells(int iLev) {
    for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
      AoS& particles = pti.GetArrayOfStructs();
      const auto& status = cell_status(iLev)[pti].array();
      for (auto& p : particles) {
        amrex::IntVect loIdx;
        amrex::RealVect dShift;
        find_cell_index_exp(p.pos(), Geom(iLev).ProbLo(),
                            Geom(iLev).InvCellSize(), loIdx, dShift);
        if (bit::is_lev_boundary(status(loIdx))) {
          p.id() = -1;
        }
      }
    }
  }
  void calculate_particle_quality(amrex::Vector<amrex::MultiFab>& quality);
};

class IOParticles : public PicParticles {
public:
  IOParticles() = delete;

  IOParticles(PicParticles& other, Grid* gridIn, amrex::Real no2outL,
              amrex::Real no2outV, amrex::Real no2OutM, amrex::RealBox IORange);
  ~IOParticles() = default;
};
#endif
