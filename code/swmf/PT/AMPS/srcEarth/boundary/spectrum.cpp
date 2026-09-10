#include "spectrum.h"

/**
 * @file spectrum.cpp
 * @brief Definition of the global spectrum object and initialization helpers.
 *
 * The heavy lifting (parsing/validation/evaluation) is implemented in cSpectrum
 * (header-only for now). This translation unit only provides:
 *   - the single global instance gSpectrum
 *   - initialization helpers used by the AMPS_PARAM parser
 *
 * Keeping gSpectrum in a .cpp avoids multiple-definition link errors.
 */

// Global spectrum instance (default-constructed as Type::Unknown).
cSpectrum gSpectrum;

void InitGlobalSpectrumFromKeyValueMap(
    const std::unordered_map<std::string, std::string>& kv) {
  gSpectrum = cSpectrum::FromKeyValueMap(kv);
}

void InitGlobalSpectrumFromKeyValueMap(
    const std::map<std::string, std::string>& kv) {
  // Convert to unordered_map for reuse (order is irrelevant).
  std::unordered_map<std::string, std::string> u;
  for (std::map<std::string, std::string>::const_iterator it = kv.begin(); it != kv.end(); ++it) {
    u[it->first] = it->second;
  }
  InitGlobalSpectrumFromKeyValueMap(u);
}

//------------------------------------------------------------------------------
// Tecplot spectrum input writer
//------------------------------------------------------------------------------
void WriteSpectrumInputTecplot(const std::string& filename,
                               const cSpectrum& s,
                               int n_analytic) {
  if (n_analytic < 2) n_analytic = 2;

  std::ofstream out(filename.c_str());
  if (!out) {
    throw std::runtime_error("Failed to open Tecplot output file '" + filename + "' for writing");
  }

  // Header
  out << "TITLE=\"Spectrum parsed from input\"\n";
  out << "VARIABLES=\"E [MeV]\",\"J_perMeV\"\n";

  std::vector<double> E;
  std::vector<double> J;

  if (s.GetType() == cSpectrum::Type::Table) {
    // Requirement: preserve table values.
    const std::vector<double>& Et = s.TableEnergy_MeV();
    const std::vector<double>& Jt = s.TableFlux_perMeV();
    E.reserve(Et.size());
    J.reserve(Jt.size());

    // Keep only points inside [Emin,Emax] (inclusive). The table loader may
    // contain points outside; injection uses Emin/Emax as a hard bound.
    const double Emin = s.Emin_MeV();
    const double Emax = s.Emax_MeV();
    for (size_t i = 0; i < Et.size() && i < Jt.size(); ++i) {
      if (Et[i] < Emin || Et[i] > Emax) continue;
      E.push_back(Et[i]);
      J.push_back(Jt[i]);
    }

    // Fallback: if user table does not intersect [Emin,Emax], still emit the
    // full table (better a record than an empty file).
    if (E.size() < 2) {
      E.assign(Et.begin(), Et.end());
      J.assign(Jt.begin(), Jt.end());
    }
  }
  else {
    // Requirement: analytic spectra should cover large energy ranges well.
    // Use log-spaced energies ("exponential splitting") between Emin and Emax.
    const double Emin = s.Emin_MeV();
    const double Emax = s.Emax_MeV();
    if (!(Emin > 0.0) || !(Emax > Emin)) {
      throw std::runtime_error("WriteSpectrumInputTecplot: invalid Emin/Emax for spectrum");
    }

    E.resize(static_cast<size_t>(n_analytic));
    J.resize(static_cast<size_t>(n_analytic));

    const double ratio = Emax / Emin;
    for (int i = 0; i < n_analytic; ++i) {
      const double t = (n_analytic == 1) ? 0.0 : (double)i / (double)(n_analytic - 1);
      const double e = Emin * std::pow(ratio, t);
      E[static_cast<size_t>(i)] = e;
      J[static_cast<size_t>(i)] = s.GetSpectrumPerMeV(e);
    }
  }

  out << "ZONE T=\"SPECTRUM\" I=" << E.size() << " F=POINT\n";
  out.setf(std::ios::scientific);
  out.precision(12);
  for (size_t i = 0; i < E.size() && i < J.size(); ++i) {
    out << E[i] << " " << J[i] << "\n";
  }
}
