#pragma once
/**
 * @file spectrum.h
 * @brief cSpectrum definition parsed from AMPS_PARAM.in and used for particle injection.
 *
 * The website/wizard produces several spectrum types. This header wraps the spectrum
 * type and parameters into a single class with a unified API:
 *
 *   double cSpectrum::GetSpectrum(double E_J) const;
 *
 * where E_J is kinetic energy (per nucleon, if applicable) in Joules.
 *
 * Notes on units:
 * - Many user-facing spectra are specified as differential flux per (MeV/n):
 *     dF/dE_MeV  [ (m^2 s sr MeV)^-1 ]   (or similar)
 * - GetSpectrum(E_J) returns the differential flux per Joule:
 *     dF/dE_J = dF/dE_MeV * dE_MeV/dE_J = dF/dE_MeV / MeV_IN_J
 *
 * If your injector only needs relative weights (PDF), the absolute scaling is irrelevant,
 * but the Joule-vs-MeV conversion is still important for consistent integration.
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <map>
#include <utility>
#include <vector>

#include "specfunc.h"

class cSpectrum {
public:
  enum class Type : uint8_t {
    PowerLaw,
    PowerLawCutoff,
    LisForceField,
    Band,
    Table,
    Unknown
  };

  // ---------- Construction ----------
  cSpectrum() = default;

  static cSpectrum MakePowerLaw(double J0_perMeV, double gamma, double E0_MeV,
                               double Emin_MeV, double Emax_MeV) {
    cSpectrum s;
    s.type_ = Type::PowerLaw;
    s.spec_j0_ = J0_perMeV;
    s.spec_gamma_ = gamma;
    s.spec_e0_MeV_ = E0_MeV;
    s.spec_emin_MeV_ = Emin_MeV;
    s.spec_emax_MeV_ = Emax_MeV;
    s.ValidateOrThrow();
    return s;
  }

  static cSpectrum MakePowerLawCutoff(double J0_perMeV, double gamma, double E0_MeV,
                                     double Ec_MeV, double Emin_MeV, double Emax_MeV) {
    cSpectrum s;
    s.type_ = Type::PowerLawCutoff;
    s.spec_j0_ = J0_perMeV;
    s.spec_gamma_ = gamma;
    s.spec_e0_MeV_ = E0_MeV;
    s.spec_ec_MeV_ = Ec_MeV;
    s.spec_emin_MeV_ = Emin_MeV;
    s.spec_emax_MeV_ = Emax_MeV;
    s.ValidateOrThrow();
    return s;
  }

  /**
   * @brief Force-field modulated LIS power law.
   *
   * Parameters are specified in the same convention as the wizard:
   * - LIS normalization/spec index and pivot energy define the unmodulated LIS.
   * - phi_MV is the modulation potential in MV (i.e., MeV for charge=1).
   *
   * Implementation uses a common "force-field approximation" (Gleeson & Axford):
   *   J_mod(E) = J_LIS(E + phi) * (E (E + 2 m)) / ((E+phi) (E+phi + 2 m))
   * where E, m, phi are in MeV (per nucleon) and m is rest mass energy.
   *
   * Default rest mass is proton mass energy (938.2720813 MeV).
   */
  static cSpectrum MakeLisForceField(double LisJ0_perMeV, double LisGamma,
                                    double E0_MeV, double Phi_MV,
                                    double Emin_MeV, double Emax_MeV,
                                    double rest_mass_MeV = 938.2720813) {
    cSpectrum s;
    s.type_ = Type::LisForceField;
    s.spec_lis_j0_ = LisJ0_perMeV;
    s.spec_lis_gamma_ = LisGamma;
    s.spec_e0_MeV_ = E0_MeV;
    s.spec_phi_MV_ = Phi_MV;
    s.rest_mass_MeV_ = rest_mass_MeV;
    s.spec_emin_MeV_ = Emin_MeV;
    s.spec_emax_MeV_ = Emax_MeV;
    s.ValidateOrThrow();
    return s;
  }

  /**
   * @brief Band-like spectrum (piecewise with smooth transition).
   *
   * We follow the standard "Band function" form used in high-energy astrophysics,
   * adapted to your parameter names:
   * - gamma1 : low-energy spectral index (positive -> falling with energy)
   * - gamma2 : high-energy spectral index (positive -> falling with energy)
   * - E0_MeV : characteristic energy (MeV) controlling the rollover
   *
   * In canonical Band notation, indices are often (alpha, beta) where N(E) ~ E^alpha
   * at low E; here we assume user enters positive slopes so we use -gamma.
   *
   * Break energy: E_break = (gamma2 - gamma1) * E0.
   */
  static cSpectrum MakeBand(double J0_perMeV, double gamma1, double gamma2,
                           double E0_MeV, double Emin_MeV, double Emax_MeV) {
    cSpectrum s;
    s.type_ = Type::Band;
    s.spec_j0_ = J0_perMeV;
    s.spec_gamma1_ = gamma1;
    s.spec_gamma2_ = gamma2;
    s.spec_e0_MeV_ = E0_MeV;
    s.spec_emin_MeV_ = Emin_MeV;
    s.spec_emax_MeV_ = Emax_MeV;
    s.ValidateOrThrow();
    return s;
  }

  /**
   * @brief Table spectrum loaded from either a legacy 2-column file or a
   *        time-dependent file with one timestamp row and many energy columns.
   *
   * For time-dependent files, reference_epoch_utc determines which time row is
   * selected. The nearest row in time is used. If reference_epoch_utc is empty
   * or cannot be parsed, the first valid time row is used.
   */
  static cSpectrum MakeTable(std::string table_file,
                            double Emin_MeV, double Emax_MeV,
                            std::string reference_epoch_utc = std::string()) {
    cSpectrum s;
    s.type_ = Type::Table;
    s.table_file_ = std::move(table_file);
    s.table_reference_epoch_utc_ = std::move(reference_epoch_utc);
    s.spec_emin_MeV_ = Emin_MeV;
    s.spec_emax_MeV_ = Emax_MeV;
    s.LoadTableOrThrow();
    s.ValidateOrThrow();
    return s;
  }

  // ---------- Parsing helpers ----------
  /**
   * @brief Build a cSpectrum from key/value pairs (strings), e.g. parsed from AMPS_PARAM.in.
   *
   * Expected keys (case-insensitive):
   *  - SPECTRUM_TYPE
   *  - SPEC_EMIN, SPEC_EMAX
   *  - POWER_LAW: SPEC_J0, SPEC_GAMMA, SPEC_E0
   *  - POWER_LAW_CUTOFF: + SPEC_EC
   *  - LIS_FORCE_FIELD: SPEC_LIS_J0, SPEC_LIS_GAMMA, SPEC_E0, SPEC_PHI
   *  - BAND: SPEC_J0, SPEC_GAMMA1, SPEC_GAMMA2, SPEC_E0
   *  - TABLE: SPEC_TABLE_FILE
   */
  static cSpectrum FromKeyValueMap(const std::unordered_map<std::string, std::string>& kv) {
    const auto get = [&](const std::string& k) -> std::string {
      auto it = kv.find(k);
      if (it != kv.end()) return it->second;
      // try case-insensitive lookup
      std::string kl = ToUpper(k);
      for (auto it2 = kv.begin(); it2 != kv.end(); ++it2) {
        if (ToUpper(it2->first) == kl) return it2->second;
      }
      return {};
    };

    const std::string type_s = ToUpper(Trim(get("SPECTRUM_TYPE")));
    const double Emin = ParseDoubleOrThrow(get("SPEC_EMIN"), "SPEC_EMIN");
    const double Emax = ParseDoubleOrThrow(get("SPEC_EMAX"), "SPEC_EMAX");

    if (type_s == "POWER_LAW") {
      return MakePowerLaw(
          ParseDoubleOrThrow(get("SPEC_J0"), "SPEC_J0"),
          ParseDoubleOrThrow(get("SPEC_GAMMA"), "SPEC_GAMMA"),
          ParseDoubleOrThrow(get("SPEC_E0"), "SPEC_E0"),
          Emin, Emax);
    }

    if (type_s == "POWER_LAW_CUTOFF") {
      return MakePowerLawCutoff(
          ParseDoubleOrThrow(get("SPEC_J0"), "SPEC_J0"),
          ParseDoubleOrThrow(get("SPEC_GAMMA"), "SPEC_GAMMA"),
          ParseDoubleOrThrow(get("SPEC_E0"), "SPEC_E0"),
          ParseDoubleOrThrow(get("SPEC_EC"), "SPEC_EC"),
          Emin, Emax);
    }

    if (type_s == "LIS_FORCE_FIELD") {
      return MakeLisForceField(
          ParseDoubleOrThrow(get("SPEC_LIS_J0"), "SPEC_LIS_J0"),
          ParseDoubleOrThrow(get("SPEC_LIS_GAMMA"), "SPEC_LIS_GAMMA"),
          ParseDoubleOrThrow(get("SPEC_E0"), "SPEC_E0"),
          ParseDoubleOrThrow(get("SPEC_PHI"), "SPEC_PHI"),
          Emin, Emax);
    }

    if (type_s == "BAND") {
      return MakeBand(
          ParseDoubleOrThrow(get("SPEC_J0"), "SPEC_J0"),
          ParseDoubleOrThrow(get("SPEC_GAMMA1"), "SPEC_GAMMA1"),
          ParseDoubleOrThrow(get("SPEC_GAMMA2"), "SPEC_GAMMA2"),
          ParseDoubleOrThrow(get("SPEC_E0"), "SPEC_E0"),
          Emin, Emax);
    }

    if (type_s == "TABLE") {
      std::string f = Trim(get("SPEC_TABLE_FILE"));
      if (f.empty()) exit(__LINE__,__FILE__,"Missing required key SPEC_TABLE_FILE for SPECTRUM_TYPE=TABLE");
      const std::string refEpochUTC = Trim(get("SPEC_TABLE_REFERENCE_EPOCH_UTC"));
      return MakeTable(f, Emin, Emax, refEpochUTC);
    }

    std::ostringstream oss;
    oss << "Unrecognized SPECTRUM_TYPE='" << type_s
        << "'. Supported types: POWER_LAW, POWER_LAW_CUTOFF, LIS_FORCE_FIELD, BAND, TABLE";
    exit(__LINE__,__FILE__,oss.str().c_str()); 

    cSpectrum t;
    return t; //the code should not make to this point -- it is hear just to make compiler happy
  }

  /**
   * @brief Build a cSpectrum from a parsed key/value map (std::map variant).
   *
   * Many existing AMPS utilities store parsed sections as std::map for deterministic ordering.
   * This overload avoids forcing the caller (e.g., the parser) to convert containers.
   */
  static cSpectrum FromKeyValueMap(const std::map<std::string, std::string>& kv) {
    // Convert to an unordered_map and reuse the primary implementation.
    std::unordered_map<std::string, std::string> u;
    u.reserve(kv.size());
    for (const auto& it : kv) u.emplace(it.first, it.second);
    return FromKeyValueMap(u);
  }

  // ---------- Accessors ----------
  Type GetType() const noexcept { return type_; }
  const std::string& GetTableFile() const noexcept { return table_file_; }
  bool IsTimeDependentTable() const noexcept { return table_is_time_dependent_; }
  std::size_t TableSnapshotCount() const noexcept { return table_snapshots_.size(); }
  const std::string& ActiveTableEpochUTC() const noexcept { return selected_table_epoch_utc_; }

  void SetEvaluationUnixSeconds(double unixTime) {
    if (!table_is_time_dependent_ || table_snapshots_.empty() || !std::isfinite(unixTime)) return;
    ActivateTimeDependentSnapshotByIndex_(FindNearestTimeDependentSnapshotIndex_(unixTime));
  }

  void SetEvaluationEpochUTC(const std::string& epochUTC) {
    if (!table_is_time_dependent_ || table_snapshots_.empty()) return;
    double unixTime = 0.0;
    if (!ParseIsoUtcToUnixSeconds_(epochUTC, unixTime)) return;
    SetEvaluationUnixSeconds(unixTime);
  }

  /**
   * @brief Direct access to table points (only meaningful for Type::Table).
   *
   * Exposed for diagnostics and Tecplot writers that must preserve the exact
   * user-provided table values.
   */
  const std::vector<double>& TableEnergy_MeV() const noexcept { return table_E_MeV_; }
  const std::vector<double>& TableFlux_perMeV() const noexcept { return table_J_perMeV_; }

  double Emin_MeV() const noexcept { return spec_emin_MeV_; }
  double Emax_MeV() const noexcept { return spec_emax_MeV_; }

  // ---------- Main API ----------
  /**
   * @brief Differential spectrum at kinetic energy E_J (Joules).
   * @return dF/dE_J  (same physical flux as user inputs, but per Joule).
   */
  double GetSpectrum(double E_J) const {
    if (!(E_J > 0.0)) return 0.0;

    double E_MeV = E_J / MeV_IN_J;

    // Energy-grid values used by the density/flux integrators are commonly
    // constructed in MeV, converted to Joules, and then converted back to MeV
    // here.  A mathematically exact endpoint such as 0.99 or 99 MeV can move a
    // few floating-point ulps below SPEC_EMIN during that round trip.  A strict
    // comparison would then classify the first integration node as outside the
    // spectrum and incorrectly remove half of the first trapezoid.  Accept and
    // clamp values that differ from either configured bound only by roundoff;
    // values farther outside the support remain zero exactly as before.
    if (!ClampEnergyToConfiguredSupport_(E_MeV)) return 0.0;

    const double val_perMeV = GetSpectrumPerMeV_(E_MeV);
    if (!(val_perMeV > 0.0)) return 0.0;

    return val_perMeV / MeV_IN_J;
  }

  /**
   * @brief Convenience: spectrum per MeV at E_MeV.
   */
  double GetSpectrumPerMeV(double E_MeV) const {
    if (!(E_MeV > 0.0)) return 0.0;

    // Apply the same endpoint policy as GetSpectrum().  Keeping both public
    // entry points consistent prevents a caller using MeV directly from seeing
    // different support behavior than a caller using Joules.
    if (!ClampEnergyToConfiguredSupport_(E_MeV)) return 0.0;

    return GetSpectrumPerMeV_(E_MeV);
  }

  // ---------- Utilities ----------
  static constexpr double EV_IN_J = 1.602176634e-19;
  static constexpr double MeV_IN_J = 1.0e6 * EV_IN_J;

  static std::string TypeToString(Type t) {
    switch (t) {
      case Type::PowerLaw: return "POWER_LAW";
      case Type::PowerLawCutoff: return "POWER_LAW_CUTOFF";
      case Type::LisForceField: return "LIS_FORCE_FIELD";
      case Type::Band: return "BAND";
      case Type::Table: return "TABLE";
      default: return "UNKNOWN";
    }
  }

private:
  /**
   * @brief Accept an energy inside the configured spectrum support, allowing
   *        only a floating-point roundoff-sized excursion at either endpoint.
   *
   * Spectrum energies frequently follow the path
   *
   *   MeV -> Joules -> MeV.
   *
   * The result can differ from the original endpoint by one or several ulps.
   * Such a value is physically the endpoint and must not be rejected.  The
   * tolerance is relative to the largest relevant energy scale and is kept at
   * only 32 machine epsilons, so it cannot mask a physically meaningful value
   * outside [SPEC_EMIN,SPEC_EMAX].  Accepted near-endpoint values are clamped
   * to the exact configured bound before table interpolation or analytic
   * spectrum evaluation.
   *
   * @param[in,out] E_MeV Energy in MeV.  It is clamped to a configured bound
   *                     only when it lies within the roundoff tolerance.
   * @return true when the energy belongs to the support; false when it is
   *         genuinely outside the configured interval or is non-finite.
   */
  bool ClampEnergyToConfiguredSupport_(double& E_MeV) const noexcept {
    if (!std::isfinite(E_MeV)) return false;

    const double scale = std::max({
        1.0,
        std::fabs(E_MeV),
        std::fabs(spec_emin_MeV_),
        std::fabs(spec_emax_MeV_)
    });

    const double tolerance =
        32.0 * std::numeric_limits<double>::epsilon() * scale;

    if (E_MeV < spec_emin_MeV_ - tolerance ||
        E_MeV > spec_emax_MeV_ + tolerance) {
      return false;
    }

    if (E_MeV < spec_emin_MeV_) E_MeV = spec_emin_MeV_;
    if (E_MeV > spec_emax_MeV_) E_MeV = spec_emax_MeV_;

    return true;
  }

  // ---------- Internal evaluation ----------
  double GetSpectrumPerMeV_(double E_MeV) const {
    switch (type_) {
      case Type::PowerLaw: {
        // J(E) = J0 * (E/E0)^(-gamma)
        const double x = E_MeV / spec_e0_MeV_;
        return spec_j0_ * std::pow(x, -spec_gamma_);
      }
      case Type::PowerLawCutoff: {
        // J(E) = J0 * (E/E0)^(-gamma) * exp(-E/Ec)
        const double x = E_MeV / spec_e0_MeV_;
        return spec_j0_ * std::pow(x, -spec_gamma_) * std::exp(-E_MeV / spec_ec_MeV_);
      }
      case Type::LisForceField: {
        // Force-field modulation. Treat Phi_MV as MeV for Z=1. For other charge states, caller can
        // scale Phi before construction.
        const double phi_MeV = spec_phi_MV_;
        const double E_LIS = E_MeV + phi_MeV;

        const double x = E_LIS / spec_e0_MeV_;
        const double J_LIS = spec_lis_j0_ * std::pow(x, -spec_lis_gamma_);

        const double m = rest_mass_MeV_;
        const double num = E_MeV * (E_MeV + 2.0 * m);
        const double den = E_LIS * (E_LIS + 2.0 * m);
        if (!(den > 0.0)) return 0.0;

        return J_LIS * (num / den);
      }
      case Type::Band: {
        // Standard Band-like form with indices -gamma1, -gamma2 and rollover E0.
        // Define alpha=-gamma1, beta=-gamma2.
        const double alpha = -spec_gamma1_;
        const double beta  = -spec_gamma2_;
        const double E0 = spec_e0_MeV_;

        const double Eb = (alpha - beta) * E0; // = (gamma2-gamma1)*E0
        if (!(Eb > 0.0) || !(E0 > 0.0)) return 0.0;

        if (E_MeV <= Eb) {
          // A * (E/E0)^alpha * exp(-E/E0)
          const double x = E_MeV / E0;
          return spec_j0_ * std::pow(x, alpha) * std::exp(-x);
        } else {
          // A * (Eb/E0)^(alpha-beta) * exp(beta-alpha) * (E/E0)^beta
          const double pref = std::pow(Eb / E0, (alpha - beta)) * std::exp(beta - alpha);
          const double x = E_MeV / E0;
          return spec_j0_ * pref * std::pow(x, beta);
        }
      }
      case Type::Table: {
        return InterpLogLog_(E_MeV);
      }
      default:
        return 0.0;
    }
  }

  // ---------- Validation / loading ----------
  void ValidateOrThrow() const {
    if (type_ == Type::Unknown) {
      exit(__LINE__,__FILE__,"cSpectrum has Unknown type");
    }
    if (!(spec_emin_MeV_ > 0.0) || !(spec_emax_MeV_ > spec_emin_MeV_)) {
      exit(__LINE__,__FILE__,"cSpectrum invalid energy bounds: require 0 < SPEC_EMIN < SPEC_EMAX (MeV)");
    }

    switch (type_) {
      case Type::PowerLaw:
        RequirePositive_("SPEC_J0", spec_j0_);
        RequirePositive_("SPEC_E0", spec_e0_MeV_);
        RequireFinite_("SPEC_GAMMA", spec_gamma_);
        break;
      case Type::PowerLawCutoff:
        RequirePositive_("SPEC_J0", spec_j0_);
        RequirePositive_("SPEC_E0", spec_e0_MeV_);
        RequirePositive_("SPEC_EC", spec_ec_MeV_);
        RequireFinite_("SPEC_GAMMA", spec_gamma_);
        break;
      case Type::LisForceField:
        RequirePositive_("SPEC_LIS_J0", spec_lis_j0_);
        RequirePositive_("SPEC_E0", spec_e0_MeV_);
        RequirePositive_("REST_MASS_MEV", rest_mass_MeV_);
        RequireFinite_("SPEC_LIS_GAMMA", spec_lis_gamma_);
        RequireFinite_("SPEC_PHI", spec_phi_MV_);
        if (spec_phi_MV_ < 0.0) exit(__LINE__,__FILE__,"SPEC_PHI must be >= 0");
        break;
      case Type::Band:
        RequirePositive_("SPEC_J0", spec_j0_);
        RequirePositive_("SPEC_E0", spec_e0_MeV_);
        RequireFinite_("SPEC_GAMMA1", spec_gamma1_);
        RequireFinite_("SPEC_GAMMA2", spec_gamma2_);
        if ((spec_gamma2_ - spec_gamma1_) <= 0.0) {
          exit(__LINE__,__FILE__,"BAND requires SPEC_GAMMA2 > SPEC_GAMMA1 (so break energy is positive)");
        }
        break;
      case Type::Table:
        if (table_E_MeV_.size() < 2) exit(__LINE__,__FILE__,"TABLE spectrum requires >= 2 data points");
        break;
      default:
        break;
    }
  }

  enum class TableFileFormat : uint8_t { Unknown, TwoColumn, TimeDependent };

  static bool ParseDoubleNoThrow_(const std::string& s, double& x) {
    const std::string t = Trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    x = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || !std::isfinite(x)) return false;
    while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    return end && *end == '\0';
  }

  static std::vector<std::string> SplitWhitespace_(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
  }

  static std::string StripInlineComment_(const std::string& line) {
    std::size_t cut = std::string::npos;
    for (char c : {'#','!'}) {
      const std::size_t p = line.find(c);
      if (p != std::string::npos && (cut == std::string::npos || p < cut)) cut = p;
    }
    return (cut == std::string::npos) ? Trim(line) : Trim(line.substr(0, cut));
  }

  static bool LooksLikeIsoUtcToken_(const std::string& s) {
    return (s.find('T') != std::string::npos) && (s.find('-') != std::string::npos || s.find('/') != std::string::npos);
  }

  static bool ParseIsoUtcToUnixSeconds_(const std::string& s, double& unixOut) {
    std::tm tm = {};
    std::istringstream iss(s);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (iss.fail()) {
      std::istringstream iss2(s);
      iss2 >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
      if (iss2.fail()) return false;
    }
#if defined(_WIN32)
    std::time_t tt = _mkgmtime(&tm);
#else
    std::time_t tt = timegm(&tm);
#endif
    if (tt == static_cast<std::time_t>(-1)) return false;
    unixOut = static_cast<double>(tt);
    return true;
  }

  static bool TryParseEnergyGridHeader_(const std::string& rawLine, std::vector<double>& energyGridOut) {
    std::string s = Trim(rawLine);
    if (s.empty()) return false;
    if (s[0]=='#' || s[0]=='!') s = Trim(s.substr(1));
    const std::string u = ToUpper(s);
    const std::string prefix = "ENERGY_MEV:";
    if (u.rfind(prefix, 0) != 0) return false;

    const std::string values = Trim(s.substr(prefix.size()));
    const auto toks = SplitWhitespace_(values);
    if (toks.empty()) exit(__LINE__,__FILE__,"Spectrum time-dependent file has ENERGY_MEV header with no values");

    energyGridOut.clear();
    energyGridOut.reserve(toks.size());
    for (const auto& tok : toks) {
      double e = 0.0;
      if (!ParseDoubleNoThrow_(tok, e) || !(e > 0.0)) {
        std::ostringstream oss;
        oss << "Invalid energy value '" << tok << "' in ENERGY_MEV header of '" << rawLine << "'";
        exit(__LINE__,__FILE__,oss.str().c_str());
      }
      energyGridOut.push_back(e);
    }
    return true;
  }

  TableFileFormat DetectTableFileFormat_() const {
    std::ifstream in(table_file_);
    if (!in) {
      std::ostringstream oss;
      oss << "Failed to open SPEC_TABLE_FILE='" << table_file_ << "'";
      exit(__LINE__,__FILE__,oss.str().c_str());
    }

    bool sawEnergyGrid = false;
    std::string line;
    while (std::getline(in, line)) {
      const std::string trimmed = Trim(line);
      if (trimmed.empty()) continue;

      std::vector<double> dummy;
      if (TryParseEnergyGridHeader_(trimmed, dummy)) {
        sawEnergyGrid = true;
        continue;
      }

      if (trimmed[0]=='#' || trimmed[0]=='!') continue;

      const std::string data = StripInlineComment_(trimmed);
      if (data.empty()) continue;
      const auto toks = SplitWhitespace_(data);
      if (toks.empty()) continue;

      if (toks.size()==2) {
        double e=0.0, j=0.0;
        if (ParseDoubleNoThrow_(toks[0], e) && ParseDoubleNoThrow_(toks[1], j)) {
          return TableFileFormat::TwoColumn;
        }
      }

      if (toks.size()>=3 && LooksLikeIsoUtcToken_(toks[0])) return TableFileFormat::TimeDependent;
      if (sawEnergyGrid && toks.size()>=3) return TableFileFormat::TimeDependent;
    }

    return TableFileFormat::Unknown;
  }

  static void FinalizeTableVectorsOrThrow_(std::vector<double>& E_MeV,
                                          std::vector<double>& J_perMeV,
                                          const std::string& sourceLabel) {
    if (E_MeV.size() < 2) {
      std::ostringstream oss;
      oss << "TABLE data '" << sourceLabel << "' did not contain >=2 valid (positive) rows";
      exit(__LINE__,__FILE__,oss.str().c_str());
    }

    std::vector<size_t> idx(E_MeV.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b){ return E_MeV[a] < E_MeV[b]; });

    std::vector<double> E2, J2;
    E2.reserve(idx.size()); J2.reserve(idx.size());
    for (size_t k : idx) { E2.push_back(E_MeV[k]); J2.push_back(J_perMeV[k]); }
    E_MeV.swap(E2);
    J_perMeV.swap(J2);

    std::vector<double> E3, J3;
    for (size_t i = 0; i < E_MeV.size(); ++i) {
      if (!E3.empty() && std::fabs(E_MeV[i] - E3.back()) <= 0.0) {
        J3.back() = J_perMeV[i];
      } else {
        E3.push_back(E_MeV[i]);
        J3.push_back(J_perMeV[i]);
      }
    }
    E_MeV.swap(E3);
    J_perMeV.swap(J3);
  }

  void FinalizeLoadedTableOrThrow_() {
    FinalizeTableVectorsOrThrow_(table_E_MeV_, table_J_perMeV_, table_file_);
  }

  void ActivateTimeDependentSnapshotByIndex_(std::size_t idx) {
    if (table_snapshots_.empty()) {
      table_E_MeV_.clear();
      table_J_perMeV_.clear();
      selected_table_epoch_utc_.clear();
      active_snapshot_index_ = -1;
      return;
    }
    if (idx >= table_snapshots_.size()) idx = table_snapshots_.size() - 1;
    active_snapshot_index_ = static_cast<int>(idx);
    table_E_MeV_ = table_snapshots_[idx].E_MeV;
    table_J_perMeV_ = table_snapshots_[idx].J_perMeV;
    selected_table_epoch_utc_ = table_snapshots_[idx].epochUTC;
  }

  std::size_t FindNearestTimeDependentSnapshotIndex_(double unixTime) const {
    if (table_snapshots_.empty()) return 0;
    std::size_t best = 0;
    double bestAbsDt = std::fabs(table_snapshots_[0].unixTime_s - unixTime);
    for (std::size_t i = 1; i < table_snapshots_.size(); ++i) {
      const double dt = std::fabs(table_snapshots_[i].unixTime_s - unixTime);
      if (dt < bestAbsDt) {
        bestAbsDt = dt;
        best = i;
      }
    }
    return best;
  }

  void LoadTwoColumnTableOrThrow_() {
    table_is_time_dependent_ = false;
    table_snapshots_.clear();
    active_snapshot_index_ = -1;

    std::ifstream in(table_file_);
    if (!in) {
      std::ostringstream oss;
      oss << "Failed to open SPEC_TABLE_FILE='" << table_file_ << "'";
      exit(__LINE__,__FILE__,oss.str().c_str());
    }

    std::string line;
    size_t lineno = 0;
    while (std::getline(in, line)) {
      ++lineno;
      line = Trim(line);
      if (line.empty() || line[0] == '#' || line[0] == '!') continue;

      const std::string data = StripInlineComment_(line);
      if (data.empty()) continue;

      std::istringstream iss(data);
      double e = 0.0, j = 0.0;
      if (!(iss >> e >> j)) {
        std::ostringstream oss;
        oss << "Bad TABLE line " << lineno << " in '" << table_file_ << "': expected two columns (E_MeV flux_perMeV)";
        exit(__LINE__,__FILE__,oss.str().c_str());
      }
      if (!(e > 0.0) || !(j > 0.0)) continue;
      table_E_MeV_.push_back(e);
      table_J_perMeV_.push_back(j);
    }

    FinalizeLoadedTableOrThrow_();
  }

  void LoadTimeDependentTableOrThrow_() {
    std::ifstream in(table_file_);
    if (!in) {
      std::ostringstream oss;
      oss << "Failed to open SPEC_TABLE_FILE='" << table_file_ << "'";
      exit(__LINE__,__FILE__,oss.str().c_str());
    }

    table_is_time_dependent_ = true;
    table_snapshots_.clear();
    active_snapshot_index_ = -1;

    bool haveRef = false;
    double refTime = 0.0;
    if (!table_reference_epoch_utc_.empty()) haveRef = ParseIsoUtcToUnixSeconds_(table_reference_epoch_utc_, refTime);

    std::vector<double> energyGrid;
    bool haveGrid = false;

    std::string line;
    size_t lineno = 0;
    while (std::getline(in, line)) {
      ++lineno;
      const std::string trimmed = Trim(line);
      if (trimmed.empty()) continue;

      std::vector<double> headerGrid;
      if (TryParseEnergyGridHeader_(trimmed, headerGrid)) {
        energyGrid = headerGrid;
        haveGrid = true;
        continue;
      }

      if (trimmed[0]=='#' || trimmed[0]=='!') continue;

      const std::string data = StripInlineComment_(trimmed);
      if (data.empty()) continue;
      const auto toks = SplitWhitespace_(data);
      if (toks.size() < 3 || !LooksLikeIsoUtcToken_(toks[0])) continue;

      if (!haveGrid) {
        std::ostringstream oss;
        oss << "Time-dependent TABLE file '" << table_file_ << "' contains data rows before ENERGY_MEV header";
        exit(__LINE__,__FILE__,oss.str().c_str());
      }
      if (toks.size() != energyGrid.size() + 1) {
        std::ostringstream oss;
        oss << "Time-dependent TABLE line " << lineno << " in '" << table_file_ << "' has "
            << (toks.size()-1) << " flux columns, but ENERGY_MEV defines " << energyGrid.size() << " energies";
        exit(__LINE__,__FILE__,oss.str().c_str());
      }

      double rowTime = 0.0;
      if (!ParseIsoUtcToUnixSeconds_(toks[0], rowTime)) {
        std::ostringstream oss;
        oss << "Invalid UTC timestamp '" << toks[0] << "' in time-dependent TABLE line "
            << lineno << " of '" << table_file_ << "'";
        exit(__LINE__,__FILE__,oss.str().c_str());
      }

      std::vector<double> rowE;
      std::vector<double> rowJ;
      rowE.reserve(energyGrid.size());
      rowJ.reserve(energyGrid.size());
      for (std::size_t i=1; i<toks.size(); ++i) {
        double v = 0.0;
        if (!ParseDoubleNoThrow_(toks[i], v)) {
          std::ostringstream oss;
          oss << "Invalid flux value '" << toks[i] << "' in time-dependent TABLE line " << lineno
              << " of '" << table_file_ << "'";
          exit(__LINE__,__FILE__,oss.str().c_str());
        }
        if (!(v > 0.0) || !std::isfinite(v)) continue;
        rowE.push_back(energyGrid[i-1]);
        rowJ.push_back(v);
      }

      if (rowE.size() < 2) continue;

      {
        std::ostringstream label;
        label << table_file_ << " @ " << toks[0];
        FinalizeTableVectorsOrThrow_(rowE, rowJ, label.str());
      }

      TableSnapshot snap;
      snap.unixTime_s = rowTime;
      snap.epochUTC = toks[0];
      snap.E_MeV.swap(rowE);
      snap.J_perMeV.swap(rowJ);
      table_snapshots_.push_back(std::move(snap));
    }

    if (!haveGrid) {
      std::ostringstream oss;
      oss << "Time-dependent TABLE file '" << table_file_ << "' does not contain ENERGY_MEV header";
      exit(__LINE__,__FILE__,oss.str().c_str());
    }
    if (table_snapshots_.empty()) {
      std::ostringstream oss;
      oss << "Time-dependent TABLE file '" << table_file_
          << "' does not contain any time snapshots with at least two valid positive bins";
      exit(__LINE__,__FILE__,oss.str().c_str());
    }

    std::sort(table_snapshots_.begin(), table_snapshots_.end(),
              [](const TableSnapshot& a, const TableSnapshot& b) {
                return a.unixTime_s < b.unixTime_s;
              });

    const std::size_t i0 = haveRef ? FindNearestTimeDependentSnapshotIndex_(refTime) : std::size_t(0);
    ActivateTimeDependentSnapshotByIndex_(i0);
  }

  void LoadTableOrThrow() {
    table_E_MeV_.clear();
    table_J_perMeV_.clear();
    selected_table_epoch_utc_.clear();
    table_snapshots_.clear();
    table_is_time_dependent_ = false;
    active_snapshot_index_ = -1;

    const TableFileFormat fmt = DetectTableFileFormat_();
    switch (fmt) {
      case TableFileFormat::TwoColumn:
        LoadTwoColumnTableOrThrow_();
        return;
      case TableFileFormat::TimeDependent:
        LoadTimeDependentTableOrThrow_();
        return;
      case TableFileFormat::Unknown:
      default: {
        std::ostringstream oss;
        oss << "Could not determine spectrum TABLE format for file '" << table_file_ << "'";
        exit(__LINE__,__FILE__,oss.str().c_str());
      }
    }
  }

  double InterpLogLog_(double E_MeV) const {
    if (table_E_MeV_.empty()) return 0.0;
    if (E_MeV <= table_E_MeV_.front()) return table_J_perMeV_.front();
    if (E_MeV >= table_E_MeV_.back())  return table_J_perMeV_.back();

    auto it = std::upper_bound(table_E_MeV_.begin(), table_E_MeV_.end(), E_MeV);
    const size_t i1 = size_t(it - table_E_MeV_.begin());
    const size_t i0 = i1 - 1;

    const double x0 = table_E_MeV_[i0];
    const double x1 = table_E_MeV_[i1];
    const double y0 = table_J_perMeV_[i0];
    const double y1 = table_J_perMeV_[i1];

    if (!(x1 > x0) || !(y0 > 0.0) || !(y1 > 0.0)) return 0.0;

    const double lx0 = std::log(x0), lx1 = std::log(x1), lx = std::log(E_MeV);
    const double ly0 = std::log(y0), ly1 = std::log(y1);

    const double t = (lx - lx0) / (lx1 - lx0);
    const double ly = (1.0 - t) * ly0 + t * ly1;
    return std::exp(ly);
  }

  // ---------- String/parse helpers ----------
  static std::string Trim(std::string s) {
    auto issp = [](unsigned char c){ return std::isspace(c) != 0; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    return s;
  }

  static std::string ToUpper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
  }

  static double ParseDoubleOrThrow(const std::string& v, const char* key) {
    const std::string s = Trim(v);
    if (s.empty()) {
      std::ostringstream oss;
      oss << "Missing required key " << key;
      exit(__LINE__,__FILE__,oss.str().c_str()); 
    }
    char* end = nullptr;
    const double x = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || !std::isfinite(x)) {
      std::ostringstream oss;
      oss << "Invalid numeric value for " << key << ": '" << s << "'";
      exit(__LINE__,__FILE__,oss.str().c_str()); 
    }
    return x;
  }

  static void RequirePositive_(const char* key, double v) {
    if (!(v > 0.0) || !std::isfinite(v)) {
      std::ostringstream oss;
      oss << key << " must be > 0";
      exit(__LINE__,__FILE__,oss.str().c_str()); 
    }
  }

  static void RequireFinite_(const char* key, double v) {
    if (!std::isfinite(v)) {
      std::ostringstream oss;
      oss << key << " must be finite";
      exit(__LINE__,__FILE__,oss.str().c_str()); 
    }
  }

private:
  // ---------- Stored definition ----------
  Type type_ = Type::Unknown;

  // Bounds (MeV/n)
  double spec_emin_MeV_ = 0.0;
  double spec_emax_MeV_ = 0.0;

  // Common / power law style
  double spec_j0_ = 0.0;        // per MeV
  double spec_gamma_ = 0.0;
  double spec_e0_MeV_ = 0.0;

  // Cutoff
  double spec_ec_MeV_ = 0.0;

  // LIS force-field
  double spec_lis_j0_ = 0.0;    // per MeV

double spec_lis_gamma_ = 0.0;
  double spec_phi_MV_ = 0.0;    // MV ~ MeV for Z=1
  double rest_mass_MeV_ = 938.2720813;

  // Band
  double spec_gamma1_ = 0.0;
  double spec_gamma2_ = 0.0;

  struct TableSnapshot {
    double unixTime_s = 0.0;
    std::string epochUTC;
    std::vector<double> E_MeV;
    std::vector<double> J_perMeV;
  };

  // Table
  std::string table_file_;
  std::string table_reference_epoch_utc_;
  std::string selected_table_epoch_utc_;
  bool table_is_time_dependent_ = false;
  int active_snapshot_index_ = -1;
  std::vector<TableSnapshot> table_snapshots_;
  std::vector<double> table_E_MeV_;
  std::vector<double> table_J_perMeV_;
};

//------------------------------------------------------------------------------
// Global spectrum instance
//------------------------------------------------------------------------------
/**
 * @brief Global spectrum instance initialized by the AMPS_PARAM.in parser.
 *
 * Why global?
 *  - Injection routines are often called from multiple places and kept lightweight; passing
 *    a spectrum object through many call chains can be intrusive in a large legacy codebase.
 *  - Keeping a single, validated instance ensures consistent spectrum usage in injection,
 *    diagnostics, and Tecplot outputs.
 *
 * IMPORTANT:
 *  - This object is defined in spectrum.cpp.
 *  - It must be initialized exactly once after parsing #SPECTRUM, before any injection uses it.
 */
extern cSpectrum gSpectrum;

//------------------------------------------------------------------------------
// Global initializer
//------------------------------------------------------------------------------
/**
 * @brief Initialize the global spectrum from a parsed key/value map (unordered_map variant).
 * Throws std::runtime_error on unknown type or missing/invalid parameters.
 */
void InitGlobalSpectrumFromKeyValueMap(
    const std::unordered_map<std::string, std::string>& kv);

/**
 * @brief Initialize the global spectrum from a parsed key/value map (std::map variant).
 * Convenience overload for parsers that store sections as std::map.
 */
void InitGlobalSpectrumFromKeyValueMap(
    const std::map<std::string, std::string>& kv);

//------------------------------------------------------------------------------
// Tecplot writer for spectrum input
//------------------------------------------------------------------------------
/**
 * @brief Write a Tecplot ASCII file with the spectrum that was parsed from the input.
 *
 * Requirements implemented:
 *  1) Energy is written in MeV.
 *  2) If the spectrum is TABLE, the original table points are written.
 *  3) Otherwise (analytic), energies are log-spaced ("exponential splitting") between
 *     SPEC_EMIN and SPEC_EMAX using @p n_analytic points.
 *
 * The output is meant as a faithful record of what was parsed and what will be used
 * by injection/boundary-condition code.
 */
void WriteSpectrumInputTecplot(const std::string& filename,
                               const cSpectrum& s,
                               int n_analytic = 200);



