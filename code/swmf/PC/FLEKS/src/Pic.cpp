#include <algorithm>
#include <cctype>
#include <limits>
#include <math.h>
#include <vector>

#include <AMReX_Algorithm.H>
#include <AMReX_CArena.H>
#include <AMReX_FabArrayBase.H>
#include <AMReX_MultiFabUtil.H>

#if defined(__linux__)
#include <malloc.h>
#endif

#include "GridUtility.h"
#include "LinearSolver.h"
#include "Pic.h"
#include "Timer.h"

using namespace amrex;

//==========================================================
void Pic::read_param(const std::string& command, ReadParam& param) {

  if (command == "#PIC") {
    param.read_var("usePIC", usePIC);
  } else if (command == "#SOLVEEM") {
    param.read_var("solveEM", solveEM);
  } else if (command == "#PARTMODE") {
    std::string s;
    param.read_var("partMode", s);
    if (s == "SEP")
      pMode = PartMode::SEP;
    else if (s == "PIC")
      pMode = PartMode::PIC;
    else
      Abort("Error: wrong input for partMode.");
  } else if (command == "#PARTICLEBOXBOUNDARY") {
    int iSpecies;
    std::string lo, hi;
    param.read_var("iSpecies", iSpecies);
    if (iSpecies < 0)
      Abort("Error: negative species index in #PARTICLEBOXBOUNDARY.");
    // nSpecies is only known in post_process_param(), so pBCs grows on
    // demand here and is padded to nSpecies there.
    if (iSpecies >= static_cast<int>(pInfo.pBCs.size())) {
      pInfo.pBCs.resize(iSpecies + 1);
      pInfo.pBCsSet.resize(iSpecies + 1, 0);
    }
    pInfo.pBCsSet[iSpecies] = 1;

    for (int i = 0; i < nDim; ++i) {
      param.read_var("particleBoxBoundaryLo", lo);
      param.read_var("particleBoxBoundaryHi", hi);
      pInfo.pBCs[iSpecies].set(i, 0, ParticleBC::parse(lo));
      pInfo.pBCs[iSpecies].set(i, 1, ParticleBC::parse(hi));
    }
  } else if (command == "#FIELDBOXBOUNDARY" ||
             command == "#BFIELDBOXBOUNDARY") {
    if (command == "#BFIELDBOXBOUNDARY")
      add_bc_warning("#BFIELDBOXBOUNDARY is deprecated; use "
                     "#FIELDBOXBOUNDARY instead.");
    fieldBCSet_ = true;

    std::string lo, hi;
    for (int i = 0; i < nDim; ++i) {
      param.read_var("fieldBoxBoundaryLo", lo);
      param.read_var("fieldBoxBoundaryHi", hi);
      bcField.set(i, 0, FieldBC::parse(lo));
      bcField.set(i, 1, FieldBC::parse(hi));
    }
  } else if (command == "#ABSORB") {
    param.read_var("charSpeed", absorbCharSpeed);
  } else if (command == "#INFLOW") {
    double tmp;
    param.read_var("rho", tmp);
    inflowRho_ = tmp; // [amu/cc]
    param.read_var("ux", tmp);
    inflowUx_ = tmp; // [km/s]
    param.read_var("uy", tmp);
    inflowUy_ = tmp; // [km/s]
    param.read_var("uz", tmp);
    inflowUz_ = tmp; // [km/s]
    param.read_var("T", tmp);
    inflowT_ = tmp; // [K]
    inflowDefined_ = true;
  } else if (command == "#WAVEBC") {
    waveBC.read_param(param, fi);
  } else if (command == "#MEMORY") {
    param.read_var("dnMemory", dnMemory);
  } else if (command == "#RANDOMPARTICLESLOCATION") {
    param.read_var("isParticleLocationRandom", pInfo.isParticleLocationRandom);
  } else if (command == "#CONSTANTPPV") {
    param.read_var("isPPVconstant", pInfo.isPPVconstant);
  } else if (command == "#PRESPLITTING") {
    param.read_var("doPreSplitting", pInfo.doPreSplitting);
  } else if (command == "#DIVE") {
    param.read_var("doCorrectDivE", doCorrectDivE);
    if (doCorrectDivE) {
      param.read_var("nDivECorrection", nDivECorrection);
    }
  } else if (command == "#EXPLICITPIC") {
    param.read_var("useExplicitPIC", useExplicitPIC);
  } else if (command == "#EFIELDSOLVER") {
    Real tol;
    int nIter;
    param.read_var("tol", tol);
    param.read_var("nIter", nIter);
    eSolver.set_tol(tol);
    eSolver.set_nIter(nIter);
  } else if (command == "#PARTICLES") {
    param.read_var("npcelx", pInfo.nPartPerCell[ix_]);
    param.read_var("npcely", pInfo.nPartPerCell[iy_]);
    if (nDim == 3)
      param.read_var("npcelz", pInfo.nPartPerCell[iz_]);
  } else if (command == "#SOURCEPARTICLES") {
    param.read_var("npcelx", nSourcePPC[ix_]);
    param.read_var("npcely", nSourcePPC[iy_]);
    if (nDim == 3)
      param.read_var("npcelz", nSourcePPC[iz_]);
  } else if (command == "#KINETICSOURCE") {
    param.read_var("kineticSource", kineticSource);
  } else if (command == "#ELECTRON") {
    param.read_var("qom", qomEl);
  } else if (command == "#DISCRETIZE" || command == "#DISCRETIZATION") {
    param.read_var("theta", fsolver.theta);
    param.read_var("coefDiff", fsolver.coefDiff);
  } else if (command == "#COMOVING") {
    param.read_var("solveFieldInCoMov", solveFieldInCoMov);
    param.read_var("nSmoothBackGroundU", nSmoothBackGroundU);
  } else if (command == "#UPWINDE") {
    param.read_var("useUpwindE", useUpwindE);
    param.read_var("limiterThetaE", limiterThetaE);
  } else if (command == "#LAGGEDLIMITER") {
    param.read_var("useLaggedLimiter", fsolver.useLaggedLimiter);
  } else if (command == "#CMAXE") {
    param.read_var("cMaxE", cMaxE);
  } else if (command == "#SMOOTHE") {
    param.read_var("doSmoothE", doSmoothE);
    if (doSmoothE) {
      param.read_var("nSmoothE", nSmoothE);
    }
  } else if (command == "#SMOOTHJ") {
    param.read_var("doSmoothJ", doSmoothJ);
    if (doSmoothJ) {
      param.read_var("nSmoothJ", nSmoothJ);
      param.read_var("coefSmoothJ", coefSmoothJ);
    }
  } else if (command == "#SMOOTHMOMENTS") {
    param.read_var("doSmoothMoments", doSmoothMoments);
    if (doSmoothMoments) {
      param.read_var("nSmoothMoments", nSmoothMoments);
      param.read_var("coefSmoothMoments", coefSmoothMoments);
    }
  } else if (command == "#UPWINDB") {
    param.read_var("useUpwindB", useUpwindB);
    param.read_var("theta", limiterThetaB);
    if (useUpwindB) {
      useHyperbolicCleaning = true;
    }
    param.read_optional("fixedUpwindVel", fixedUpwindVel);
  } else if (command == "#FIXEDUMAX") {
    param.read_optional("fixedUMax", fixedUMax);
  } else if (command == "#DIVB") {
    param.read_var("useHyperbolicCleaning", useHyperbolicCleaning);
    if (useHyperbolicCleaning) {
      param.read_var("hypDecay", hypDecay);
    }
  } else if (command == "#RESAMPLING") {
    param.read_var("doReSampling", doReSampling);
    if (doReSampling) {
      param.read_var("reSamplingLowLimit", reSamplingLowLimit);
      param.read_var("reSamplingHighLimit", reSamplingHighLimit);
      param.read_var("maxWeightRatio", maxWeightRatio);
    }
  } else if (command == "#FASTMERGE") {
    param.read_var("fastMerge", pInfo.fastMerge);
    if (pInfo.fastMerge) {
      param.read_var("nMergeOld", pInfo.nPartCombine);
      param.read_var("nMergeNew", pInfo.nPartNew);
      param.read_var("nMergeTry", pInfo.nMergeTry);
      param.read_var("mergeRatioMax", pInfo.mergeRatioMax);
    }
  } else if (command == "#ADAPTIVESOURCEPPC") {
    param.read_var("adaptiveSourcePPC", adaptiveSourcePPC);
  } else if (command == "#MERGELIGHT") {
    param.read_var("mergeLight", pInfo.mergeLight);
    if (pInfo.mergeLight) {
      param.read_var("mergePartRatioMax", pInfo.mergePartRatioMax);
    }
  } else if (command == "#VACUUM") {
    param.read_var("vacuum", pInfo.vacuumIO);
  } else if (command == "#PARTICLELEVRATIO") {
    param.read_var("particleLevRatio", pInfo.pLevRatio);
  } else if (command == "#OHION") {
    param.read_var("rAnalytic", pInfo.ionOH.rAnalytic);
    param.read_var("doGetFromOH", pInfo.ionOH.doGetFromOH);

    if (!pInfo.ionOH.doGetFromOH) {
      param.read_var("rCutoff", pInfo.ionOH.rCutoff);
      param.read_var("swRho", pInfo.ionOH.swRho);
      param.read_var("swT", pInfo.ionOH.swT);
      param.read_var("swU", pInfo.ionOH.swU);
    }
  } else if (command == "#SUPID") {
    int n = 0;
    param.read_var("nSpecies", n);
    for (int i = 0; i < n; ++i) {
      int supid;
      param.read_var("supid", supid);
      pInfo.supIDs.push_back(supid);
    }
  } else if (command == "#MAXCHARGEEXCHANGERATE") {
    param.read_var("maxChargeExchangeRate", maxExchangeRatioLimit);
  } else if (command == "#TESTCASE") {
    std::string testcase;
    param.read_var("testCase", testcase);

    ic_ = ICRegistry::instance().create(testcase);
    if (!ic_) {
      std::string known;
      for (const auto& n : ICRegistry::instance().names()) {
        if (!known.empty())
          known += ", ";
        known += n;
      }
      amrex::Abort("Unknown #TESTCASE name '" + testcase +
                   "'. Registered names: " + known + ".");
    }
    ic_->read_param(param);
  } else if (command == "#WAVEIC") {
    if (!ic_) {
      amrex::Abort("The #WAVEIC block must follow a #TESTCASE that selects a "
                   "wave initial condition (waveic / lightwave / hybridwave / "
                   "convectionwave / ionacousticwave).");
    }
    ic_->read_param(param);
  } else if (command == "#FADEEVIC") {
    if (!ic_ || std::string(ic_->name()) != "fadeev") {
      amrex::Abort("The #FADEEVIC block must follow a #TESTCASE that selects "
                   "the fadeev (magnetic reconnection) initial condition.");
    }
    ic_->read_param(param);
  } else if (command == "#HYBRIDPIC") {
    param.read_var("useHybridPIC", useHybridPIC);
  } else if (command == "#RESISTIVITY") {
    param.read_var("etaResistivity", etaResistivitySI);
  } else if (command == "#ELECTRONTEMPERATURE") {
    param.read_var("electronTemperature", electronTemperatureEV);
    param.read_var("electronGamma", electronGamma);
    param.read_var("electronDensity0", electronDensity0In);
  } else if (command == "#BSUBCYCLE") {
    param.read_var("nBSubcycle", nBSubcycle);
  } else if (command == "#HALLTERM") {
    param.read_var("useHallTerm", useHallTerm);
  } else if (command == "#HYPERRESISTIVITY") {
    param.read_var("etaHyperSI", etaHyperSI);
    param.read_var("etaHyperMode", etaHyperMode);
    param.read_var("etaHyperCh", etaHyperCh);
  } else if (command == "#MINIMUMDENSITY") {
    param.read_var("rhoMinOhm", rhoMinOhm);
  } else if (command == "#FIELDINTEGRATOR") {
    param.read_var("fieldIntegrator", fieldIntegrator);
  } else if (command == "#AVGFIELDB") {
    param.read_var("useAvgFieldB", useAvgFieldB);
    param.read_var("nAvgFieldB", nAvgFieldB);
  } else if (command == "#SELECTPARTICLE") {
    param.read_var("doSelectParticle", doSelectParticle);
    if (doSelectParticle) {
      param.read_var("selectParticleInputFile", selectParticleInputFile);
    }
  }
}

//==========================================================
// Report de-duplicated boundary-condition warnings and abort.
void Pic::report_bc_warnings(const std::string& context) {
  if (bcWarnings_.empty())
    return;

  std::string msg;
  for (const std::string& w : bcWarnings_)
    msg += "\n  - " + w;
  amrex::Abort("Error: " + context + " boundary conditions:" + msg);
}

//==========================================================
// Autofill periodic boundaries from Geometry for field and particle BCs,
// warning on conflicting user configurations.
void Pic::apply_periodicity_autofill(const Geometry& gm) {
  static const char* const dimNames[3] = { "x", "y", "z" };
  const int nSpeciesBC = static_cast<int>(pInfo.pBCs.size());
  const int nBCsSet = static_cast<int>(pInfo.pBCsSet.size());

  for (int d = 0; d < nDim; ++d) {
    if (!gm.isPeriodic(d))
      continue;

    for (int side = 0; side < 2; ++side) {
      if (fieldBCSet_) {
        const int type = bcField.face(d, side);
        if (type != FieldBC::periodic)
          add_bc_warning(
              std::string("dimension ") + dimNames[d] +
              " is periodic (#PERIODICITY), but the field boundary was set "
              "to '" +
              FieldBC::to_string(static_cast<FieldBC::Type>(type)) +
              "'; using 'periodic'.");
      }
      bcField.set(d, side, FieldBC::periodic);

      for (int i = 0; i < nSpeciesBC; ++i) {
        if (i < nBCsSet && pInfo.pBCsSet[i] != 0) {
          const int type = pInfo.pBCs[i].face(d, side);
          if (type != ParticleBC::periodic)
            add_bc_warning(
                std::string("dimension ") + dimNames[d] +
                " is periodic (#PERIODICITY), but the particle boundary of "
                "species " +
                std::to_string(i) + " was set to '" +
                ParticleBC::to_string(static_cast<ParticleBC::Type>(type)) +
                "'; using 'periodic'.");
        }
        pInfo.pBCs[i].set(d, side, ParticleBC::periodic);
      }
    }
  }
}

//==========================================================
// Validate boundary condition consistency across field, particles, and
// geometry. Field vs particle checks are skipped if field BC was not explicitly
// set.
void Pic::validate_bc_pairing(const Geometry& gm) {
  static const char* const dimNames[3] = { "x", "y", "z" };
  static const char* const faceNames[3][2] = { { "x-lo", "x-hi" },
                                               { "y-lo", "y-hi" },
                                               { "z-lo", "z-hi" } };

  const bool isStandalone = domainParameters.isStandalone;
  const bool hasNonPeriodic = !gm.isAllPeriodic();

  if (isStandalone && hasNonPeriodic) {
    if (!fieldBCSet_)
      amrex::Abort(
          "Error: #FIELDBOXBOUNDARY command is required when there are "
          "non-periodic boundaries in standalone mode.");
    if (usePIC && (pInfo.pBCsSet.empty() || pInfo.pBCsSet[0] == 0))
      amrex::Abort(
          "Error: #PARTICLEBOXBOUNDARY command is required when there are "
          "non-periodic boundaries in standalone mode.");
  }

  const int nSpeciesBC = static_cast<int>(pInfo.pBCs.size());
  const int nBCsSet = static_cast<int>(pInfo.pBCsSet.size());
  const int nCheck = std::min(nSpeciesBC, nBCsSet);

  for (int d = 0; d < nDim; ++d) {
    const bool dimPeriodic = gm.isPeriodic(d);
    const int fLo = bcField.face(d, 0);
    const int fHi = bcField.face(d, 1);

    if (!dimPeriodic) {
      if (fLo == FieldBC::periodic)
        add_bc_warning(std::string("field boundary ") + faceNames[d][0] +
                       " is 'periodic' but #PERIODICITY is F for that "
                       "dimension.");
      if (fHi == FieldBC::periodic)
        add_bc_warning(std::string("field boundary ") + faceNames[d][1] +
                       " is 'periodic' but #PERIODICITY is F for that "
                       "dimension.");
    }
    if ((fLo == FieldBC::periodic) != (fHi == FieldBC::periodic))
      add_bc_warning(std::string("field boundary ") + dimNames[d] +
                     " is periodic on one side only; #PERIODICITY applies to "
                     "a whole dimension.");

    for (int side = 0; side < 2; ++side) {
      const auto fType = static_cast<FieldBC::Type>(bcField.face(d, side));
      const char* face = faceNames[d][side];

      if (fType == FieldBC::inflow && !inflowDefined_)
        add_bc_warning(std::string("field boundary ") + face +
                       " is 'inflow' but no #INFLOW block was given; the face "
                       "falls back to a zero-gradient copy.");

      if (fType == FieldBC::wave && !waveBC.active)
        add_bc_warning(std::string("field boundary ") + face +
                       " is 'wave' but no #WAVEBC block was given; the face "
                       "carries no wave source.");

      for (int i = 0; i < nCheck; ++i) {
        if (pInfo.pBCsSet[i] == 0)
          continue;

        const auto pType =
            static_cast<ParticleBC::Type>(pInfo.pBCs[i].face(d, side));

        auto what = [&]() {
          return "species " + std::to_string(i) + " particle boundary " + face;
        };

        if (pType == ParticleBC::periodic && !dimPeriodic)
          add_bc_warning(what() + " is 'periodic' but #PERIODICITY is F for "
                                  "that dimension.");

        if (!fieldBCSet_)
          continue; // Field is default coupled: skip field-pairing checks.

        if (pType == ParticleBC::inflow && fType != FieldBC::inflow &&
            fType != FieldBC::fixed)
          add_bc_warning(what() + " is 'inflow' but the field boundary is '" +
                         FieldBC::to_string(fType) +
                         "'; the injected flux has no upstream field to "
                         "match.");

        if (fType == FieldBC::conducting &&
            (pType == ParticleBC::outflow || pType == ParticleBC::vacuum ||
             pType == ParticleBC::absorb))
          add_bc_warning(what() + " is '" + ParticleBC::to_string(pType) +
                         "' on a 'conducting' wall: particles leave through "
                         "the wall.");

        if (pType == ParticleBC::absorb && fType != FieldBC::absorb)
          add_bc_warning(what() + " is 'absorb' but the field boundary is '" +
                         FieldBC::to_string(fType) +
                         "'; only the particles are absorbed.");
      }
    }
  }

  // In hybrid PIC, only centerB is evolved; wall BCs constrain B and close
  // the ghost ring for E.
  if (useHybridPIC) {
    bool hasWall = false;
    for (int d = 0; d < nDim && !hasWall; ++d) {
      for (int side = 0; side < 2; ++side) {
        const auto t = static_cast<FieldBC::Type>(bcField.face(d, side));
        if (t == FieldBC::conducting || t == FieldBC::wave) {
          hasWall = true;
          break;
        }
      }
    }
    if (hasWall)
      amrex::Print() << "  Note: hybrid solver: only centerB is evolved, so a "
                     << "conducting / wave field boundary constrains "
                     << "B; for the Ohm's-law E it only closes the ghost ring "
                     << "(it is not an independent constraint).\n";
  }
}

//==========================================================
void Pic::post_process_param() {
  fi->set_plasma_charge_and_mass(qomEl);
  nSpecies = fi->get_nS();
  // Species without a #PARTICLEBOXBOUNDARY block keep the default (coupled),
  // or inherit from species 0 if species 0 was specified.
  if (static_cast<int>(pInfo.pBCs.size()) < nSpecies) {
    pInfo.pBCs.resize(nSpecies);
    pInfo.pBCsSet.resize(nSpecies, 0);
  }

  const bool hasSpecies0 = (!pInfo.pBCsSet.empty() && pInfo.pBCsSet[0] != 0);
  if (hasSpecies0) {
    for (int i = 1; i < nSpecies; ++i) {
      if (pInfo.pBCsSet[i] == 0) {
        pInfo.pBCs[i] = pInfo.pBCs[0];
        pInfo.pBCsSet[i] = 1;
      }
    }
  }

  fsolver.mode = (!fsolver.useLaggedLimiter && limiterThetaE != 0)
                     ? FieldSolverMode::NewtonKrylov
                     : FieldSolverMode::GMRES;

  // Classify species: negative charge -> electron, otherwise kinetic ion.
  kineticSpecies_.clear();
  iElectron_ = -1;
  for (int i = 0; i < nSpecies; ++i) {
    // Guard: parts may not be fully populated yet.
    if (i < (int)parts.size() && parts[i] && parts[i]->get_charge() < 0) {
      if (iElectron_ < 0)
        iElectron_ = i;
    } else {
      kineticSpecies_.push_back(i);
    }
  }

  // Hybrid and implicit solver are mutually exclusive.
  if (useHybridPIC)
    solveEM = false;

  // Convert input units to normalized code units.
  if (useHybridPIC) {
    // Resistivity SI->code conversions are deferred to
    // finalize_units_conversion().

    useRK4 = (fieldIntegrator == "rk4");
    if (fieldIntegrator != "rk4" && fieldIntegrator != "ssprk3") {
      amrex::Print() << "  WARNING: unknown #FIELDINTEGRATOR '"
                     << fieldIntegrator << "'; defaulting to 'rk4'\n";
      fieldIntegrator = "rk4";
      useRK4 = true;
    }
    amrex::Print() << "  fieldIntegrator: " << fieldIntegrator << "\n";
    amrex::Print() << "  useAvgFieldB: " << useAvgFieldB
                   << "   nAvgFieldB: " << nAvgFieldB << "\n";
    if (nAvgFieldB < 1)
      nAvgFieldB = 1;
    if (electronTemperatureEV > 0) {
      // Te_code = Te_eV * e / (mp * uNorm_SI^2)
      double unormSI = fi->get_unorm_si();
      electronTemperature = electronTemperatureEV * cUnitChargeSI /
                            (cProtonMassSI * unormSI * unormSI);
      amrex::Print() << "  electronTemperature: " << electronTemperatureEV
                     << " [eV] -> " << electronTemperature << " [code units]\n";
    }

    // Conversion to code units deferred until convert_electron_density0()
    // (Si2NoRho is not yet available here).
    if (rhoMinOhm <= 0)
      rhoMinOhm = 0.0; // resolved to 1e-6*electronDensity0 on first advance
  }
  // report_bc_warnings() is called by Domain after BC autofill and validation.
}

//==========================================================
void Pic::fill_new_cells() {
  std::string nameFunc = "Pic::fill_new_cells";

  if (isGridEmpty)
    return;

  if (usePIC && !doNeedFillNewCell)
    return;

  timing_func(nameFunc);

  if (!usePIC) {
    // If this method is called when PIC component is off, it suggests the test
    // particle component is activated. The test particle component copies EM
    // field from PIC, so PIC EM field should be updated here.

    update_grid_status();
  }

  if (pInfo.isPPVconstant || pInfo.doPreSplitting) {
    SetTargetPPC(2);
    isTargetPPCDefined = true;
    for (int i = 0; i < nSpecies; i++) {
      parts[i]->set_is_target_ppc_defined(isTargetPPCDefined);
    }
  }
  if (initEM) {
    fill_E_B_fields();
  }

  // Every registered InitialCondition plug-in seeds its fields through the
  // narrow PicICFields facade (LightWave, HybridWave, ConvectionWave, ...). The
  // hybrid-wave velocity kick and all per-particle modifications are applied
  // inside fill_particles() via the plugin.
  if (ic_) {
    PicICFields icf = ic_fields();
    ic_->set_fields(icf);
  }

  if (usePIC) {
    // Macroparticle seeding (and any per-particle modifications such as the
    // beam bulk override or the hybrid-wave Alfven velocity kick) is routed
    // through the InitialCondition plugin during fill_particles().
    fill_particles();
    sum_moments(true);
    // div(E)-correction fields are full-PIC only.
    if (!useHybridPIC) {
      if (finest_level == 0) {
        sum_to_center(false);
      } else if (doCorrectDivE) {
        for (int iLev = 0; iLev < n_lev(); iLev++) {
          sum_to_center_amr(false, iLev);
        }
      }
    }
  }

  doNeedFillNewCell = false;
}

//==========================================================
void Pic::distribute_arrays(const Vector<BoxArray>& cGridsOld) {

  // The last one is the sum of all species.
  if (nodePlasma.empty()) {
    nodePlasma.resize(nSpecies + 1);
  }
  // Per-species deposit targets; last entry = sum of all species.
  if (centerPlasma.empty()) {
    centerPlasma.resize(nSpecies + 1);
  }
  if (centerPlasmaPrev.empty()) {
    centerPlasmaPrev.resize(nSpecies + 1);
  }
  if (centerPlasmaSum.empty()) {
    centerPlasmaSum.resize(nSpecies + 1);
  }

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    if (reportParticleQuality) {
      distribute_FabArray(particleQuality[iLev], cGrids[iLev],
                          DistributionMap(iLev), 18, 0);
    }
    distribute_FabArray(targetPPC[iLev], cGrids[iLev], DistributionMap(iLev), 1,
                        nGst);
    distribute_FabArray(centerB[iLev], cGrids[iLev], DistributionMap(iLev), 3,
                        nGst);
    distribute_FabArray(nodeB[iLev], nGrids[iLev], DistributionMap(iLev), 3,
                        nGst);
    distribute_FabArray(nodeE[iLev], nGrids[iLev], DistributionMap(iLev), 3,
                        nGst);
    distribute_FabArray(nodeEth[iLev], nGrids[iLev], DistributionMap(iLev), 3,
                        nGst);

    bool doMoveData = false;
    // div(E)/div(B) correction and implicit E-solver arrays (full-PIC only).
    if (!useHybridPIC) {
      distribute_FabArray(centerNetChargeOld[iLev], cGrids[iLev],
                          DistributionMap(iLev), 1, nGst);
      distribute_FabArray(centerNetChargeN[iLev], cGrids[iLev],
                          DistributionMap(iLev), 1, nGst);
      distribute_FabArray(centerNetChargeNew[iLev], cGrids[iLev],
                          DistributionMap(iLev), 1, nGst);
      distribute_FabArray(centerDivE[iLev], cGrids[iLev], DistributionMap(iLev),
                          1, nGst);
      distribute_FabArray(centerPhi[iLev], cGrids[iLev], DistributionMap(iLev),
                          1, nGst);

      distribute_FabArray(divB[iLev], cGrids[iLev], DistributionMap(iLev), 3,
                          nGst, doMoveData);
      distribute_FabArray(hypPhi[iLev], cGrids[iLev], DistributionMap(iLev), 3,
                          nGst, doMoveData);

      if (!useExplicitPIC) {
        distribute_FabArray(nodeMM[iLev], nGrids[iLev], DistributionMap(iLev),
                            1, 1, doMoveData);
      }
    }
    if (useHybridPIC) {
      // Previous-step ion moments for the Ohm's-law interpolation.
      if (nodePlasmaPrev.empty()) {
        nodePlasmaPrev.resize(nSpecies + 1);
      }
      // Hyper-resistivity scratch: centerLapB = Laplacian(B); nodeHyperE
      // node-centred.
      distribute_FabArray(centerLapB[iLev], cGrids[iLev], DistributionMap(iLev),
                          3, nGst, doMoveData);
      distribute_FabArray(nodeHyperE[iLev], nGrids[iLev], DistributionMap(iLev),
                          3, nGst, doMoveData);

      // RK4 / ssprk3 shared intermediate solver scratch.
      distribute_FabArray(centerBstage[iLev], cGrids[iLev],
                          DistributionMap(iLev), 3, nGst, doMoveData);
      for (int kk = 0; kk < 4; ++kk)
        distribute_FabArray(kStage[iLev][kk], cGrids[iLev],
                            DistributionMap(iLev), 3, nGst, doMoveData);

      // Time-averaged B scratch (cell + node), used when useAvgFieldB is set.
      distribute_FabArray(centerBavg[iLev], cGrids[iLev], DistributionMap(iLev),
                          3, nGst, doMoveData);
      distribute_FabArray(nodeBavg[iLev], nGrids[iLev], DistributionMap(iLev),
                          3, nGst, doMoveData);

      // rk3/rk4 persistent scratch: centerBstart = B_n; centerBstar =
      // (trial+B_n)/2.
      distribute_FabArray(centerBstart[iLev], cGrids[iLev],
                          DistributionMap(iLev), 3, nGst, doMoveData);
      distribute_FabArray(centerBstar[iLev], cGrids[iLev],
                          DistributionMap(iLev), 3, nGst, doMoveData);

      // Cell-centred hybrid solver fields.
      distribute_FabArray(centerEhybrid[iLev], cGrids[iLev],
                          DistributionMap(iLev), 3, nGst, doMoveData);
      distribute_FabArray(centerJ[iLev], cGrids[iLev], DistributionMap(iLev), 3,
                          nGst, doMoveData);
      distribute_FabArray(centerEstage[iLev], cGrids[iLev],
                          DistributionMap(iLev), 3, nGst, doMoveData);
      distribute_FabArray(centerHyperE[iLev], cGrids[iLev],
                          DistributionMap(iLev), 3, nGst, doMoveData);
      for (auto& pl : centerPlasmaSum) {
        if (pl.empty())
          pl.resize(n_lev_max());
        distribute_FabArray(pl[iLev], cGrids[iLev], DistributionMap(iLev),
                            nMoments, nGst, doMoveData);
      }
      for (auto& pl : centerPlasma) {
        if (pl.empty())
          pl.resize(n_lev_max());
        distribute_FabArray(pl[iLev], cGrids[iLev], DistributionMap(iLev),
                            nMoments, nGst, doMoveData);
      }
      for (auto& pl : centerPlasmaPrev) {
        if (pl.empty())
          pl.resize(n_lev_max());
        // Ohm's law reads only rho + 3 momentum, so stored slim (like
        // nodePlasmaPrev).
        distribute_FabArray(pl[iLev], cGrids[iLev], DistributionMap(iLev),
                            nHybridMomentsComps, nGst, doMoveData);
      }
      // Hybrid-only node-grid previous-step moments (J^{n-1/2}), slim layout.
      for (auto& pl : nodePlasmaPrev) {
        if (pl.empty())
          pl.resize(n_lev_max());
        distribute_FabArray(pl[iLev], nGrids[iLev], DistributionMap(iLev),
                            nHybridMomentsComps, nGst, doMoveData);
      }
    }
    distribute_FabArray(dBdt[iLev], nGrids[iLev], DistributionMap(iLev), 3,
                        nGst, doMoveData);

    // mMach: node grid for full-PIC, cell grid for hybrid.
    if (useHybridPIC) {
      distribute_FabArray(mMach[iLev], cGrids[iLev], DistributionMap(iLev), 1,
                          nGst, doMoveData);
    } else {
      distribute_FabArray(mMach[iLev], nGrids[iLev], DistributionMap(iLev), 1,
                          nGst, doMoveData);
    }

    // Co-moving frame fields (eBg/uBg), div(E) mass matrix (centerMM), implicit
    // E current (jHat), and node-centred moments (nodePlasma): full-PIC only.
    if (!useHybridPIC) {
      distribute_FabArray(eBg[iLev], nGrids[iLev], DistributionMap(iLev), 3,
                          nGst, doMoveData);

      distribute_FabArray(uBg[iLev], nGrids[iLev], DistributionMap(iLev), 3,
                          nGst, doMoveData);

      distribute_FabArray(centerMM[iLev], cGrids[iLev], DistributionMap(iLev),
                          1, nGst, doMoveData);

      distribute_FabArray(jHat[iLev], nGrids[iLev], DistributionMap(iLev), 3,
                          nGst, doMoveData);

      for (auto& pl : nodePlasma) {
        if (pl.empty())
          pl.resize(n_lev_max());
        distribute_FabArray(pl[iLev], nGrids[iLev], DistributionMap(iLev),
                            nMoments, nGst, doMoveData);
      }
    }
  }

  distribute_grid_arrays(cGridsOld);
}

//==========================================================
void Pic::pre_regrid() {
  if (!parts.empty()) {
    for (int i = 0; i < nSpecies; ++i) {
      // Label the particles outside the OLD PIC region. It should be called
      // before active region is updated.
      parts[i]->label_particles_outside_active_region();
    }
  }
}

void Pic::post_regrid() {

  distribute_arrays(cGridsOld);

  {
    iTot = nSpecies;
    if (plasmaEnergy.empty()) {
      plasmaEnergy.resize(nSpecies + 1);
    }
  }
  //===========Move field data around end====================

  //--------------particles-----------------------------------
  if (parts.empty()) {
    // Let the plugin apply any particle-count override (e.g. LightWave /
    // TopHat force zero macroparticles) after #PARTICLES has been fully parsed
    // so it always wins.
    if (ic_)
      ic_->apply_particle_override(pInfo);

    for (int i = 0; i < nSpecies; ++i) {
      auto ptr = std::make_unique<PicParticles>(
          this, fi, tc, i, fi->get_species_charge(i), fi->get_species_mass(i),
          pInfo, pMode, ic_.get());

      parts.push_back(std::move(ptr));

      auto ptrSource = std::make_unique<PicParticles>(
          this, fi, tc, i, fi->get_species_charge(i), fi->get_species_mass(i),
          pInfo, pMode, ic_.get());

      sourceParts.push_back(std::move(ptrSource));
    }

    if (waveBC.active) {
      for (auto& p : parts) {
        p->waveVelocityKick = [this](const Real* pos, Real t, Real& dvx,
                                     Real& dvy, Real& dvz) {
          wave_velocity_kick(pos, t, dvx, dvy, dvz);
        };
      }
    }
  } else {
    for (int i = 0; i < nSpecies; ++i) {
      // Label the particles outside the NEW PIC region.
      parts[i]->label_particles_outside_active_region_general();

      parts[i]->redistribute_particles();
    }
  }

  // Propagate field wave boundaries to each species to drive wave velocity
  // kicks.
  for (int d = 0; d < nDim; ++d) {
    const bool isWaveLo = (bcField.face(d, 0) == FieldBC::wave);
    const bool isWaveHi = (bcField.face(d, 1) == FieldBC::wave);
    for (auto& p : parts) {
      p->set_wave_face(d, 0, isWaveLo);
      p->set_wave_face(d, 1, isWaveHi);
    }
  }
  //--------------particles-----------------------------------

  // This part does not really work for multi-level.
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    int n = get_local_node_or_cell_number(nodeE[iLev]);
    eSolver.init(n, nDim3, nDim, matvec_E_solver);

    // divESolver uses the full-PIC-only centerDivE array.
    if (!useHybridPIC) {
      n = get_local_node_or_cell_number(centerDivE[iLev]);
      divESolver.init(n, 1, nDim, matvec_divE_accurate);
    }
  }
}

//==========================================================
void Pic::fill_new_node_E() {
  {
    Real xL = 0, xR = 0;
    if (ic_ && ic_->is_tophat()) {
      xL = 0.75 * Geom(0).ProbLo()[ix_] + 0.25 * Geom(0).ProbHi()[ix_];
      xR = 0.75 * Geom(0).ProbHi()[ix_] + 0.25 * Geom(0).ProbLo()[ix_];
    }

    int iLev = 0;
    for (MFIter mfi(nodeE[iLev]); mfi.isValid(); ++mfi) {
      FArrayBox& fab = nodeE[iLev][mfi];
      const Box& box = mfi.validbox();
      const Array4<Real>& arrE = fab.array();
      const auto& status = nodeStatus[iLev][mfi].array();

      ParallelFor(box, [&](int i, int j, int k) {
        IntVect ijk = { AMREX_D_DECL(i, j, k) };
        if (bit::is_new(status(ijk))) {
          if (ic_ && ic_->is_tophat()) {
            const Real x =
                Geom(iLev).CellCenter(i, ix_) - 0.5 * Geom(iLev).CellSize(ix_);
            if (x > xL && x < xR) {
              arrE(ijk, iy_) = 1;
            }
          } else {
            arrE(ijk, ix_) = fi->get_ex(mfi, ijk, iLev);
            arrE(ijk, iy_) = fi->get_ey(mfi, ijk, iLev);
            arrE(ijk, iz_) = fi->get_ez(mfi, ijk, iLev);
          }
        }
      });
    }
  }
  if (finest_level > 0) {
    for (int iLev = 1; iLev < n_lev(); iLev++) {
      fill_fine_lev_new_from_coarse(
          nodeE[iLev - 1], nodeE[iLev], 0, nodeE[iLev - 1].nComp(),
          ref_ratio[iLev - 1], Geom(iLev - 1), Geom(iLev), node_status(iLev),
          node_bilinear_interp);
    }
  }
}

//==========================================================
void Pic::fill_new_node_B() {
  {
    Real xL = 0, xR = 0;
    if (ic_ && ic_->is_tophat()) {
      xL = 0.75 * Geom(0).ProbLo()[ix_] + 0.25 * Geom(0).ProbHi()[ix_];
      xR = 0.75 * Geom(0).ProbHi()[ix_] + 0.25 * Geom(0).ProbLo()[ix_];
    }

    int iLev = 0;
    for (MFIter mfi(nodeB[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();
      const Array4<Real>& arrB = nodeB[iLev][mfi].array();
      const auto& status = nodeStatus[iLev][mfi].array();

      ParallelFor(box, [&](int i, int j, int k) {
        IntVect ijk = { AMREX_D_DECL(i, j, k) };
        if (bit::is_new(status(ijk))) {
          if (ic_ && ic_->is_tophat()) {
            const Real x =
                Geom(iLev).CellCenter(i, ix_) - 0.5 * Geom(iLev).CellSize(ix_);
            if (x > xL && x < xR) {
              arrB(ijk, iz_) = 1;
            }
          } else {
            arrB(ijk, ix_) = fi->get_bx(mfi, ijk, iLev);
            arrB(ijk, iy_) = fi->get_by(mfi, ijk, iLev);
            arrB(ijk, iz_) = fi->get_bz(mfi, ijk, iLev);
          }
        }
      });
    }
  }

  if (finest_level > 0) {
    for (int iLev = 1; iLev < n_lev(); iLev++) {
      fill_fine_lev_new_from_coarse(
          nodeB[iLev - 1], nodeB[iLev], 0, nodeB[iLev - 1].nComp(),
          ref_ratio[iLev - 1], Geom(iLev - 1), Geom(iLev), node_status(iLev),
          node_bilinear_interp);
    }
  }
}

//==========================================================
void Pic::fill_new_center_B() {
  {
    int iLev = 0;
    for (MFIter mfi(centerB[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();
      const Array4<Real>& centerArr = centerB[iLev][mfi].array();
      const auto& nodeArr = nodeB[iLev][mfi].array();
      const auto& status = cellStatus[iLev][mfi].array();

      ParallelFor(
          box, centerB[iLev].nComp(), [&](int i, int j, int k, int iVar) {
            IntVect ijk = { AMREX_D_DECL(i, j, k) };

            if (bit::is_new(status(ijk))) {
              centerArr(ijk, iVar) = 0;

              Box subBox(ijk, ijk + 1);
              ParallelFor(subBox, [&](int ii, int jj, int kk) {
                const Real coef = (nDim == 2 ? 0.25 : 0.125);
                centerArr(ijk, iVar) += coef * nodeArr(ii, jj, kk, iVar);
              });
            }
          });
    }
  }
  if (finest_level > 0) {
    auto& cellInterp = *get_cell_interp();
    for (int iLev = 1; iLev < n_lev(); iLev++) {
      fill_fine_lev_new_from_coarse(centerB[iLev - 1], centerB[iLev], 0,
                                    centerB[iLev - 1].nComp(),
                                    ref_ratio[iLev - 1], Geom(iLev - 1),
                                    Geom(iLev), cell_status(iLev), cellInterp);
    }
  }
}

//==========================================================
void Pic::fill_E_B_fields() {
  fill_new_node_E();
  fill_new_node_B();
  fill_new_center_B();

  //-----Coarse (iLev=0) grid boundary/internal ghost cells are filled----

  nodeE[0].FillBoundary(Geom(0).periodicity());
  nodeB[0].FillBoundary(Geom(0).periodicity());
  centerB[0].FillBoundary(Geom(0).periodicity());
  // NOTE: apply_field_bc() also applies the wave hard source.
  apply_field_bc(nodeStatus[0], nodeB[0], 0, nDim3, &Pic::get_node_B, 0, true);
  apply_field_bc(nodeStatus[0], nodeE[0], 0, nDim3, &Pic::get_node_E, 0, false);
  apply_field_bc(cellStatus[0], centerB[0], 0, centerB[0].nComp(),
                 &Pic::get_center_B, 0, true);

  //-----Fine (iLev>0) grid boundary/internal ghost cells are filled----
  auto& cellInterp = *get_cell_interp();
  for (int iLev = 1; iLev <= finest_level; iLev++) {
    nodeE[iLev].FillBoundary();
    nodeB[iLev].FillBoundary();
    centerB[iLev].FillBoundary();

    fill_fine_lev_bny_from_coarse(nodeE[iLev - 1], nodeE[iLev], 0,
                                  nodeE[iLev - 1].nComp(), ref_ratio[iLev - 1],
                                  Geom(iLev - 1), Geom(iLev), node_status(iLev),
                                  node_bilinear_interp);

    fill_fine_lev_bny_from_coarse(nodeB[iLev - 1], nodeB[iLev], 0,
                                  nodeB[iLev - 1].nComp(), ref_ratio[iLev - 1],
                                  Geom(iLev - 1), Geom(iLev), node_status(iLev),
                                  node_bilinear_interp);

    fill_fine_lev_bny_from_coarse(centerB[iLev - 1], centerB[iLev], 0,
                                  centerB[iLev - 1].nComp(),
                                  ref_ratio[iLev - 1], Geom(iLev - 1),
                                  Geom(iLev), cell_status(iLev), cellInterp);
  }

  // Initial-condition / restart E is node-centred (nodeE). centerEhybrid is
  // seeded from it by averaging the node values to the cell centres, which
  // plays the role of E0 for the very first hybrid particle Boris push.
  if (useHybridPIC) {
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      average_node_to_center(nodeE[iLev], centerEhybrid[iLev]);
      centerEhybrid[iLev].FillBoundary(Geom(iLev).periodicity());
      // Match full-PIC nodeE: the same face type closes the ghost ring of
      // the initial cell-centred E as well.
      apply_field_bc(cellStatus[iLev], centerEhybrid[iLev], 0,
                     centerEhybrid[iLev].nComp(), &Pic::get_center_E, iLev,
                     false);
    }
  }
}

//==========================================================
void Pic::fill_particles() {
  inject_particles_for_new_cells();
  inject_particles_for_boundary_cells();
}

void Pic::fill_source_particles() {
  if (kineticSource)
    return;

  bool doSelectRegion = false;
#ifdef _PT_COMPONENT_
  doSelectRegion = (nSpecies == 4);
#endif

  if (source) {
    for (int i : kineticSpecies_) {
      parts[i]->add_particles_source(source, stateOH, tc->get_dt(), nSourcePPC,
                                     doSelectRegion, adaptiveSourcePPC);
    }
  }
}

//==========================================================
void Pic::update_part_loc_to_half_stage() {
  std::string nameFunc = "Pic::update_part_loc_to_half_stage";

  timing_func(nameFunc);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (int i = 0; i < nSpecies; ++i) {
      if (useHybridPIC && parts[i]->get_charge() < 0)
        continue;
      // Use the time-averaged B in the Boris half-stage position push when
      // enabled (falls back to the instantaneous B before the first average is
      // initialised).
      const auto& nodeBhalf =
          (useAvgFieldB && isBavgInit) ? nodeBavg[iLev] : nodeB[iLev];
      parts[i]->update_position_to_half_stage(nodeEth[iLev], nodeBhalf,
                                              tc->get_dt());
    }
  }

  inject_particles_for_boundary_cells();
}

//==========================================================
void Pic::re_sampling() {
  std::string nameFunc = "Pic::re_sampling";

  timing_func(nameFunc);

  if (doReSampling) {
    for (int i = 0; i < nSpecies; ++i) {
      if (!pInfo.doPreSplitting) {
        if (maxWeightRatio > 1) {
          parts[i]->limit_weight(maxWeightRatio, parts[i]->is_neutral());
        }
        parts[i]->split(reSamplingLowLimit, parts[i]->is_neutral());
        parts[i]->merge(reSamplingHighLimit);
      } else {
        if (maxWeightRatio > 1) {
          parts[i]->limit_weight_new(maxWeightRatio, parts[i]->is_neutral());
        }
        parts[i]->split_new(reSamplingLowLimit, parts[i]->is_neutral());
        parts[i]->merge_new(reSamplingHighLimit);
      }
    }
  }
}

//==========================================================
void Pic::particle_mover() {
  std::string nameFunc = "Pic::mover";

  timing_func(nameFunc);

  // if (useExplicitPIC) {

  // MultiFab tmpE(nGrids[iLev], DistributionMap(iLev), 3, nGst);
  // // nodeE/nodeEth is at t_n/t_{n+1}, tmpE is at t_{n+0.5}
  // MultiFab::LinComb(tmpE, 0.5, nodeEth[iLev], 0, 0.5, nodeE[iLev], 0, 0,
  //                   nodeE[iLev].nComp(), nodeE[iLev].nGrow());
  // for (int i = 0; i < nSpecies; ++i) {
  //   parts[i]->mover(tmpE, nodeB[iLev], iLev, tc->get_dt(),
  //                   tc->get_next_dt());
  // }

  // } else {

  Real dt = tc->get_dt();
  Real dtnext = tc->get_next_dt();

  // Time-averaged B when enabled.
  const Vector<MultiFab>& nodeBpush =
      (useAvgFieldB && isBavgInit) ? nodeBavg : nodeB;
  const Vector<MultiFab>& centerBpush =
      (useAvgFieldB && isBavgInit) ? centerBavg : centerB;
  const Vector<MultiFab>& nodeEpush = nodeEth;
  if (useHybridPIC) {
    for (int i : kineticSpecies_) {
      parts[i]->mover_cell_centered(centerEhybrid, centerBpush, eBg, uBg, dt,
                                    dtnext);
    }
  } else {
    for (int i : kineticSpecies_) {
      parts[i]->mover(nodeEpush, nodeBpush, eBg, uBg, dt, dtnext);
    }
  }

  for (int i : kineticSpecies_) {
    parts[i]->redistribute_particles();
  }
}

//==========================================================
void Pic::calc_mass_matrix() {
  std::string nameFunc = "Pic::calc_mass_matrix";

  if (isGridEmpty)
    return;

  timing_func(nameFunc);

  for (int iLev = 0; iLev < n_lev(); iLev++) {

    jHat[iLev].setVal(0.0);

    if (!useExplicitPIC) {
      const RealMM mm0(0.0);
      nodeMM[iLev].setVal(mm0);
    }

    for (int i = 0; i < nSpecies; ++i) {
      if (useExplicitPIC) {
        parts[i]->calc_jhat(jHat[iLev], nodeB[iLev], tc->get_dt());
      } else {
        parts[i]->calc_mass_matrix(nodeMM[iLev], jHat[iLev], nodeB[iLev],
                                   uBg[iLev], tc->get_dt(), iLev,
                                   solveFieldInCoMov);
      }
    }

    if (useExplicitPIC && nSpecies > 0) {
      parts[0]->apply_jhat_mirror(jHat[iLev], iLev);
    }

    Real invVol = 1;
    for (int i = 0; i < nDim; ++i) {
      invVol *= Geom(iLev).InvCellSize(i);
    }

    jHat[iLev].mult(invVol, 0, jHat[iLev].nComp(), jHat[iLev].nGrow());
    jHat[iLev].SumBoundary(Geom(iLev).periodicity());
    jHat[iLev].FillBoundary(Geom(iLev).periodicity());

    if (doSmoothJ) {
      for (int icount = 0; icount < nSmoothJ; icount++) {
        smooth_multifab(jHat[iLev], iLev, icount % 2 + 1, coefSmoothJ);
      }
    }

    if (!useExplicitPIC) {
      nodeMM[iLev].SumBoundary(Geom(iLev).periodicity());
      nodeMM[iLev].FillBoundary(Geom(iLev).periodicity());
    }
  }

  for (int iLev = n_lev() - 2; iLev >= 0; iLev--) {
    sum_two_lev_interface_node(jHat[iLev], jHat[iLev + 1], 0,
                               jHat[iLev].nComp(), ref_ratio[iLev], Geom(iLev),
                               Geom(iLev + 1), node_status(iLev + 1));
  }

  for (int iLev = n_lev() - 2; iLev >= 0; iLev--) {
    sum_two_lev_interface_node(
        nodeMM[iLev], nodeMM[iLev + 1], 0, nodeMM[iLev].nComp(),
        ref_ratio[iLev], Geom(iLev), Geom(iLev + 1), node_status(iLev + 1));
  }

  // WARNING: interp_from_coarse_to_fine_for_domain_edge might be needed here
}
//==========================================================
void Pic::calc_mass_matrix_amr() {
  std::string nameFunc = "Pic::calc_mass_matrix";

  if (isGridEmpty)
    return;
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    nodeMM[iLev].setVal(0.0);
    jHat[iLev].setVal(0.0);
  }
  if (skipMassMatrix)
    return;

  timing_func(nameFunc);
  //////////////////////////////////////////////////////////////////////
  amrex::Vector<amrex::Vector<amrex::MultiFab> > jhc;
  amrex::Vector<amrex::MultiFab> jhf;
  amrex::Vector<amrex::Vector<UMultiFab<RealMM> > > nmmc;
  amrex::Vector<UMultiFab<RealMM> > nmmf;
  jhc.resize(n_lev());
  jhf.resize(n_lev());
  nmmc.resize(n_lev());
  nmmf.resize(n_lev());
  for (int iLev = 1; iLev < n_lev(); iLev++) {
    jhc[iLev].resize(iLev);
    nmmc[iLev].resize(iLev);
  }
  for (int iLev = 1; iLev < n_lev(); iLev++) {
    BoxArray bac = nodeB[iLev].boxArray();
    for (int i = iLev - 1; i >= 0; i--) {
      bac.coarsen(ref_ratio[iLev]);
      jhc[iLev][i].define(bac, nodeB[iLev].DistributionMap(), 3, 0);
      nmmc[iLev][i].define(bac, nodeB[iLev].DistributionMap(),
                           nodeMM[iLev].nComp(), 0);
      jhc[iLev][i].setVal(0.0);
      nmmc[iLev][i].setVal(0.0);
    }
  }
  for (int iLev = 0; iLev < n_lev() - 1; iLev++) {
    BoxArray baf = nodeB[iLev].boxArray();
    baf.refine(ref_ratio[iLev]);
    jhf[iLev].define(baf, nodeB[iLev].DistributionMap(), 3, 0);
    nmmf[iLev].define(baf, nodeB[iLev].DistributionMap(), nodeMM[iLev].nComp(),
                      0);
    jhf[iLev].setVal(0.0);
    nmmf[iLev].setVal(0.0);
  }
  //////////////////////////////////////////////////////////////////////
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (int i = 0; i < nSpecies; ++i) {
      parts[i]->calc_mass_matrix_amr(nodeMM[iLev], nmmc, nmmf, jHat[iLev], jhc,
                                     jhf, nodeB[iLev], uBg[iLev], tc->get_dt(),
                                     iLev, solveFieldInCoMov, cellStatus);
    }
  }
  //////////////////////////////////////////////////////////////////////
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    jHat[iLev].SumBoundary(Geom(iLev).periodicity());
    nodeMM[iLev].SumBoundary(Geom(iLev).periodicity());
  }
  Vector<Real> invVol(n_lev());
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    invVol[iLev] = 1.0;
    for (int i = 0; i < nDim; ++i) {
      invVol[iLev] *= Geom(iLev).InvCellSize(i);
    }
  }

  for (int iLev = finest_level - 1; iLev >= 0; iLev--) {
    for (int i = finest_level; i > iLev; i--) {
      jHat[iLev].ParallelAdd(jhc[i][iLev]);
      nmmc[i][iLev].mult(invVol[iLev] / invVol[i]);
      nodeMM[iLev].ParallelAdd(nmmc[i][iLev]);
    }
  }
  for (int iLev = finest_level; iLev > 0; iLev--) {
    jHat[iLev].ParallelAdd(jhf[iLev - 1]);
    nmmf[iLev - 1].mult(invVol[iLev] / invVol[iLev - 1]);
    nodeMM[iLev].ParallelAdd(nmmf[iLev - 1]);
  }

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    Real invVol = 1;
    for (int i = 0; i < nDim; ++i) {
      invVol *= Geom(iLev).InvCellSize(i);
    }
    jHat[iLev].mult(invVol, 0, jHat[iLev].nComp(), jHat[iLev].nGrow());
    jHat[iLev].FillBoundary(Geom(iLev).periodicity());
    nodeMM[iLev].FillBoundary(Geom(iLev).periodicity());
  }

  //////////// Fill empty nodeMM elements////////////////
  // for (int iLev = 0; iLev < n_lev(); iLev++) {
  //   for (MFIter mfi(nodeMM[iLev]); mfi.isValid(); ++mfi) {
  //     // Finalize the mass matrix calculation.
  //     const Box box = mfi.validbox();
  //     const auto lo = lbound(box);
  //     const auto hi = ubound(box);

  //     Array4<RealMM> const& mmArr = nodeMM[iLev][mfi].array();

  //     // We only need the mass matrix on the physical nodes. But the first
  //     // layer
  //     // of the ghost nodes may contributes to the physical nodes below
  //     (ghost
  //     // node constributes as a sender). So, we need the '-1' and '+1' staff.
  //     const int iMin = lo.x - 1, jMin = lo.y - 1,
  //               kMin = nDim > 2 ? lo.z - 1 : 0;
  //     const int iMax = hi.x + 1, jMax = hi.y + 1,
  //               kMax = nDim > 2 ? hi.z + 1 : 0;

  //     int gps, gpr; // gp_send, gp_receive
  //     for (int k1 = kMin; k1 <= kMax; k1++)
  //       for (int j1 = jMin; j1 <= jMax; j1++)
  //         for (int i1 = iMin; i1 <= iMax; i1++) {
  //           const int kp = 2;
  //           const int kr = nDim > 2 ? k1 + kp - 1 : 0;
  //           if (kr > kMax || kr < kMin)
  //             continue;
  //           auto& datas0 = mmArr(i1, j1, k1);
  //           for (int jp = 0; jp < 3; jp++) {
  //             const int jr = j1 + jp - 1;
  //             if (jr > jMax || jr < jMin)
  //               continue;
  //             const int jpr = 2 - jp;
  //             for (int ip = 0; ip < 3; ip++) {
  //               const int ir = i1 + ip - 1;
  //               if (ir > iMax || ir < iMin)
  //                 continue;
  //               const int ipr = 2 - ip;
  //               gpr = jpr * 3 + ipr;
  //               gps = 18 + jp * 3 + ip; // gps = kp*9+jp*3+kp

  //               Real* const datar = &(mmArr(ir, jr, kr)[gpr * 9]);
  //               const Real* const datas = &(datas0[gps * 9]);
  //               for (int idx = 0; idx < 9; idx++) {
  //                 datar[idx] = datas[idx];
  //               } // idx
  //             } // kp
  //           } // jp
  //         } // k1
  //   }
  // }
  ///////////////////////////////////////////////////////////////
}
//==========================================================
void Pic::sum_moments(bool updateDt) {
  std::string nameFunc = "Pic::sum_moments";
  if (isGridEmpty)
    return;

  timing_func(nameFunc);

  plasmaEnergy[iTot] = 0;
  for (int i = 0; i < nSpecies; ++i) {
    Real energy = 0.0;
    if (useHybridPIC) {
      // Cell-centred moment deposit into centerPlasma[i].
      energy = parts[i]->sum_moments_cell_centered(centerPlasma[i]);
    } else {
      energy = parts[i]->sum_moments(nodePlasma[i], nodeB, tc->get_dt());
    }
    plasmaEnergy[i] = energy;
    plasmaEnergy[iTot] += energy;
  }

  if (updateDt) {
    amrex::Vector<amrex::Real> uMax(n_lev());
    amrex::Vector<amrex::Real> dxMin(n_lev());
    amrex::Vector<amrex::Real> dtMax(n_lev());
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      const auto& dx = Geom(iLev).CellSize();
      dxMin[iLev] = min(AMREX_D_DECL(dx[ix_], dx[iy_], dx[iz_]));

      if (tc->get_cfl() > 0 || doReport) {
        uMax[iLev] = 0.0;
        for (int i = 0; i < nSpecies; ++i) {
          amrex::MultiFab& momMF =
              useHybridPIC ? centerPlasma[i][iLev] : nodePlasma[i][iLev];
          Real uMaxSpecies = parts[i]->calc_max_thermal_velocity(momMF);
          ParallelDescriptor::ReduceRealMax(uMaxSpecies);

          if (doReport) {
            Print() << printPrefix << std::setprecision(5) << "lev " << iLev
                    << " Species " << i << ": max(uth) = " << uMaxSpecies
                    << std::endl;
          }

          if (uMaxSpecies > uMax[iLev]) {
            uMax[iLev] = uMaxSpecies;
          }
        }

        // Generic override of the CFL signal speed (e.g. the old TopHat
        // option used a fixed value of 1.0). A negative fixedUMax keeps the
        // particle-thermal-velocity estimate.
        if (fixedUMax >= 0) {
          uMax[iLev] = fixedUMax;
        }

        dtMax[iLev] = (uMax[iLev] > 0.0) ? dxMin[iLev] / uMax[iLev]
                                         : std::numeric_limits<Real>::max();
      }
    }

    if (tc->get_cfl() > 0) {
      Real dt0 = *std::min_element(dtMax.begin(), dtMax.end());
      Real dt = tc->get_cfl() * dt0;
      tc->set_next_dt(dt);

      if (tc->get_dt() < 0) {
        tc->set_dt(dt);
      }
    }

    if (doReport) {
      Print() << printPrefix << std::setprecision(5)
              << "dt = " << tc->get_dt_si()
              << " dtNext = " << tc->get_next_dt_si() << std::endl;

      for (int iLev = 0; iLev < n_lev(); iLev++) {
        Print() << printPrefix << std::setprecision(5) << "iLev = " << iLev
                << " : CFL(dtNext) = " << tc->get_next_dt() / dtMax[iLev]
                << std::endl;
      }
    }
  }

  if (useHybridPIC) {
    // Cell-centred hybrid moments: sum the per-species deposits into
    // centerPlasmaSum and sync the nodePlasma output mirror (once per step,
    // so the plot / restart / tracker path that reads nodePlasma sees correct
    // data -- the node-sync bridge of the hybrid solver).
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      centerPlasmaSum[nSpecies][iLev].setVal(0.0);
    }

    for (int i = 0; i < nSpecies; ++i) {
      parts[i]->convert_to_fluid_moments(centerPlasma[i]);
    }

    for (int i : kineticSpecies_) {
      for (int iLev = 0; iLev < n_lev(); iLev++) {
        // centerPlasmaSum[nSpecies] holds the sum of all kinetic-ion species.
        // kineticSpecies_ excludes the (implicit fluid) electron.
        MultiFab::Add(centerPlasmaSum[nSpecies][iLev], centerPlasma[i][iLev], 0,
                      0, nMoments, nGst);
      }
    }

    // Fill ghost cells for centerPlasmaSum so that assemble_ohm_E's 2-dx
    // stencil reads valid data at MPI boundaries and coarse-fine interfaces.
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      centerPlasmaSum[nSpecies][iLev].FillBoundary(Geom(iLev).periodicity());
    }

    // Mirror ion moments into the physical-wall ghost cells for smooth
    // pressure-gradient / Hall stencils at a wall.
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      apply_centerPlasma_BC(cell_status(iLev), centerPlasmaSum[nSpecies][iLev],
                            iLev);
      apply_centerPlasma_BC(cell_status(iLev), centerPlasmaPrev[nSpecies][iLev],
                            iLev);
    }

    // Fill coarse-fine interface ghost cells for centerPlasmaSum and
    // centerPlasmaPrev on fine levels from the coarse level. Without this,
    // assemble_ohm_E's pressure-gradient stencil reads zero/stale ghost cells
    // at the coarse-fine interface, causing incorrect E fields and runaway
    // particle heating.
    if (finest_level > 0 && useHybridPIC) {
      auto& cellInterp = *get_cell_interp();
      for (int iLev = 1; iLev < n_lev(); iLev++) {
        fill_fine_lev_bny_from_coarse(
            centerPlasmaSum[nSpecies][iLev - 1],
            centerPlasmaSum[nSpecies][iLev], 0,
            centerPlasmaSum[nSpecies][iLev].nComp(), ref_ratio[iLev - 1],
            Geom(iLev - 1), Geom(iLev), cell_status(iLev), cellInterp);

        fill_fine_lev_bny_from_coarse(
            centerPlasmaPrev[nSpecies][iLev - 1],
            centerPlasmaPrev[nSpecies][iLev], 0,
            centerPlasmaPrev[nSpecies][iLev].nComp(), ref_ratio[iLev - 1],
            Geom(iLev - 1), Geom(iLev), cell_status(iLev), cellInterp);
      }
    }
  } else {
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      nodePlasma[nSpecies][iLev].setVal(0.0);
    }

    for (int i = 0; i < nSpecies; ++i) {
      parts[i]->convert_to_fluid_moments(nodePlasma[i]);
    }

    for (int i : kineticSpecies_) {
      for (int iLev = 0; iLev < n_lev(); iLev++) {
        // nodePlasma[nSpecies] holds the sum of all ion species.
        // kineticSpecies_ excludes the (implicit fluid) electron.
        MultiFab::Add(nodePlasma[nSpecies][iLev], nodePlasma[i][iLev], 0, 0,
                      nMoments, nGst);
      }
    }
  }

  if (!useHybridPIC) {
    calc_mach_number();
  }

  isMomentsUpdated = true;
}

//==========================================================
void Pic::finalize_units_conversion() {
  // Convert input units to code units using normalization factors from fi.
  convert_resistivity();
  convert_electron_density0();
  convert_inflow_state();
}

//==========================================================
void Pic::convert_resistivity() {
  if (!useHybridPIC)
    return;

  const Real Si2NoV = fi->get_Si2NoV();
  const Real Si2NoL = fi->get_Si2NoL();

  // Resistive term eta*J: [eta] = [U]*[L], so
  // eta_code = 4*pi * eta_SI * Si2NoV * Si2NoL.
  if (etaResistivitySI > 0) {
    etaResistivity = fourPI * etaResistivitySI * Si2NoV * Si2NoL;
    amrex::Print() << "  etaResistivity: " << etaResistivitySI << " [m^2/s] -> "
                   << etaResistivity << " [code units]"
                   << "  (Si2NoV = " << Si2NoV << ", Si2NoL = " << Si2NoL
                   << ")\n";
  }

  // Hyper-resistive term eta_h*nabla^2 J: [eta_h] = [U]*[L]^3, so
  // eta_h_code = 4*pi * eta_h_SI * Si2NoV * Si2NoL^3. A single physical value
  // is used on every level (the same choice as grid mode in update_B_hybrid).
  if (etaHyperSI > 0 && etaHyperMode == "si") {
    const Real etaHyper = fourPI * etaHyperSI * Si2NoV * std::pow(Si2NoL, 3);
    for (int iLev = 0; iLev < n_lev_max(); ++iLev)
      etaHyperLev[iLev] = etaHyper;
    amrex::Print() << "  etaHyper: " << etaHyperSI << " [m^4/s, si] -> "
                   << etaHyper << " [code units]\n";
  }

  // Guard against uninitialized normalization producing non-positive
  // coefficients.
  if ((etaResistivitySI > 0 && !(etaResistivity > 0)) ||
      (etaHyperSI > 0 && etaHyperMode == "si" &&
       (etaHyperLev.empty() || !(etaHyperLev[0] > 0)))) {
    amrex::Abort("Pic::convert_resistivity: the SI->code conversion produced a "
                 "non-positive resistivity. Check the normalization "
                 "(#NORMALIZATION lNormSI / uNormSI).");
  }
}

//==========================================================
void Pic::convert_electron_density0() {
  // Convert electron density from amu/cc to code units.
  electronDensity0 =
      electronDensity0In * 1.0e6 * cProtonMassSI * fi->get_Si2NoRho();

  // Auto density floor in code units.
  if (rhoMinOhm <= 0)
    rhoMinOhm = 1.0e-6 * electronDensity0;

  amrex::Print() << "  electronDensity0: " << electronDensity0In
                 << " [amu/cc] -> " << electronDensity0
                 << " [code units]  (Si2NoRho = " << fi->get_Si2NoRho()
                 << ")\n";
}

//==========================================================
void Pic::convert_inflow_state() {
  if (!inflowDefined_)
    return;

  // Convert density from amu/cc to code units.
  const double Si2NoRho = fi->get_Si2NoRho();
  inflowRho_ *= 1.0e6 * cProtonMassSI * Si2NoRho;

  // Convert velocity from km/s to code units.
  const double vFactor = 1.0e3 * fi->get_Si2NoV();
  inflowUx_ *= vFactor;
  inflowUy_ *= vFactor;
  inflowUz_ *= vFactor;

  // Convert temperature T [K] to code units (kT / (m_p * uNorm^2)).
  const double unormSI = fi->get_unorm_si();
  inflowT_ = cBoltzmannSI * inflowT_ / (cProtonMassSI * unormSI * unormSI);

  // Publish converted state to FluidInterface for boundary particle injection.
  FluidInterfaceParameters::InflowVel baseVel;
  baseVel.nDens = inflowRho_;
  baseVel.ux = inflowUx_;
  baseVel.uy = inflowUy_;
  baseVel.uz = inflowUz_;
  baseVel.vth = 0.0;

  amrex::Vector<FluidInterfaceParameters::InflowVel> stateVec(nSpecies,
                                                              baseVel);
  const int nParts = static_cast<int>(parts.size());
  for (int iS = 0; iS < nSpecies; ++iS) {
    const double mass_i =
        (iS < nParts && parts[iS]) ? parts[iS]->get_mass() : 1.0;
    if (inflowT_ > 0 && mass_i > 0)
      stateVec[iS].vth = std::sqrt(inflowT_ / mass_i);
  }
  fi->set_inflow_state(stateVec);
  fi->set_inflow_defined(true);

  amrex::Print() << "  #INFLOW state (code units):"
                 << " n=" << inflowRho_ << " u=(" << inflowUx_ << ","
                 << inflowUy_ << "," << inflowUz_ << ")"
                 << " vth=" << (inflowT_ > 0 ? std::sqrt(inflowT_) : 0.0)
                 << "  (Si2NoRho=" << Si2NoRho
                 << ", Si2NoV=" << fi->get_Si2NoV() << ")\n";
}

//==========================================================
// Ma = u/vth
void Pic::calc_mach_number() {
  for (int iLev = 0; iLev < n_lev(); iLev++) {

    // Hybrid: Mach number from the live cell-centred summed ion moments
    // (centerPlasmaSum[nSpecies]). Full-PIC: from the node-centred
    // nodePlasma[nSpecies]. mMach is allocated on the matching grid.
    const auto& momentsMF = useHybridPIC ? centerPlasmaSum[nSpecies][iLev]
                                         : nodePlasma[nSpecies][iLev];
    for (MFIter mfi(momentsMF); mfi.isValid(); ++mfi) {
      const Box& box = mfi.fabbox();
      const Array4<const Real>& moments = momentsMF[mfi].array();
      const Array4<Real>& mach = mMach[iLev][mfi].array();

      ParallelFor(box, [&](int i, int j, int k) {
        Real rho = moments(i, j, k, iRho_);
        if (rho <= 0) {
          mach(i, j, k) = 0;
          return;
        }

        Real u = moments(i, j, k, iUx_) / rho;
        Real v = moments(i, j, k, iUy_) / rho;
        Real w = moments(i, j, k, iUz_) / rho;
        Real uBulk = sqrt(u * u + v * v + w * w);

        Real p = (moments(i, j, k, iPxx_) + moments(i, j, k, iPyy_) +
                  moments(i, j, k, iPzz_)) /
                 3.0;
        Real vth = sqrt(gamma0 * p / rho);

        mach(i, j, k) = uBulk / max(vth, 1e-99);
      });
    }
  }
}

//==========================================================
void Pic::calc_cost_per_cell() {
  const BalanceStrategy balanceStrategy = domainParameters.balanceStrategy;
  const int cellWeight = domainParameters.cellWeight;
  if (!isMomentsUpdated && balanceStrategy == BalanceStrategy::Particle) {
    sum_moments(false);
  }
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    if (balanceStrategy == BalanceStrategy::Cell) {
      cellCost[iLev].setVal(1.0);
    } else {
      // Balance by particles or hybrid. Hybrid: cellCost and the summed ion
      // moments are both cell-centred, so copy iNum_ directly (no cell->node->
      // cell roundtrip and no need to materialize the deferred nodePlasma
      // mirror). Full-PIC: average the node-centred nodePlasma particle count
      // to the cell grid.
      if (useHybridPIC) {
        MultiFab::Copy(cellCost[iLev], centerPlasmaSum[nSpecies][iLev], iNum_,
                       0, cellCost[iLev].nComp(), cellCost[iLev].nGrow());
      } else {
        average_node_to_cellcenter(
            cellCost[iLev], 0, nodePlasma[nSpecies][iLev], iNum_,
            cellCost[iLev].nComp(), cellCost[iLev].nGrow());
      }
    }

    for (MFIter mfi(cellCost[iLev]); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();

      const Array4<Real>& cost = cellCost[iLev][mfi].array();
      const Array4<int const> status = cellStatus[iLev][mfi].array();

      ParallelFor(box, [&](int i, int j, int k) {
        if (bit::is_refined(status(i, j, k))) {
          cost(i, j, k) = 0;
        } else if (bit::is_domain_edge(status(i, j, k))) {
          // When calculating cost for each cell, the ghost cells are
          // excluded. However, ghost cells also take time to update (e.g.
          // fill boundary, launch and update boundary particles...).
          // Therefore, the cost of ghost cells is added to the cost of the
          // corresponding valid cells. The factor of 2 is just a guess.
          cost(i, j, k) *= 2;
        }

        if (balanceStrategy == BalanceStrategy::Particle ||
            balanceStrategy == BalanceStrategy::Hybrid) {
          // 1. The cells have been refined also allocated and use memory.
          // 2. It looks like these cells need calculations when
          // interpolating between levels.
          // 3. The number 10 is chosen by experience.
          cost(i, j, k) += cellWeight;
        }
      });
    }
  }
}

//==========================================================
void Pic::divE_correction() {
  std::string nameFunc = "Pic::divE_correction";

  timing_func(nameFunc);

  for (int iIter = 0; iIter < nDivECorrection; iIter++) {

    sum_to_center(true);

    if (doReport)
      Print() << "\n-----" << printPrefix << " div(E) correction at iter "
              << iIter << "----------" << std::endl;

    calculate_phi(divESolver, 0);

    divE_correct_particle_position();
  }

  for (int i = 0; i < nSpecies; ++i) {
    // The particles outside the simulation domain is marked for deletion
    // inside divE_correct_particle_position(). redistribute_particles()
    // deletes these particles. In order to get correct moments, re-inject
    // particles in the ghost cells.
    parts[i]->redistribute_particles();
  }

  inject_particles_for_boundary_cells();

  sum_to_center(false);
}

//==========================================================
void Pic::divE_correct_particle_position() {
  std::string nameFunc = "Pic::correct_position";

  timing_func(nameFunc);
  for (int iLev = 0; iLev < n_lev(); ++iLev) {
    for (int i = 0; i < nSpecies; ++i) {
      parts[i]->divE_correct_position(centerPhi, iLev);
    }
  }
}

//==========================================================
void Pic::calculate_phi(LinearSolver& solver, int iLev) {
  std::string nameFunc = "Pic::calculate_phi";

  timing_func(nameFunc);

  {
    MultiFab residual(cGrids[iLev], DistributionMap(iLev), 1, nGst);

    solver.reset(get_local_node_or_cell_number(centerDivE[iLev]));
    // div_node_to_center(nodeE[iLev], residual, Geom(iLev).InvCellSize());
    MultiFab::Copy(residual, centerDivE[iLev], 0, 0, 1, nGst);
    Real coef = 1.0 / rhoTheta;

    MultiFab::LinComb(residual, coef, residual, 0, -fourPI * coef,
                      centerNetChargeN[iLev], 0, 0, residual.nComp(),
                      residual.nGrow());
    if (finest_level > 0) {
      skip_cells_divE_correction(residual, cellStatus[iLev], iLev);
    }

    convert_3d_to_1d(residual, solver.rhs, iLev);

    BL_PROFILE_VAR("Pic::phi_iterate", solve);
    solver.solve(iLev, doReport);
    BL_PROFILE_VAR_STOP(solve);

    convert_1d_to_3d(solver.xLeft, centerPhi[iLev], iLev);
    centerPhi[iLev].FillBoundary(Geom(iLev).periodicity());
  }
}

//==========================================================
void Pic::divE_accurate_matvec(const double* vecIn, double* vecOut, int iLev) {
  std::string nameFunc = "Pic::divE_matvec";
  timing_func(nameFunc);

  // const int iLev = 0;
  zero_array(vecOut, divESolver.get_nSolve());

  MultiFab inMF(cGrids[iLev], DistributionMap(iLev), 1, nGst);

  convert_1d_to_3d(vecIn, inMF, iLev);
  inMF.FillBoundary(0, 1, IntVect(1), Geom(iLev).periodicity());

  MultiFab outMF(cGrids[iLev], DistributionMap(iLev), 1, nGst);
  outMF.setVal(0.0);

  for (MFIter mfi(inMF); mfi.isValid(); ++mfi) {
    const Box& box = mfi.validbox();

    const Array4<Real>& lArr = outMF[mfi].array();
    const Array4<Real const>& rArr = inMF[mfi].array();
    const Array4<RealCMM>& mmArr = centerMM[iLev][mfi].array();

    ParallelFor(box, [&](int i, int j, int k) {
      IntVect ijk = { AMREX_D_DECL(i, j, k) };
      Box subBox(ijk - 1, ijk + 1);

      ParallelFor(subBox, [&](int i2, int j2, int k2) {
        const int gp = (i2 - i + 1) * 9 + (j2 - j + 1) * 3 + k2 - k + 1;
        lArr(i, j, k) += rArr(i2, j2, k2) * mmArr(i, j, k)[gp];
      });
    });
  }
  outMF.mult(fourPI * fourPI);
  convert_3d_to_1d(outMF, vecOut, iLev);
}

//==========================================================
void Pic::sum_to_center(bool isBeforeCorrection) {
  std::string nameFunc = "Pic::sum_to_center";

  timing_func(nameFunc);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    centerNetChargeNew[iLev].setVal(0.0);

    const RealCMM mm0(0.0);
    centerMM[iLev].setVal(mm0);

    bool doNetChargeOnly = !isBeforeCorrection;

    for (int i = 0; i < nSpecies; ++i) {
      parts[i]->sum_to_center(centerNetChargeNew[iLev], centerMM[iLev],
                              doNetChargeOnly, iLev);
    }
    if (!doNetChargeOnly) {
      centerMM[iLev].SumBoundary(Geom(iLev).periodicity());
    }

    centerNetChargeNew[iLev].SumBoundary(Geom(iLev).periodicity());

    if (iLev == 0) {
      apply_BC(cellStatus[iLev], centerNetChargeNew[iLev], 0,
               centerNetChargeNew[iLev].nComp(), &Pic::get_zero, iLev);
    }

    MultiFab::LinComb(
        centerNetChargeN[iLev], 1 - rhoTheta, centerNetChargeOld[iLev], 0,
        rhoTheta, centerNetChargeNew[iLev], 0, 0,
        centerNetChargeN[iLev].nComp(), centerNetChargeN[iLev].nGrow());

    if (!isBeforeCorrection) {
      MultiFab::Copy(centerNetChargeOld[iLev], centerNetChargeNew[iLev], 0, 0,
                     centerNetChargeOld[iLev].nComp(),
                     centerNetChargeOld[iLev].nGrow());
    }
  }
}

//==========================================================
void Pic::sum_to_center_amr(bool isBeforeCorrection, int iLev) {

  std::string nameFunc = "Pic::sum_to_center";
  timing_func(nameFunc);

  bool doNetChargeOnly = !isBeforeCorrection;

  centerNetChargeNew[iLev].setVal(0.0);
  const RealCMM mm0(0.0);
  centerMM[iLev].setVal(mm0);

  MultiFab jf;
  MultiFab jc;
  int fLev = iLev + 1;
  int cLev = iLev - 1;
  if (iLev == 0) {
    cLev = iLev;
  }
  if (iLev == finest_level) {
    fLev = iLev;
  }
  {
    BoxArray bac = centerB[cLev].boxArray();
    bac.refine(ref_ratio[iLev]);
    jc.define(bac, centerB[cLev].DistributionMap(), 1, 1);
    jc.setVal(0.0);
    BoxArray baf = centerB[fLev].boxArray();
    baf.coarsen(ref_ratio[iLev]);
    baf.grow(1);
    jf.define(baf, centerB[fLev].DistributionMap(), 1, 1);
    jf.setVal(0.0);
  }
  for (int i = 0; i < nSpecies; ++i) {
    parts[i]->sum_to_center_amr(centerNetChargeNew[iLev], jc, jf,
                                centerMM[iLev], doNetChargeOnly, iLev);
  }

  if (!doNetChargeOnly) {
    centerMM[iLev].SumBoundary(Geom(iLev).periodicity());
  }

  centerNetChargeNew[iLev].SumBoundary(Geom(iLev).periodicity());
  jc.SumBoundary();
  jf.SumBoundary();
  amrex::MultiFab tmp;
  tmp.define(centerB[iLev].boxArray(), centerB[iLev].DistributionMap(), 1, 0);
  tmp.setVal(0.0);
  tmp.ParallelCopy(jc);
  MultiFab::Add(centerNetChargeNew[iLev], tmp, 0, 0, 1, 0);
  tmp.setVal(0.0);
  tmp.ParallelCopy(jf);
  MultiFab::Add(centerNetChargeNew[iLev], tmp, 0, 0, 1, 0);

  if (iLev == 0) {
    apply_BC(cellStatus[iLev], centerNetChargeNew[iLev], 0,
             centerNetChargeNew[iLev].nComp(), &Pic::get_zero, iLev);
  }

  MultiFab::LinComb(
      centerNetChargeN[iLev], 1 - rhoTheta, centerNetChargeOld[iLev], 0,
      rhoTheta, centerNetChargeNew[iLev], 0, 0, centerNetChargeN[iLev].nComp(),
      centerNetChargeN[iLev].nGrow());

  if (!isBeforeCorrection) {
    MultiFab::Copy(centerNetChargeOld[iLev], centerNetChargeNew[iLev], 0, 0,
                   centerNetChargeOld[iLev].nComp(),
                   centerNetChargeOld[iLev].nGrow());
  }
}

//==========================================================
void Pic::update(bool doReportIn) {
  std::string nameFunc = "Pic::update";

  if (isGridEmpty || !usePIC)
    return;

  timing_func(nameFunc);

  doReport = doReportIn;

  Real tStart = second();

  if (reportParticleQuality) {
    if (tc->get_cycle() % 20 == 0) {
      WriteParticleQualityToParaView();
    }
  }

  // Co-moving frame solver is full-PIC only.
  if (!useHybridPIC &&
      (solveFieldInCoMov || useUpwindB || (useUpwindE && cMaxE <= 0))) {
    update_U0_E0();
  }

  if (solveEM) {
    if (finest_level == 0) {
      calc_mass_matrix();
    } else {
      calc_mass_matrix_amr();
    }
  }

  if (solveEM) {
    update_E();
  }

  // Hybrid path: the particle Boris push happens BEFORE the moment deposit and
  // the Ohm's-law E computation. The push uses the nodeEth and B^n computed at
  // the end of the previous step's B update.
  if (useHybridPIC && isFirstHybridStep) {
    seed_first_hybrid_step();
  }

  particle_mover();

  // Calling re_sampling after particle mover so that all the particles
  // outside the domain have been deleted.
  re_sampling();

  charge_exchange();

  // Apply chemical loss (recombination, etc.) by reducing particle
  // weights.  Must come before fill_source_particles() so that loss
  // and source are applied in the correct order within one step.
  if (source && source->use_loss_source()) {
    for (int i = 0; i < nSpecies; ++i) {
      parts[i]->apply_loss(source, tc->get_dt());
    }
  }

  if (source) {
    fill_source_particles();
  }

  inject_particles_for_boundary_cells();

  // Inject incoming flux at physical inflow faces before the moment deposit.
  // Kept outside inject_particles_for_boundary_cells to avoid t=0
  // pre-injection.
  if (usePIC) {
    const Real dt = tc->get_dt();
    for (int i = 0; i < nSpecies; ++i) {
      parts[i]->inject_flux_at_inflow_faces(dt);
    }
  }

  isMomentsUpdated = false;

  if (solveEM) {
    if (projectDownEmFields) {
      project_down_E();
    }
    update_B();
  } else if (useHybridPIC) {
    // Deposit the fresh ion moments (J^{n+1/2}) AFTER the particle push. The
    // previous deposit (J^{n-1/2}) is first saved into nodePlasmaPrev so the
    // Ohm's law can time-interpolate between the two (hstep scheme).
    save_current_moments_to_prev();
    sum_moments(false);
    smooth_moments();
    update_B_hybrid();
    isFirstHybridStep = false;
  }

  // Only to be turned on if DivE error needs to be visulaized when DivE
  // cleaning is not turned on

  // for (int i = 0; i < 2; i++) {
  //   sum_moments(true);
  //   sum_moments(false);
  // }

  if (solveEM && doCorrectDivE) {
    if (finest_level == 0) {
      divE_correction();
    } else {
      amr_divE_correction();
    }
  }

  tc->set_dt(tc->get_next_dt());

#ifdef _PC_COMPONENT_
  //  For PT simulations, moments are only useful for output. So, there is no
  //  need to call sum_moments() for every step.
  sum_moments(true);
#endif

#ifdef _PT_COMPONENT_
  if (maxExchangeRatio > maxExchangeRatioLimit) {
    Real dtNew = tc->get_dt() * maxExchangeRatioLimit / maxExchangeRatio;
    tc->set_dt(dtNew);
    tc->set_next_dt(dtNew);
    Print() << printPrefix << " maxExchangeRatio = " << maxExchangeRatio
            << " maxExchangeRatioLimit = " << maxExchangeRatioLimit
            << " dt is reduced to " << tc->get_dt_si() << std::endl;
  } else {
    if (tc->get_dt() < tc->get_dt_max()) {
      // Increase dt if allowed.
      Real dtnow = tc->get_dt();
      Real dtNew = min(dtnow * maxExchangeRatioLimit / maxExchangeRatio,
                       tc->get_dt_max());

      if (dtNew > dtnow * (1 + 1e-6)) {
        tc->set_dt(dtNew);
        tc->set_next_dt(dtNew);
        Print() << printPrefix << " maxExchangeRatio = " << maxExchangeRatio
                << " maxExchangeRatioLimit = " << maxExchangeRatioLimit
                << " dt is increased to " << tc->get_dt_si() << std::endl;
      }
    }
  }
#endif

  if (doReport) {
    Real tEnd = second();
    Real nPoint = activeRegion.d_numPts();
    int nProc = ParallelDescriptor::NProcs();
    // The unit of the speed is (cell per processor per second)
    Real speed = nPoint / nProc / (tEnd - tStart);

    // speedNorm is a value obtained from tests.
    Real speedNorm = 1000;
    Print() << printPrefix
            << "Normalized PIC simulation speed = " << speed / speedNorm
            << " (performance is good if the value >> 1 and bad if <<1 )"
            << std::endl;

    report_load_balance();
  }

  if (dnMemory > 0 && tc->get_cycle() % dnMemory == 0) {
    Print() << printPrefix << "Load balance before freeing memory:\n";
    report_load_balance();
    Print() << printPrefix << "Freeing memory...\n";
    free_memory();
    Print() << printPrefix << "Load balance after freeing memory:\n";
    report_load_balance();
  }
}

//==========================================================
void Pic::free_memory() {
  std::string nameFunc = "Pic::free_memory";
  timing_func(nameFunc);

  if (auto* p = dynamic_cast<amrex::CArena*>(amrex::The_Arena())) {
    p->freeUnused();
  }
  if (auto* p = dynamic_cast<amrex::CArena*>(amrex::The_Pinned_Arena())) {
    p->freeUnused();
  }

  amrex::FabArrayBase::flushTileArrayCache();

  amrex::Arena::PrintUsage();

  for (int i = 0; i < nSpecies; ++i) {
    amrex::Print() << "[Particles " << i << " Before ShrinkToFit]: ";
    parts[i]->PrintCapacity();
    parts[i]->ShrinkToFit();
    amrex::Print() << "[Particles " << i << " After ShrinkToFit]: ";
    parts[i]->PrintCapacity();
  }

#if defined(__linux__)
  // Force glibc to return free pages on the heap to the OS.
  // This is necessary because glibc often holds onto freed blocks internally.
  malloc_trim(0);
#endif
}

//==========================================================
void Pic::update_U0_E0() {
  std::string nameFunc = "Pic::update_U0_E0";
  timing_func(nameFunc);

  // Full-PIC only: eBg/uBg and nodePlasma are not allocated for hybrid.
  if (useHybridPIC)
    return;

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    uBg[iLev].setVal(0.0);
    eBg[iLev].setVal(0.0);
    for (MFIter mfi(uBg[iLev]); mfi.isValid(); ++mfi) {
      const Array4<Real>& arrU = uBg[iLev][mfi].array();
      const Array4<const Real>& arrMoments =
          nodePlasma[nSpecies][iLev][mfi].array();

      const Array4<const int>& status = nodeStatus[iLev][mfi].array();

      // Fill in the physical nodes
      ParallelFor(mfi.validbox(), [&](int i, int j, int k) {
        const Real rho = arrMoments(i, j, k, iRho_);
        if (rho > 0) {
          const Real invRho = 1. / rho;
          for (int iu = iUx_; iu <= iUz_; iu++)
            arrU(i, j, k, iu - iUx_) = arrMoments(i, j, k, iu) * invRho;
        }
      });

      // Fill in ghost nodes
      ParallelFor(mfi.fabbox(), [&](int i, int j, int k) {
        IntVect ijk = { AMREX_D_DECL(i, j, k) };
        if (bit::is_domain_boundary(status(ijk))) {
          const int iFluid = 0;
          for (int iDir = 0; iDir < nDim3; iDir++) {
            arrU(i, j, k, iDir) =
                get_node_fluid_u(mfi, ijk, iDir, iLev, iFluid);
          }
        }
      });
    }

    uBg[iLev].FillBoundary(Geom(iLev).periodicity());

    for (int i = 0; i < nSmoothBackGroundU; ++i)
      smooth_multifab(uBg[iLev], iLev, i % 2 + 1);

    for (MFIter mfi(uBg[iLev]); mfi.isValid(); ++mfi) {
      const Array4<Real>& arrU = uBg[iLev][mfi].array();
      const Array4<Real>& arrE = eBg[iLev][mfi].array();
      const Array4<Real>& arrB = nodeB[iLev][mfi].array();

      const Array4<const int>& status = nodeStatus[iLev][mfi].array();

      // Fill in the physical nodes
      ParallelFor(mfi.validbox(), [&](int i, int j, int k) {
        const Real& bx = arrB(i, j, k, ix_);
        const Real& by = arrB(i, j, k, iy_);
        const Real& bz = arrB(i, j, k, iz_);

        const Real& ux = arrU(i, j, k, ix_);
        const Real& uy = arrU(i, j, k, iy_);
        const Real& uz = arrU(i, j, k, iz_);

        arrE(i, j, k, ix_) = -uy * bz + uz * by;
        arrE(i, j, k, iy_) = -uz * bx + ux * bz;
        arrE(i, j, k, iz_) = -ux * by + uy * bx;
      });

      // Fill in boundary nodes
      ParallelFor(mfi.fabbox(), [&](int i, int j, int k) {
        IntVect ijk = { AMREX_D_DECL(i, j, k) };
        if (bit::is_domain_boundary(status(ijk))) {
          arrE(i, j, k, ix_) = get_node_E(mfi, ijk, ix_, iLev);
          arrE(i, j, k, iy_) = get_node_E(mfi, ijk, iy_, iLev);
          arrE(i, j, k, iz_) = get_node_E(mfi, ijk, iz_, iLev);
        }
      });
    }

    eBg[iLev].FillBoundary(Geom(iLev).periodicity());

    // for (int i = 0; i < nSmoothE; ++i)
    //   smooth_multifab(eBg[iLev], iLev, i % 2 + 1);
  }
}

//==========================================================
void Pic::update_E() {
  if (useExplicitPIC) {
    update_E_expl();
  } else {
    update_E_impl();
  }
}

//==========================================================
void Pic::update_E_expl() {
  std::string nameFunc = "Pic::update_E_expl";

  timing_func(nameFunc);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    MultiFab::Copy(nodeEth[iLev], nodeE[iLev], 0, 0, nodeE[iLev].nComp(),
                   nodeE[iLev].nGrow());
    apply_field_bc(cellStatus[iLev], centerB[iLev], 0, centerB[iLev].nComp(),
                   &Pic::get_center_B, iLev, true);
  }
  const Real dt = tc->get_dt();
  RealVect dt2dx;
  for (int i = 0; i < nDim; ++i) {
    dt2dx[i] = dt * Geom(0).InvCellSize(i);
  }
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    curl_center_to_node(centerB[iLev], nodeE[iLev], dt2dx.begin());
    MultiFab::Saxpy(nodeE[iLev], -fourPI * dt, jHat[iLev], 0, 0,
                    nodeE[iLev].nComp(), nodeE[iLev].nGrow());

    MultiFab::Add(nodeE[iLev], nodeEth[iLev], 0, 0, nodeE[iLev].nComp(),
                  nodeE[iLev].nGrow());

    nodeE[iLev].FillBoundary(Geom(iLev).periodicity());
    apply_field_bc(nodeStatus[iLev], nodeE[iLev], 0, nDim3, &Pic::get_node_E,
                   iLev, false);
  }
}

//==========================================================
void Pic::update_E_impl() {
  std::string nameFunc = "Pic::update_E_impl";

  timing_func(nameFunc);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    eSolver.reset(get_local_node_or_cell_number(nodeE[iLev]));

    update_E_rhs(eSolver.rhs, iLev);

    // Both solver modes compute one correction in eSolver.xLeft.
    if (fsolver.mode == FieldSolverMode::NewtonKrylov) {
      solve_E_newton_krylov(iLev);
    } else {
      solve_E_gmres(iLev);
    }

    nodeEth[iLev].setVal(0.0);
    convert_1d_to_3d(eSolver.xLeft, nodeEth[iLev], iLev);
    nodeEth[iLev].SumBoundary(Geom(iLev).periodicity());
    nodeEth[iLev].FillBoundary(Geom(iLev).periodicity());

    if (doSmoothE) {
      smooth_E(nodeEth[iLev], iLev);
    }

    MultiFab::Add(nodeEth[iLev], nodeE[iLev], 0, 0, nodeEth[iLev].nComp(),
                  nGst);

    MultiFab::LinComb(nodeE[iLev], -(1.0 - fsolver.theta) / fsolver.theta,
                      nodeE[iLev], 0, 1. / fsolver.theta, nodeEth[iLev], 0, 0,
                      nodeE[iLev].nComp(), nGst);

    if (iLev == 0) {

      // NOTE: the wave hard source is applied inside apply_field_bc().
      apply_field_bc(nodeStatus[iLev], nodeE[iLev], 0, nDim3, &Pic::get_node_E,
                     iLev, false);
      apply_field_bc(nodeStatus[iLev], nodeEth[iLev], 0, nDim3,
                     &Pic::get_node_E, iLev, false);
    }

    if (doSmoothE) {
      smooth_E(nodeEth[iLev], iLev);
      smooth_E(nodeE[iLev], iLev);
    }
    div_node_to_center(nodeE[iLev], centerDivE[iLev], Geom(iLev).InvCellSize());
  }
}

//==========================================================
void Pic::solve_E_gmres(int iLev) {
  convert_3d_to_1d(nodeE[iLev], eSolver.xLeft, iLev);

  update_E_matvec(eSolver.xLeft, eSolver.matvec, iLev, false);

  // Original linear solve: A * delta = rhs - A(E_old).
  for (int i = 0; i < eSolver.get_nSolve(); ++i) {
    eSolver.rhs[i] -= eSolver.matvec[i];
    eSolver.xLeft[i] = 0;
  }

  if (doReport)
    Print() << "\n-------" << printPrefix
            << " GMRES E solver ------------------" << std::endl;

  BL_PROFILE_VAR("Pic::E_iterate", eSolver);
  eSolver.solve(iLev, doReport);
  BL_PROFILE_VAR_STOP(eSolver);
}

//==========================================================
void Pic::solve_E_newton_krylov(int iLev) {
  const int nSolve = eSolver.get_nSolve();

  std::vector<double> base(nSolve);
  std::vector<double> baseMatvec(nSolve);
  std::vector<double> work(nSolve);

  convert_3d_to_1d(nodeE[iLev], base.data(), iLev);

  update_E_matvec(base.data(), baseMatvec.data(), iLev, false);

  // One Newton step: J(E_old) * delta = rhs - F(E_old).
  for (int i = 0; i < nSolve; ++i) {
    eSolver.rhs[i] -= baseMatvec[i];
    eSolver.xLeft[i] = 0;
  }

  if (doReport)
    Print() << "\n-------" << printPrefix
            << " Newton-Krylov E solver ------------------" << std::endl;

  BL_PROFILE_VAR("Pic::E_iterate", eSolver);
  const MPI_Comm iComm = ParallelDescriptor::Communicator();
  const double normBase =
      sqrt(dot_product_mpi(base.data(), base.data(), nSolve, iComm));

  // GMRES only sees this linearized Jacobian-vector product.
  auto jacobianFreeMatvec = [this, &base, &baseMatvec, &work, nSolve, normBase,
                             iComm](const double* vecIn, double* vecOut,
                                    const int iLevIn) {
    const double normDirection =
        sqrt(dot_product_mpi(vecIn, vecIn, nSolve, iComm));
    const double epsilon =
        fleks_jfnk::finite_difference_epsilon(normBase, normDirection);

    auto nonlinearMatvec = [this](const double* in, double* out,
                                  const int iLevMatvec) {
      update_E_matvec(in, out, iLevMatvec, false);
    };

    fleks_jfnk::jacobian_free_matvec(nonlinearMatvec, base.data(),
                                     baseMatvec.data(), vecIn, vecOut,
                                     work.data(), nSolve, iLevIn, epsilon);
  };

  eSolver.solve(jacobianFreeMatvec, iLev, doReport);
  BL_PROFILE_VAR_STOP(eSolver);
}
//==========================================================
void Pic::update_E_matvec(const double* vecIn, double* vecOut, int iLev,
                          const bool useZeroBC) {
  std::string nameFunc = "Pic::E_matvec";
  timing_func(nameFunc);

  zero_array(vecOut, eSolver.get_nSolve());

  MultiFab vecMF(nGrids[iLev], DistributionMap(iLev), 3, nGst);
  vecMF.setVal(0.0);

  MultiFab matvecMF(nGrids[iLev], DistributionMap(iLev), 3, 1);
  matvecMF.setVal(0.0);

  MultiFab tempCenter3(cGrids[iLev], DistributionMap(iLev), 3, nGst);

  MultiFab tempNode3(nGrids[iLev], DistributionMap(iLev), 3, nGst);
  tempNode3.setVal(0.0);

  MultiFab tempCenter1(cGrids[iLev], DistributionMap(iLev), 1, nGst);

  convert_1d_to_3d(vecIn, vecMF, iLev);

  // The right side edges should be filled in.
  vecMF.SumBoundary(Geom(iLev).periodicity());

  // M*E needs ghost cell information.
  vecMF.FillBoundary(Geom(iLev).periodicity());

  if (isFake2D) {
    // Make sure there is no variation in the z-direction.
    Periodicity period(IntVect(AMREX_D_DECL(0, 0, 1)));
    vecMF.FillBoundary(period);
  }

  if (useZeroBC) {
    // The boundary nodes would not be filled in by convert_1d_3d. So, there
    // is not need to apply zero boundary conditions again here.
  } else {
    // Even after apply_field_bc(), the outmost layer node E is still
    // unknow. See FluidInterface::calc_current for detailed explaniation.
    if (iLev == 0) {
      apply_field_bc(nodeStatus[iLev], vecMF, 0, nDim3, &Pic::get_node_E, iLev,
                     false);
    } else {
      fill_fine_lev_bny_from_coarse(
          nodeEth[iLev - 1], vecMF, 0, nodeEth[iLev - 1].nComp(),
          ref_ratio[iLev - 1], Geom(iLev - 1), Geom(iLev), node_status(iLev),
          node_bilinear_interp);
    }
  }

  lap_node_to_node(vecMF, matvecMF, DistributionMap(iLev), Geom(iLev));

  Real delt2 = pow(fsolver.theta * tc->get_dt(), 2);
  matvecMF.mult(-delt2);

  if (useUpwindE) {
    // Explicit scheme: add the LF artificial viscosity term to the rhs
    // vis_{i+0.5} = c_max/2*(E_i+1 - E_i)
    // E_i += dt/dx*(vis_{i+0.5} - vis_{i-0.5}) = 0.5*c_max*dt*dx*lap(E_i)
    // For implicit scheme, we add it to the lhs, so the sign changes.

    const Real dx = Geom(iLev).CellSize()[0];
    const Real coe1 = -0.5 * fsolver.theta * tc->get_dt() / dx;

    for (MFIter mfi(vecMF); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();
      const Array4<Real>& arrE = vecMF[mfi].array();
      const Array4<Real>& arrE0 = nodeE[iLev][mfi].array();
      const Array4<Real>& limitE = fsolver.useLaggedLimiter ? arrE0 : arrE;
      const Array4<Real>& res = matvecMF[mfi].array();
      const Array4<Real>& arrU = uBg[iLev][mfi].array();

      ParallelFor(box, vecMF.nComp(), [&](int i, int j, int k, int iVar) {
        for (int iDir = 0; iDir < nDim; iDir++) {
          Real dii[nDim3] = { 0, 0, 0 };
          dii[iDir] = 1;

          Real cR = limiter_theta(
              limiterThetaE,
              limitE(i - dii[ix_], j - dii[iy_], k - dii[iz_], iVar),
              limitE(i, j, k, iVar),
              limitE(i + dii[ix_], j + dii[iy_], k + dii[iz_], iVar));

          Real cL = limiter_theta(
              limiterThetaE,
              limitE(i - 2 * dii[ix_], j - 2 * dii[iy_], k - 2 * dii[iz_],
                     iVar),
              limitE(i - dii[ix_], j - dii[iy_], k - dii[iz_], iVar),
              limitE(i, j, k, iVar));

          Real ur = cMaxE, ul = cMaxE;

          if (cMaxE <= 0) {
            ul = fabs(0.5 *
                      (arrU(i - dii[ix_], j - dii[iy_], k - dii[iz_], iDir) +
                       arrU(i, j, k, iDir)));
            ur = fabs(0.5 *
                      (arrU(i, j, k, iDir) +
                       arrU(i + dii[ix_], j + dii[iy_], k + dii[iz_], iDir)));
          }

          Real dE = cR * ur *
                        (arrE(i + dii[ix_], j + dii[iy_], k + dii[iz_], iVar) -
                         arrE(i, j, k, iVar)) -
                    cL * ul *
                        (arrE(i, j, k, iVar) -
                         arrE(i - dii[ix_], j - dii[iy_], k - dii[iz_], iVar));

          res(i, j, k, iVar) += coe1 * dE;
        }
      });
    }
  }

  { // grad(divE)
    div_node_to_center(vecMF, centerDivE[iLev], Geom(iLev).InvCellSize());

    if (fsolver.coefDiff > 0) {
      // Calculate cell center E for center-to-center divE.
      // The outmost boundary layer of tempCenter3 is not accurate.
      average_node_to_cellcenter(tempCenter3, 0, vecMF, 0, 3,
                                 tempCenter3.nGrow());

      //----The following comments are left here for reference------
      // Q: Why apply float BC for all boundary ghost nodes, instead of just
      // the outmost layer? A: For the example described in
      // FluidInterface::calc_current, cell (c+4, c-1) of tempCenter3-block1
      // is not accurate, so the values at (c+4, c-2) will be wrong if we only
      // apply float BC for the outmost layer.
      // apply_float_boundary(cellStatus, tempCenter3, Geom(0), 0,
      //                           tempCenter3.nComp());
      //------------------------------------------------------------

      div_center_to_center(tempCenter3, tempCenter1, Geom(iLev).InvCellSize());

      tempCenter1.FillBoundary(0, 1, IntVect(1), Geom(iLev).periodicity());

      // 1) The outmost boundary layer of tempCenter3 is not accurate.
      // 2) The 2 outmost boundary layers (all ghosts if there are 2 ghost
      // cells) of tempCenter1 are not accurate
      apply_BC(cellStatus[iLev], tempCenter1, 0, tempCenter1.nComp(),
               &Pic::get_zero, iLev);

      MultiFab::LinComb(centerDivE[iLev], 1 - fsolver.coefDiff,
                        centerDivE[iLev], 0, fsolver.coefDiff, tempCenter1, 0,
                        0, 1, 1);
    }

    grad_center_to_node(centerDivE[iLev], tempNode3, Geom(iLev).InvCellSize());

    tempNode3.mult(delt2);
    MultiFab::Add(matvecMF, tempNode3, 0, 0, matvecMF.nComp(),
                  matvecMF.nGrow());
  }

  tempNode3.setVal(0);
  update_E_M_dot_E(vecMF, tempNode3, iLev);

  MultiFab::Add(matvecMF, tempNode3, 0, 0, matvecMF.nComp(), 0);

  MultiFab::Add(matvecMF, vecMF, 0, 0, matvecMF.nComp(), 0);

  convert_3d_to_1d(matvecMF, vecOut, iLev);
}

//==========================================================
void Pic::update_E_M_dot_E(const MultiFab& inMF, MultiFab& outMF, int iLev) {
  std::string nameFunc = "Pic::update_E_M_dot_E";
  timing_func(nameFunc);

  outMF.setVal(0.0);
  Real c0 = fourPI * fsolver.theta * tc->get_dt();
  for (MFIter mfi(outMF); mfi.isValid(); ++mfi) {
    const Box& box = mfi.validbox();

    const Array4<Real const>& inArr = inMF[mfi].array();
    const Array4<Real>& outArr = outMF[mfi].array();
    const Array4<RealMM>& mmArr = nodeMM[iLev][mfi].array();

    ParallelFor(box, [&](int i, int j, int k) {
      IntVect ijk = { AMREX_D_DECL(i, j, k) };

      auto& data0 = mmArr(ijk);

      Box subBox(ijk - 1, ijk + 1);

      ParallelFor(subBox, [&](int i2, int j2, int k2) {
        const int gp = (k2 - k + 1) * 9 + (j2 - j + 1) * 3 + i2 - i + 1;
        const int idx0 = gp * 9;

        Real* const M_I = &(data0[idx0]);

        const double& vctX = inArr(i2, j2, k2, ix_); // vectX[i2][j2][k2];
        const double& vctY = inArr(i2, j2, k2, iy_);
        const double& vctZ = inArr(i2, j2, k2, iz_);
        outArr(i, j, k, ix_) +=
            (vctX * M_I[0] + vctY * M_I[1] + vctZ * M_I[2]) * c0;
        outArr(i, j, k, iy_) +=
            (vctX * M_I[3] + vctY * M_I[4] + vctZ * M_I[5]) * c0;
        outArr(i, j, k, iz_) +=
            (vctX * M_I[6] + vctY * M_I[7] + vctZ * M_I[8]) * c0;
      });
    });
  }

  // if (doSmoothJ) {
  //   for (int icount = 0; icount < nSmoothJ; icount++) {
  //     smooth_multifab(outMF, iLev, icount % 2 + 1);
  //   }
  // }
}

//==========================================================
void Pic::update_E_rhs(double* rhs, int iLev) {
  std::string nameFunc = "Pic::update_E_rhs";
  timing_func(nameFunc);

  MultiFab tempNode(nGrids[iLev], DistributionMap(iLev), 3, nGst);
  tempNode.setVal(0.0);
  MultiFab temp2Node(nGrids[iLev], DistributionMap(iLev), 3, nGst);
  temp2Node.setVal(0.0);

  if (iLev == 0) {
    apply_field_bc(cellStatus[iLev], centerB[iLev], 0, centerB[iLev].nComp(),
                   &Pic::get_center_B, iLev, true);
    apply_field_bc(nodeStatus[iLev], nodeB[iLev], 0, nodeB[iLev].nComp(),
                   &Pic::get_node_B, iLev, true);
  } else {
    fill_fine_lev_bny_from_coarse(
        centerB[iLev - 1], centerB[iLev], 0, centerB[iLev - 1].nComp(),
        ref_ratio[iLev - 1], Geom(iLev - 1), Geom(iLev), cell_status(iLev),
        *get_cell_interp());

    fill_fine_lev_bny_from_coarse(nodeB[iLev - 1], nodeB[iLev], 0,
                                  nodeB[iLev - 1].nComp(), ref_ratio[iLev - 1],
                                  Geom(iLev - 1), Geom(iLev), node_status(iLev),
                                  node_bilinear_interp);
  }
  const Real* invDx = Geom(iLev).InvCellSize();

  curl_center_to_node(centerB[iLev], tempNode, invDx);

  MultiFab::Saxpy(temp2Node, -fourPI, jHat[iLev], 0, 0, temp2Node.nComp(),
                  temp2Node.nGrow());

  MultiFab::Add(temp2Node, tempNode, 0, 0, tempNode.nComp(), temp2Node.nGrow());

  temp2Node.mult(fsolver.theta * tc->get_dt());
  MultiFab::Add(temp2Node, nodeE[iLev], 0, 0, nodeE[iLev].nComp(),
                temp2Node.nGrow());

  if (solveFieldInCoMov) {
    tempNode.setVal(0.0);
    update_E_M_dot_E(eBg[iLev], tempNode, iLev);
    MultiFab::Add(temp2Node, tempNode, 0, 0, tempNode.nComp(),
                  tempNode.nGrow());
  }

  convert_3d_to_1d(temp2Node, rhs, iLev);
}

//==========================================================
void Pic::update_B() {
  std::string nameFunc = "Pic::update_B";
  timing_func(nameFunc);

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    MultiFab dB(cGrids[iLev], DistributionMap(iLev), 3, nGst);
    curl_node_to_center(nodeEth[iLev], dB, Geom(iLev).InvCellSize());

    MultiFab::Saxpy(centerB[iLev], -tc->get_dt(), dB, 0, 0,
                    centerB[iLev].nComp(), centerB[iLev].nGrow());

    centerB[iLev].FillBoundary(Geom(iLev).periodicity());
  }
  if (projectDownEmFields && finest_level > 0) {
    for (int iLev = finest_level; iLev > 0; iLev--) {
      average_down(centerB[iLev], centerB[iLev - 1], 0, 3, ref_ratio[0]);
    }
  }
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    centerB[iLev].FillBoundary(Geom(iLev).periodicity());
    if (iLev == 0) {
      apply_field_bc(cellStatus[iLev], centerB[iLev], 0, centerB[iLev].nComp(),
                     &Pic::get_center_B, iLev, true);

    } else {
      fill_fine_lev_bny_from_coarse(
          centerB[iLev - 1], centerB[iLev], 0, centerB[iLev - 1].nComp(),
          ref_ratio[iLev - 1], Geom(iLev - 1), Geom(iLev), cell_status(iLev),
          *get_cell_interp());
    }
    MultiFab::Copy(dBdt[iLev], nodeB[iLev], 0, 0, dBdt[iLev].nComp(),
                   dBdt[iLev].nGrow());

    if (useHyperbolicCleaning) {
      div_node_to_center(nodeB[iLev], divB[iLev], Geom(iLev).InvCellSize());
    }

    if (useUpwindB || useHyperbolicCleaning) {
      correct_B(iLev);
    }

    average_center_to_node(centerB[iLev], nodeB[iLev]);
    nodeB[iLev].FillBoundary(Geom(iLev).periodicity());

    const Real invDt = 1. / tc->get_dt();
    // dBdt = (B^{n+1} - B^n)/dt;
    MultiFab::LinComb(dBdt[iLev], -invDt, dBdt[iLev], 0, invDt, nodeB[iLev], 0,
                      0, dBdt[iLev].nComp(), dBdt[iLev].nGrow());

    if (iLev == 0) {
      apply_field_bc(nodeStatus[iLev], nodeB[iLev], 0, nodeB[iLev].nComp(),
                     &Pic::get_node_B, iLev, true);
    } else {
      fill_fine_lev_bny_from_coarse(
          nodeB[iLev - 1], nodeB[iLev], 0, nodeB[iLev - 1].nComp(),
          ref_ratio[iLev - 1], Geom(iLev - 1), Geom(iLev), node_status(iLev),
          node_bilinear_interp);
    }
  }
}

//==========================================================
// Generalized Ohm's law
//   E = -U_i x B + eta J + (J x B)/rho_q - grad(Pe)/rho_q
// on cell-centred fields at an arbitrary (off-member) B state. `centerBin` is
// the trial B used for J = curl(B)/(4*pi); `centerBtimeAvg` is the
// time-averaged B used for the Hall/convection factor. Ion moments are
// time-interpolated between centerPlasmaPrev (J^{n-1/2}) and centerPlasmaSum
// (J^{n+1/2}) at hstep. Particle weights are initialized with dt = 1, so iRho_
// is the true charge density rho_q (the Hall / pressure terms divide by it
// directly).
//==========================================================
void Pic::assemble_ohm_E(const MultiFab& centerBin,
                         const MultiFab& centerBtimeAvg, MultiFab& Eout,
                         int iLev, amrex::Real hstep) {
  BL_PROFILE("Pic::assemble_ohm_E");

  const auto dx = Geom(iLev).CellSizeArray();
  const Real dxInv = 1.0 / (2.0 * dx[0]);
  const Real dyInv = 1.0 / (2.0 * dx[1]);
  const Real dzInv = (nDim > 2) ? 1.0 / (2.0 * dx[2]) : 0.0;

  // Cell-centred current J = curl(B)/(4*pi) from the trial B (2*dx central
  // difference, zero at the Nyquist wavenumber).
  const bool needJ =
      (etaResistivity > 0 || useHallTerm || etaHyperLev[iLev] > 0);
  if (needJ) {
    curl_center_to_center(centerBin, centerJ[iLev], Geom(iLev).InvCellSize());
  }

  for (MFIter mfi(Eout); mfi.isValid(); ++mfi) {
    const Box& box = mfi.validbox();
    const Array4<Real>& arrE = Eout[mfi].array();
    const Array4<Real const>& arrB = centerBtimeAvg[mfi].array();
    const Array4<Real const>& moments =
        centerPlasmaSum[nSpecies][iLev][mfi].array();
    const Array4<Real const>& momentsPrev =
        centerPlasmaPrev[nSpecies][iLev][mfi].array();
    const Array4<Real const> arrJ =
        needJ ? centerJ[iLev][mfi].array() : Array4<Real const>();

    // Moment time-interpolation weights: X(hstep) =
    // (0.5-hstep)*X^{n-1/2} + (0.5+hstep)*X^{n+1/2}.
    const Real wPrev = 0.5 - hstep;
    const Real wCur = 0.5 + hstep;

    ParallelFor(box, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      const Real rhoPrev = momentsPrev(i, j, k, iRho_);
      const Real rhoCur = moments(i, j, k, iRho_);
      const Real rho = wPrev * rhoPrev + wCur * rhoCur;
      const Real mx =
          wPrev * momentsPrev(i, j, k, iUx_) + wCur * moments(i, j, k, iUx_);
      const Real my =
          wPrev * momentsPrev(i, j, k, iUy_) + wCur * moments(i, j, k, iUy_);
      const Real mz =
          wPrev * momentsPrev(i, j, k, iUz_) + wCur * moments(i, j, k, iUz_);
      Real ui = 0, vi = 0, wi = 0;

      if (rho > 0) {
        ui = mx / rho;
        vi = my / rho;
        wi = mz / rho;
      }

      // Interpolated density at an arbitrary cell (same hstep weights), used
      // for the electron-pressure gradient closure.
      auto rho_at = [=](int ii, int jj, int kk) AMREX_GPU_DEVICE {
        return wPrev * momentsPrev(ii, jj, kk, iRho_) +
               wCur * moments(ii, jj, kk, iRho_);
      };

      Real bx = arrB(i, j, k, ix_);
      Real by = arrB(i, j, k, iy_);
      Real bz = arrB(i, j, k, iz_);

      // Convection term: E = -U_i x B
      Real ex = -(vi * bz - wi * by);
      Real ey = -(wi * bx - ui * bz);
      Real ez = -(ui * by - vi * bx);

      // J = curl(B)/(4*pi) (CGS)
      Real jx = 0.0, jy = 0.0, jz = 0.0;
      if (needJ) {
        jx = arrJ(i, j, k, ix_) / fourPI;
        jy = arrJ(i, j, k, iy_) / fourPI;
        jz = arrJ(i, j, k, iz_) / fourPI;
      }

      // eta * J
      if (etaResistivity > 0) {
        ex += etaResistivity * jx;
        ey += etaResistivity * jy;
        ez += etaResistivity * jz;
      }

      // Electron-pressure-gradient and Hall terms. The floor caps 1/rho; the
      // pressure closure itself uses the true rho. Cells with rho == 0 are
      // left inert.
      if (rho > 0) {
        const Real invRhoEff = 1.0 / amrex::max(rho, rhoMinOhm);

        // Electron pressure gradient
        Real dPe_dx = 0.0, dPe_dy = 0.0, dPe_dz = 0.0;
        if (electronTemperature > 0) {
          if (electronGamma == 1.0) {
            // Isothermal: grad(Pe) = Te * grad(rho)
            dPe_dx = electronTemperature *
                     (rho_at(i + 1, j, k) - rho_at(i - 1, j, k)) * dxInv;
            dPe_dy = electronTemperature *
                     (rho_at(i, j + 1, k) - rho_at(i, j - 1, k)) * dyInv;
            dPe_dz = (nDim > 2)
                         ? electronTemperature *
                               (rho_at(i, j, k + 1) - rho_at(i, j, k - 1)) *
                               dzInv
                         : 0.0;
          } else {
            // Adiabatic: Pe = P0 * (rho / rho0)^gamma
            Real p0 = electronDensity0 * electronTemperature;
            Real invRho0 = 1.0 / electronDensity0;

            auto calc_Pe = [=](Real r) {
              return (r > 0) ? p0 * std::pow(r * invRho0, electronGamma) : 0.0;
            };

            dPe_dx =
                (calc_Pe(rho_at(i + 1, j, k)) - calc_Pe(rho_at(i - 1, j, k))) *
                dxInv;
            dPe_dy =
                (calc_Pe(rho_at(i, j + 1, k)) - calc_Pe(rho_at(i, j - 1, k))) *
                dyInv;
            dPe_dz = (nDim > 2) ? (calc_Pe(rho_at(i, j, k + 1)) -
                                   calc_Pe(rho_at(i, j, k - 1))) *
                                      dzInv
                                : 0.0;
          }

          ex -= dPe_dx * invRhoEff;
          ey -= dPe_dy * invRhoEff;
          ez -= dPe_dz * invRhoEff;
        }

        // Hall term: (J x B) / rho_q
        if (useHallTerm) {
          Real hall_x = (jy * bz - jz * by) * invRhoEff;
          Real hall_y = (jz * bx - jx * bz) * invRhoEff;
          Real hall_z = (jx * by - jy * bx) * invRhoEff;

          ex += hall_x;
          ey += hall_y;
          ez += hall_z;
        }
      }

      arrE(i, j, k, ix_) = ex;
      arrE(i, j, k, iy_) = ey;
      arrE(i, j, k, iz_) = ez;
    });
  }

  // Hyper-resistivity: E -= (eta_h / 4*pi) * curl(nabla^2 B) (see
  // tests/hyper_resistivity). Applied before the final BC pass so ghost cells
  // include the hyper-resistive term.
  if (etaHyperLev[iLev] > 0) {
    lap_center_to_center(centerBin, centerLapB[iLev], Geom(iLev).InvCellSize());
    centerLapB[iLev].FillBoundary(Geom(iLev).periodicity());
    apply_field_bc(cellStatus[iLev], centerLapB[iLev], 0,
                   centerLapB[iLev].nComp(), &Pic::get_center_B, iLev, true);

    curl_center_to_center(centerLapB[iLev], centerHyperE[iLev],
                          Geom(iLev).InvCellSize());
    centerHyperE[iLev].FillBoundary(Geom(iLev).periodicity());
    apply_field_bc(cellStatus[iLev], centerHyperE[iLev], 0,
                   centerHyperE[iLev].nComp(), &Pic::get_center_E, iLev, false);

    const Real f = etaHyperLev[iLev] / fourPI;
    MultiFab::Saxpy(Eout, -f, centerHyperE[iLev], 0, 0, 3, 0);
  }

  Eout.FillBoundary(Geom(iLev).periodicity());
  apply_field_bc(cellStatus[iLev], Eout, 0, nDim3, &Pic::get_center_E, iLev,
                 false);
}

//==========================================================
void Pic::smooth_moments() {
  std::string nameFunc = "Pic::smooth_moments";
  timing_func(nameFunc);

  if (!doSmoothMoments || nSmoothMoments <= 0)
    return;

  // Smooth the total ion moments the Ohm's law reads. Hybrid-only.
  for (int iLev = 0; iLev < n_lev(); ++iLev) {
    MultiFab& moments = useHybridPIC ? centerPlasmaSum[nSpecies][iLev]
                                     : nodePlasma[nSpecies][iLev];
    moments.FillBoundary(Geom(iLev).periodicity());
    for (int icount = 0; icount < nSmoothMoments; ++icount) {
      smooth_multifab(moments, iLev, 1, coefSmoothMoments);
    }
    moments.FillBoundary(Geom(iLev).periodicity());
  }
}

//==========================================================
// Copy the current summed moment deposit into centerPlasmaPrev (J^{n-1/2})
// before a fresh deposit (J^{n+1/2}), so assemble_ohm_E can time-interpolate
// the two at the magnetic sub-step fraction hstep.
void Pic::save_current_moments_to_prev() {
  // Hybrid-only. Copy the rho + 3 momentum components of the current summed
  // deposit into centerPlasmaPrev so the Ohm's law can time-interpolate the
  // previous and current moments. Re-fill ghosts so grad(Pe) at box boundaries
  // reads valid values.
  std::string nameFunc = "Pic::save_current_moments_to_prev";
  timing_func(nameFunc);

  for (int iLev = 0; iLev < n_lev(); ++iLev) {
    MultiFab::Copy(centerPlasmaPrev[nSpecies][iLev],
                   centerPlasmaSum[nSpecies][iLev], 0, 0, nHybridMomentsComps,
                   centerPlasmaSum[nSpecies][iLev].nGrow());
    centerPlasmaPrev[nSpecies][iLev].FillBoundary(Geom(iLev).periodicity());
  }
}

//==========================================================
// Seed centerPlasmaPrev on the first hybrid step, where there is no previous
// deposit: initialise it from the current deposit so the time interpolation
// degrades to a plain average for that single step. Hybrid-only.
void Pic::seed_first_hybrid_step() {
  std::string nameFunc = "Pic::seed_first_hybrid_step";
  timing_func(nameFunc);

  for (int iLev = 0; iLev < n_lev(); ++iLev) {
    MultiFab::Copy(centerPlasmaPrev[nSpecies][iLev],
                   centerPlasmaSum[nSpecies][iLev], 0, 0, nHybridMomentsComps,
                   centerPlasmaSum[nSpecies][iLev].nGrow());
    centerPlasmaPrev[nSpecies][iLev].FillBoundary(Geom(iLev).periodicity());
  }
}

//==========================================================
void Pic::project_centerB_to_nodeB(int iLev) {
  centerB[iLev].FillBoundary(Geom(iLev).periodicity());
  if (iLev == 0) {
    apply_field_bc(cellStatus[iLev], centerB[iLev], 0, centerB[iLev].nComp(),
                   &Pic::get_center_B, iLev, true);
  } else {
    fill_fine_lev_bny_from_coarse(
        centerB[iLev - 1], centerB[iLev], 0, centerB[iLev - 1].nComp(),
        ref_ratio[iLev - 1], Geom(iLev - 1), Geom(iLev), cell_status(iLev),
        *get_cell_interp());
  }
  average_center_to_node(centerB[iLev], nodeB[iLev]);
  nodeB[iLev].FillBoundary(Geom(iLev).periodicity());
  if (iLev == 0) {
    apply_field_bc(nodeStatus[iLev], nodeB[iLev], 0, nodeB[iLev].nComp(),
                   &Pic::get_node_B, iLev, true);
  } else {
    fill_fine_lev_bny_from_coarse(nodeB[iLev - 1], nodeB[iLev], 0,
                                  nodeB[iLev - 1].nComp(), ref_ratio[iLev - 1],
                                  Geom(iLev - 1), Geom(iLev), node_status(iLev),
                                  node_bilinear_interp);
  }
}

// BCs for the cell-centred B (the cell-centred part of
// project_centerB_to_nodeB), called at the end of each sub-step.
void Pic::apply_centerB_BC(int iLev) { apply_centerB_BC(iLev, centerB[iLev]); }

void Pic::apply_centerB_BC(int iLev, amrex::MultiFab& mfB) {
  mfB.FillBoundary(Geom(iLev).periodicity());
  if (iLev == 0) {
    apply_field_bc(cellStatus[iLev], mfB, 0, mfB.nComp(), &Pic::get_center_B,
                   iLev, true);
  } else {
    fill_fine_lev_bny_from_coarse(
        centerB[iLev - 1], mfB, 0, mfB.nComp(), ref_ratio[iLev - 1],
        Geom(iLev - 1), Geom(iLev), cell_status(iLev), *get_cell_interp());
  }
}

//==========================================================
void Pic::project_centerB_to_nodeB_scratch(amrex::MultiFab& centerIn,
                                           amrex::MultiFab& nodeOut, int iLev) {
  // Same projection as project_centerB_to_nodeB on caller-owned scratch fields.
  centerIn.FillBoundary(Geom(iLev).periodicity());
  if (iLev == 0) {
    apply_field_bc(cellStatus[iLev], centerIn, 0, centerIn.nComp(),
                   &Pic::get_center_B, iLev, true);
  } else {
    fill_fine_lev_bny_from_coarse(
        centerB[iLev - 1], centerIn, 0, centerIn.nComp(), ref_ratio[iLev - 1],
        Geom(iLev - 1), Geom(iLev), cell_status(iLev), *get_cell_interp());
  }
  average_center_to_node(centerIn, nodeOut);
  nodeOut.FillBoundary(Geom(iLev).periodicity());
  if (iLev == 0) {
    apply_field_bc(nodeStatus[iLev], nodeOut, 0, nodeOut.nComp(),
                   &Pic::get_node_B, iLev, true);
  } else {
    fill_fine_lev_bny_from_coarse(
        nodeB[iLev - 1], nodeOut, 0, nodeOut.nComp(), ref_ratio[iLev - 1],
        Geom(iLev - 1), Geom(iLev), node_status(iLev), node_bilinear_interp);
  }
}

//==========================================================
void Pic::update_B_hybrid() {
  std::string nameFunc = "Pic::update_B_hybrid";
  timing_func(nameFunc);

  const Real dt = tc->get_dt();
  const Real subDt = dt / nBSubcycle;

  // Grid-mode hyper-resistivity: uniform eta_h based on finest dx to keep
  // diffusion stable across levels.
  if (etaHyperMode == "grid" && etaHyperCh > 0) {
    const int iFinest = n_lev() - 1;
    const auto dxFine = Geom(iFinest).CellSizeArray();
    Real dxMinFine = dxFine[0];
    for (int d = 1; d < nDim; ++d)
      dxMinFine = amrex::min(dxMinFine, dxFine[d]);

    const Real etaHyper = fourPI * etaHyperCh * std::pow(dxMinFine, 4) / dt;
    for (int iLev = 0; iLev < n_lev(); ++iLev) {
      etaHyperLev[iLev] = etaHyper;
    }
  }

  // CFL stability check for explicit resistive and hyper-resistive diffusion.
  const Real cflLimit = useRK4 ? 2.785 : 2.513;
  for (int iLev = 0; iLev < n_lev(); ++iLev) {
    if (etaResistivity <= 0 && etaHyperLev[iLev] <= 0)
      continue;

    const auto dx = Geom(iLev).CellSizeArray();
    const Box& domBox = Geom(iLev).Domain();
    Real sMax = 0.0, lMax = 0.0;
    for (int iDim = 0; iDim < nDim; ++iDim) {
      const int nCellDim = domBox.length(iDim);
      if (nCellDim < 2)
        continue;
      const Real invDx2 = 1.0 / (dx[iDim] * dx[iDim]);
      lMax += 4.0 * invDx2;
      if (nCellDim >= 3)
        sMax += invDx2;
    }
    const Real cflEta = (etaResistivity / fourPI) * sMax * subDt;
    const Real cflHyper = (etaHyperLev[iLev] / fourPI) * sMax * lMax * subDt;
    if (cflEta > cflLimit)
      amrex::Print()
          << "  [CFL warning] resistivity: eta*kmax^2*dt_sub/(4pi) = " << cflEta
          << " (> " << cflLimit << ", explicit diffusion may be unstable)\n";
    if (cflHyper > cflLimit)
      amrex::Print()
          << "  [CFL warning] hyper-resistivity: eta_h*kmax^4*dt_sub/(4pi) = "
          << cflHyper << " (> " << cflLimit
          << ", explicit 4th-order diffusion may be unstable)\n";
  }

  const Real invSubcycle = 1.0 / static_cast<Real>(nBSubcycle);

  for (int subStep = 0; subStep < nBSubcycle; ++subStep) {
    // Moment time-interpolation weights hstep for RK stages within the
    // sub-step.
    const Real g = static_cast<Real>(subStep) * invSubcycle;
    const Real hstepStart = g;
    const Real hstepHalf = g + 0.5 * invSubcycle;
    const Real hstepEnd = g + invSubcycle;

    if (useRK4) {
      // Classical RK4 on dB/dt = -curl(E_Ohm(B)) across all levels.
      const Real dtSixth = -subDt / 6.0;
      const Real dtThird = -subDt / 3.0;

      for (int iLev = 0; iLev < n_lev(); ++iLev) {
        // Stage 1: k1 = curl(E(B^n))
        assemble_ohm_E(centerB[iLev], centerB[iLev], centerEstage[iLev], iLev,
                       hstepStart);
        curl_center_to_center(centerEstage[iLev], kStage[iLev][0],
                              Geom(iLev).InvCellSize());

        // Stage 2: B2 = B^n - 0.5 dt k1; evaluate E at (B2 + B^n)/2
        MultiFab::Copy(centerBstage[iLev], centerB[iLev], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerBstage[iLev], -0.5 * subDt, kStage[iLev][0], 0, 0,
                        3, nGst);
        MultiFab::LinComb(centerBstar[iLev], 0.5, centerBstage[iLev], 0, 0.5,
                          centerB[iLev], 0, 0, 3, nGst);
        apply_centerB_BC(iLev, centerBstage[iLev]);
        apply_centerB_BC(iLev, centerBstar[iLev]);
        assemble_ohm_E(centerBstage[iLev], centerBstar[iLev],
                       centerEstage[iLev], iLev, hstepHalf);
        curl_center_to_center(centerEstage[iLev], kStage[iLev][1],
                              Geom(iLev).InvCellSize());

        // Stage 3: B3 = B^n - 0.5 dt k2; evaluate E at (B3 + B^n)/2
        MultiFab::Copy(centerBstage[iLev], centerB[iLev], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerBstage[iLev], -0.5 * subDt, kStage[iLev][1], 0, 0,
                        3, nGst);
        MultiFab::LinComb(centerBstar[iLev], 0.5, centerBstage[iLev], 0, 0.5,
                          centerB[iLev], 0, 0, 3, nGst);
        apply_centerB_BC(iLev, centerBstage[iLev]);
        apply_centerB_BC(iLev, centerBstar[iLev]);
        assemble_ohm_E(centerBstage[iLev], centerBstar[iLev],
                       centerEstage[iLev], iLev, hstepHalf);
        curl_center_to_center(centerEstage[iLev], kStage[iLev][2],
                              Geom(iLev).InvCellSize());

        // Stage 4: B4 = B^n - dt k3; evaluate E at (B4 + B^n)/2
        MultiFab::Copy(centerBstage[iLev], centerB[iLev], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerBstage[iLev], -subDt, kStage[iLev][2], 0, 0, 3,
                        nGst);
        MultiFab::LinComb(centerBstar[iLev], 0.5, centerBstage[iLev], 0, 0.5,
                          centerB[iLev], 0, 0, 3, nGst);
        apply_centerB_BC(iLev, centerBstage[iLev]);
        apply_centerB_BC(iLev, centerBstar[iLev]);
        assemble_ohm_E(centerBstage[iLev], centerBstar[iLev],
                       centerEstage[iLev], iLev, hstepEnd);
        curl_center_to_center(centerEstage[iLev], kStage[iLev][3],
                              Geom(iLev).InvCellSize());

        // Accumulate RK4: B^{n+1} = B^n + (dt/6)*(k1 + 2*k2 + 2*k3 + k4)
        MultiFab::Saxpy(centerB[iLev], dtSixth, kStage[iLev][0], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerB[iLev], dtThird, kStage[iLev][1], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerB[iLev], dtThird, kStage[iLev][2], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerB[iLev], dtSixth, kStage[iLev][3], 0, 0, 3, nGst);
        centerB[iLev].FillBoundary(Geom(iLev).periodicity());

        apply_centerB_BC(iLev);
      }
      continue;
    }

    if (fieldIntegrator == "ssprk3") {
      // Strong-stability-preserving RK3 with time-centred E evaluation.
      for (int iLev = 0; iLev < n_lev(); ++iLev) {
        MultiFab::Copy(centerBstart[iLev], centerB[iLev], 0, 0, 3, nGst);

        // Stage 1: B1 = B_n - subDt * curl(E(B_n))
        assemble_ohm_E(centerB[iLev], centerB[iLev], centerEstage[iLev], iLev,
                       hstepStart);
        curl_center_to_center(centerEstage[iLev], kStage[iLev][0],
                              Geom(iLev).InvCellSize());
        MultiFab::Copy(centerBstage[iLev], centerB[iLev], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerBstage[iLev], -subDt, kStage[iLev][0], 0, 0, 3,
                        nGst);

        // Stage 2: B2 = (3/4)*B_n + (1/4)*(B1 - subDt * curl(E(avgB2)))
        MultiFab::LinComb(centerBstar[iLev], 0.5, centerBstage[iLev], 0, 0.5,
                          centerBstart[iLev], 0, 0, 3, nGst);
        apply_centerB_BC(iLev, centerBstar[iLev]);
        assemble_ohm_E(centerBstar[iLev], centerBstar[iLev], centerEstage[iLev],
                       iLev, hstepEnd);
        curl_center_to_center(centerEstage[iLev], kStage[iLev][1],
                              Geom(iLev).InvCellSize());
        MultiFab::LinComb(centerBstage[iLev], 0.25, centerBstage[iLev], 0, 0.75,
                          centerBstart[iLev], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerBstage[iLev], -0.25 * subDt, kStage[iLev][1], 0,
                        0, 3, nGst);
        apply_centerB_BC(iLev, centerBstage[iLev]);

        // Stage 3: B^{n+1} = (1/3)*B_n + (2/3)*(B2 - subDt * curl(E(avgB3)))
        MultiFab::LinComb(centerBstar[iLev], 0.5, centerBstage[iLev], 0, 0.5,
                          centerBstart[iLev], 0, 0, 3, nGst);
        apply_centerB_BC(iLev, centerBstar[iLev]);
        assemble_ohm_E(centerBstar[iLev], centerBstar[iLev], centerEstage[iLev],
                       iLev, hstepHalf);
        curl_center_to_center(centerEstage[iLev], kStage[iLev][2],
                              Geom(iLev).InvCellSize());
        MultiFab::LinComb(centerB[iLev], 2.0 / 3.0, centerBstage[iLev], 0,
                          1.0 / 3.0, centerBstart[iLev], 0, 0, 3, nGst);
        MultiFab::Saxpy(centerB[iLev], (-2.0 / 3.0) * subDt, kStage[iLev][2], 0,
                        0, 3, nGst);
        centerB[iLev].FillBoundary(Geom(iLev).periodicity());

        apply_centerB_BC(iLev);
      }
      continue;
    }
  }

  if (projectDownEmFields && finest_level > 0) {
    for (int iLev = finest_level; iLev > 0; iLev--) {
      average_down(centerB[iLev], centerB[iLev - 1], 0, 3, ref_ratio[0]);
    }
  }

  auto& cellInterp = *get_cell_interp();
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    centerB[iLev].FillBoundary(Geom(iLev).periodicity());
    if (iLev == 0) {
      apply_field_bc(cellStatus[iLev], centerB[iLev], 0, centerB[iLev].nComp(),
                     &Pic::get_center_B, iLev, true);
    } else {
      fill_fine_lev_bny_from_coarse(centerB[iLev - 1], centerB[iLev], 0,
                                    centerB[iLev - 1].nComp(),
                                    ref_ratio[iLev - 1], Geom(iLev - 1),
                                    Geom(iLev), cell_status(iLev), cellInterp);
    }
  }

  // Running time-averaged B used in Ohm's law and the particle push.
  if (useAvgFieldB) {
    const Real alpha = (nAvgFieldB > 1) ? (1.0 - 1.0 / nAvgFieldB) : 0.0;
    const bool syncNodeBavg = !useHybridPIC;
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      if (!isBavgInit) {
        MultiFab::Copy(centerBavg[iLev], centerB[iLev], 0, 0, 3,
                       centerBavg[iLev].nGrow());
        if (syncNodeBavg) {
          MultiFab::Copy(nodeBavg[iLev], nodeB[iLev], 0, 0, 3,
                         nodeBavg[iLev].nGrow());
        }
        isBavgInit = true;
      } else {
        centerBavg[iLev].mult(alpha);
        MultiFab::Saxpy(centerBavg[iLev], 1.0 - alpha, centerB[iLev], 0, 0, 3,
                        centerBavg[iLev].nGrow());
        if (syncNodeBavg) {
          nodeBavg[iLev].mult(alpha);
          MultiFab::Saxpy(nodeBavg[iLev], 1.0 - alpha, nodeB[iLev], 0, 0, 3,
                          nodeBavg[iLev].nGrow());
        }
      }
      centerBavg[iLev].FillBoundary(Geom(iLev).periodicity());
      if (syncNodeBavg) {
        nodeBavg[iLev].FillBoundary(Geom(iLev).periodicity());
      }
      if (iLev == 0) {
        apply_field_bc(cellStatus[iLev], centerBavg[iLev], 0, 3,
                       &Pic::get_center_B, iLev, true);
      }
    }
  }

  // Fill coarse-fine interface ghost cells for centerBavg.
  if (useAvgFieldB && isBavgInit && finest_level > 0) {
    for (int iLev = 1; iLev < n_lev(); iLev++) {
      fill_fine_lev_bny_from_coarse(centerBavg[iLev - 1], centerBavg[iLev], 0,
                                    3, ref_ratio[iLev - 1], Geom(iLev - 1),
                                    Geom(iLev), cell_status(iLev), cellInterp);
    }
  }

  // Evaluate integer-step E^{n+1} into centerEhybrid for the next particle
  // push.
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    bool hasParticles = false;
    for (int i : kineticSpecies_) {
      if (parts[i]->NumberOfParticlesAtLevel(iLev, true, true) > 0) {
        hasParticles = true;
        break;
      }
    }
    if (!hasParticles) {
      continue;
    }
    const auto& cBin =
        (useAvgFieldB && isBavgInit) ? centerBavg[iLev] : centerB[iLev];
    assemble_ohm_E(cBin, cBin, centerEhybrid[iLev], iLev, 1.0);
  }

  // BCs for cell-centred E (read by the hybrid Boris push).
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    centerEhybrid[iLev].FillBoundary(Geom(iLev).periodicity());
    apply_field_bc(cellStatus[iLev], centerEhybrid[iLev], 0,
                   centerEhybrid[iLev].nComp(), &Pic::get_center_E, iLev,
                   false);
  }

  // Fill coarse-fine interface ghost cells for centerEhybrid.
  if (finest_level > 0) {
    for (int iLev = 1; iLev < n_lev(); iLev++) {
      fill_fine_lev_bny_from_coarse(centerEhybrid[iLev - 1],
                                    centerEhybrid[iLev], 0, 3,
                                    ref_ratio[iLev - 1], Geom(iLev - 1),
                                    Geom(iLev), cell_status(iLev), cellInterp);
    }
  }

  // Suppress grid-scale (odd-even) E component.
  if (doSmoothE) {
    for (int iLev = 0; iLev < n_lev(); iLev++) {
      centerEhybrid[iLev].FillBoundary(Geom(iLev).periodicity());
      smooth_E(centerEhybrid[iLev], iLev);
    }
  }
}

//==========================================================
void Pic::solve_hyp_phi(int iLev) {
  std::string nameFunc = "Pic::solve_hyp_phi";
  timing_func(nameFunc);

  // divB error propagation speed
  Real ch = 0.8 * Geom(iLev).CellSize()[ix_] / tc->get_dt();

  Real coef = -tc->get_dt() * pow(ch, 2);
  for (MFIter mfi(centerB[iLev]); mfi.isValid(); ++mfi) {
    Box box = mfi.validbox();

    const Array4<Real>& divBArr = divB[iLev][mfi].array();
    const Array4<Real>& phiArr = hypPhi[iLev][mfi].array();

    ParallelFor(box, [&](int i, int j, int k) {
      IntVect ijk = { AMREX_D_DECL(i, j, k) };
      phiArr(ijk) += coef * divBArr(ijk);
      phiArr(ijk) *= (1 - hypDecay);
    });
  }

  hypPhi[iLev].FillBoundary(Geom(iLev).periodicity());

  apply_BC(cellStatus[iLev], hypPhi[iLev], 0, hypPhi[iLev].nComp(), nullptr,
           iLev);
}
//==========================================================
void Pic::correct_B(int iLev) {
  std::string nameFunc = "Pic::correct_B";
  timing_func(nameFunc);

  if (!useUpwindB && !useHyperbolicCleaning) {
    return;
  }

  MultiFab centerDB(cGrids[iLev], DistributionMap(iLev), nDim3, nGst);
  centerDB.setVal(0.0);

  if (useUpwindB) {
    Real coef[nDim3];
    for (int i = 0; i < nDim3; ++i) {
      coef[i] = 0.5 * tc->get_dt() * Geom(iLev).InvCellSize()[i];
    }

    for (MFIter mfi(centerB[iLev]); mfi.isValid(); ++mfi) {
      Box box = mfi.validbox();

      const Array4<Real>& cB = centerB[iLev][mfi].array();
      const Array4<Real const>& nU = uBg[iLev][mfi].array();
      const Array4<Real>& dB = centerDB[mfi].array();
      const auto& status = cellStatus[iLev][mfi].array();

      // Get the face along the direction iDir for the cell (i,j,k) for the iVar
      // component
      auto get_face = [&](int iDir, int i, int j, int k, int iVar,
                          Array4<Real const> const& arr, Real& l, Real& r) {
        // Generic fixed-upwind-velocity override (e.g. the old TopHat
        // "bypass_limiter" used a constant speed of 1.0). Zero keeps the
        // normal plasma-background-velocity reconstruction below.
        if (fixedUpwindVel > 0) {
          l = fixedUpwindVel;
          r = fixedUpwindVel;
          return;
        }

        int kp1 = nDim > 2 ? k + 1 : k;
        if (iDir == ix_) {
          l = 0.25 * (arr(i, j, k, iVar) + arr(i, j + 1, k, iVar) +
                      arr(i, j, kp1, iVar) + arr(i, j + 1, kp1, iVar));
          r = 0.25 * (arr(i + 1, j, k, iVar) + arr(i + 1, j + 1, k, iVar) +
                      arr(i + 1, j, kp1, iVar) + arr(i + 1, j + 1, kp1, iVar));
        } else if (iDir == iy_) {
          l = 0.25 * (arr(i, j, k, iVar) + arr(i + 1, j, k, iVar) +
                      arr(i, j, kp1, iVar) + arr(i + 1, j, kp1, iVar));

          r = 0.25 * (arr(i, j + 1, k, iVar) + arr(i + 1, j + 1, k, iVar) +
                      arr(i, j + 1, kp1, iVar) + arr(i + 1, j + 1, kp1, iVar));

        } else if (iDir == iz_) {
          l = 0.25 * (arr(i, j, k, iVar) + arr(i, j + 1, k, iVar) +
                      arr(i + 1, j, k, iVar) + arr(i + 1, j + 1, k, iVar));

          r = 0.25 * (arr(i, j, kp1, iVar) + arr(i, j + 1, kp1, iVar) +
                      arr(i + 1, j, kp1, iVar) + arr(i + 1, j + 1, kp1, iVar));
        }
      };

      ParallelFor(box, [&](int i, int j, int k) {
        bool doDiffusion;
        Real lu[nDim3] = { 0, 0, 0 }, ru[nDim3] = { 0, 0, 0 };
        Real ul, ur;

        IntVect ijk{ AMREX_D_DECL(i, j, k) };

        // Flux along  x
        for (int iDir = 0; iDir < nDim; iDir++) {
          get_face(iDir, i, j, k, iDir, nU, lu[iDir], ru[iDir]);
        }

        ul = lu[ix_];
        ur = ru[ix_];
        doDiffusion = true;
        if ((ul > 0 && bit::is_domain_boundary(status(i - 1, j, k))) ||
            (ur < 0 && bit::is_domain_boundary(status(i + 1, j, k)))) {
          doDiffusion = false;
        }

        if (doDiffusion) {
          ul = fabs(ul);
          ur = fabs(ur);
          for (int iVar = 0; iVar < nDim3; iVar++) {
            Real cR = limiter_theta(limiterThetaB, cB(i - 1, j, k, iVar),
                                    cB(i, j, k, iVar), cB(i + 1, j, k, iVar));
            Real cL = limiter_theta(limiterThetaB, cB(i - 2, j, k, iVar),
                                    cB(i - 1, j, k, iVar), cB(i, j, k, iVar));
            ul = min(ul, 0.5 / coef[ix_]);
            ur = min(ur, 0.5 / coef[ix_]);
            dB(i, j, k, iVar) +=
                (cR * ur * (cB(i + 1, j, k, iVar) - cB(i, j, k, iVar)) -
                 cL * ul * (cB(i, j, k, iVar) - cB(i - 1, j, k, iVar))) *
                coef[ix_];
          }
        }

        // Flux along y
        ul = lu[iy_];
        ur = ru[iy_];
        doDiffusion = true;
        if ((ul > 0 && bit::is_domain_boundary(status(i, j - 1, k))) ||
            (ur < 0 && bit::is_domain_boundary(status(i, j + 1, k)))) {
          doDiffusion = false;
        }

        if (doDiffusion) {
          ul = fabs(ul);
          ur = fabs(ur);

          for (int iVar = 0; iVar < nDim3; iVar++) {
            Real cR = limiter_theta(limiterThetaB, cB(i, j - 1, k, iVar),
                                    cB(i, j, k, iVar), cB(i, j + 1, k, iVar));
            Real cL = limiter_theta(limiterThetaB, cB(i, j - 2, k, iVar),
                                    cB(i, j - 1, k, iVar), cB(i, j, k, iVar));
            ul = min(ul, 0.5 / coef[iy_]);
            ur = min(ur, 0.5 / coef[iy_]);

            dB(i, j, k, iVar) +=
                (cR * ur * (cB(i, j + 1, k, iVar) - cB(i, j, k, iVar)) -
                 cL * ul * (cB(i, j, k, iVar) - cB(i, j - 1, k, iVar))) *
                coef[iy_];
          }
        }

        if (nDim > 2 && !isFake2D) {

          // Flux along z
          ul = lu[iz_];
          ur = ru[iz_];

          doDiffusion = true;
          if ((ul > 0 && bit::is_domain_boundary(status(i, j, k - 1))) ||
              (ur < 0 && bit::is_domain_boundary(status(i, j, k + 1)))) {
            doDiffusion = false;
          }

          if (doDiffusion) {
            ul = fabs(ul);
            ur = fabs(ur);

            for (int iVar = 0; iVar < nDim3; iVar++) {
              Real cR = limiter_theta(limiterThetaB, cB(i, j, k - 1, iVar),
                                      cB(i, j, k, iVar), cB(i, j, k + 1, iVar));
              Real cL = limiter_theta(limiterThetaB, cB(i, j, k - 2, iVar),
                                      cB(i, j, k - 1, iVar), cB(i, j, k, iVar));
              ul = min(ul, 0.5 / coef[iz_]);
              ur = min(ur, 0.5 / coef[iz_]);

              dB(i, j, k, iVar) +=
                  (cR * ur * (cB(i, j, k + 1, iVar) - cB(i, j, k, iVar)) -
                   cL * ul * (cB(i, j, k, iVar) - cB(i, j, k - 1, iVar))) *
                  coef[iz_];
            }
          }
        }
      });
    } // end MFIter
  } // end useUpwindB

  if (useHyperbolicCleaning) {
    MultiFab gradPhi(cGrids[iLev], DistributionMap(iLev), nDim3, 0);
    gradPhi.setVal(0.0);

    solve_hyp_phi(iLev);

    MultiFab gradPhiNode(nGrids[iLev], DistributionMap(iLev), nDim3, 0);
    gradPhiNode.setVal(0.0);

    grad_center_to_node(hypPhi[iLev], gradPhiNode, Geom(iLev).InvCellSize());

    average_node_to_cellcenter(gradPhi, 0, gradPhiNode, 0, nDim3,
                               gradPhi.nGrow());

    for (MFIter mfi(centerB[iLev]); mfi.isValid(); ++mfi) {
      Box box = mfi.validbox();

      const Array4<Real>& dB = centerDB[mfi].array();
      const Array4<Real>& gradPhiArr = gradPhi[mfi].array();

      ParallelFor(box, [&](int i, int j, int k) {
        IntVect ijk{ AMREX_D_DECL(i, j, k) };
        for (int iVar = 0; iVar < nDim3; iVar++) {
          dB(ijk, iVar) += -tc->get_dt() * gradPhiArr(ijk, iVar);
        }
      });
    } // end MFIter
  } // end useHyperbolicCleaning

  MultiFab::Add(centerB[iLev], centerDB, 0, 0, nDim3, 0);

  centerB[iLev].FillBoundary(Geom(iLev).periodicity());
}

//==========================================================
void Pic::smooth_multifab(MultiFab& mf, int iLev, int di, Real coef) {
  std::string nameFunc = "Pic::smooth_multifab";
  timing_func(nameFunc);

  MultiFab mfOld(mf.boxArray(), mf.DistributionMap(), mf.nComp(), mf.nGrow());

  auto smooth_dir = [&](int iDir) {
    int dIdx[3] = { 0, 0, 0 };
    dIdx[iDir] = di;

    MultiFab::Copy(mfOld, mf, 0, 0, mf.nComp(), mf.nGrow());

    for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
      const Box& box = mfi.validbox();

      Array4<Real> const& arrE = mf[mfi].array();
      Array4<Real> const& arrTmp = mfOld[mfi].array();

      ParallelFor(box, mf.nComp(), [&](int i, int j, int k, int iVar) {
        const Real weightSelf = 1 - coef;
        const Real WeightNei = coef / 2.0;

        const Real neiSum =
            arrTmp(i - dIdx[ix_], j - dIdx[iy_], k - dIdx[iz_], iVar) +
            arrTmp(i + dIdx[ix_], j + dIdx[iy_], k + dIdx[iz_], iVar);

        arrE(i, j, k, iVar) =
            weightSelf * arrTmp(i, j, k, iVar) + WeightNei * neiSum;
      });
    }

    mf.FillBoundary(Geom(iLev).periodicity());
  };

  smooth_dir(ix_);
  if (nDim > 1)
    smooth_dir(iy_);
  if (nDim > 2 && !isFake2D)
    smooth_dir(iz_);
}

//==========================================================
void Pic::smooth_E(MultiFab& mfE, int iLev) {
  std::string nameFunc = "Pic::smooth_E";
  timing_func(nameFunc);

  for (int icount = 0; icount < nSmoothE; icount++) {
    smooth_multifab(mfE, iLev, icount % 2 + 1);
  }
}
//==========================================================
void Pic::project_down_E() {
  if (finest_level > 0) {
    for (int iLev = finest_level; iLev > 0; iLev--) {
      amrex::MultiFab tmp(nGrids[iLev], DistributionMap(iLev), 3, 0);
      tmp.setVal(0.0);
      for (MFIter mfi(tmp); mfi.isValid(); ++mfi) {
        const Box& box = mfi.validbox();
        const Array4<Real>& arrE = nodeE[iLev][mfi].array();
        const Array4<Real>& arrTmp = tmp[mfi].array();
        ParallelFor(box, [&](int i, int j, int k) {
          for (int iVar = 0; iVar < 3; iVar++) {
            if (nDim == 3) {
              arrTmp(i, j, k, iVar) =
                  0.5 * arrE(i, j, k, iVar) +
                  (1 / 12.0) *
                      (arrE(i + 1, j, k, iVar) + arrE(i - 1, j, k, iVar) +
                       arrE(i, j + 1, k, iVar) + arrE(i, j - 1, k, iVar) +
                       arrE(i, j, k + 1, iVar) + arrE(i, j, k - 1, iVar));
            } else {
              arrTmp(i, j, k, iVar) =
                  0.5 * arrE(i, j, k, iVar) +
                  (1 / 8.0) *
                      (arrE(i + 1, j, k, iVar) + arrE(i - 1, j, k, iVar) +
                       arrE(i, j + 1, k, iVar) + arrE(i, j - 1, k, iVar));
            }
          }
        });
      }
      fill_fine_lev_edge_from_coarse(
          nodeE[iLev - 1], tmp, 0, nodeE[iLev].nComp(), ref_ratio[iLev],
          Geom(iLev - 1), Geom(iLev), node_status(iLev), node_bilinear_interp);
      average_down_nodal(tmp, nodeE[iLev - 1], ref_ratio[iLev - 1]);
    }
    for (int iLev = 0; iLev <= finest_level; iLev++) {
      nodeE[iLev].FillBoundary(Geom(iLev).periodicity());
    }
  }
}
//==========================================================
// Apply boundary conditions to electromagnetic fields (E or B).
// Visits each boundary face and applies the corresponding BC configured in
// bcField.
//==========================================================
void Pic::apply_field_bc(const iMultiFab& status, MultiFab& mf,
                         const int iStart, const int nComp, GETVALUE func,
                         const int iLev, const bool isB) {
  std::string nameFunc = "Pic::apply_field_bc";
  timing_func(nameFunc);

  if (Geom(iLev).isAllPeriodic())
    return;
  if (mf.nGrow() == 0)
    return;

  // Base fill: float on open faces, or evaluate state from func elsewhere.
  apply_BC(status, mf, iStart, nComp, func, iLev, &bcField);

  // Dedicated wall operators applied per configured face type.
  apply_conducting_wall(status, mf, iStart, nComp, iLev, bcField, isB);
  apply_absorbing_wall(status, mf, iStart, nComp, iLev, bcField, isB);
  apply_inflow_wall(status, mf, iStart, nComp, iLev, bcField, isB);

  // Wave boundary condition overwrites faces where active.
  if (waveBC.active) {
    const Real t = tc ? tc->get_time() : 0.0;
    apply_wave_field(status, mf, iStart, nComp, iLev, bcField, isB ? 0 : 1, t);
  }
}

//==========================================================
void Pic::apply_BC(const iMultiFab& status, MultiFab& mf, const int iStart,
                   const int nComp, GETVALUE func, const int iLev,
                   const BoxBC<FieldBC::Type>* bc) {
  std::string nameFunc = "Pic::apply_BC";
  timing_func(nameFunc);

  if (Geom(iLev).isAllPeriodic())
    return;
  if (mf.nGrow() == 0)
    return;

  bool useFloatBC = (func == nullptr);
  BoxArray ba = convert(activeRegion, mf.boxArray().ixType());

  const IntVect& ngrow = mf.nGrowVect();
  if (nDim > 2 &&
      Geom(iLev).Domain().bigEnd(iz_) == Geom(iLev).Domain().smallEnd(iz_)) {
    ba.grow(iz_, ngrow[iz_]);
  }

  if (bc != nullptr) {
    for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
      const Box& bxFab = mfi.fabbox();
      const Box& bxValid = mfi.validbox();

      if (!ba.contains(bxFab)) {
        Array4<Real> const& arr = mf[mfi].array();
        const Array4<const int>& statusArr = status[mfi].array();

        ParallelFor(bxFab, [&](int i, int j, int k) {
          if (bit::is_lev_boundary(statusArr(i, j, k, 0))) {
            int ip, jp, kp;
            bool useFloat = use_float(i, j, k, ip, jp, kp, *bc, bxValid);

            if (useFloat) {
              for (int iVar = iStart; iVar < iStart + nComp; iVar++) {
                arr(i, j, k, iVar) = arr(ip, jp, kp, iVar);
              }
            } else if (func) {
              for (int iVar = iStart; iVar < iStart + nComp; iVar++) {
                arr(i, j, k, iVar) = (this->*func)(
                    mfi, IntVect{ AMREX_D_DECL(i, j, k) }, iVar - iStart, iLev);
              }
            }
          }
        });
      }
    }
    return;
  }

  if (useFloatBC) {
    for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
      const Box& bxFab = mfi.fabbox();
      const Box& bxValid = mfi.validbox();

      if (!ba.contains(bxFab)) {
        Array4<Real> const& arr = mf[mfi].array();
        const Array4<const int>& statusArr = status[mfi].array();

        Box box = bxValid;
        box.grow(1);

        ParallelFor(box, [&](int i, int j, int k) {
          if (bit::is_lev_boundary(statusArr(i, j, k, 0))) {
            bool isNeiFound = false;
            const int kmin = (nDim > 2) ? -1 : 0;
            const int kmax = (nDim > 2) ? 1 : 0;
            for (int kk = kmin; kk <= kmax && !isNeiFound; ++kk) {
              for (int jj = -1; jj <= 1 && !isNeiFound; ++jj) {
                for (int ii = -1; ii <= 1 && !isNeiFound; ++ii) {
                  if (!bit::is_lev_boundary(
                          statusArr(i + ii, j + jj, k + kk, 0))) {
                    isNeiFound = true;
                    for (int iVar = iStart; iVar < iStart + nComp; ++iVar) {
                      arr(i, j, k, iVar) = arr(i + ii, j + jj, k + kk, iVar);
                    }
                  }
                }
              }
            }
          }
        });
      }
    }
  } else {
    for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
      const Box& bx = mfi.fabbox();

      if (!ba.contains(bx)) {
        Array4<Real> const& arr = mf[mfi].array();
        const Array4<const int>& statusArr = status[mfi].array();

        auto lo = IntVect(bx.loVect());
        auto hi = IntVect(bx.hiVect());
        if (nDim > 2 && Geom(iLev).Domain().bigEnd(iz_) ==
                            Geom(iLev).Domain().smallEnd(iz_)) {
          lo[iz_]++;
          hi[iz_]--;
        }

        Box box0(lo, hi);

        ParallelFor(box0, nComp, [&](int i, int j, int k, int iVar) {
          if (bit::is_lev_boundary(statusArr(i, j, k, 0))) {
            arr(i, j, k, iStart + iVar) = (this->*func)(
                mfi, IntVect{ AMREX_D_DECL(i, j, k) }, iVar, iLev);
          }
        });
      }
    }
  }
}

//==========================================================
void Pic::apply_conducting_wall(const iMultiFab& status, MultiFab& mf,
                                const int iStart, const int nComp,
                                const int iLev, const BoxBC<FieldBC::Type>& bc,
                                bool isB) {
  std::string nameFunc = "Pic::apply_conducting_wall";
  timing_func(nameFunc);

  if (Geom(iLev).isAllPeriodic())
    return;
  if (mf.nGrow() == 0)
    return;

  bool hasConducting = false;
  for (int d = 0; d < nDim; ++d) {
    if (bc.lo[d] == FieldBC::conducting || bc.hi[d] == FieldBC::conducting) {
      hasConducting = true;
      break;
    }
  }
  if (!hasConducting)
    return;

  BoxArray ba = convert(activeRegion, mf.boxArray().ixType());
  const IntVect& ngrow = mf.nGrowVect();
  if (nDim > 2 &&
      Geom(iLev).Domain().bigEnd(iz_) == Geom(iLev).Domain().smallEnd(iz_)) {
    ba.grow(iz_, ngrow[iz_]);
  }

  const IntVect domLo = Geom(iLev).Domain().smallEnd();
  const IntVect domHi = Geom(iLev).Domain().bigEnd();
  const IndexType ixType = mf.boxArray().ixType();

  bool isNode[3] = { false, false, false };
  int loBnd[3] = { 0, 0, 0 };
  int hiBnd[3] = { 0, 0, 0 };
  for (int d = 0; d < nDim; ++d) {
    isNode[d] = (ixType[d] == IndexType::NODE);
    loBnd[d] = domLo[d];
    hiBnd[d] = isNode[d] ? (domHi[d] + 1) : domHi[d];
  }

  for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
    const Box& bxFab = mfi.fabbox();
    if (ba.contains(bxFab))
      continue;

    Array4<Real> const& arr = mf[mfi].array();
    const Array4<const int>& statusArr = status[mfi].array();
    const Box& bxValid = mfi.validbox();

    ParallelFor(bxFab, [&](int i, int j, int k) {
      IntVect ijk{ AMREX_D_DECL(i, j, k) };

      // 1. Boundary nodes on physical wall (node-centred only).
      for (int d = 0; d < nDim; ++d) {
        if (!isNode[d])
          continue;

        const bool onLoWall =
            (bc.lo[d] == FieldBC::conducting) && (ijk[d] == loBnd[d]);
        const bool onHiWall =
            (bc.hi[d] == FieldBC::conducting) && (ijk[d] == hiBnd[d]);
        if (onLoWall || onHiWall) {
          bool inValid = true;
          for (int od = 0; od < nDim; ++od) {
            if (od != d && (ijk[od] < bxValid.smallEnd(od) ||
                            ijk[od] > bxValid.bigEnd(od))) {
              inValid = false;
              break;
            }
          }
          if (inValid) {
            for (int iVar = 0; iVar < nComp; ++iVar) {
              const int comp = iStart + iVar;
              if (isB) {
                if (iVar == d)
                  arr(i, j, k, comp) = 0.0;
              } else {
                if (iVar != d)
                  arr(i, j, k, comp) = 0.0;
              }
            }
          }
        }
      }

      // 2. Ghost cells/nodes outside domain.
      if (!bit::is_lev_boundary(statusArr(i, j, k, 0)))
        return;

      for (int d = 0; d < nDim; ++d) {
        bool isLow = (bc.lo[d] == FieldBC::conducting) && (ijk[d] < loBnd[d]);
        bool isHigh = (bc.hi[d] == FieldBC::conducting) && (ijk[d] > hiBnd[d]);
        if (!isLow && !isHigh)
          continue;

        IntVect m = ijk;
        if (isNode[d])
          m[d] = isLow ? (2 * loBnd[d] - ijk[d]) : (2 * hiBnd[d] - ijk[d]);
        else
          m[d] =
              isLow ? (2 * loBnd[d] - 1 - ijk[d]) : (2 * hiBnd[d] + 1 - ijk[d]);

        for (int iVar = 0; iVar < nComp; ++iVar) {
          const int comp = iStart + iVar;
          if (isB) {
            if (iVar == d)
              arr(i, j, k, comp) = 0.0;
            else
              arr(i, j, k, comp) = arr(m, comp);
          } else {
            if (iVar != d)
              arr(i, j, k, comp) = 0.0;
            else
              arr(i, j, k, comp) = arr(m, comp);
          }
        }
      }
    });
  }
}

//==========================================================
void Pic::apply_absorbing_wall(const iMultiFab& status, MultiFab& mf,
                               const int iStart, const int nComp,
                               const int iLev, const BoxBC<FieldBC::Type>& bc,
                               bool isB) {
  std::string nameFunc = "Pic::apply_absorbing_wall";
  timing_func(nameFunc);

  (void)isB;
  if (Geom(iLev).isAllPeriodic())
    return;
  if (mf.nGrow() == 0)
    return;

  bool hasAbsorb = false;
  for (int d = 0; d < nDim; ++d) {
    if (bc.lo[d] == FieldBC::absorb || bc.hi[d] == FieldBC::absorb) {
      hasAbsorb = true;
      break;
    }
  }
  if (!hasAbsorb)
    return;

  const Real dt = tc ? tc->get_dt() : 0.0;
  if (dt <= 0.0)
    return;

  // Characteristic speed; default c=1, override via #ABSORB.
  const Real cs = (absorbCharSpeed > 0.0) ? absorbCharSpeed : 1.0;

  BoxArray ba = convert(activeRegion, mf.boxArray().ixType());
  const IntVect& ngrow = mf.nGrowVect();
  if (nDim > 2 &&
      Geom(iLev).Domain().bigEnd(iz_) == Geom(iLev).Domain().smallEnd(iz_)) {
    ba.grow(iz_, ngrow[iz_]);
  }

  const IntVect domLo = Geom(iLev).Domain().smallEnd();
  const IntVect domHi = Geom(iLev).Domain().bigEnd();
  const IndexType ixType = mf.boxArray().ixType();
  const Real* dx = Geom(iLev).CellSize();

  bool isNode[3] = { false, false, false };
  int loBnd[3] = { 0, 0, 0 };
  int hiBnd[3] = { 0, 0, 0 };
  Real decay[3] = { 0.0, 0.0, 0.0 };
  Real drive[3] = { 0.0, 0.0, 0.0 };

  for (int d = 0; d < nDim; ++d) {
    isNode[d] = (ixType[d] == IndexType::NODE);
    loBnd[d] = domLo[d];
    hiBnd[d] = isNode[d] ? (domHi[d] + 1) : domHi[d];

    const Real drive0 = cs * dt / dx[d];
    decay[d] = (1.0 - drive0) / (1.0 + drive0);
    drive[d] = 2.0 * drive0 / (1.0 + drive0);
  }

  for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
    const Box& bxFab = mfi.fabbox();
    if (ba.contains(bxFab))
      continue;

    Array4<Real> const& arr = mf[mfi].array();
    const Array4<const int>& statusArr = status[mfi].array();

    ParallelFor(bxFab, [&](int i, int j, int k) {
      if (!bit::is_lev_boundary(statusArr(i, j, k, 0)))
        return;

      IntVect ijk{ AMREX_D_DECL(i, j, k) };

      for (int d = 0; d < nDim; ++d) {
        bool isLow = (bc.lo[d] == FieldBC::absorb) && (ijk[d] < loBnd[d]);
        bool isHigh = (bc.hi[d] == FieldBC::absorb) && (ijk[d] > hiBnd[d]);
        if (!isLow && !isHigh)
          continue;

        IntVect m = ijk;
        if (isNode[d])
          m[d] = isLow ? (2 * loBnd[d] - ijk[d]) : (2 * hiBnd[d] - ijk[d]);
        else
          m[d] =
              isLow ? (2 * loBnd[d] - 1 - ijk[d]) : (2 * hiBnd[d] + 1 - ijk[d]);

        for (int iVar = 0; iVar < nComp; ++iVar) {
          const int comp = iStart + iVar;
          arr(i, j, k, comp) =
              decay[d] * arr(i, j, k, comp) + drive[d] * arr(m, comp);
        }
      }
    });
  }
}

//==========================================================
void Pic::apply_inflow_wall(const iMultiFab& status, MultiFab& mf,
                            const int iStart, const int nComp, const int iLev,
                            const BoxBC<FieldBC::Type>& bc, bool isB) {
  std::string nameFunc = "Pic::apply_inflow_wall";
  timing_func(nameFunc);

  (void)isB; // zero-gradient copy is component-agnostic

  if (!fi->get_inflow_defined())
    return;
  if (Geom(iLev).isAllPeriodic())
    return;
  if (mf.nGrow() == 0)
    return;

  bool hasInflow = false;
  for (int d = 0; d < nDim; ++d) {
    if (bc.lo[d] == FieldBC::inflow || bc.hi[d] == FieldBC::inflow) {
      hasInflow = true;
      break;
    }
  }
  if (!hasInflow)
    return;

  BoxArray ba = convert(activeRegion, mf.boxArray().ixType());
  const IntVect& ngrow = mf.nGrowVect();
  if (nDim > 2 &&
      Geom(iLev).Domain().bigEnd(iz_) == Geom(iLev).Domain().smallEnd(iz_)) {
    ba.grow(iz_, ngrow[iz_]);
  }

  const IntVect domLo = Geom(iLev).Domain().smallEnd();
  const IntVect domHi = Geom(iLev).Domain().bigEnd();
  const IndexType ixType = mf.boxArray().ixType();

  bool isNode[3] = { false, false, false };
  int loBnd[3] = { 0, 0, 0 };
  int hiBnd[3] = { 0, 0, 0 };
  for (int d = 0; d < nDim; ++d) {
    isNode[d] = (ixType[d] == IndexType::NODE);
    loBnd[d] = domLo[d];
    hiBnd[d] = isNode[d] ? (domHi[d] + 1) : domHi[d];
  }

  for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
    const Box& bxFab = mfi.fabbox();
    if (ba.contains(bxFab))
      continue;

    Array4<Real> const& arr = mf[mfi].array();
    const Array4<const int>& statusArr = status[mfi].array();

    ParallelFor(bxFab, [&](int i, int j, int k) {
      if (!bit::is_lev_boundary(statusArr(i, j, k, 0)))
        return;

      IntVect ijk{ AMREX_D_DECL(i, j, k) };

      for (int d = 0; d < nDim; ++d) {
        bool isLow = (bc.lo[d] == FieldBC::inflow) && (ijk[d] < loBnd[d]);
        bool isHigh = (bc.hi[d] == FieldBC::inflow) && (ijk[d] > hiBnd[d]);
        if (!isLow && !isHigh)
          continue;

        IntVect m = ijk;
        m[d] = isLow ? loBnd[d] : hiBnd[d];

        for (int iVar = 0; iVar < nComp; ++iVar) {
          arr(i, j, k, iStart + iVar) = arr(m, iStart + iVar);
        }
      }
    });
  }
}

//==========================================================
// Mirror ion moments into physical-wall ghost cells for smooth Ohm/Hall
// stencils.
void Pic::apply_centerPlasma_BC(const iMultiFab& status, MultiFab& mf,
                                const int iLev) {
  std::string nameFunc = "Pic::apply_centerPlasma_BC";
  timing_func(nameFunc);

  if (Geom(iLev).isAllPeriodic())
    return;
  if (mf.nGrow() == 0)
    return;

  const IntVect& ngrow = mf.nGrowVect();
  BoxArray ba = convert(activeRegion, mf.boxArray().ixType());
  if (nDim > 2 &&
      Geom(iLev).Domain().bigEnd(iz_) == Geom(iLev).Domain().smallEnd(iz_)) {
    ba.grow(iz_, ngrow[iz_]);
  }

  const IntVect domLo = Geom(iLev).Domain().smallEnd();
  const IntVect domHi = Geom(iLev).Domain().bigEnd();
  const int nComp = mf.nComp();

  for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
    const Box& bxFab = mfi.fabbox();
    if (ba.contains(bxFab))
      continue;

    Array4<Real> const& arr = mf[mfi].array();
    const Array4<const int>& statusArr = status[mfi].array();

    ParallelFor(bxFab, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      if (!bit::is_lev_boundary(statusArr(i, j, k, 0)))
        return;

      IntVect ijk{ AMREX_D_DECL(i, j, k) };
      IntVect m = ijk;
      bool touched = false;

      for (int d = 0; d < nDim; ++d) {
        if (ijk[d] < domLo[d]) {
          m[d] = 2 * domLo[d] - 1 - ijk[d];
          touched = true;
        } else if (ijk[d] > domHi[d]) {
          m[d] = 2 * domHi[d] + 1 - ijk[d];
          touched = true;
        }
      }

      if (!touched)
        return;
      for (int comp = 0; comp < nComp; ++comp) {
        arr(i, j, k, comp) = arr(m, comp);
      }
    });
  }
}

//==========================================================
void Pic::apply_wave_field(const iMultiFab& status, MultiFab& mf,
                           const int iStart, const int nComp, const int iLev,
                           const BoxBC<FieldBC::Type>& bc, int iField, Real t) {
  std::string nameFunc = "Pic::apply_wave_field";
  timing_func(nameFunc);

  (void)bc;
  if (!waveBC.active)
    return;
  if (Geom(iLev).isAllPeriodic())
    return;
  if (mf.nGrow() == 0)
    return;

  bool hasField = false;
  for (const auto& f : waveBC.faces) {
    for (const auto& c : f.comps) {
      if (c.iField == iField) {
        hasField = true;
        break;
      }
    }
    if (hasField)
      break;
  }
  if (!hasField)
    return;

  BoxArray ba = convert(activeRegion, mf.boxArray().ixType());
  const IntVect& ngrow = mf.nGrowVect();
  if (nDim > 2 &&
      Geom(iLev).Domain().bigEnd(iz_) == Geom(iLev).Domain().smallEnd(iz_)) {
    ba.grow(iz_, ngrow[iz_]);
  }

  const Real* plo = Geom(iLev).ProbLo();
  const Real* dx = Geom(iLev).CellSize();

  for (MFIter mfi(mf); mfi.isValid(); ++mfi) {
    const Box& bxFab = mfi.fabbox();
    const Box& bxValid = mfi.validbox();
    if (ba.contains(bxFab))
      continue;

    Array4<Real> const& arr = mf[mfi].array();
    const Array4<const int>& statusArr = status[mfi].array();

    ParallelFor(bxFab, [&](int i, int j, int k) {
      if (!bit::is_lev_boundary(statusArr(i, j, k, 0)))
        return;

      Real pos[3] = { plo[0] + dx[0] * i, plo[1] + dx[1] * j,
                      plo[2] + dx[2] * k };

      Real waveVal[3] = { 0.0, 0.0, 0.0 };
      bool hasWave = false;

      for (const auto& f : waveBC.faces) {
        const int d = f.direction;
        const int side = f.side;
        const int idx = (d == 0) ? i : (d == 1) ? j : k;
        bool onFace = (side == 0 && idx < bxValid.smallEnd(d)) ||
                      (side == 1 && idx > bxValid.bigEnd(d));
        if (!onFace)
          continue;

        for (const auto& c : f.comps) {
          if (c.iField != iField)
            continue;
          hasWave = true;
          const Real val = waveBC.value(c, t, pos);
          if (iField == 0 || iField == 1) {
            for (int iVar = 0; iVar < std::min(nComp, 3); ++iVar) {
              waveVal[iVar] += val * c.pol[iVar];
            }
          } else {
            waveVal[0] += val;
          }
        }
      }

      if (hasWave) {
        if (iField == 0 || iField == 1) {
          for (int iVar = 0; iVar < nComp; ++iVar) {
            arr(i, j, k, iStart + iVar) = waveVal[iVar % 3];
          }
        } else if (nComp > 0) {
          arr(i, j, k, iStart) = waveVal[0];
        }
      }
    });
  }
}

//==========================================================
void Pic::wave_velocity_kick(const Real* pos, Real t, Real& dvx, Real& dvy,
                             Real& dvz) {
  dvx = dvy = dvz = 0.0;
  if (!waveBC.active)
    return;
  for (const auto& f : waveBC.faces) {
    for (const auto& c : f.comps) {
      if (c.iField != 2) // velocity kick
        continue;
      const Real val = waveBC.value(c, t, pos);
      dvx += val * c.pol[0];
      dvy += val * c.pol[1];
      dvz += val * c.pol[2];
    }
  }
}

//==========================================================
Real Pic::calc_E_field_energy() {
  Real sum = 0;
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (MFIter mfi(centerB[iLev]); mfi.isValid(); ++mfi) {
      FArrayBox& fab = nodeE[iLev][mfi];
      const auto& status = cell_status(iLev)[mfi].array();
      Box box = mfi.validbox();
      const Array4<Real>& arr = fab.array();

      Real sumLoc = 0;
      ParallelFor(box, [&](int i, int j, int k) {
        IntVect ijk = { AMREX_D_DECL(i, j, k) };

        if (!bit::is_refined(status(ijk))) {
          Box subBox(ijk, ijk + 1);
          ParallelFor(subBox, [&](int ii, int jj, int kk) {
            IntVect ijk0 = { AMREX_D_DECL(ii, jj, kk) };
            sumLoc += pow(arr(ijk0, ix_), 2) + pow(arr(ijk0, iy_), 2) +
                      pow(arr(ijk0, iz_), 2);
          });
        }
      });

      Real avg = (nDim == 3) ? 0.125 : 0.25;

      sum += sumLoc * 0.5 * avg * get_cell_volume(iLev) / fourPI;
    }
    ParallelDescriptor::ReduceRealSum(sum,
                                      ParallelDescriptor::IOProcessorNumber());

    if (!ParallelDescriptor::IOProcessor())
      sum = 0;
  }
  return sum;
}

//==========================================================
Real Pic::calc_B_field_energy() {
  Real sum = 0;

  for (int iLev = 0; iLev < n_lev(); iLev++) {
    for (MFIter mfi(centerB[iLev]); mfi.isValid(); ++mfi) {
      FArrayBox& fab = centerB[iLev][mfi];
      const auto& status = cell_status(iLev)[mfi].array();

      const Box& box = mfi.validbox();
      const Array4<Real>& arr = fab.array();

      Real sumLoc = 0;
      ParallelFor(box, [&](int i, int j, int k) {
        IntVect ijk = { AMREX_D_DECL(i, j, k) };
        if (!bit::is_refined(status(ijk))) {
          sumLoc += pow(arr(i, j, k, ix_), 2) + pow(arr(i, j, k, iy_), 2) +
                    pow(arr(i, j, k, iz_), 2);
        }
      });

      sum += sumLoc * get_cell_volume(iLev) * 0.5 / fourPI;
    }
  }
  ParallelDescriptor::ReduceRealSum(sum,
                                    ParallelDescriptor::IOProcessorNumber());

  if (!ParallelDescriptor::IOProcessor())
    sum = 0;

  return sum;
}

//==========================================================
void Pic::convert_1d_to_3d(const double* const p, MultiFab& MF, int iLev) {
  std::string nameFunc = "Pic::convert_1d_to_3d";
  timing_func(nameFunc);

  bool isCenter = MF.ixType().cellCentered();

  MF.setVal(0.0);

  int iCount = 0;
  for (MFIter mfi(MF, doTiling); mfi.isValid(); ++mfi) {
    const Box& box = mfi.tilebox();

    const Array4<Real>& arr = MF[mfi].array();

    const auto& nodeArr = nodeStatus[iLev][mfi].array();

    ParallelFor(box, MF.nComp(), [&](int i, int j, int k, int iVar) {
      if (isCenter || bit::is_owner(nodeArr(i, j, k))) {
        arr(i, j, k, iVar) = p[iCount++];
      }
    });
  }
}

//==========================================================
void Pic::convert_3d_to_1d(const MultiFab& MF, double* const p, int iLev) {
  std::string nameFunc = "Pic::convert_3d_to_1d";
  timing_func(nameFunc);

  bool isCenter = MF.ixType().cellCentered();

  int iCount = 0;
  for (MFIter mfi(MF, doTiling); mfi.isValid(); ++mfi) {
    const Box& box = mfi.tilebox();

    const Array4<Real const>& arr = MF[mfi].array();

    const auto& nodeArr = nodeStatus[iLev][mfi].array();

    ParallelFor(box, MF.nComp(), [&](int i, int j, int k, int iVar) {
      if (isCenter || bit::is_owner(nodeArr(i, j, k))) {
        p[iCount++] = arr(i, j, k, iVar);
      }
    });
  }
}

//==========================================================
void Pic::report_load_balance(bool doReportSummary, bool doReportDetail) {
  // This function report the min, max, and average of the local memory usage,
  // blocks, cells and particles among all the MPIs.
  if (!doReportSummary && !doReportDetail)
    return;

  std::string nameFunc = "Pic::monitor";
  timing_func(nameFunc);

  int iNBlk_ = 0, iNCell_ = 1, iNParts_ = 2, iMem_ = 3 * (n_lev() + 1),
      nLocal = iMem_ + 1;

  Vector<float> localInfo(nLocal, 0);

  int nProc = ParallelDescriptor::NProcs();

  Vector<int> rc(nProc, nLocal), disp(nProc, 0);
  for (int i = 0; i < nProc; ++i) {
    disp[i] = i * nLocal;
  }

  localInfo[iMem_] = (float)read_mem_usage();

  const int iBt = n_lev() * 3 + iNBlk_;
  const int iCt = n_lev() * 3 + iNCell_;
  const int iPt = n_lev() * 3 + iNParts_;
  localInfo[iBt] = 0;
  localInfo[iCt] = 0;
  localInfo[iPt] = 0;
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    const int iB = iLev * 3 + iNBlk_;
    const int iC = iLev * 3 + iNCell_;
    const int iP = iLev * 3 + iNParts_;

    localInfo[iB] = (float)centerB[iLev].local_size();
    localInfo[iC] = (float)get_local_node_or_cell_number(centerB[iLev]);

    localInfo[iP] = 0;
    for (auto& part : parts) {
      localInfo[iP] += (float)part->NumberOfParticlesAtLevel(iLev, false, true);
    }

    localInfo[iBt] += localInfo[iB];
    localInfo[iCt] += localInfo[iC];
    localInfo[iPt] += localInfo[iP];
  }

  Vector<float> globalInfo;
  if (ParallelDescriptor::IOProcessor()) {
    globalInfo.resize(nLocal * nProc);
  }

  int iop = ParallelDescriptor::IOProcessorNumber();
  ParallelDescriptor::Gatherv(localInfo.dataPtr(), nLocal, globalInfo.data(),
                              rc, disp, iop);

  if (ParallelDescriptor::IOProcessor()) {

    if (doReportSummary) {

      Vector<float> maxVal(nLocal, 0);
      Vector<float> minVal(nLocal, 1e10);
      Vector<float> avgVal(nLocal, 0);
      Vector<int> maxLoc(nLocal, 0);

      for (int iProc = 0; iProc < nProc; iProc++)
        for (int iType = 0; iType < nLocal; iType++) {
          const float val = globalInfo[disp[iProc] + iType];
          if (val > maxVal[iType]) {
            maxVal[iType] = val;
            maxLoc[iType] = iProc;
          }

          if (val < minVal[iType])
            minVal[iType] = val;

          avgVal[iType] += val;
        }

      for (int iType = 0; iType < nLocal; iType++) {
        avgVal[iType] /= nProc;
      }

      printf("\n===============================Load balance "
             "report=============================\n");
      printf("|     Value          |      Min      |     Avg      |      Max "
             "    "
             "|where(max)|\n");

      Vector<std::string> varType = {
        "|Blocks # of",
        "|Cells  # of",
        "|Parts  # of",
        "|Memory(MB)          |",
      };

      for (int iLev = 0; iLev <= n_lev(); iLev++) {
        for (int i = iNBlk_; i <= iNParts_; ++i) {
          int idx = iLev * 3 + i;
          if (iLev < n_lev()) {
            printf("%s lev  %d %s %13.1f |%13.1f |%13.1f | %9d|\n",
                   varType[i].c_str(), iLev, " |", minVal[idx], avgVal[idx],
                   maxVal[idx], maxLoc[idx]);
          } else {
            printf("%s all levs| %13.1f |%13.1f |%13.1f | %9d|\n",
                   varType[i].c_str(), minVal[idx], avgVal[idx], maxVal[idx],
                   maxLoc[idx]);
          }
        }
        printf(
            "----------------------------------------------------------------"
            "---------------\n");
      }
      printf("%s %13.1f |%13.1f |%13.1f | %9d|\n", varType[3].c_str(),
             minVal[iMem_], avgVal[iMem_], maxVal[iMem_], maxLoc[iMem_]);

      printf("================================================================"
             "===============\n\n");
    }

    if (doReportDetail) {
      printf("\n");
      printf("=======================Work load of each MPI "
             "rank====================");
      for (int iLev = 1; iLev <= n_lev(); iLev++) {
        printf("=============================================");
      }
      printf("\n");

      printf("rank    |   Memory(MB) ");
      for (int iLev = 0; iLev < n_lev(); iLev++) {
        printf("| Blocks lev %d |  Cells lev %d |  Parts lev %d ", iLev, iLev,
               iLev);
      }
      printf("| Blocks all   |  Cells all   |  Parts all   |\n");

      for (int rank = 0; rank < nProc; rank++) {
        float* info = globalInfo.data() + rank * nLocal;
        printf("%6d  |%13.1f ", rank, info[iMem_]);
        for (int iLev = 0; iLev <= n_lev(); iLev++) {
          printf("|%13.1f |%13.1f |%13.1f ", info[iLev * 3 + iNBlk_],
                 info[iLev * 3 + iNCell_], info[iLev * 3 + iNParts_]);
        }
        printf("|\n");
      }

      printf("================================================================="
             "====");
      for (int iLev = 1; iLev <= n_lev(); iLev++) {
        printf("=============================================");
      }
      printf("\n\n");
    }
  }
}

void Pic::charge_exchange() {
  timing_func("Pic::charge_exchange");

  if (!stateOH || !sourcePT2OH || !source)
    return;

  if (!kineticSource)
    source->set_node_fluid_to_zero();

  bool doSelectRegion = false;
#ifdef _PT_COMPONENT_
  doSelectRegion = (nSpecies == 4);
#endif

  maxExchangeRatio = 0;
  for (int i = 0; i < nSpecies; ++i) {
    Real rate = 0;
    parts[i]->charge_exchange(tc->get_dt(), stateOH, sourcePT2OH, source,
                              kineticSource, sourceParts, doSelectRegion,
                              product(nSourcePPC), rate);
    if (rate > maxExchangeRatio)
      maxExchangeRatio = rate;
  }

  if (kineticSource) {
    for (int i = 0; i < nSpecies; ++i) {
      parts[i]->add_source_particles(sourceParts[i], nSourcePPC,
                                     adaptiveSourcePPC);
      sourceParts[i]->clearParticles();
    }

  } else {
    // 'source' is applied to generate new particles every step, so
    // sum_boundary() is called here to correct boundary nodes. Boundary nodes
    // of 'sourcePT2OH' should be corrected just before PT->OH coupling,
    // instead of here.
    source->sum_boundary();

#ifdef _PT_COMPONENT_
    bool doRegionSplit = (nSpecies == 4);
    if (doRegionSplit) {
      source->sum_to_single_source();
    }
#endif

    source->convert_moment_to_velocity(true, false);
  }
}

void Pic::amr_divE_correction() {
  std::string nameFunc = "Pic::divE_correction";

  timing_func(nameFunc);

  for (int iIter = 0; iIter < nDivECorrection; iIter++) {
    for (int iLev = finest_level; iLev >= 0; iLev--) {
      sum_to_center_amr(true, iLev);
      skip_cells_divE_correction(centerMM[iLev], cell_status(iLev), iLev);
      calculate_phi(divESolver, iLev);
      for (int i = 0; i < nSpecies; ++i) {
        parts[i]->divE_correct_position(centerPhi, iLev);
      }
      if (finest_level > 0) {
        for (int i = 0; i < nSpecies; ++i) {
          parts[i]->Redistribute();
        }
      }
    }
  }

  inject_particles_for_boundary_cells();
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    sum_to_center_amr(false, iLev);
    if (iLev > 0) {
      skip_cells_divE_correction(centerNetChargeN[iLev], cell_status(iLev),
                                 iLev);
      skip_cells_divE_correction(centerDivE[iLev], cell_status(iLev), iLev);
    }
  }
}
