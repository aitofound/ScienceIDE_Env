#ifndef _EXOSOURCE_H_
#define _EXOSOURCE_H_

#include "SourceInterface.h"

class UserSource : public SourceInterface {
public:
  UserSource(const FluidInterface& other, int id, std::string tag,
             FluidType typeIn, const DomainParameters& dp)
      : SourceInterface(other, id, tag, typeIn, dp) {
    info = "Exosphere Source";
    useFluidSource = true;
  }

  // ---- Exosphere density profiles ----

  double get_exosphere_density(double r) const override {
    if (exosphereType == "None") return 0.0;
    if (r < get_rPlanet_SI()) return 0.0;

    double sum = 0.0;
    if (exosphereType == "Exponential") {
      for (int i = 0; i < nExoComponent; ++i) {
        if (exoH0[i] > 0.0) {
          sum += exoN0[i] * exp(-(r - get_rPlanet_SI()) / exoH0[i]);
        }
      }
    } else if (exosphereType == "Power-Law") {
      for (int i = 0; i < nExoComponent; ++i) {
        if (r > 0.0) {
          sum += exoN0[i] * pow(get_rPlanet_SI() / r, exoK0[i]);
        }
      }
    } else if (exosphereType == "Chamberlain") {
      for (int i = 0; i < nExoComponent; ++i) {
        if (get_rPlanet_SI() > 0.0 && r > 0.0) {
          sum += exoN0[i] * exp(-exoH0[i] * (1.0 / get_rPlanet_SI() - 1.0 / r));
        }
      }
    }
    return sum;
  }

  double get_exosphere_component_density(double r,
                                         int iC) const override {
    if (exosphereType == "None") return 0.0;
    if (iC < 0 || iC >= nExoComponent) return 0.0;
    if (r < get_rPlanet_SI()) return 0.0;

    if (exosphereType == "Exponential") {
      if (exoH0[iC] > 0.0) {
        return exoN0[iC] * exp(-(r - get_rPlanet_SI()) / exoH0[iC]);
      }
    } else if (exosphereType == "Power-Law") {
      if (r > 0.0) {
        return exoN0[iC] * pow(get_rPlanet_SI() / r, exoK0[iC]);
      }
    } else if (exosphereType == "Chamberlain") {
      if (get_rPlanet_SI() > 0.0 && r > 0.0) {
        return exoN0[iC] * exp(-exoH0[iC] * (1.0 / get_rPlanet_SI() - 1.0 / r));
      }
    }
    return 0.0;
  }

  //-------------------------------------------------------------------
  // Check whether a point at (x, y, z) [m] relative to planet center
  // lies inside the planetary shadow cylinder.
  bool is_in_shadow(double x, double y, double z) const {
    if (!useShadowCylinder) return false;
    // Project position vector onto solar direction.
    // solarDir points from planet center toward the Sun.
    double proj = x * solarDir[0] + y * solarDir[1] + z * solarDir[2];
    if (proj >= 0.0) return false;  // dayside — not in shadow
    // Finite-height check: only shadow points within the cylinder height.
    if (-proj > shadowCylinderHalfHeight)
      return false;
    // Perpendicular distance from the sun–planet axis.
    double r2 = x * x + y * y + z * z;
    double perp2 = r2 - proj * proj;
    return perp2 <= shadowCylinderRadius * shadowCylinderRadius;
  }

  //-------------------------------------------------------------------
  // Compute electron temperature in eV from PIC-normalized pressure
  // and number density.  pe: electron pressure (PIC units).
  // ne: electron number density (PIC units).
  amrex::Real electron_temperature(amrex::Real pe, amrex::Real ne) const {
    if (ne <= 0.0) return 0.0;
    // T_eV = (m_p/e) * uNorm^2 * (pe/ne)
    const amrex::Real protonMassPerCharge =
        cProtonMassSI / cUnitChargeSI;   // [kg/C]
    const amrex::Real ur2 =
        get_unorm_si() * get_unorm_si(); // [m^2/s^2]
    return protonMassPerCharge * ur2 * pe / ne;
  }

  //=================================================================
  // Per-cell source term functions.
  //
  // Each of the three ionization processes is implemented as a function
  // operating on a single spatial cell.  They return the ionization
  // frequency nu [s^-1] contributed by that process; multiplying by the
  // neutral number density gives the production rate S_n [m^-3 s^-1].
  //=================================================================

  //-------------------------------------------------------------------
  // Photoionization frequency [s^-1] at position xyz [m] (measured from
  // the planet center) for neutral component iC.  Returns zero inside
  // the planetary shadow cylinder (nightside).
  amrex::Real photoionization_rate(const double xyz[3], int iC) const {
    // Zero inside the planetary shadow cylinder (nightside).
    if (is_in_shadow(xyz[0], xyz[1], xyz[2])) return 0.0;
    double r2 = xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2];
    double r_m = sqrt(r2);
    double ratio = get_rPlanet_SI() / r_m;
    // Geometric dilution: nu = nu0 * (r_planet / r)^2
    return photoNu0[iC] * ratio * ratio;
  }

  //-------------------------------------------------------------------
  // Electron-impact ionization frequency [s^-1] for neutral component
  // iC.  ne: electron number density [m^-3]; Te_eV: electron temperature
  // [eV].  Returns ne * <sigma*v>_impact, or zero when the plasma state
  // is not positive.
  amrex::Real impact_ionization_rate(amrex::Real ne, amrex::Real Te_eV,
                                     int iC) const {
    if (ne <= 0.0 || Te_eV <= 0.0) return 0.0;
    // Voronov 1997 fit: <sigma*v> in cm^3/s, converted to m^3/s.
    double Ei = impactEIon[iC];  // ionization energy [eV]
    double A = impactA[iC];      // Voronov A [cm^3/s]
    double K = impactK[iC];      // Voronov K
    double X = impactX[iC];      // Voronov X
    double t = Te_eV / Ei;
    double sigma_v = A * pow(t, K) / (X + t) * exp(-Ei / Te_eV) * 1e-6;
    return ne * sigma_v;
  }

  //-------------------------------------------------------------------
  // Charge-exchange frequency [s^-1] for neutral component iC reacting
  // with ion species iSp.  The ion density and bulk velocity are fetched
  // from the plasma state (other) at cell (mfi, idx, iLev).  The
  // cross-section is looked up from the matrix cxSigma[iC, iIon] where
  // iIon = iSp - 1 is the 0-based ion index.
  //
  // The neutrals are assumed to be at rest (zero bulk velocity), so the
  // ion speed alone is used as the ion-neutral relative speed.
  amrex::Real charge_exchange_rate(const FluidInterface& other,
                                   const amrex::MFIter& mfi,
                                   const amrex::IntVect& idx, int iLev,
                                   int iSp, int iC) const {
    int iIon = iSp - 1;  // 0-based ion index for matrix lookup
    double ni = other.get_number_density(mfi, idx, iSp, iLev);
    double ux_i = other.get_ux(mfi, idx, iSp, iLev);
    double uy_i = other.get_uy(mfi, idx, iSp, iLev);
    double uz_i = other.get_uz(mfi, idx, iSp, iLev);
    double u_mag_SI =
        sqrt(ux_i * ux_i + uy_i * uy_i + uz_i * uz_i) * get_unorm_si();
    if (ni <= 0.0 || u_mag_SI <= 0.0) return 0.0;
    // Constant cross-section model: sigmaCX in [cm^2], convert to [m^2].
    double sigma = cxSigma[iC * nCXIonSpecies + iIon];
    return ni * sigma * 1e-4 * u_mag_SI;
  }

  //-------------------------------------------------------------------
  // Require #EXOSPHERE to have been read (with nComponent > 0) before an
  // ionization command is parsed.  Domain syncs nExoComponent from fi into
  // this object just before dispatching ionization commands, so a missing or
  // out-of-order #EXOSPHERE leaves nExoComponent == 0 and is caught here.
  void require_exosphere(const std::string& command) const {
    if (nExoComponent > 0) return;
    amrex::Abort(printPrefix + "Error: " + command + " requires "
                 + "#EXOSPHERE with nComponent > 0 to be specified before "
                 + command + ".");
  }

  //-------------------------------------------------------------------
  // Read ionization-related parameter commands from PARAM.in.
  void read_param(const std::string& command, ReadParam& param) override {
    if (command == "#PHOTOIONIZATION") {
      usePhotoIonization = true;
      // The neutral component count is inherited from #EXOSPHERE.
      require_exosphere(command);
      photoNu0.resize(nExoComponent);
      for (int i = 0; i < nExoComponent; ++i) {
        param.read_var("nuPhoto0", photoNu0[i]);
      }
    } else if (command == "#ELECTRONIMPACT") {
      useElectronImpact = true;
      // The neutral component count is inherited from #EXOSPHERE.
      require_exosphere(command);
      impactEIon.resize(nExoComponent);
      impactA.resize(nExoComponent);
      impactK.resize(nExoComponent);
      impactX.resize(nExoComponent);
      for (int i = 0; i < nExoComponent; ++i) {
        param.read_var("eIon", impactEIon[i]);
        param.read_var("Acoeff", impactA[i]);
        param.read_var("Kcoeff", impactK[i]);
        param.read_var("Xcoeff", impactX[i]);
      }
    } else if (command == "#CHARGEEXCHANGE") {
      useChargeExchange = true;
      // The neutral component count is inherited from #EXOSPHERE.
      require_exosphere(command);
      param.read_var("nIonSpecies", nCXIonSpecies);
      cxSigma.resize(nExoComponent * nCXIonSpecies);
      for (int iC = 0; iC < nExoComponent; ++iC) {
        for (int iIon = 0; iIon < nCXIonSpecies; ++iIon) {
          param.read_var("sigmaCX", cxSigma[iC * nCXIonSpecies + iIon]);
        }
      }
    } else if (command == "#SHADOWCYLINDER") {
      useShadowCylinder = true;
      param.read_var("solarDirX", solarDir[0]);
      param.read_var("solarDirY", solarDir[1]);
      param.read_var("solarDirZ", solarDir[2]);
      param.read_var("radius", shadowCylinderRadius);
      param.read_var("halfHeight", shadowCylinderHalfHeight);
      // Normalize the solar direction vector.
      double norm = sqrt(solarDir[0] * solarDir[0] +
                         solarDir[1] * solarDir[1] +
                         solarDir[2] * solarDir[2]);
      if (norm > 0.0) {
        solarDir[0] /= norm;
        solarDir[1] /= norm;
        solarDir[2] /= norm;
      }
    } else if (command == "#RECOMBINATION") {
      useRecombination = true;
      int nRecomb;
      param.read_var("nReactions", nRecomb);
      recombIonIndex.resize(nRecomb);
      recombRate0.resize(nRecomb);
      recombTempExp.resize(nRecomb);
      recombRefTemp.resize(nRecomb);
      for (int i = 0; i < nRecomb; ++i) {
        param.read_var("ionSpecies", recombIonIndex[i]);
        param.read_var("rateCoef", recombRate0[i]);
        param.read_var("tempExponent", recombTempExp[i]);
        param.read_var("refTemp", recombRefTemp[i]);
      }
    } else if (command == "#CHEMISTRY") {
      useChemistry = true;
      int nRxns;
      param.read_var("nReactions", nRxns);
      chemReactions.resize(nRxns);
      for (int i = 0; i < nRxns; ++i) {
        param.read_var("reactantIon", chemReactions[i].reactantIon);
        param.read_var("productIon", chemReactions[i].productIon);
        param.read_var("neutralComp", chemReactions[i].neutralComp);
        param.read_var("rateType", chemReactions[i].rateType);
        param.read_var("rateCoef", chemReactions[i].rateCoef);
        param.read_var("tempExp", chemReactions[i].tempExp);
        param.read_var("refTemp", chemReactions[i].refTemp);
      }
    }
  }

  //-------------------------------------------------------------------
  // Validate consistency between exosphere and ionization commands.
  void post_process_param() override {
    // ---- Recombination validation (always executed, even without
    // exosphere, since recombination does not depend on neutrals). ----
    if (useRecombination) {
      if (nS < 1) {
        amrex::Abort(printPrefix + "Error: #RECOMBINATION requires "
                     + "plasma species. Use #PLASMA to set species.");
      }
      // Recombination requires electron density and temperature, which
      // are only available in useElectronFluid mode (species 0 = electron).
      if (!useElectronFluid) {
        amrex::Abort(printPrefix + "Error: #RECOMBINATION requires "
                     + "useElectronFluid = true (species 0 must be the "
                     + "electron). Set #PLASMA with an electron species.");
      }
      const int nIonS = nS - 1;
      for (int i = 0; i < static_cast<int>(recombIonIndex.size()); ++i) {
        int iSp = recombIonIndex[i];
        if (iSp < 1 || iSp > nIonS) {
          amrex::Abort(printPrefix + "Error: #RECOMBINATION ionSpecies "
                       + std::to_string(iSp) + " is out of range [1, "
                       + std::to_string(nIonS) + "].");
        }
        if (recombRate0[i] <= 0.0) {
          amrex::Abort(printPrefix + "Error: #RECOMBINATION rateCoef must "
                       + "be positive (got "
                       + std::to_string(recombRate0[i]) + ").");
        }
      }
    }

    // ---- Chemistry validation ----
    if (useChemistry) {
      if (nS < 2) {
        amrex::Abort(printPrefix + "Error: #CHEMISTRY requires at least "
                     + "2 plasma species (electron + 1 ion).");
      }
      const int nIonS = nS - 1;
      for (int i = 0; i < static_cast<int>(chemReactions.size()); ++i) {
        const auto& rxn = chemReactions[i];
        if (rxn.reactantIon < 0 || rxn.reactantIon > nIonS) {
          amrex::Abort(printPrefix + "Error: #CHEMISTRY reactantIon "
                       + std::to_string(rxn.reactantIon) + " out of range "
                       + "[0, " + std::to_string(nIonS) + "].");
        }
        if (rxn.productIon < 0 || rxn.productIon > nIonS) {
          amrex::Abort(printPrefix + "Error: #CHEMISTRY productIon "
                       + std::to_string(rxn.productIon) + " out of range "
                       + "[0, " + std::to_string(nIonS) + "].");
        }
        if (rxn.reactantIon == 0 && rxn.productIon == 0) {
          amrex::Abort(printPrefix + "Error: #CHEMISTRY reaction "
                       + std::to_string(i) + " has both reactantIon and "
                       + "productIon = 0 (no-op).");
        }
        if (rxn.rateType == 1 && rxn.neutralComp < 0) {
          amrex::Abort(printPrefix + "Error: #CHEMISTRY photoionization "
                       + "reaction " + std::to_string(i)
                       + " requires a neutral component.");
        }
        if (rxn.rateType == 0 && rxn.neutralComp < 0 &&
            rxn.reactantIon == 0) {
          amrex::Abort(printPrefix + "Error: #CHEMISTRY thermal reaction "
                       + std::to_string(i) + " with no neutral and no "
                       + "reactant ion is invalid.");
        }
      }
    }

    if (exosphereType == "None") return;

    // The source code assumes species 0 is the electron (used for ne and Te
    // in electron impact ionization, and skipped in the exosphere-to-ion
    // mapping where component iC -> species iC+1).  Enforce this here.
    if (nS < 1) {
      amrex::Abort(printPrefix + "Error: no plasma species defined. "
                   + "Use #PLASMA to set species.");
    }
    if (QoQi_S[0] >= 0.0) {
      amrex::Abort(printPrefix + "Error: species 0 must be the electron "
                   + "(negative charge). Got Q/Qi[0] = "
                   + std::to_string(QoQi_S[0])
                   + ". Reorder #PLASMA so the electron is first.");
    }
    if (nExoComponent > nS - 1) {
      amrex::Abort(printPrefix + "Error: #EXOSPHERE nComponent ("
                   + std::to_string(nExoComponent)
                   + ") exceeds the number of ion species ("
                   + std::to_string(nS - 1)
                   + "). Add more ion species in #PLASMA.");
    }

    if (useChargeExchange) {
      // nExoComponent is used directly as the neutral component count for
      // charge exchange (no separate nComponent in #CHARGEEXCHANGE).  The
      // cross-section matrix was sized to nExoComponent * nCXIonSpecies in
      // read_param; only the ion-species count needs validating here.
      if (nCXIonSpecies != nS - 1) {
        amrex::Abort(printPrefix + "Error: #CHARGEEXCHANGE nIonSpecies ("
                     + std::to_string(nCXIonSpecies)
                     + ") != number of ion species ("
                     + std::to_string(nS - 1) + ")");
      }
    }

    // The neutral temperature exoT0[iC] is used as the source temperature
    // for the pressure source term in set_source.
    for (int iC = 0; iC < nExoComponent; ++iC) {
      if (exoT0[iC] <= 0.0) {
        amrex::Abort(printPrefix + "Error: #EXOSPHERE T0 for component "
                     + std::to_string(iC) + " must be positive (got "
                     + std::to_string(exoT0[iC])
                     + " K). It is used as the ionization source temperature.");
      }
    }
  }

  //=================================================================
  // Chemistry helper functions (extracted from set_source)
  //=================================================================

  //-------------------------------------------------------------------
  // Compute the SI reaction frequency [s^-1] for a single chemistry
  // reaction at position xyz [m] with radial distance r_val [m].
  // ne and Te_eV are the electron number density (normalized) and
  // temperature [eV], used for thermal rate coefficients.
  double chem_reaction_rate(const ChemistryReaction& rxn,
                            const double xyz[3], double r_val,
                            double ne, double Te_eV) const {
    if (rxn.rateType == 1) {
      // Photoionization: rate = nu0 * (Rp/r)^2 [s^-1]
      if (is_in_shadow(xyz[0], xyz[1], xyz[2])) return 0.0;
      double ratio = get_rPlanet_SI() / r_val;
      return rxn.rateCoef * ratio * ratio;
    }

    // Thermal rate coefficient k(Te) [cm^3/s] -> [m^3/s]
    double k_si = rxn.rateCoef * 1e-6;
    if (rxn.tempExp != 0.0 && Te_eV > 0.0) {
      double Te_K = Te_eV * cUnitChargeSI / cBoltzmannSI;
      k_si *= pow(rxn.refTemp / Te_K, rxn.tempExp);
    }

    if (rxn.neutralComp >= 0) {
      // Charge exchange: rate = k * n_neutral [s^-1]
      double n_neutral =
          get_exosphere_component_density(r_val, rxn.neutralComp);
      return k_si * n_neutral;
    }

    // Recombination: rate = k * n_e [s^-1]
    // ne is normalized; convert to SI: n_si = n_norm / (get_Si2NoRho() * mp)
    return k_si * ne / (get_Si2NoRho() * cProtonMassSI);
  }

  //-------------------------------------------------------------------
  // Apply the source term for a single chemistry reaction to the
  // per-cell source accumulators (srcRho, srcP, srcRhoU*).
  // rate: SI reaction frequency [s^-1].
  void chem_apply_source(const ChemistryReaction& rxn, double rate,
                         const FluidInterface& other,
                         const amrex::MFIter& mfi,
                         const amrex::IntVect& idx, int iLev,
                         int nIonS, double r_val,
                         std::vector<double>& srcRho,
                         std::vector<double>& srcP,
                         std::vector<double>& srcRhoUx,
                         std::vector<double>& srcRhoUy,
                         std::vector<double>& srcRhoUz) const {
    if (rxn.productIon <= 0 || rxn.productIon > nIonS) return;

    int iSpProd = rxn.productIon;
    double mass_prod = get_species_mass(iSpProd);

    if (rxn.reactantIon > 0 && rxn.reactantIon <= nIonS) {
      // Cross-species CX: product inherits reactant velocity and
      // temperature.
      int iSpReac = rxn.reactantIon;
      double rho_reac_norm =
          other.get_value(mfi, idx, iRho_I[iSpReac], iLev);
      if (rho_reac_norm <= 0.0) return;

      double mass_reac = get_species_mass(iSpReac);
      // Normalized number density: n_norm = rho_norm / mass_amu
      double n_reac_norm = rho_reac_norm / mass_reac;
      // Convert to SI: n_si = n_norm / (get_Si2NoRho() * cProtonMassSI)
      double n_reac_si =
          n_reac_norm / (get_Si2NoRho() * cProtonMassSI);

      // Source mass density rate (SI): srcRho = rate * n_si * m_prod * mp
      double srcRho_si =
          rate * n_reac_si * mass_prod * cProtonMassSI;
      srcRho[iSpProd] += srcRho_si;

      // Source momentum: product inherits reactant velocity (normalized).
      double ux_reac = other.get_ux(mfi, idx, iSpReac, iLev);
      double uy_reac = other.get_uy(mfi, idx, iSpReac, iLev);
      double uz_reac = other.get_uz(mfi, idx, iSpReac, iLev);
      srcRhoUx[iSpProd] += srcRho_si * ux_reac;
      srcRhoUy[iSpProd] += srcRho_si * uy_reac;
      srcRhoUz[iSpProd] += srcRho_si * uz_reac;

      // Source pressure: product inherits reactant temperature.
      // Each reacting ion transfers its thermal pressure to the
      // product: srcP = rate * P_reac (SI) [Pa/s].
      double p_reac_norm =
          other.get_value(mfi, idx, iP_I[iSpReac], iLev);
      double p_reac_si = p_reac_norm / get_Si2NoP();
      srcP[iSpProd] += rate * p_reac_si;
    } else {
      // Photoionization: source from neutral at rest (zero velocity).
      double n_neutral =
          get_exosphere_component_density(r_val, rxn.neutralComp);
      double S_n = n_neutral * rate;  // [m^-3 s^-1]
      srcRho[iSpProd] += S_n * mass_prod * cProtonMassSI;
      // Pressure from neutral temperature
      if (rxn.neutralComp < nExoComponent) {
        srcP[iSpProd] += S_n * mass_prod * cProtonMassSI *
            cBoltzmannSI * exoT0[rxn.neutralComp];
      }
    }
  }

  //-------------------------------------------------------------------
  // Apply the loss term for a single chemistry reaction to the
  // per-cell loss array.  rate: SI reaction frequency [s^-1].
  void chem_apply_loss(const ChemistryReaction& rxn, double rate,
                       const FluidInterface& other,
                       const amrex::MFIter& mfi,
                       const amrex::IntVect& idx, int iLev,
                       int nIonS, int i, int j, int k,
                       amrex::Array4<amrex::Real>& lossArr) const {
    if (rxn.reactantIon <= 0 || rxn.reactantIon > nIonS) return;

    int iSpReac = rxn.reactantIon;
    int iIonLoss = iSpReac - 1;
    double rho_reac_norm =
        other.get_value(mfi, idx, iRho_I[iSpReac], iLev);
    if (rho_reac_norm <= 0.0) return;

    // Loss rate (normalized) = rate * rho_norm / Si2NoT
    double lossRho_norm = rate * rho_reac_norm / get_Si2NoT();
    if (lossRho_norm > 0.0) {
      lossArr(i, j, k, iIonLoss) += lossRho_norm;
    }
  }

  //-------------------------------------------------------------------
  // Apply #RECOMBINATION loss terms for a single cell.
  // ne: normalized electron number density; Te_eV: electron temp [eV].
  void apply_recombination_loss(const FluidInterface& other,
                                const amrex::MFIter& mfi,
                                const amrex::IntVect& idx, int iLev,
                                int nIonS, double ne, double Te_eV,
                                int i, int j, int k,
                                amrex::Array4<amrex::Real>& lossArr) const {
    for (int iR = 0; iR < static_cast<int>(recombIonIndex.size()); ++iR) {
      int iSp = recombIonIndex[iR];
      if (iSp < 1 || iSp > nIonS) continue;
      int iIon = iSp - 1;  // 0-based index in nodeLossFluid

      // k(Te) [cm^3/s] -> [m^3/s]
      double k_si = recombRate0[iR] * 1e-6;
      if (Te_eV > 0.0 && recombTempExp[iR] != 0.0) {
        double Te_K = Te_eV * cUnitChargeSI / cBoltzmannSI;
        k_si *= pow(recombRefTemp[iR] / Te_K, recombTempExp[iR]);
      }

      // rho_ion in normalized mass density (from plasma state).
      double rho_ion_norm =
          other.get_value(mfi, idx, iRho_I[iSp], iLev);
      if (rho_ion_norm <= 0.0) continue;

      // Loss rate (normalized):
      // lossRho_si = k_si * ne_si * rho_ion_si
      // lossRho_norm = lossRho_si * get_Si2NoRho() / Si2NoT
      //             = k_si * [ne/(get_Si2NoRho()*mp)] * [rho_norm/get_Si2NoRho()] *
      //               get_Si2NoRho() / Si2NoT
      //             = k_si * ne * rho_norm / (get_Si2NoRho() * mp * Si2NoT)
      double lossRho_norm = k_si * ne * rho_ion_norm /
          (get_Si2NoRho() * cProtonMassSI * get_Si2NoT());
      if (lossRho_norm > 0.0) {
        lossArr(i, j, k, iIon) = lossRho_norm;
      }
    }
  }

  //-------------------------------------------------------------------
  // Set nodeFluid from plasma-state-dependent ionization processes.
  void set_source(const FluidInterface& other) override {
    std::string nameFunc = "FS:get_source_from_fluid";
    amrex::Print() << nameFunc << " is called.";

    set_node_fluid(other);
    set_node_loss_fluid_to_zero();

    // Global NODE box.
    const amrex::Box gbx = convert(Geom(0).Domain(), { AMREX_D_DECL(1, 1, 1) });

    const bool doPhoto = usePhotoIonization;
    const bool doImpact = useElectronImpact;
    const bool doCX = useChargeExchange;
    const bool doRecomb = useRecombination;
    const bool doChem = useChemistry;

    for (int iLev = 0; iLev < n_lev(); iLev++) {
      if (!nodeFluid[iLev].empty()) {
        for (amrex::MFIter mfi(nodeFluid[iLev]); mfi.isValid(); ++mfi) {
          const amrex::Real* dx = Geom(iLev).CellSize();
          const auto plo = Geom(iLev).ProbLo();

          // For each block, looping through all nodes, including ghost nodes.
          const amrex::Box& box = mfi.fabbox();
          const auto lo = lbound(box);
          const auto hi = ubound(box);

          const amrex::Array4<amrex::Real>& arr = nodeFluid[iLev][mfi].array();

          // Loss array (allocated when useRecombination or useChemistry
          // is true, which is guaranteed when doRecomb or doChem is true).
          amrex::Array4<amrex::Real> lossArr;
          if (doRecomb || doChem) {
            lossArr = nodeLossFluid[iLev][mfi].array();
          }

          for (int k = lo.z; k <= hi.z; ++k)
            for (int j = lo.y; j <= hi.y; ++j)
              for (int i = lo.x; i <= hi.x; ++i) {
                amrex::IntVect idx = { AMREX_D_DECL(i, j, k) };
                for (int iDim = 0; iDim < nDim; iDim++) {
                  if (Geom(iLev).isPeriodic(iDim)) {
                    idx[iDim] = shift_periodic_index(
                        idx[iDim], gbx.smallEnd(iDim), gbx.bigEnd(iDim));
                  }
                }
                double xyz[3] = { 0, 0, 0 };
                for (int iDim = 0; iDim < get_fluid_dimension(); iDim++) {
                  xyz[iDim] = idx[iDim] * dx[iDim] + plo[iDim];
                }

                double r_val = 0.0;
                for (int d = 0; d < 3; ++d) {
                  r_val += xyz[d] * xyz[d];
                }
                r_val = sqrt(r_val);

                // Pre-fetch plasma state once per cell (lazy evaluation)
                double ne = 0, pe = 0, Te_eV = 0;
                bool plasmaFetched = false;
                auto fetch_electron_plasma = [&, &other=other]() {
                  if (plasmaFetched) return;
                  ne = other.get_number_density(mfi, idx, 0, iLev);
                  pe = other.get_p(mfi, idx, 0, iLev);
                  Te_eV = electron_temperature(pe, ne);
                  plasmaFetched = true;
                };

                // Per-ion-species source accumulators.
                // Species 0 = electron (skipped), 1 = H+, 2 = O+, ...
                // Exosphere component iC maps to ion species (iC + 1).
                // srcRho[iS], srcP[iS] in SI [kg m^-3 s^-1, Pa s^-1].
                // srcRhoU[iS] in SI [kg m^-2 s^-2] (momentum rate).
                // For cross-species CX, product ion inherits reactant
                // velocity; srcRhoU stores the accumulated momentum rate.
                const int nIonS = nS - 1;  // number of ion species
                std::vector<double> srcRho(nIonS + 1, 0.0);
                std::vector<double> srcP(nIonS + 1, 0.0);
                std::vector<double> srcRhoUx(nIonS + 1, 0.0);
                std::vector<double> srcRhoUy(nIonS + 1, 0.0);
                std::vector<double> srcRhoUz(nIonS + 1, 0.0);

                // ---- Accumulate ALL source terms into srcRho/srcP/srcRhoU ----
                // All source processes (exosphere ionization + chemistry)
                // must be accumulated BEFORE writing to nodeFluid, so that
                // the source particle creation sees the complete source.

                // Exosphere-based ionization sources (photoionization,
                // electron impact, charge exchange).
                for (int iC = 0; iC < nExoComponent; ++iC) {
                  const int iSp = iC + 1;
                  if (iSp > nIonS) break;

                  double dens_i =
                      get_exosphere_component_density(r_val, iC);

                  double nu_tot = 0.0;
                  if (doPhoto)
                    nu_tot += photoionization_rate(xyz, iC);
                  if (doImpact) {
                    fetch_electron_plasma();
                    nu_tot += impact_ionization_rate(ne, Te_eV, iC);
                  }
                  if (doCX) {
                    for (int iSpCX = 1; iSpCX <= nIonS; ++iSpCX) {
                      nu_tot += charge_exchange_rate(other, mfi, idx, iLev,
                                                     iSpCX, iC);
                    }
                  }

                  double S_n = dens_i * nu_tot;
                  double mass_amu = get_species_mass(iSp);
                  srcRho[iSp] += S_n * mass_amu * cProtonMassSI;
                  srcP[iSp] +=
                      S_n * mass_amu * cProtonMassSI * cBoltzmannSI *
                      exoT0[iC];
                }

                // General chemistry source terms (#CHEMISTRY).
                // Must be accumulated here (before nodeFluid write) so the
                // source particle creation picks them up.
                if (doChem) {
                  fetch_electron_plasma();
                  for (int iR = 0;
                       iR < static_cast<int>(chemReactions.size()); ++iR) {
                    const auto& rxn = chemReactions[iR];
                    double rate = chem_reaction_rate(rxn, xyz, r_val,
                                                     ne, Te_eV);
                    if (rate <= 0.0) continue;
                    chem_apply_source(rxn, rate, other, mfi, idx, iLev,
                                      nIonS, r_val, srcRho, srcP,
                                      srcRhoUx, srcRhoUy, srcRhoUz);
                  }
                }

                // ---- Write accumulated source terms to nodeFluid ----
                for (int iFluid = 0; iFluid < nFluid; iFluid++) {
                  arr(i, j, k, iRho_I[iFluid]) = 0;
                  arr(i, j, k, iUx_I[iFluid]) = 0;
                  arr(i, j, k, iUy_I[iFluid]) = 0;
                  arr(i, j, k, iUz_I[iFluid]) = 0;
                  arr(i, j, k, iP_I[iFluid]) = 0;
                }

                bool anySource = false;
                for (int iSp = 1; iSp <= nIonS; ++iSp) {
                  if (srcRho[iSp] > 0) {
                    anySource = true;
                    double rho_norm = srcRho[iSp] * get_Si2NoRho() / get_Si2NoT();
                    arr(i, j, k, iRho_I[iSp]) = rho_norm;
                    arr(i, j, k, iUx_I[iSp]) =
                        srcRhoUx[iSp] * get_Si2NoRho() / get_Si2NoT();
                    arr(i, j, k, iUy_I[iSp]) =
                        srcRhoUy[iSp] * get_Si2NoRho() / get_Si2NoT();
                    arr(i, j, k, iUz_I[iSp]) =
                        srcRhoUz[iSp] * get_Si2NoRho() / get_Si2NoT();
                    arr(i, j, k, iP_I[iSp]) =
                        srcP[iSp] * get_Si2NoP() / get_Si2NoT();
                  }
                }
                if (anySource && iPe >= 0) {
                  double srcPe = 0.0;
                  for (int iSp = 1; iSp <= nIonS; ++iSp)
                    srcPe += srcP[iSp];
                  arr(i, j, k, iPe) = srcPe * get_Si2NoP() / get_Si2NoT();
                }

                // ---- Compute ALL loss terms (after nodeFluid write) ----
                // Loss terms write to nodeLossFluid (lossArr), which is
                // separate from nodeFluid.

                // Recombination loss (#RECOMBINATION).
                if (doRecomb) {
                  fetch_electron_plasma();
                  apply_recombination_loss(other, mfi, idx, iLev, nIonS,
                                            ne, Te_eV, i, j, k, lossArr);
                }

                // Chemistry loss terms (#CHEMISTRY).
                if (doChem) {
                  fetch_electron_plasma();
                  for (int iR = 0;
                       iR < static_cast<int>(chemReactions.size()); ++iR) {
                    const auto& rxn = chemReactions[iR];
                    double rate = chem_reaction_rate(rxn, xyz, r_val,
                                                     ne, Te_eV);
                    if (rate <= 0.0) continue;
                    chem_apply_loss(rxn, rate, other, mfi, idx, iLev,
                                    nIonS, i, j, k, lossArr);
                  }
                }
              } // for k
        }
      }
    }

    if (!isGridEmpty && useCurrent) {
      // The current stored in nodeFluid are used to initialize particle
      // velocities. Since the source particles only contribute little to the
      // total current, set current to zero.
      for (int iLev = 0; iLev < n_lev(); iLev++) {
        amrex::MultiFab currentMF(nodeFluid[iLev], amrex::make_alias, iJx, 3);
        currentMF.setVal(0, currentMF.nGrow());
      }
    }
  }
};

#endif
