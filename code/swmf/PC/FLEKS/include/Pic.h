#ifndef _PIC_H_
#define _PIC_H_

#include <iostream>
#include <set>
#include <string>

#include "Array1D.h"
#include "Bit.h"
#include "Constants.h"
#include "DomainParameters.h"
#include "FleksDistributionMap.h"
#include "FluidInterface.h"
#include "Grid.h"
#include "InitialCondition.h"
#include "LinearSolver.h"
#include "OHInterface.h"
#include "Particles.h"
#include "ReadParam.h"
#include "SourceInterface.h"
#include "TimeCtr.h"
#include "UMultiFab.h"
#include "WaveBC.h"

class ParticleTracker;
class Pic;

class FieldSolver {
public:
  amrex::Real theta;
  amrex::Real coefDiff;
  bool useLaggedLimiter;
  FieldSolverMode mode;
  FieldSolver() {
    theta = 0.51;
    coefDiff = 0.1;
    useLaggedLimiter = false;
    mode = FieldSolverMode::GMRES;
  }
};

typedef amrex::Real (Pic::*GETVALUE)(amrex::MFIter &mfi, amrex::IntVect ijk,
                                     int iVar, const int iLev);

typedef void (Pic::*PicWriteAmrex)(const std::string &filename,
                                   const std::string varName);

// The grid is defined in DomainGrid. This class contains the data on the grid.
class Pic : public Grid {
  friend PlotWriter;
  friend ParticleTracker;
  // private variables
private:
  bool usePIC = true;
  bool solveEM = true;
  bool initEM = true;

  // ---- Hybrid PIC (kinetic ions + fluid electrons) solver ----
  bool useHybridPIC = false;
  // Resistive term eta * J. SI input [m^2/s], converted to code units.
  amrex::Real etaResistivitySI = 0.0;
  amrex::Real etaResistivity = 0.0;
  // Electron pressure gradient. Input [eV], converted to code units.
  amrex::Real electronTemperatureEV = 0.0;
  amrex::Real electronTemperature = 0.0;
  // Polytropic index for the adiabatic electron pressure closure.
  amrex::Real electronGamma = 1.0;
  // Reference charge density. Input [amu/cc], converted to code units.
  amrex::Real electronDensity0In = 1.0;
  amrex::Real electronDensity0 = 0.0;
  // Number of sub-steps for the B-field update within one dt.
  int nBSubcycle = 1;
  // Hall term in the generalized Ohm's law.
  bool useHallTerm = true;

  // Hyper-resistivity (fourth-order) term in the Ohm's law:
  //   E -= eta_h * nabla^2 J = -(eta_h/4*pi) * nabla x (nabla^2 B).
  // etaHyperMode selects how etaHyperSI is interpreted:
  //   "si"   -> direct physical value [m^4/s], converted to code units;
  //   "grid" -> CFL-scaled eta_h = C_h * dx^4 / dt_sub (dimless C_h).
  // etaHyperLev[iLev] (code units, 0 disables) is the value actually applied.
  amrex::Real etaHyperSI = 0.0;
  std::string etaHyperMode = "si";
  amrex::Real etaHyperCh = 0.01;
  amrex::Vector<amrex::Real> etaHyperLev;

  // Minimum charge density in the Hall and electron pressure gradient term.
  // <= 0 means auto: 1e-6 * electronDensity0.
  amrex::Real rhoMinOhm = 0.0;

  bool useExplicitPIC = false;
  bool projectDownEmFields = true;
  bool skipMassMatrix = false;
  bool reportParticleQuality = false;

  PartMode pMode = PartMode::PIC;

  FluidInterface *fi = nullptr;
  FluidInterface *stateOH = nullptr;
  FluidInterface *sourcePT2OH = nullptr;
  SourceInterface *source = nullptr;

  // Wave-injection boundary-condition manager (#WAVEBC).
  WaveBoundaryManager waveBC;
  TimeCtr *tc = nullptr;

  const DomainParameters &domainParameters;

  amrex::Vector<amrex::MultiFab> nodeE;
  amrex::Vector<amrex::MultiFab> nodeEth;
  amrex::Vector<amrex::MultiFab> nodeB;
  amrex::Vector<amrex::MultiFab> divB;
  amrex::Vector<amrex::MultiFab> centerB;
  // Hybrid hyper-resistivity scratch fields.
  amrex::Vector<amrex::MultiFab> centerLapB; // nabla^2 B  (stage A)
  amrex::Vector<amrex::MultiFab> nodeHyperE; // nabla x (nabla^2 B) (stage B)
  // Hybrid RK method shared intermediate solver scratch.
  amrex::Vector<amrex::MultiFab> centerBstage;
  // kStage[iLev][0..3]: the stage curls curl(E_stage) for the level-iLev.
  amrex::Vector<amrex::Vector<amrex::MultiFab> > kStage;

  // RK persistent scratch.
  amrex::Vector<amrex::MultiFab> centerBstart;
  amrex::Vector<amrex::MultiFab> centerBstar; // time-centered state used by E

  amrex::Vector<amrex::MultiFab> dBdt;
  amrex::Vector<amrex::MultiFab> particleQuality;

  // Running time-averaged magnetic field for the hybrid solver, only used
  // inside the generalized Ohm's law and the particle Boris push.
  amrex::Vector<amrex::MultiFab> centerBavg; // cell-centred <B>
  amrex::Vector<amrex::MultiFab> nodeBavg;   // node-centred <B>
  bool isBavgInit = false;                   // first-step copy flag for the EMA

  // Hyperbolic cleaning
  bool useHyperbolicCleaning = false;
  amrex::Vector<amrex::MultiFab> hypPhi;
  amrex::Real hypDecay = 0.1;

  // Background velocity and electric field.
  amrex::Vector<amrex::MultiFab> uBg;
  amrex::Vector<amrex::MultiFab> eBg;

  // Mach number: u/v_th
  amrex::Vector<amrex::MultiFab> mMach;

  amrex::Vector<amrex::UMultiFab<RealMM> > nodeMM;

  // ------divE correction--------------
  // Old @ t=t_{n-1/2}; N @ t=t_n; New @ t=t_{n+1/2}
  amrex::Vector<amrex::MultiFab> centerNetChargeOld, centerNetChargeN,
      centerNetChargeNew;
  amrex::Vector<amrex::MultiFab> centerDivE, centerPhi;
  amrex::Vector<amrex::UMultiFab<RealCMM> > centerMM;
  const amrex::Real rhoTheta = 0.51;
  //--------------------------------------

  LinearSolver eSolver;
  LinearSolver divESolver;

  int nSpecies;
  int iTot;

  // Hybrid species IDs
  // iElectron_ (-1 if none); kineticSpecies_ = non-electron species.
  int iElectron_ = -1;
  std::vector<int> kineticSpecies_;
  amrex::Vector<amrex::Vector<amrex::MultiFab> > nodePlasma;
  // Ion moments at J^{n-1/2}; interpolated with current nodePlasma
  // by hstep inside assemble_ohm_E.
  amrex::Vector<amrex::Vector<amrex::MultiFab> > nodePlasmaPrev;
  // ---- Hybrid cell-centred fields ----
  // The hybrid step reads/writes these; nodeE/nodeB/nodePlasma are write-only
  // output mirrors refreshed once per step for plot/restart/tracker paths.
  amrex::Vector<amrex::MultiFab> centerEhybrid;
  amrex::Vector<amrex::MultiFab> centerJ;
  amrex::Vector<amrex::MultiFab> centerEstage; // E at a stage B
  amrex::Vector<amrex::MultiFab> centerHyperE; // hyper-resistivity E
  // Per-species moments
  amrex::Vector<amrex::Vector<amrex::MultiFab> > centerPlasma;
  amrex::Vector<amrex::Vector<amrex::MultiFab> > centerPlasmaSum;
  amrex::Vector<amrex::Vector<amrex::MultiFab> > centerPlasmaPrev;
  amrex::Vector<amrex::Real> plasmaEnergy;

  bool isMomentsUpdated = false;

  amrex::Vector<amrex::MultiFab> jHat;

  amrex::Vector<std::unique_ptr<PicParticles> > parts;
  amrex::Vector<std::unique_ptr<PicParticles> > sourceParts;

  amrex::Real qomEl = -100;

  // Particle Per Cell (PPC) of source particles.
  amrex::IntVect nSourcePPC = { AMREX_D_DECL(0, 0, 0) };
  bool adaptiveSourcePPC = false;
  bool kineticSource = false;
  amrex::Real maxExchangeRatio = 0;
  amrex::Real maxExchangeRatioLimit = 1;

  FieldSolver fsolver;

  bool doCorrectDivE = true;
  int nDivECorrection = 3;

  bool doReSampling = true;
  amrex::Real reSamplingLowLimit = 0.8;
  amrex::Real reSamplingHighLimit = 1.5;
  amrex::Real maxWeightRatio = 1.0;

  bool solveFieldInCoMov = false;
  int nSmoothBackGroundU = 0;

  bool useUpwindE = false;
  amrex::Real limiterThetaE = 0;
  amrex::Real cMaxE = -1;
  bool useUpwindB = false;
  amrex::Real limiterThetaB = 0;
  // Override upwind velocity in correct_B(). 0 = use plasma background
  // velocity.
  amrex::Real fixedUpwindVel = 0.0;

  // Override uMax for CFL estimate. < 0 = estimate from particle thermal
  // velocity.
  amrex::Real fixedUMax = -1.0;

  bool doSmoothJ = false;
  int nSmoothJ = 0;
  amrex::Real coefSmoothJ = 0.5;

  // Smoothing of ion moments before the generalized Ohm's law.
  bool doSmoothMoments = false;
  int nSmoothMoments = 0;
  amrex::Real coefSmoothMoments = 0.5;

  std::string fieldIntegrator = "rk4"; // B integrator
  bool useRK4 = false;

  // Guard: true on the first hybrid step before nodePlasmaPrev is seeded.
  bool isFirstHybridStep = true;

  // EMA-averaged B fed to Ohm's law and Boris push.
  bool useAvgFieldB = false;
  int nAvgFieldB = 10;

  bool doSmoothE = false;
  int nSmoothE = 0;

  // Plug-in initial condition via #TESTCASE registry.
  std::unique_ptr<InitialCondition> ic_;

  ParticlesInfo pInfo;

  BoxBC<FieldBC::Type> bcField;

  // De-duplicated boundary-condition warnings
  std::set<std::string> bcWarnings_;

  // Record a boundary-condition warning; empty messages are ignored.
  void add_bc_warning(const std::string &msg) {
    if (!msg.empty())
      bcWarnings_.insert(msg);
  }

  bool fieldBCSet_ = false;

  // Characteristic speed for the absorbing BC; 0 = auto (light speed).
  amrex::Real absorbCharSpeed = 0.0;

  // Inflow boundary upstream state set by #INFLOW (in code units).
  bool inflowDefined_ = false;
  amrex::Real inflowRho_ = 0.0;
  amrex::Real inflowUx_ = 0.0, inflowUy_ = 0.0, inflowUz_ = 0.0;
  amrex::Real inflowT_ = 0.0;

  // select particle params
  bool doSelectParticle = false;
  std::string selectParticleInputFile;

  bool doReport = false;
  int dnMemory = -1;

  std::string logFile;

  // public methods
public:
  Pic(amrex::Geometry const &gm, amrex::AmrInfo const &amrInfo, int nGst,
      FluidInterface *fluidIn, TimeCtr *tcIn, int id,
      const DomainParameters &parameters)
      : Grid(gm, amrInfo, nGst, id, "pic"),
        fi(fluidIn),
        tc(tcIn),
        domainParameters(parameters) {
    eSolver.set_tol(1e-6);
    eSolver.set_nIter(200);

    divESolver.set_tol(0.01);
    divESolver.set_nIter(20);

    //-----------------------------------------------------
    centerB.resize(n_lev_max());
    nodeB.resize(n_lev_max());
    dBdt.resize(n_lev_max());
    nodeE.resize(n_lev_max());
    nodeEth.resize(n_lev_max());
    divB.resize(n_lev_max());
    hypPhi.resize(n_lev_max());
    centerLapB.resize(n_lev_max());
    nodeHyperE.resize(n_lev_max());
    centerBstage.resize(n_lev_max());
    centerEhybrid.resize(n_lev_max());
    centerJ.resize(n_lev_max());
    centerEstage.resize(n_lev_max());
    centerHyperE.resize(n_lev_max());
    centerBavg.resize(n_lev_max());
    nodeBavg.resize(n_lev_max());
    centerBstart.resize(n_lev_max());
    centerBstar.resize(n_lev_max());
    kStage.resize(n_lev_max());
    for (int iL = 0; iL < n_lev_max(); ++iL)
      kStage[iL].resize(4);
    etaHyperLev.resize(n_lev_max(), 0.0);
    targetPPC.resize(n_lev_max());
    if (reportParticleQuality) {
      particleQuality.resize(n_lev_max());
    }
    eBg.resize(n_lev_max());
    uBg.resize(n_lev_max());

    mMach.resize(n_lev_max());

    centerNetChargeOld.resize(n_lev_max());
    centerNetChargeN.resize(n_lev_max());
    centerNetChargeNew.resize(n_lev_max());

    centerDivE.resize(n_lev_max());
    centerPhi.resize(n_lev_max());

    nodeMM.resize(n_lev_max());
    centerMM.resize(n_lev_max());

    jHat.resize(n_lev_max());

#ifdef _PT_COMPONENT_
    kineticSource = true;
    initEM = false;
    solveEM = false;

    doCorrectDivE = false;

    pMode = PartMode::Neutral;
#endif
  };
  ~Pic() {};

  void free_memory();

  void update(bool doReportIn = false);

  PicParticles *get_particle_pointer(int i) { return parts[i].get(); }
  // TODO: no longer needed if we fix the output variable ordering.
  bool get_useHybridPIC() const { return useHybridPIC; }

  // Returns the cell-centred coarse-to-fine interpolater to use.  For the
  // hybrid solver (useHybridPIC), uses CellConservativeLinear (lincc_interp,
  // 2nd-order conservative with slope limiting) for higher accuracy at
  // coarse-fine interfaces.  For the full-PIC solver, keeps CellBilinear
  // (cell_bilinear_interp) since the cell-centred fields are output-only
  // mirrors.
  amrex::Interpolater *get_cell_interp() const {
    return useHybridPIC
               ? static_cast<amrex::Interpolater *>(&amrex::lincc_interp)
               : static_cast<amrex::Interpolater *>(
                     &amrex::cell_bilinear_interp);
  }

  void set_stateOH(OHInterface *in) { stateOH = in; }
  void set_sourceOH(OHInterface *in) { sourcePT2OH = in; }
  void set_fluid_source(SourceInterface *in) { source = in; }

  //--------------Initialization begin-------------------------------
  virtual void pre_regrid() override;
  virtual void post_regrid() override;

  void distribute_arrays(const amrex::Vector<amrex::BoxArray> &cGridsOld);

  void fill_new_cells();
  void fill_E_B_fields();

  void fill_new_node_E();

  void fill_new_node_B();
  void fill_new_center_B();

  void fill_particles();

  // Narrow facade used by InitialCondition plug-ins (see InitialCondition.h).
  // These expose only the field arrays an IC needs; the facade is NOT a friend
  // of Pic.
  amrex::MultiFab &get_node_E(int iLev) { return nodeE[iLev]; }
  amrex::MultiFab &get_node_B(int iLev) { return nodeB[iLev]; }
  amrex::MultiFab &get_center_B(int iLev) { return centerB[iLev]; }
  PicICFields ic_fields() { return PicICFields(*this); }

  void init_source(const FluidInterface &interfaceIn) {
    // To be implemented

    //   sourceInterface = interfaceIn;
  }

  //----------------Initialization end-------------------------------

  void charge_exchange();

  void sum_moments(bool updateDt = false);

  void calc_mach_number();
  bool is_inside_cell_plot_region(const PlotWriter &writerIn, int const ix,
                                  int const iy, int const iz, double const x,
                                  double const y, double const z) const;

  // Convert SI input parameters to code units after normalization is finalized.
  void finalize_units_conversion();
  void convert_resistivity();
  void convert_electron_density0();
  void convert_inflow_state();

  void calc_mass_matrix();
  void calc_mass_matrix_amr();

  void update_part_loc_to_half_stage();

  void particle_mover();

  void re_sampling();

  void fill_source_particles();

  void inject_particles_for_new_cells() {
    if (!usePIC)
      return;

    for (auto &pts : parts) {
      pts->add_particles_domain();
    }
  }

  void inject_particles_for_boundary_cells() {
    if (!usePIC)
      return;

    for (auto &pts : parts) {
      pts->inject_particles_at_boundary();
    }
  }

  //------------Coupler related begin--------------
  void update_cells_for_pt();
  void get_fluid_state_for_points(const int nDim, const int nPoint,
                                  const double *const xyz_I,
                                  double *const data_I, const int nVar);
  void read_param(const std::string &command, ReadParam &param);
  void post_process_param();

  void report_bc_warnings(const std::string &context);
  void apply_periodicity_autofill(const amrex::Geometry &gm);
  void validate_bc_pairing(const amrex::Geometry &gm);
  //------------Coupler related end--------------

  //-------------Electric field solver begin-------------
  void update_E();
  void update_E_impl();
  void update_E_expl();
  void solve_E_gmres(int iLev);
  void solve_E_newton_krylov(int iLev);
  void update_E_rhs(double *rhos, int iLev);
  void update_E_matvec(const double *vecIn, double *vecOut, int iLev,
                       const bool useZeroBC = true);
  void update_E_M_dot_E(const amrex::MultiFab &inMF, amrex::MultiFab &outMF,
                        int iLev);

  void smooth_E(amrex::MultiFab &mfE, int iLev);
  void project_down_E();

  void smooth_multifab(amrex::MultiFab &mf, int iLev, int di,
                       amrex::Real coef = 0.5);

  void update_U0_E0();

  //-------------Hybrid PIC solver (kinetic ions + fluid electrons)-------------
  void smooth_moments();
  void update_B_hybrid();
  void project_centerB_to_nodeB(int iLev);
  // Apply periodic and physical boundary conditions to cell-centred B
  // (e.g. intermediate RK trial states that need fresh ghosts for Ohm's law
  // stencils).
  void apply_centerB_BC(int iLev);
  void apply_centerB_BC(int iLev, amrex::MultiFab &mfB);
  void project_centerB_to_nodeB_scratch(amrex::MultiFab &centerIn,
                                        amrex::MultiFab &nodeOut, int iLev);
  // Evaluate the Ohm's law E = -U_i x B + eta J + (J x B)/rho_q -
  // grad(Pe)/rho_q at an off-member B state (J from `centerBin`,
  // Hall/convection B from `centerBtimeAvg`), writing E into `Eout`. Ion
  // moments are time-interpolated between centerPlasmaPrev (J^{n-1/2}) and
  // centerPlasmaSum (J^{n+1/2}) at the sub-step fraction `hstep`: X =
  // (0.5-hstep)X^{n-1/2} + (0.5+hstep)X^{n+1/2}.
  void assemble_ohm_E(const amrex::MultiFab &centerBin,
                      const amrex::MultiFab &centerBtimeAvg,
                      amrex::MultiFab &Eout, int iLev, amrex::Real hstep);
  void save_current_moments_to_prev();
  void seed_first_hybrid_step();

  //-------------Electric field solver end-------------

  void update_B();

  void correct_B(int iLev);

  void solve_hyp_phi(int iLev);

  //-------------div(E) correction begin----------------
  void divE_correction();
  void amr_divE_correction();
  void divE_accurate_matvec(const double *vecIn, double *vecOut, int iLev);
  void divE_correct_particle_position();
  void sum_to_center(bool isBeforeCorrection);
  void sum_to_center_amr(bool isBeforeCorrection, int iLev);
  void calculate_phi(LinearSolver &solver, int iLev);
  //-------------div(E) correction end----------------

  void report_load_balance(bool doReportSummary = true,
                           bool doReportDetail = false);

  void calc_cost_per_cell();

  void convert_1d_to_3d(const double *const p, amrex::MultiFab &MF, int iLev);

  void convert_3d_to_1d(const amrex::MultiFab &MF, double *const p, int iLev);

  //--------------- IO begin--------------------------------
  void find_output_list(const PlotWriter &writerIn, long int &nPointAllProc,
                        VectorPointList &pointList_II, amrex::RealVect &xMin_D,
                        amrex::RealVect &xMax_D);

  void get_field_var(const VectorPointList &pointList_II,
                     const std::vector<std::string> &sVar_I,
                     MDArray<double> &var_II);
  double get_var(std::string_view var, const int iLev, const amrex::IntVect ijk,
                 const amrex::MFIter &mfi, bool isValidMFI = true);
  void save_restart_header(std::ofstream &headerFile);
  void save_restart_data();
  amrex::Vector<std::array<int, 3> > read_select_particle_input();
  void read_restart();
  void write_log(bool doForce = false, bool doCreateFile = false);
  void write_plots(bool doForce = false);
  void write_amrex(const PlotWriter &pw, double const timeNow,
                   int const iCycle);
  void write_amrex_field(const PlotWriter &pw, double const timeNow,
                         int const iCycle,
                         const std::string plotVars = "X E B plasma",
                         const std::string filenameIn = std::string(),
                         const amrex::BoxArray baOut = amrex::BoxArray());
  void write_amrex_particle(const PlotWriter &pw, double const timeNow,
                            int const iCycle);

  void set_IO_geom(amrex::Vector<amrex::Geometry> &geomIO,
                   const PlotWriter &pw);
  //--------------- IO end--------------------------------

  //--------------- Boundary begin ------------------------
  // Dispatch boundary conditions for electromagnetic fields (`isB` selects B vs
  // E).
  void apply_field_bc(const amrex::iMultiFab &status, amrex::MultiFab &mf,
                      const int iStart, const int nComp, GETVALUE func,
                      const int iLev, const bool isB);

  // Generic fill: zero-gradient copy on open faces, or values from `func`.
  void apply_BC(const amrex::iMultiFab &status, amrex::MultiFab &mf,
                const int iStart, const int nComp, GETVALUE func,
                const int iLev, const BoxBC<FieldBC::Type> *bc = nullptr);

  // Conducting wall: zero normal B / tangential E, mirror remaining components.
  void apply_conducting_wall(const amrex::iMultiFab &status,
                             amrex::MultiFab &mf, const int iStart,
                             const int nComp, const int iLev,
                             const BoxBC<FieldBC::Type> &bc, bool isB);

  // Absorbing wall: matched-impedance ghost-cell blending.
  void apply_absorbing_wall(const amrex::iMultiFab &status, amrex::MultiFab &mf,
                            const int iStart, const int nComp, const int iLev,
                            const BoxBC<FieldBC::Type> &bc, bool isB);

  // Inflow wall: zero-gradient copy of adjacent edge cell into ghost cells.
  void apply_inflow_wall(const amrex::iMultiFab &status, amrex::MultiFab &mf,
                         const int iStart, const int nComp, const int iLev,
                         const BoxBC<FieldBC::Type> &bc, bool isB);

  // Mirror ion moments into physical-wall ghost cells (hybrid solver).
  void apply_centerPlasma_BC(const amrex::iMultiFab &status,
                             amrex::MultiFab &mf, const int iLev);

  // Inject wave source into boundary ghost cells (iField: 0 = B, 1 = E).
  void apply_wave_field(const amrex::iMultiFab &status, amrex::MultiFab &mf,
                        const int iStart, const int nComp, const int iLev,
                        const BoxBC<FieldBC::Type> &bc, int iField,
                        amrex::Real t);

  // Compute wave velocity perturbation for particle injection.
  void wave_velocity_kick(const amrex::Real *pos, amrex::Real t,
                          amrex::Real &dvx, amrex::Real &dvy, amrex::Real &dvz);

  // Map ghost cell (i,j,k) at open faces to adjacent edge cell for
  // zero-gradient copy.
  bool use_float(const int i, const int j, const int k, int &ip, int &jp,
                 int &kp, const BoxBC<FieldBC::Type> &bc,
                 const amrex::Box &bxValid) const {
    bool useFloat = false;
    ip = i;
    jp = j;
    kp = k;

    if (i < bxValid.smallEnd(ix_) &&
        (bc.lo[ix_] == FieldBC::outflow || bc.lo[ix_] == FieldBC::inflow)) {
      useFloat = true;
      ip = bxValid.smallEnd(ix_);
    }
    if (i > bxValid.bigEnd(ix_) &&
        (bc.hi[ix_] == FieldBC::outflow || bc.hi[ix_] == FieldBC::inflow)) {
      useFloat = true;
      ip = bxValid.bigEnd(ix_);
    }

    if (j < bxValid.smallEnd(iy_) &&
        (bc.lo[iy_] == FieldBC::outflow || bc.lo[iy_] == FieldBC::inflow)) {
      useFloat = true;
      jp = bxValid.smallEnd(iy_);
    }

    if (j > bxValid.bigEnd(iy_) &&
        (bc.hi[iy_] == FieldBC::outflow || bc.hi[iy_] == FieldBC::inflow)) {
      useFloat = true;
      jp = bxValid.bigEnd(iy_);
    }

    if (nDim > 2) {
      if (k < bxValid.smallEnd(iz_) &&
          (bc.lo[iz_] == FieldBC::outflow || bc.lo[iz_] == FieldBC::inflow)) {
        useFloat = true;
        kp = bxValid.smallEnd(iz_);
      }

      if (k > bxValid.bigEnd(iz_) &&
          (bc.hi[iz_] == FieldBC::outflow || bc.hi[iz_] == FieldBC::inflow)) {
        useFloat = true;
        kp = bxValid.bigEnd(iz_);
      }
    }
    return useFloat;
  }

  amrex::Real get_zero(amrex::MFIter &mfi, amrex::IntVect ijk, int iVar,
                       int iLev) {
    return 0.0;
  }

  inline amrex::Real get_node_fluid_u(amrex::MFIter &mfi, amrex::IntVect ijk,
                                      int iVar, const int iLev, int iFluid) {
    amrex::Real u;
    if (iVar == ix_)
      u = fi->get_fluid_ux(mfi, ijk, iFluid, iLev);
    if (iVar == iy_)
      u = fi->get_fluid_uy(mfi, ijk, iFluid, iLev);
    if (iVar == iz_)
      u = fi->get_fluid_uz(mfi, ijk, iFluid, iLev);

    return u;
  }

  inline amrex::Real get_node_E(amrex::MFIter &mfi, amrex::IntVect ijk,
                                int iVar, const int iLev) {
    amrex::Real e;
    if (iVar == ix_)
      e = fi->get_ex(mfi, ijk, iLev);
    if (iVar == iy_)
      e = fi->get_ey(mfi, ijk, iLev);
    if (iVar == iz_)
      e = fi->get_ez(mfi, ijk, iLev);

    return e;
  }

  inline amrex::Real get_node_B(amrex::MFIter &mfi, amrex::IntVect ijk,
                                int iVar, const int iLev) {
    amrex::Real b;
    if (iVar == ix_)
      b = fi->get_bx(mfi, ijk, iLev);
    if (iVar == iy_)
      b = fi->get_by(mfi, ijk, iLev);
    if (iVar == iz_)
      b = fi->get_bz(mfi, ijk, iLev);

    return b;
  }

  inline amrex::Real get_center_B(amrex::MFIter &mfi, amrex::IntVect ijk,
                                  int iVar, const int iLev) {
    return fi->get_center_b(mfi, ijk, iVar, iLev);
  }

  inline amrex::Real get_center_E(amrex::MFIter &mfi, amrex::IntVect ijk,
                                  int iVar, const int iLev) {
    amrex::Real e;
    if (iVar == ix_)
      e = fi->get_ex(mfi, ijk, iLev);
    if (iVar == iy_)
      e = fi->get_ey(mfi, ijk, iLev);
    if (iVar == iz_)
      e = fi->get_ez(mfi, ijk, iLev);

    return e;
  }

  //--------------- Boundary end ------------------------

  // Make a new level from scratch using provided BoxArray and
  // DistributionMapping. Only used during initialization. overrides the pure
  // virtual function in AmrCore
  virtual void MakeNewLevelFromScratch(
      int iLev, amrex::Real time, const amrex::BoxArray &ba,
      const amrex::DistributionMapping &dm) override {
    std::string nameFunc = "Pic::MakeNewLevelFromScratch";
    amrex::Print() << printPrefix << nameFunc << " iLev = " << iLev
                   << std::endl;
  };

  // Make a new level using provided BoxArray and DistributionMapping and
  // fill with interpolated coarse level data.
  // overrides the pure virtual function in AmrCore
  virtual void MakeNewLevelFromCoarse(
      int iLev, amrex::Real time, const amrex::BoxArray &ba,
      const amrex::DistributionMapping &dm) override {
    std::string nameFunc = "Pic::MakeNewLevelFromCoarse";
    amrex::Print() << printPrefix << nameFunc << " iLev = " << iLev
                   << std::endl;
  };

  void WriteDivEErrorToParaView() {
    amrex::Vector<amrex::MultiFab> errorDivE;
    errorDivE.resize(n_lev());
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      errorDivE[iLev].define(cGrids[iLev], DistributionMap(iLev), 1, nGst);
      errorDivE[iLev].setVal(0.0);

      for (amrex::MFIter mfi(errorDivE[iLev]); mfi.isValid(); ++mfi) {
        const amrex::Box &box = mfi.validbox();
        const amrex::Array4<amrex::Real> &error = errorDivE[iLev][mfi].array();
        const amrex::Array4<amrex::Real const> divEcc =
            centerDivE[iLev][mfi].array();
        const amrex::Array4<amrex::Real const> qcc =
            centerNetChargeN[iLev][mfi].array();
        const auto &status = cell_status(iLev)[mfi].array();

        amrex::ParallelFor(box, [&](int i, int j, int k) {
          error(i, j, k) =
              sqrt(pow((4.0 * dPI * qcc(i, j, k) - 1.0 * divEcc(i, j, k)), 2));
          if (bit::is_refined(status(i, j, k))) {
            error(i, j, k) = 0;
          }
        });
      }
    }
    WriteMF(errorDivE, finest_level, "errorDivE");
  }

  void SetTargetPPC(int npresplitcells) {
    if (pInfo.isPPVconstant && pInfo.doPreSplitting) {
      amrex::Abort(
          "ConstantPPV and PreSplitting cannot be true at the same time");
    }
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      for (amrex::MFIter mfi(targetPPC[iLev]); mfi.isValid(); ++mfi) {
        const amrex::Box &box = mfi.fabbox();
        const auto &ppcArr = targetPPC[iLev][mfi].array();
        amrex::ParallelFor(box, [&](int i, int j, int k) noexcept {
          amrex::IntVect ijk = { AMREX_D_DECL(i, j, k) };
          ppcArr(ijk, 0) = product(pInfo.nPartPerCell);
        });
      }
    }
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      for (amrex::MFIter mfi(targetPPC[iLev]); mfi.isValid(); ++mfi) {
        const amrex::Box &box = mfi.validbox();
        const auto &ppcArr = targetPPC[iLev][mfi].array();
        const auto &status = cell_status(iLev)[mfi].array();
        amrex::ParallelFor(box, [&](int i, int j, int k) noexcept {
          amrex::IntVect ijk = { AMREX_D_DECL(i, j, k) };
          if (pInfo.isPPVconstant) {
            int tmp = 1;
            for (int i = 0; i < nDim; i++) {
              tmp *=
                  (pInfo.nPartPerCell[i] / pow((ref_ratio[iLev].max()), iLev));
            }
            ppcArr(ijk, 0) = tmp;
          } else {
            ppcArr(ijk, 0) = product(pInfo.nPartPerCell);
          }
          if (pInfo.doPreSplitting) {
            for (int ii = -npresplitcells; ii <= npresplitcells; ii++) {
              for (int jj = -npresplitcells; jj <= npresplitcells; jj++) {
                for (int kk = -npresplitcells; kk <= npresplitcells; kk++) {
                  amrex::IntVect ijk2 =
                      ijk + amrex::IntVect{ AMREX_D_DECL(ii, jj, kk) };
                  if (bit::is_refined(status(ijk2)) &&
                      !bit::is_refined(status(ijk))) {
                    ppcArr(ijk, 0) = product(pInfo.nPartPerCell) *
                                     pow(ref_ratio[iLev].max(), nDim);
                  }
                }
              }
            }
          }
        });
      }
    }
  }

  void WriteParticleQualityToParaView() {
    parts[0]->calculate_particle_quality(particleQuality);
    WriteMF(particleQuality, finest_level, "particleQuality0");
    parts[1]->calculate_particle_quality(particleQuality);
    WriteMF(particleQuality, finest_level, "particleQuality1");
  }
  // private methods
private:
  amrex::Real calc_E_field_energy();
  amrex::Real calc_B_field_energy();
  AMREX_EXPORT amrex::UNode_FourthOrder<amrex::Real> node_fourth_order_interp;
};

void find_output_list_caller(const PlotWriter &writerIn,
                             long int &nPointAllProc,
                             VectorPointList &pointList_II,
                             amrex::RealVect &xMin_D, amrex::RealVect &xMax_D);

void get_field_var_caller(const VectorPointList &pointList_II,
                          const std::vector<std::string> &sVar_I,
                          MDArray<double> &var_II);

#endif
