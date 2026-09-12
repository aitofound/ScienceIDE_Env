

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <time.h>

#include <sys/time.h>
#include <sys/resource.h>

//$Id$


//#include "vt_user.h"
//#include <VT.h>

//the particle class
#include "constants.h"
#include "sep.h"
#include "util/sep_cli.h"
#include "debug/sep_debug_fieldline_datum.h"

#include "tests.h"

void amps_init();
void amps_init_mesh();
void amps_time_step();

enum class CMEScenario { Fast, Slow };


//the physical simulation time 
double SimulationTime=0.0;


// Configure the single model for the requested scenario (called only on change)
static inline void configure_swcme1d(CMEScenario scenario){
  static bool inited = false;

  if (inited==true) return;

  inited=true;

  if (scenario == CMEScenario::Fast){
	  SEP::sw1d.SetAmbient(400.0, 6.0, 5.0, 1.2e5)                 // Vsw[km/s], n1AU[cm^-3], B1AU[nT], T[K]
      .SetCME(1.05, 1900.0, 8e-8)                         // r0[Rs],   V0_sh[km/s],  Γ[1/km]
      .SetGeometry(0.12, 0.22)                            // sheath & ME thickness @1 AU [AU]
      .SetSmoothing(0.010, 0.020, 0.030)                  // shock/LE/TE widths @1 AU [AU]
      .SetSheathEjecta(1.25, 2.0, 1.12, 0.50, 0.80);      // rc_floor, ramp_p, Vshe_LE, fME, VME
  } else {
	  SEP::sw1d.SetAmbient(380.0, 5.0, 4.5, 1.0e5)
      .SetCME(1.05, 950.0, 3e-8)
      .SetGeometry(0.08, 0.18)
      .SetSmoothing(0.015, 0.030, 0.050)
      .SetSheathEjecta(1.15, 1.5, 1.08, 0.60, 0.90);
  }

  SEP::SW1DAdapter::EnableSheathClamp(true);      // optional stability aid

}

/**
 * Advance one global step:
 *  - builds the per-time StepState for the selected scenario
 *  - publishes it to the mover through the adapter
 *  - uses a single static Model 'sw' (no multiple instances)
 */
void advance_sw1d(double dt) { 
  static double t_since_launch=0.0;
  static int ncall=0;

  ncall++;

  // Build per-time cache and publish to mover via adapter
  swcme1d::StepState S = SEP::sw1d.prepare_step(t_since_launch);
  SEP::SW1DAdapter::SetModelAndState(&SEP::sw1d, S);

  t_since_launch+=dt;



  // One call does everything: n, V, Br, Bphi, |B|, ∇·V → Tecplot POINT file
  if ((PIC::ThisThread==0)&&(ncall%10==0)) {
    char fname[200];

    const int N = 400;
    static double r[N];
    const double rmin = 1.05*_SUN__RADIUS_, rmax = 2.00*_AU_;     // 0.2–2 AU
    for (int i=0;i<N;++i){
      double t = double(i)/(N-1);
      r[i] = rmin*std::pow(rmax/rmin, t);            // log-spacing (nice for r^-2)
    }

    sprintf(fname,"sw_profile_%i.dat",ncall); 
    SEP::sw1d.write_tecplot_radial_profile_from_r(S, r, N, fname,SimulationTime);
  }
}

int main(int argc,char **argv) {
  //      MPI_Init(&argc,&argv);

  // --------------------------------------------------------------------------
  // Parse standalone-driver command-line options before the AMPS/SEP model is
  // initialized.  The CLI is intentionally kept in srcSEP/util so the parsing
  // logic is separated from the physics driver and can be reused or extended
  // without cluttering main.cpp.  If the user requests help, exit immediately
  // before reading input files or allocating AMPS data structures.
  // --------------------------------------------------------------------------
  SEP::Util::CLI::Options cli_options;
  if (!SEP::Util::CLI::ParseCommandLine(argc, argv, cli_options, std::cout, std::cerr)) {
    if (PIC::ThisThread == 0) SEP::Util::CLI::PrintHelp(argv[0], std::cerr);
    return 1;
  }

  if (cli_options.printHelp) {
    if (PIC::ThisThread == 0) SEP::Util::CLI::PrintHelp(argv[0], std::cout);
    return 0;
  }


  SEP::ShockModelType=SEP::cShockModelType::SwCme1d;

  //read post-compile input file  
  if (PIC::PostCompileInputFileName!="") {
     SEP::Parser::ReadFile(PIC::PostCompileInputFileName);
  }


  //set up shock wave model 
  configure_swcme1d(CMEScenario::Fast); 

  //output parameters of the sshock 
  SEP::sw1d.write_tecplot_shock_vs_time(2.0*24.0*3600, 200, "shock_vs_time.dat");

  // --------------------------------------------------------------------------
  // Configure the optional turbulence physics from command-line options before
  // registering AMPS field-line datums.  The selected turbulence model affects
  // which segment datums must be allocated.  In particular, the new
  // wave-number-resolved model needs an additional 2*NK spectral-energy datum.
  // --------------------------------------------------------------------------
  SEP::Util::CLI::ApplyTurbulenceOptions(cli_options);
  if (PIC::ThisThread == 0) SEP::Util::CLI::PrintTurbulenceOptions(cli_options, std::cout);

  //setup datum to store the segment's data for the Alfven turbulence model 
  if (SEP::AlfvenTurbulence_Kolmogorov::ActiveFlag) { 
    PIC::FieldLine::cFieldLineSegment::AddDatumStored(&SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy); 
    PIC::FieldLine::cFieldLineSegment::AddDatumStored(&SEP::AlfvenTurbulence_Kolmogorov::WaveEnergyDensity);

    // The wave-number-resolved model stores E+(k_j) and E-(k_j) in an
    // additional hidden segment datum.  It is registered only when selected by
    // the CLI to avoid increasing memory in legacy integrated-turbulence runs.
    if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
      PIC::FieldLine::cFieldLineSegment::AddDatumStored(
          &SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::SpectralWaveEnergy);

      // Hidden diagnostic datum used by the 2-D spectrum writer.  It stores the
      // per-bin wave-energy exchange rates caused by cascade, particle coupling,
      // and reflection during the current main-loop iteration.  It is registered
      // only in wave-number-resolved mode to avoid extra memory in legacy runs.
      PIC::FieldLine::cFieldLineSegment::AddDatumStored(
          &SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::SpectralWaveEnergyExchangeRate);
    }

    PIC::FieldLine::cFieldLineSegment::AddDatumStored(&SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::S);
    PIC::FieldLine::cFieldLineSegment::AddDatumStored(&SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::S_pm);


    PIC::FieldLine::cFieldLineSegment::AddDatumStored(&SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming);
    PIC::FieldLine::cFieldLineSegment::AddDatumStored(&SEP::AlfvenTurbulence_Kolmogorov::G_minus_streaming);
    PIC::FieldLine::cFieldLineSegment::AddDatumStored(&SEP::AlfvenTurbulence_Kolmogorov::gamma_plus_array);
    PIC::FieldLine::cFieldLineSegment::AddDatumStored(&SEP::AlfvenTurbulence_Kolmogorov::gamma_minus_array);

  }

  //set up datum to store distance of a field line vertex to the location of the shock 
  PIC::FieldLine::UserDefinedfDataProcessingManager=SEP::FieldLine::CalculateVertexShockDistances; 

  amps_init_mesh();
  amps_init();

  //init the Alfven turbulence IC
  if (SEP::AlfvenTurbulence_Kolmogorov::ActiveFlag) SEP::AlfvenTurbulence_Kolmogorov::ModelInit::Init(); 

  // --------------------------------------------------------------------------
  // Optional development diagnostics.
  //
  // TestManager() performs standalone field-line/model tests and is useful when
  // debugging the SEP/turbulence implementation.  It should not run by default
  // in production simulations because it can add extra diagnostic work/output
  // and may change the intended run flow.  The CLI therefore leaves it OFF
  // unless explicitly requested with one of:
  //   --test-manager on
  //   --testmanager on
  //   --run-test-manager
  // The compile-time field-line mode check is kept because TestManager() relies
  // on the field-line infrastructure being present.
  // --------------------------------------------------------------------------
  if (cli_options.runTestManager) {
    if (_PIC_FIELD_LINE_MODE_==_PIC_MODE_ON_) {
      TestManager();
    }
    else if (PIC::ThisThread == 0) {
      std::cout << "WARNING: --run-test-manager/--test-manager was requested, "
                << "but _PIC_FIELD_LINE_MODE_ is OFF; TestManager() is skipped.\n";
    }
  }

  int TotalIterations=(_PIC_NIGHTLY_TEST_MODE_==_PIC_MODE_ON_) ? PIC::RequiredSampleLength+10 : 100000001;  

  SEP::Diffusion::GetPitchAngleDiffusionCoefficient=SEP::Diffusion::Jokopii1966AJ::GetPitchAngleDiffusionCoefficient;

  //init turbulence wave energy 
  double B0_1AU = 5.0e-9;        // 5 nT magnetic field
  double turbulence_level = 0.2; // 20% turbulence

  SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,0);

  SEP::AlfvenTurbulence_Kolmogorov::InitializeWaveEnergyFromPhysicalParameters(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy, B0_1AU, 0.01,0.01, -2.0,false,true);


  SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,0);

  //exchenge the initial wave energy density and output in a file
  PIC::FieldLine::Parallel::MPIGatherDatumStoredAtEdge(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,0); 

  SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,0);

  SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,1);


  PIC::FieldLine::Parallel::MPIBcastDatumStoredAtEdge(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,0);

  SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,0);
  SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,1);

  // Capture the right-boundary W- initial condition after the turbulence wave
  // energy has been initialized and the edge data have been synchronized.
  //
  // Legacy/integrated model:
  //   store one W- density at the last segment.
  //
  // Wave-number-resolved model:
  //   first expand the integrated E+,E- initial condition into E+(k_j),E-(k_j)
  //   using the Kolmogorov log-bin weights, then store the full W-(k_j) density
  //   at the last segment.  This keeps the right boundary fixed as a
  //   pre-existing spectral turbulence reservoir rather than an artificial
  //   time-growing source.
  if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
    SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::InitializeSpectrumFromIntegratedEnergy(
        SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
    SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::ResetRightBoundarySpectrumInitialCondition();
    SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::CaptureRightBoundarySpectrumInitialCondition();
    SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::EnforceRightBoundarySpectrumInitialCondition();
    SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::UpdateIntegratedEnergyFromSpectrum(
        SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
  }
  else {
    SEP::AlfvenTurbulence_Kolmogorov::ResetRightBoundaryEminusInitialCondition();
    SEP::AlfvenTurbulence_Kolmogorov::CaptureRightBoundaryEminusInitialCondition(
        SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
    SEP::AlfvenTurbulence_Kolmogorov::EnforceRightBoundaryEminusInitialCondition(
        SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
  }


  //set background plasma density 
auto set_background_plasma_density = []() {
    // reference density at 1 AU [m⁻³]
    constexpr double n0 = 5.0e6;
    // 1 astronomical unit in meters
    constexpr double AU = 1.495978707e11;

    // Loop over all field lines
    for (int fldIdx = 0; fldIdx < PIC::FieldLine::nFieldLine; ++fldIdx) {
        auto fieldLine = &PIC::FieldLine::FieldLinesAll[fldIdx];
        if (!fieldLine) continue;

        int nSeg = fieldLine->GetTotalSegmentNumber();
        if (nSeg < 1) continue;

        // Loop over all segments in this field line
        for (int segIdx = 0; segIdx < nSeg; ++segIdx) {
            auto seg = fieldLine->GetSegment(segIdx);
            if (!seg) continue;

            // Process both end‐points (left & right) of the segment
            PIC::FieldLine::cFieldLineVertex* vertices[2] = { seg->GetBegin(), seg->GetEnd() };
            for (auto vtx : vertices) {
                if (!vtx) continue;

                // get pointer to {x,y,z} [m]
                double* X = vtx->GetX();
                // compute radial distance from Sun [m]
                double r = std::sqrt(X[0]*X[0] + X[1]*X[1] + X[2]*X[2]);

                // scale density as n0/(r/AU)^2
                double density = n0 / ((r/AU) * (r/AU));

                // store it in the vertex
                vtx->SetPlasmaDensity(density); //SetDatum(density, PIC::FieldLine::DatumAtVertexPlasmaDensity);
            }
        }
    }
};


 set_background_plasma_density();

  // Calculate turbulence wave enregy density from wave energy integrated over the segments of the magnetic tube:
auto CalculateWaveEnergyDensity = [&]() {
    // Focus specifically on SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy
    auto& integrated_energy = SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy;
    auto& energy_density = SEP::AlfvenTurbulence_Kolmogorov::WaveEnergyDensity;

    int local_count = 0;

    for (int fl = 0; fl < PIC::FieldLine::nFieldLine; fl++) {
        auto& field_line = PIC::FieldLine::FieldLinesAll[fl];
        int num_segs = field_line.GetTotalSegmentNumber();

        for (int seg = 0; seg < num_segs; seg++) {
            auto* segment = field_line.GetSegment(seg);
            if (!segment) continue;

            // Only process segments belonging to this thread
            if (segment->Thread == PIC::ThisThread) {
                // Focus on getting CellIntegratedWaveEnergy values
                double* energy_data = segment->GetDatum_ptr(integrated_energy);
                double* density_data = segment->GetDatum_ptr(energy_density);

                // Use SEP::FieldLine::GetSegmentVolume for volume calculation
                double volume = SEP::FieldLine::GetSegmentVolume(segment, fl);

                if (energy_data && density_data && volume > 0.0) {
                    // CellIntegratedWaveEnergy stores the conservative variables
                    // E+ and E- integrated over the current magnetic-tube segment.
                    // WaveEnergyDensity is the printable AMPS field-line datum used
                    // in amps.FieldLines.out=*.dat.  Its first two elements must be
                    // the plotted wave-energy densities W+ and W-, while its third
                    // element is the normalized cross helicity sigma_c.  We keep
                    // sigma_c in the same datum as W+ and W- rather than registering
                    // a separate output datum so that the generic field-line writer
                    // prints the three turbulence diagnostics as one adjacent block:
                    //   "W+", "W-", "sigma_c".
                    const double Wplus  = energy_data[0] / volume;
                    const double Wminus = energy_data[1] / volume;

                    density_data[0] = Wplus;
                    density_data[1] = Wminus;

                    // sigma_c is the normalized Elsasser/turbulence imbalance.  It
                    // is bounded by [-1,1] for non-negative W+ and W-.  A zero value
                    // is used when both wave populations vanish to avoid division by
                    // zero and to keep the output finite.
                    const double Wsum = Wplus + Wminus;
                    density_data[2] = (Wsum > 0.0) ? (Wplus - Wminus) / Wsum : 0.0;

                    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
                      validate_numeric(density_data[0],__LINE__,__FILE__);
                      validate_numeric(density_data[1],__LINE__,__FILE__);
                      validate_numeric(density_data[2],__LINE__,__FILE__);
                    }

                    local_count++;
                }
            }
        }
    }

    // MPI operations to gather and broadcast results
    PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(energy_density);

    SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::WaveEnergyDensity,0);
    SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::WaveEnergyDensity,2); 

    if (PIC::ThisThread == 0) {
        std::cout << "Compact wave energy density calculation completed with MPI operations" << std::endl;
    }
};


 //calculate the wave energy density
 CalculateWaveEnergyDensity();

    SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::WaveEnergyDensity,0);
    SEP::AlfvenTurbulence_Kolmogorov::TestPrintEPlusValues(SEP::AlfvenTurbulence_Kolmogorov::WaveEnergyDensity,2);


    //PIC::FieldLine::Output("fl-edge-test.dat",false); 

//hooks for calculating the magnertic tube radius and the volume of the field line segment
PIC::FieldLine::SegmentVolume=SEP::FieldLine::GetSegmentVolume;



  vector<vector<double> > DeltaE_plus, DeltaE_minus;


  // --------------------------------------------------------------------------
  // ResetWaveParticleStreamingAccumulators()
  // --------------------------------------------------------------------------
  // G_plus_streaming and G_minus_streaming are not physical wave-energy state
  // variables.  They are one-time Monte-Carlo accumulators filled by the particle
  // mover during the current AMPS time step and then MPI-summed before the
  // turbulence wave-particle coupling manager is called.  Therefore they must be
  // zeroed on every MPI rank, on every local/ghost copy of every field-line
  // segment, before particles are moved.  If stale values remain on non-owning
  // ranks, MPIAllReduceDatumStoredAtEdge() can repeatedly re-sum old source terms
  // and produce artificially large growth/damping rates.
  //
  // Keep the reset local to main.cpp rather than relying on a coupling manager to
  // clear only owned segments after use.  The particle mover is the producer of
  // these source terms, so the safe place to clear them is immediately before the
  // particle mover is entered through amps_time_step().
  // --------------------------------------------------------------------------
  auto ResetWaveParticleStreamingAccumulators = []() {
    auto ResetDatum = [](PIC::Datum::cDatumStored& Datum) {
      for (int iFieldLine=0; iFieldLine<PIC::FieldLine::nFieldLine; ++iFieldLine) {
        for (PIC::FieldLine::cFieldLineSegment* Segment =
                 PIC::FieldLine::FieldLinesAll[iFieldLine].GetFirstSegment();
             Segment != NULL;
             Segment = Segment->GetNext()) {
          double* data = Segment->GetDatum_ptr(Datum);
          if (data == NULL) continue;

          for (int i=0; i<Datum.length; ++i) data[i]=0.0;
        }
      }
    };

    ResetDatum(SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming);
    ResetDatum(SEP::AlfvenTurbulence_Kolmogorov::G_minus_streaming);
    ResetDatum(SEP::AlfvenTurbulence_Kolmogorov::gamma_plus_array);
    ResetDatum(SEP::AlfvenTurbulence_Kolmogorov::gamma_minus_array);
  };


  //time step
  for (long int niter=0;niter<TotalIterations;niter++) {
    if (SEP::AlfvenTurbulence_Kolmogorov::ActiveFlag &&
        SEP::AlfvenTurbulence_Kolmogorov::ParticleCouplingMode) {
      ResetWaveParticleStreamingAccumulators();
    }

    // ----------------------------------------------------------------------
    // Debug-only validation of field-line datums at the very beginning of the
    // main iteration.
    //
    // This check is intentionally placed before amps_time_step(), shock
    // injection, particle/turbulence coupling, advection, reflection, cascade,
    // and all MPI synchronization performed later in the iteration.  If an AMPS
    // field-line MPI unpack routine later finds a non-finite or overflowed
    // value, these pre-iteration checks help determine whether the bad value
    // already existed in the local field-line state or was created by one of the
    // subsequent operators / MPI reductions in the current iteration.
    //
    // The helper takes the datum as an argument, so additional datums can be
    // checked by adding one more call here.  The calls are protected by the AMPS
    // debugger-mode macro and are therefore inactive in normal production runs.
    // ----------------------------------------------------------------------
    if (_PIC_DEBUGGER_MODE_ == _PIC_DEBUGGER_MODE_ON_) {
      SEP::Debug::ValidateFieldLineDatum(
          SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,
          "CellIntegratedWaveEnergy",
          "beginning of main iteration",
          niter,
          __LINE__,
          __FILE__);

      SEP::Debug::ValidateFieldLineDatum(
          SEP::AlfvenTurbulence_Kolmogorov::WaveEnergyDensity,
          "WaveEnergyDensity",
          "beginning of main iteration",
          niter,
          __LINE__,
          __FILE__);

      SEP::Debug::ValidateFieldLineDatum(
          SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming,
          "G_plus_streaming",
          "beginning of main iteration",
          niter,
          __LINE__,
          __FILE__);

      SEP::Debug::ValidateFieldLineDatum(
          SEP::AlfvenTurbulence_Kolmogorov::G_minus_streaming,
          "G_minus_streaming",
          "beginning of main iteration",
          niter,
          __LINE__,
          __FILE__);

      if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
        SEP::Debug::ValidateFieldLineDatum(
            SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::SpectralWaveEnergy,
            "WaveNumberResolved::SpectralWaveEnergy",
            "beginning of main iteration",
            niter,
            __LINE__,
            __FILE__);

        SEP::Debug::ValidateFieldLineDatum(
            SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::SpectralWaveEnergyExchangeRate,
            "WaveNumberResolved::SpectralWaveEnergyExchangeRate",
            "beginning of main iteration",
            niter,
            __LINE__,
            __FILE__);
      }
    }
    //SEP::InitDriftVelData();

    static double rsh0=SEP::ParticleSource::ShockWave::Tenishev2005::rShock; 
    
    if (niter==0) {
      switch (SEP::ShockModelType) {
      case SEP::cShockModelType::Analytic1D:
        rsh0=SEP::ParticleSource::ShockWave::Tenishev2005::rShock; 
        break;
      case SEP::cShockModelType::SwCme1d:
        rsh0=SEP::SW1DAdapter::gState.r_sh_m;
        break;
      }
    }


    amps_time_step();
    SimulationTime+=PIC::ParticleWeightTimeStep::GlobalTimeStep[0]; 

    //advance the shock+CME model
   advance_sw1d(PIC::ParticleWeightTimeStep::GlobalTimeStep[0]);


    if (SEP::AlfvenTurbulence_Kolmogorov::ActiveFlag) {

      // Function to increment integrated wave energy due to shock passing
      if (niter!=0) {
         double rsh1;

         switch (SEP::ShockModelType) {
         case SEP::cShockModelType::Analytic1D:
           rsh1=SEP::ParticleSource::ShockWave::Tenishev2005::rShock;
           break;
         case SEP::cShockModelType::SwCme1d:
           rsh1=SEP::SW1DAdapter::gState.r_sh_m;
           break;
         }

	 SEP::ParticleSource::ShockWave::ShockTurbulenceEnergyInjection(rsh0, rsh1, PIC::ParticleWeightTimeStep::GlobalTimeStep[0]);

         // The present shock turbulence source is formulated for the legacy
         // branch-integrated E+ and E- datum.  In the wave-number-resolved
         // model, immediately project the updated integrated shock increment
         // back to E±(k_j), preserving the local spectral shape where possible
         // and using a Kolmogorov distribution only when a branch previously had
         // no spectral energy.
         if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
           SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::ProjectIntegratedEnergyToSpectrum(
               SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
         }

	 rsh0=rsh1;
      }

      if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
        // Reset per-bin source/sink diagnostics after the shock source has been
        // applied and before particle coupling/reflection/cascade for this
        // iteration.  The requested Tecplot diagnostics describe the exchange
        // rates due to cascade, particle interaction, and reflection only; shock
        // injection and spatial advection are not included in these local-rate
        // columns.
        SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::ResetSpectralEnergyExchangeRates();
      }

      if (SEP::ParticleMoverPtr!=SEP::ParticleMover_FocusedTransport_WaveScattering) { // in case SEP::ParticleMover_FocusedTransport_WaveScattering(), particle/turbulence coupling is already done 

      // Function to increment integrated wave energy due to shock passing
      //reduce S
      PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtEdge(SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming);
      PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtEdge(SEP::AlfvenTurbulence_Kolmogorov::G_minus_streaming);

      SEP::AlfvenTurbulence_Kolmogorov::TestPrintDatum(SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming,0,"g+",0);
      SEP::AlfvenTurbulence_Kolmogorov::TestPrintDatum(SEP::AlfvenTurbulence_Kolmogorov::G_minus_streaming,0,"g-",0);

      SEP::AlfvenTurbulence_Kolmogorov::TestPrintDatumMPI(SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming,"g+",0);
      SEP::AlfvenTurbulence_Kolmogorov::TestPrintDatumMPI(SEP::AlfvenTurbulence_Kolmogorov::G_minus_streaming,"g-",0);


      SEP::AlfvenTurbulence_Kolmogorov::AnalyzeMaxSegmentParticles(SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming,"G_plus_streaming" ,0);
      SEP::AlfvenTurbulence_Kolmogorov::AnalyzeMaxSegmentParticles(SEP::AlfvenTurbulence_Kolmogorov::G_minus_streaming,"G_minus_streaming" ,0);


//      SEP::AlfvenTurbulence_Kolmogorov::SetDatumAll(0.0,SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming);
//      SEP::AlfvenTurbulence_Kolmogorov::TestPrintDatumMPI(SEP::AlfvenTurbulence_Kolmogorov::G_plus_streaming,"g+",0);

      //couple particles and turbulence  
//      SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::UpdateAllSegmentsWaveEnergyWithParticleCoupling(
//		     SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,
//		    SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::S,
//		   PIC::ParticleWeightTimeStep::GlobalTimeStep[0]); 

      if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
        // New spectral coupling: G±(k_j) modifies the same E±(k_j) bin, and
        // the equal-and-opposite particle energy change is redistributed only
        // to particles whose resonant wave number falls into that bin.
        SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::WaveParticleCouplingManager(
            SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,
            PIC::ParticleWeightTimeStep::GlobalTimeStep[0]);

        SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::EnforceRightBoundarySpectrumInitialCondition();
        SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::UpdateIntegratedEnergyFromSpectrum(
            SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
      }
      else {
        // Legacy coupling: growth rates are integrated over k before the two
        // branch-integrated energies E+ and E- are updated.
        SEP::AlfvenTurbulence_Kolmogorov::IsotropicSEP::WaveParticleCouplingManager(
            SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,
            PIC::ParticleWeightTimeStep::GlobalTimeStep[0]);

        // The wave-particle coupling operator updates E+ and E- in every segment.
        // Re-apply the fixed right-boundary W- condition immediately afterward so
        // the boundary remains equal to the pre-existing initial turbulence state.
        SEP::AlfvenTurbulence_Kolmogorov::EnforceRightBoundaryEminusInitialCondition(
            SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
      }
      }


      // ----------------------------------------------------------------------
      // Advect turbulence energy along field lines.
      //
      // The explicit wave-energy advection step is limited by an Alfven-speed
      // CFL condition.  The particle global time step can be much larger than
      // the Alfven crossing time of the smallest field-line segment, especially
      // near boundaries.  Advancing the turbulence with the full particle time
      // step can therefore pile energy up at the edge cells.  Subcycle the
      // turbulence advection with the global stable time step returned by
      // GetGlobalMaxStableTimeStep().  The function already includes its own
      // safety factor when estimating the CFL limit.
      //
      // The last segment of each field line is treated as a fixed reservoir of
      // pre-existing inward-propagating turbulence W-.  The initial W- value is
      // captured after the turbulence initial condition is generated and is
      // restored after each turbulence operator.  This prevents the right
      // boundary from either draining away by advection or growing by an
      // artificial source.  The TurbulenceLevelEnd argument is retained only for
      // backward-compatible call signatures and is not used to inject W-.
      // ----------------------------------------------------------------------
      {
        const double dt_total_turbulence = PIC::ParticleWeightTimeStep::GlobalTimeStep[0];
        double dt_done_turbulence = 0.0;

        while (dt_done_turbulence < dt_total_turbulence) {
          double dt_cfl_turbulence = SEP::AlfvenTurbulence_Kolmogorov::GetGlobalMaxStableTimeStep();

          // Guard against invalid CFL estimates.  This should only happen if no
          // valid local field-line segment is found, but using the remaining
          // time avoids an infinite loop and preserves the previous behavior in
          // that degenerate case.
          if (dt_cfl_turbulence <= 0.0 || !std::isfinite(dt_cfl_turbulence)) {
            dt_cfl_turbulence = dt_total_turbulence - dt_done_turbulence;
          }

          const double dt_subcycle = std::min(dt_cfl_turbulence, dt_total_turbulence - dt_done_turbulence);

          if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
            // Spectral advection: every E+(k_j) and E-(k_j) bin is transported
            // independently with the same finite-volume Alfvénic flux geometry.
            // The compact integrated datum is refreshed afterward so the rest of
            // the model and the standard output still see E+ and E-.
            SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::AdvectSpectrumAllFieldLines(
                dt_subcycle,
                0.01,  // inner-boundary total W+ turbulence level, distributed over k
                0.0);  // retained argument; spectral right-boundary W-(k) is fixed

            SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::UpdateIntegratedEnergyFromSpectrum(
                SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
            PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(
                SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
          }
          else {
            SEP::AlfvenTurbulence_Kolmogorov::AdvectTurbulenceEnergyAllFieldLines(
                DeltaE_plus, DeltaE_minus,
                SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy,
                dt_subcycle,
                0.01,  // inner-boundary W+ source level
                0.0);  // retained argument; right-boundary W- is fixed to its captured initial value

            // The advection operator updates only locally-owned field-line segments.
            // The next subcycle reads neighbor states to compute finite-volume face
            // fluxes.  Refresh the ghost/edge copies after every subcycle; otherwise
            // MPI-domain interfaces use stale E+/E- values during all later subcycles,
            // producing artificial jumps and incorrect interior evolution of W+/W-.
            PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(
                SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);

            // MPI synchronization may overwrite the locally prescribed boundary-cell
            // value on the owner rank.  Restore the fixed right-boundary W- density
            // immediately after synchronization so both diagnostics and subsequent
            // flux calculations use the intended pre-existing turbulence value.
            SEP::AlfvenTurbulence_Kolmogorov::EnforceRightBoundaryEminusInitialCondition(
                SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
          }

          dt_done_turbulence += dt_subcycle;
        }
      }

      //model the effect of wave reflection 
      if (SEP::AlfvenTurbulence_Kolmogorov::Reflection::active==true) {
        double C_reflection=0.6;

        if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
          // Fully spectral reflection: convert E+(k_j) and E-(k_j) into one
          // another at the same wave-number bin j.  This replaces the older
          // compatibility path that first reflected the branch-integrated E± and
          // then projected the result back onto E±(k).  The integrated datum is
          // refreshed afterward only so the compact W+,W-,sigma_c diagnostics and
          // legacy helper routines continue to see the branch sums.
          SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::ReflectSpectrumAllFieldLines(
              PIC::ParticleWeightTimeStep::GlobalTimeStep[0],C_reflection,0.0,false);

          SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::UpdateIntegratedEnergyFromSpectrum(
              SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
          PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(
              SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
        }
        else {
          SEP::AlfvenTurbulence_Kolmogorov::Reflection::ReflectTurbulenceEnergyAllFieldLines(
              PIC::ParticleWeightTimeStep::GlobalTimeStep[0],C_reflection,0.0,false);

          // Reflection can convert part of W+ into W- in the boundary segment.
          // Keep the last-segment W- fixed to the captured initial value so the
          // boundary condition remains prescribed rather than dynamically evolved.
          SEP::AlfvenTurbulence_Kolmogorov::EnforceRightBoundaryEminusInitialCondition(
              SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
        }
      }

      // Configure cascade
      if (SEP::AlfvenTurbulence_Kolmogorov::Cascade::active==true) { 
        SEP::AlfvenTurbulence_Kolmogorov::Cascade::SetCascadeCoefficient(0.8);             // C_nl
        SEP::AlfvenTurbulence_Kolmogorov::Cascade::SetDefaultPerpendicularCorrelationLength(1.0e7); // 10,000 km
        SEP::AlfvenTurbulence_Kolmogorov::Cascade::SetDefaultEffectiveArea(1.0);           // V_cell = Δs
        SEP::AlfvenTurbulence_Kolmogorov::Cascade::SetElectronHeatingFraction(0.3);        // 30% to electrons
        SEP::AlfvenTurbulence_Kolmogorov::Cascade::EnableCrossHelicityModulation(false);
        SEP::AlfvenTurbulence_Kolmogorov::Cascade::EnableTwoSweepIMEX(false);
  
        // Advance cascade for all field lines (ΔE arrays accumulate changes)
        // Optional: stronger physics
        SEP::AlfvenTurbulence_Kolmogorov::Cascade::EnableCrossHelicityModulation(true);
        SEP::AlfvenTurbulence_Kolmogorov::Cascade::EnableTwoSweepIMEX(true);

        if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
          // Fully spectral cascade: move energy in log-k space within each
          // segment, from bin j to j+1, using the same model parameters as the
          // integrated cascade configuration above: C_nl=0.8, lambda_perp=1e7 m,
          // cross-helicity modulation ON, and a two-sweep update.  Only the
          // energy that reaches k_max is removed as unresolved dissipation.
          // The compact integrated datum is then refreshed as the sum over k.
          SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::CascadeSpectrumAllFieldLines(
              PIC::ParticleWeightTimeStep::GlobalTimeStep[0],
              0.8,     // C_nl, same value used by the legacy cascade operator
              1.0e7,   // lambda_perp [m], same 10,000 km value used above
              true,    // enable per-bin cross-helicity modulation
              true,    // two half-sweeps for a Picard-like update
              false);  // enable_logging

          SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::UpdateIntegratedEnergyFromSpectrum(
              SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
          PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(
              SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
        }
        else {
          SEP::AlfvenTurbulence_Kolmogorov::Cascade::CascadeTurbulenceEnergyAllFieldLines(
              PIC::ParticleWeightTimeStep::GlobalTimeStep[0],/*enable_logging=*/ false);

          // Nonlinear cascade/damping changes both Elsasser wave populations.
          // Restore only the outer-boundary W- value; all interior segments and
          // the outer-boundary W+ outflow remain governed by the cascade update.
          SEP::AlfvenTurbulence_Kolmogorov::EnforceRightBoundaryEminusInitialCondition(
              SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
        }
      }

    
      //scatter wave energy   
      if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive()) {
        PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(
            SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::SpectralWaveEnergy);
        SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::EnforceRightBoundarySpectrumInitialCondition();
        SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::UpdateIntegratedEnergyFromSpectrum(
            SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
      }
      else {
        PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);

        // Edge synchronization can refresh data stored on field-line boundaries.
        // Enforce the prescribed right-boundary W- value once more before derived
        // wave-energy-density diagnostics are calculated.
        SEP::AlfvenTurbulence_Kolmogorov::EnforceRightBoundaryEminusInitialCondition(
            SEP::AlfvenTurbulence_Kolmogorov::CellIntegratedWaveEnergy);
      }


      //calculate the wave energy density 
      CalculateWaveEnergyDensity();

      // --------------------------------------------------------------------
      // Optional 2-D spectral turbulence diagnostics.
      //
      // This output exists only for the wave-number-resolved model because the
      // legacy model does not store E+(k_j) and E-(k_j).  The diagnostic is
      // intentionally controlled by a CLI cadence rather than written every
      // iteration: one file contains all field lines, all segments, and all
      // 128 wave-number bins, so output every time step can become large.
      //
      // The call is placed here, after all wave-energy source/transport terms
      // and after CalculateWaveEnergyDensity(), so the compact W+,W-,sigma_c
      // field-line output and the spectral W±(s,k),sigma_c(s,k) output refer
      // to the same turbulence state.  The shock model was advanced near the
      // beginning of this iteration, so the output title includes the current
      // shock location at this simulation time.
      // --------------------------------------------------------------------
      if (SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::IsActive() &&
          cli_options.spectralOutputInterval > 0 &&
          ((niter+1) % cli_options.spectralOutputInterval == 0)) {
        PIC::FieldLine::Parallel::MPIAllGatherDatumStoredAtEdge(
            SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::SpectralWaveEnergy);

        SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::OutputSpectrumTecplot2D(
            niter+1,SimulationTime);
      }

/*
      if ((niter+1)%2==0)  {   
         char fname[300];
	 sprintf(fname,"fl-%i-%e.dat",PIC::ThisThread,rsh0/_AU_);

	 //PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtVertex(&PIC::FieldLine::DatumAtVertexParticleWeight);
         //PIC::FieldLine::Parallel::MPIAllReduceDatumStoredAtVertex(&PIC::FieldLine::DatumAtVertexParticleCosPitchAngle);

	 PIC::FieldLine::Output(fname,false);

	 //PIC::FieldLine::Parallel::SetDatumStoredAtVertex(0.0,&PIC::FieldLine::DatumAtVertexParticleWeight);
	 //PIC::FieldLine::Parallel::SetDatumStoredAtVertex(0.0,&PIC::FieldLine::DatumAtVertexParticleCosPitchAngle);
      }
*/

    }


    //PIC::ParticleSplitting::Split::SplitWithVelocityShift_FL(10,200);
    //
    //PIC::ParticleSplitting::FledLine::WeightedParticleMerging(20,20,20,500,800); 
    //PIC::ParticleSplitting::FledLine::WeightedParticleSplitting(20,20,20,500,800);    
  }


  char fname[400];

  sprintf(fname,"%s/test_SEP.dat",PIC::OutputDataFileDirectory);
  PIC::RunTimeSystemState::GetMeanParticleMicroscopicParameters(fname);

  cout << "End of the run:" << PIC::nTotalSpecies << endl;

  MPI_Finalize();
  return EXIT_SUCCESS;
}
