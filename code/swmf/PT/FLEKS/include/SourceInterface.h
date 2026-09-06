#ifndef _SOURCEINTERFACE_H_
#define _SOURCEINTERFACE_H_

#include "FluidInterface.h"

// An abstract class for source implementations.
// Ionization parameters are stored here (not in FluidInterface) so that
// FluidInterface is only touched for neutral-profile data that the MHD
// coupling layer needs.
class SourceInterface : public FluidInterface {
protected:
  std::string info = "SourceInterface class";
  bool useFluidSource = false;

  // ---- Ionization process flags (set by #PHOTOIONIZATION, etc.) ----
  bool usePhotoIonization = false;
  bool useElectronImpact = false;
  bool useChargeExchange = false;

  // ---- Photoionization (#PHOTOIONIZATION command) ----
  // The neutral component count is nExoComponent (from #EXOSPHERE);
  // #EXOSPHERE must appear before #PHOTOIONIZATION in PARAM.in.
  amrex::Vector<double> photoNu0; // ionization rate at planet surface [s^-1]

  // ---- Shadow cylinder (#SHADOWCYLINDER command) ----
  bool useShadowCylinder = false;
  double solarDir[3] = { 0.0, 0.0,
                         0.0 }; // unit vector from planet center toward the Sun
  double shadowCylinderRadius = 0.0; // radius of planetary shadow cylinder [m]
  double shadowCylinderHalfHeight =
      0.0; // half-height along anti-solar direction [m]

  // ---- Electron impact ionization (#ELECTRONIMPACT command) ----
  // The neutral component count is nExoComponent (from #EXOSPHERE);
  // #EXOSPHERE must appear before #ELECTRONIMPACT in PARAM.in.
  amrex::Vector<double> impactEIon; // ionization energy [eV]
  amrex::Vector<double> impactA;    // Voronov A coefficient [cm^3/s]
  amrex::Vector<double> impactK;    // Voronov K coefficient
  amrex::Vector<double> impactX;    // Voronov X coefficient

  // ---- Charge exchange (#CHARGEEXCHANGE command) ----
  // The number of neutral components is nExoComponent (from #EXOSPHERE);
  // #EXOSPHERE must appear before #CHARGEEXCHANGE in PARAM.in so that
  // nExoComponent is available.
  int nCXIonSpecies = 0; // number of ion species that exchange charge
  // Cross-section matrix [cm^2], flattened as [iC * nCXIonSpecies + iIon].
  // Row iC is the neutral component (0..nExoComponent-1); column iIon is
  // the ion species (0-based among ions, i.e. iIon = iSp - 1 where iSp is
  // the fluid index).
  amrex::Vector<double> cxSigma;

  // ---- Recombination (#RECOMBINATION command) ----
  // Dissociative recombination: ion+ + e- -> neutrals.
  // Requires useElectronFluid = true (species 0 = electron).
  bool useRecombination = false;
  // Per-reaction parameters (parallel arrays, size = nRecombReactions):
  amrex::Vector<int> recombIonIndex;   // 1-based ion species index (iSp)
  amrex::Vector<double> recombRate0;   // base rate coefficient k0 [cm^3/s]
  amrex::Vector<double> recombTempExp; // temperature exponent alpha
  amrex::Vector<double> recombRefTemp; // reference temperature T_ref [K]
  // k(Te) = k0 * (T_ref / Te_K)^alpha

  // ---- General chemistry (#CHEMISTRY command) ----
  // Supports cross-species ion conversion, photoionization, and
  // recombination through a unified reaction format.
  //   reactantIon + neutral -> productIon + neutral'
  // reactantIon=0 means no reactant ion (photoionization).
  // productIon=0 means no product ion (recombination).
  // neutralComp=-1 means no neutral needed (recombination).
  bool useChemistry = false;
  struct ChemistryReaction {
    int reactantIon; // 0 = none, 1+ = ion species index
    int productIon;  // 0 = none, 1+ = ion species index
    int neutralComp; // -1 = none, 0+ = exosphere component
    int rateType;    // 0 = thermal k(T), 1 = photoionization (1/r^2)
    double rateCoef; // k0 [cm^3/s] for thermal, nu0 [s^-1] for photo
    double tempExp;  // alpha: k = k0 * (Tref/Te)^alpha
    double refTemp;  // T_ref [K]
  };
  amrex::Vector<ChemistryReaction> chemReactions;

  // ---- Loss term storage ----
  // nodeLossFluid stores the mass-density LOSS RATE (positive = loss)
  // for each ion species, in PIC-normalized units.  It is parallel to
  // nodeFluid but only has nS-1 components (one per ion species, 0-based:
  // component i = ion species i+1).  Particles::apply_loss() reads this
  // to reduce existing particle weights proportionally.
  // Allocated whenever useRecombination or useChemistry is true.
  amrex::Vector<amrex::MultiFab> nodeLossFluid;

  const DomainParameters& domainParameters;

public:
  SourceInterface(const FluidInterface& other, int id, std::string tag,
                  FluidType typeIn, const DomainParameters& dp)
      : FluidInterface(other, id, tag, typeIn), domainParameters(dp) {
    initFromSWMF = false;
  }

  virtual ~SourceInterface() = default;

  virtual std::string get_info() const { return info; }

  virtual void sum_to_single_source() {
    amrex::Print()
        << "Warning: SourceInterface::sum_to_single_source is called but not "
           "implemented."
        << std::endl;
  };

  virtual void set_source(const FluidInterface& other) {
    if (!useFluidSource)
      return;

    amrex::Print() << "Warning: SourceInterface::set_source is called but not "
                      "implemented."
                   << std::endl;
  };

  /// Read ionization-related parameter commands.
  /// Override in UserSource to handle #PHOTOIONIZATION, #ELECTRONIMPACT,
  /// #CHARGEEXCHANGE.
  virtual void read_param(const std::string& command, ReadParam& param) {
    amrex::ignore_unused(command, param);
  }

  /// Validate consistency across ionization commands after all parameters
  /// have been read.  Called after read_param() for all commands.
  virtual void post_process_param() {}

  /// Get total neutral exosphere density at radial distance r (SI units).
  /// Default returns 0.0 — override in UserSource for exosphere profiles.
  virtual double get_exosphere_density(double r) const { return 0.0; }

  /// Get single-component neutral exosphere density at radial distance r.
  /// Default returns 0.0 — override in UserSource for exosphere profiles.
  virtual double get_exosphere_component_density(double r, int iC) const {
    return 0.0;
  }

  // ---- Loss term management ----
  // These methods manage nodeLossFluid, which stores per-ion mass-density
  // loss rates for apply_loss() to consume.  The layout and distribution
  // parallel nodeFluid but with only nS-1 components (ions only).

  void post_regrid() override {
    FluidInterface::post_regrid(); // distributes nodeFluid
    distribute_loss_arrays();
  }

  void distribute_loss_arrays() {
    if (!(useRecombination || useChemistry))
      return;
    if (nodeLossFluid.empty())
      nodeLossFluid.resize(n_lev_max());
    const int nIon = nS > 1 ? nS - 1 : 0;
    if (nIon == 0)
      return;
    const bool doCopy = true;
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      distribute_FabArray(nodeLossFluid[iLev], nGrids[iLev],
                          DistributionMap(iLev), nIon, nGst, doCopy);
    }
  }

  void set_node_loss_fluid_to_zero() {
    if (!(useRecombination || useChemistry))
      return;
    for (int iLev = 0; iLev < n_lev(); ++iLev) {
      if (!nodeLossFluid[iLev].empty())
        nodeLossFluid[iLev].setVal(0.0);
    }
  }

  void sum_loss_boundary() {
    if (!(useRecombination || useChemistry))
      return;
    // Use FillBoundary (copy) instead of SumBoundary (sum) for loss rates.
    // Each node's loss rate is computed independently and should NOT be
    // summed across shared/periodic nodes.  FillBoundary fills ghost cells
    // with the correct values from the interior, handling periodic BCs.
    for (int iLev = 0; iLev < n_lev(); ++iLev) {
      if (!nodeLossFluid[iLev].empty())
        nodeLossFluid[iLev].FillBoundary(Geom(iLev).periodicity());
    }
  }

  /// Read loss rate for ion species iIon (0-based) at cell ijk.
  /// Returns the normalized mass-density loss rate (positive = loss).
  amrex::Real get_loss_value(const amrex::MFIter& mfi, const amrex::IntVect ijk,
                             const int iIon, const int iLev = 0) const {
    const auto& arr = nodeLossFluid[iLev][mfi].const_array();
    return arr(ijk, iIon);
  }

  bool use_loss_source() const { return useRecombination || useChemistry; }

  /// Check whether nodeLossFluid is allocated and non-empty at iLev.
  bool has_loss_array(int iLev) const {
    return (useRecombination || useChemistry) &&
           iLev < static_cast<int>(nodeLossFluid.size()) &&
           !nodeLossFluid[iLev].empty();
  }
};

#endif
