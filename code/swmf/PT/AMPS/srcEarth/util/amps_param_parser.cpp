//======================================================================================
// amps_param_parser.cpp
//======================================================================================
//
// See amps_param_parser.h for the complete input file format specification,
// type conversion rules, and error-handling contract.
//
//======================================================================================
// PARSING ALGORITHM
//======================================================================================
//
// The parser is a single-pass line-by-line state machine. State is a "current section"
// string, updated whenever a '#'-prefixed section header is encountered:
//
//   state = ""                 (top of file, no section active)
//   for each line L:
//     strip leading/trailing whitespace from L
//     strip comment text after '!' from L
//     if L starts with '#': state = ToUpper(L without '#')
//     else if L is "POINTS_BEGIN": start accumulating points
//     else if L is "POINTS_END":   stop accumulating points
//     else if L is blank: continue
//     else: tokenise L into KEY and VALUE; dispatch to section handler
//
// Dispatch (section handlers are static lambdas capturing the AmpsParam& result):
//   "RUN_INFO"             -> ParseRunInfo
//   "CALCULATION_MODE"     -> ParseCalcMode
//   "CUTOFF_RIGIDITY"      -> ParseCutoffRigidity
//   "DENSITY_SPECTRUM"     -> ParseDensitySpectrum
//   "BOUNDARY_ANISOTROPY"  -> ParseBoundaryAnisotropy
//   "PARTICLE_SPECIES"     -> ParseSpecies
//   "BACKGROUND_FIELD"     -> ParseBackgroundField
//   "DOMAIN_BOUNDARY"      -> ParseDomainBoundary
//   "OUTPUT_DOMAIN"        -> ParseOutputDomain
//   "NUMERICAL"            -> ParseNumerical
//   "SPECTRUM"             -> validate against supported spectrum keywords, store
//                              raw keys in result.spectrum, and build
//                              result.particleSpectrum during post-parse init
//   everything else        -> terminate through exit(__LINE__,__FILE__,msg)
//                              with a message identifying the unknown keyword
//
//======================================================================================
// TOKENISATION
//======================================================================================
//
// Each non-blank, non-comment line within a section is split on whitespace into
// exactly two tokens: KEY and VALUE. If the line has more than two tokens, everything
// after the first whitespace gap is taken as VALUE (allowing values with spaces, e.g.
// epoch strings like "2003-11-20 06:00"). The rule is:
//
//   pos = line.find_first_of(" \t")
//   key   = line.substr(0, pos)
//   value = trim(line.substr(pos+1))
//
// This handles the most common format (key and value separated by one or more spaces
// or a tab) robustly without requiring a specific delimiter character.
//
//======================================================================================
// POINTS_BEGIN / POINTS_END BLOCK
//======================================================================================
//
// Point coordinates are parsed as three space-separated doubles per line:
//   x  y  z
// Only lines with exactly three tokens in floating-point format are accepted.
// Lines with fewer tokens raise std::runtime_error. Extra trailing tokens are ignored.
//
//======================================================================================
// POST-PARSE VALIDATION
//======================================================================================
//
// After the parse loop completes, a validation pass checks:
//
//   (1) If DS_BOUNDARY_MODE = ANISOTROPIC and the #BOUNDARY_ANISOTROPY section
//       was not present: emit a std::runtime_error with a helpful message listing
//       the required keys.
//
//   (2) If CUTOFF_SAMPLING is neither VERTICAL nor ISOTROPIC: warning on stderr,
//       silently fall back to ISOTROPIC.
//
//   (3) If DS_ENERGY_SPACING is neither LOG nor LINEAR: warning on stderr,
//       silently fall back to LOG.
//
//   (4) If FIELD_MODEL is not in {T96, T05, DIPOLE}: std::runtime_error.
//
// Physical range checks (e.g., Emin < Emax, rInner > 0) are mostly delegated to
// post-parse validation or solver startup.  Structural validation is strict: an
// unknown section or keyword terminates execution immediately because ignoring it
// could leave unintended default parameters active.
//
//======================================================================================
// DESIGN CHOICES
//======================================================================================
//
//   (A) All string keys are ToUpper'd before comparison so that both
//       "Dst" and "DST" are accepted.
//
//   (B) Comment stripping happens before tokenisation, so:
//         DST  -50.0  ! nT storm main phase
//       correctly extracts value = "-50.0".
//
//   (C) Boolean values (T/F/TRUE/FALSE/1/0) are parsed by the ToBool helper,
//       which is also exported in the header for use by the CLI.
//
//   (D) The #SPECTRUM section is stored in two forms:
//         * result.spectrum         — raw key/value strings for the existing
//                                     boundary/spectrum.cpp initializer
//         * result.particleSpectrum — typed metadata parsed here for early
//                                     validation and clearer initialization
//       This keeps the parser decoupled from the injection implementation while
//       still making the spectrum choice explicit in AmpsParam.  Unknown spectrum
//       keywords are rejected before the downstream initializer is called.
//
//======================================================================================

#include "amps_param_parser.h"
#include "../boundary/spectrum.h"  // cSpectrum + global spectrum init
#include "specfunc.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>
#include <map>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <limits>
#include <cstdio>

#ifndef _NO_SPICE_CALLS_
#include "SpiceUsr.h"
#endif

namespace EarthUtil {

//======================================================================================
// String / unit-conversion utilities
// These must be defined before any function that calls them.
//======================================================================================

std::string Trim(const std::string& s) {
  size_t a=0;
  while (a<s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  size_t b=s.size();
  while (b>a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
  return s.substr(a,b-a);
}

std::string ToUpper(std::string s) {
  std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::toupper(c);});
  return s;
}

static inline std::string ToLower(std::string s) {
  std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);});
  return s;
}

// Parse a comma- and/or whitespace-separated floating-point list used by compact
// AMPS_PARAM directives such as CUTOFF_RIGIDITY_LIST_GV.  Delimiters are normalized
// to spaces before std::istringstream extraction so either of these forms is valid:
//
//   0.424,0.498,0.588
//   0.424 0.498 0.588
//
// The helper deliberately rejects any non-numeric trailing token.  Silently accepting
// a partial list could omit a requested validation rigidity while still producing a
// plausible-looking output file.
static std::vector<double> ParseStrictDoubleList_(const std::string& text,
                                                   const std::string& keyword) {
  std::string normalized=text;
  std::replace(normalized.begin(),normalized.end(),',',' ');

  std::istringstream in(normalized);
  std::vector<double> values;
  double value=0.0;
  while (in >> value) values.push_back(value);

  if (!in.eof()) {
    std::ostringstream msg;
    msg << "Cannot parse " << keyword << " as a numeric list: '" << text << "'";
    exit(__LINE__,__FILE__,msg.str().c_str());
  }
  return values;
}

bool ToBool(const std::string& sIn) {
  std::string s=ToUpper(Trim(sIn));
  if (s=="T"||s=="TRUE"||s=="1"||s=="YES"||s=="Y") return true;
  if (s=="F"||s=="FALSE"||s=="0"||s=="NO"||s=="N") return false;
  { std::ostringstream _exit_msg; _exit_msg << "Cannot parse boolean token: '"+sIn+"'"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  return false; //The code should not come to this point -- it is here just to make the compiler happy
}

//======================================================================================
// Strict AMPS_PARAM.in keyword validation helpers
//======================================================================================
//
// The parser intentionally fails immediately when it sees an unrecognized section or
// keyword.  This is safer than the previous permissive behavior because a misspelled
// keyword such as N_TRAJECTORY instead of N_TRAJECTORIES can otherwise be ignored
// silently, leaving a compiled-in/default value active and producing a scientifically
// misleading run.  All fail-fast paths use the AMPS exit(__LINE__,__FILE__,msg)
// convention requested for model-level fatal errors.
//

static inline bool ContainsToken(const std::vector<std::string>& list,
                                 const std::string& token) {
  return std::find(list.begin(), list.end(), token) != list.end();
}

static std::string JoinTokens(const std::vector<std::string>& list,
                              const std::string& sep) {
  std::ostringstream out;
  for (std::size_t i=0; i<list.size(); ++i) {
    if (i != 0) out << sep;
    out << list[i];
  }
  return out.str();
}

static const std::vector<std::string>& RecognizedAmpsParamSections() {
  static const std::vector<std::string> sections = {
    "#RUN_INFO",
    "#CALCULATION_MODE",
    "#CUTOFF_RIGIDITY",
    "#PARTICLE_SPECIES",
    "#BACKGROUND_FIELD",
    "#ELECTRIC_FIELD",
    "#DOMAIN_BOUNDARY",
    "#OUTPUT_DOMAIN",
    "#NUMERICAL",
    "#MODE3D_MESH",
    "#DENSITY_3D",
    "#PARTICLE_TRAJECTORY",
    "#DENSITY_SPECTRUM",
    "#SPECTRUM",
    "#BOUNDARY_ANISOTROPY",
    "#OUTPUT_OPTIONS",
    "#TEMPORAL",
    "#ENERGY_CHANNELS"
  };
  return sections;
}

static inline bool IsRecognizedAmpsParamSection(const std::string& section) {
  return ContainsToken(RecognizedAmpsParamSections(), ToUpper(Trim(section)));
}

static const std::vector<std::string>& RecognizedSpectrumKeys() {
  static const std::vector<std::string> keys = {
    // Type selectors.  SPECTRUM_TYPE is the canonical key; TYPE is retained as a
    // legacy/user-friendly alias already understood by ParseParticleSpectrum().
    "SPECTRUM_TYPE", "TYPE",

    // Common energy/intensity parameters and parser aliases.
    "SPEC_J0", "J0", "J_REFERENCE", "SPEC_J_REFERENCE",
    "SPEC_GAMMA", "GAMMA", "SPECTRAL_INDEX", "SPEC_SPECTRAL_INDEX",
    "SPEC_E0", "E0", "E0_MEV", "E_REFERENCE", "SPEC_E0_MEV", "SPEC_E_REFERENCE",
    "SPEC_EMIN", "EMIN", "ENERGY_MIN", "SPEC_ENERGY_MIN",
    "SPEC_EMAX", "EMAX", "ENERGY_MAX", "SPEC_ENERGY_MAX",

    // Cutoff, LIS force-field, and Band-function parameters.
    "SPEC_EC", "SPEC_ECUT", "ECUT", "E_CUTOFF", "SPEC_E_CUTOFF", "CUTOFF_ENERGY_MEV",
    "SPEC_LIS_J0", "SPEC_LIS_GAMMA", "SPEC_PHI",
    "PHI_MV", "MODULATION_POTENTIAL_MV", "FORCE_FIELD_PHI_MV",
    "SPEC_GAMMA1", "SPEC_GAMMA2",
    "ALPHA", "BAND_ALPHA", "BETA", "BAND_BETA",
    "E_BREAK", "BREAK_ENERGY_MEV", "BAND_EBREAK",

    // Table-spectrum inputs.  SPEC_TABLE_REFERENCE_EPOCH_UTC is usually inserted
    // by the parser after it resolves the event time, but accepting it in the file
    // keeps the strict validator compatible with explicit expert inputs.
    "SPEC_TABLE_FILE", "TABLE_FILE", "SPECTRUM_FILE", "FILE",
    "SPEC_TABLE_REFERENCE_EPOCH_UTC"
  };
  return keys;
}

static inline bool IsRecognizedSpectrumKey(const std::string& key) {
  return ContainsToken(RecognizedSpectrumKeys(), ToUpper(Trim(key)));
}

static void FailUnknownSection(const std::string& fileName,
                               int lineNo,
                               const std::string& section,
                               const std::string& rawLine) {
  std::ostringstream msg;
  msg << "Unrecognized AMPS_PARAM.in section '" << section << "' in file '"
      << fileName << "' at input line " << lineNo << ". Raw line: '"
      << Trim(rawLine) << "'. The code is terminated because unknown input-file "
      << "sections are not ignored: a misspelled or unsupported section would "
      << "prevent its settings from being applied and could leave default model "
      << "parameters active. Check the section spelling or add explicit support "
      << "for this section in util/amps_param_parser.cpp. Recognized sections: "
      << JoinTokens(RecognizedAmpsParamSections(), ", ");
  exit(__LINE__,__FILE__,msg.str().c_str());
}

static void FailUnknownKeyword(const std::string& fileName,
                               int lineNo,
                               const std::string& section,
                               const std::string& key,
                               const std::string& rawLine) {
  std::ostringstream msg;
  msg << "Unrecognized AMPS_PARAM.in keyword '" << key << "'";
  if (Trim(section).empty()) {
    msg << " outside of any section";
  }
  else {
    msg << " in section '" << section << "'";
  }
  msg << " in file '" << fileName << "' at input line " << lineNo
      << ". Raw line: '" << Trim(rawLine) << "'. The code is terminated "
      << "because unknown input keywords are not ignored: this usually indicates "
      << "a typo, a keyword placed in the wrong section, or a new option that "
      << "has not been connected to the parser. Ignoring it could leave the "
      << "default value active and make the run inconsistent with the requested "
      << "AMPS_PARAM.in settings. Check the keyword/section spelling or add an "
      << "explicit parser branch in util/amps_param_parser.cpp.";

  if (ToUpper(Trim(section)) == "#SPECTRUM") {
    msg << " Recognized #SPECTRUM keywords are: "
        << JoinTokens(RecognizedSpectrumKeys(), ", ");
  }
  if (ToUpper(Trim(section)) == "#OUTPUT_OPTIONS") {
    msg << " The #OUTPUT_OPTIONS section is currently reserved; no active "
        << "keywords are implemented for it in this parser.";
  }

  exit(__LINE__,__FILE__,msg.str().c_str());
}

//======================================================================================
// Density + spectrum (gridless) parsing helpers
//======================================================================================

static EarthUtil::DensitySpectrumParam::Spacing ParseEnergySpacingToken(const std::string& s) {
  const std::string t = ToUpper(Trim(s));
  if (t=="LOG") return EarthUtil::DensitySpectrumParam::Spacing::LOG;
  if (t=="LINEAR") return EarthUtil::DensitySpectrumParam::Spacing::LINEAR;
  { std::ostringstream _exit_msg; _exit_msg << "DS_ENERGY_SPACING must be LOG or LINEAR (got '" + Trim(s) + "')"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  return EarthUtil::DensitySpectrumParam::Spacing::LINEAR; //The code should not come to this point -- it is here just to make the compiler happy
}

static inline void SplitKV(const std::string& line,std::string& key,std::string& value) {
  std::istringstream iss(line);
  iss >> key;
  std::getline(iss,value);
  value=Trim(value);
}

static inline double ParsePositiveFiniteDouble(const std::string& value,
                                               const std::string& keyword,
                                               int lineNo) {
  const std::string text = Trim(value);
  std::size_t parsedChars = 0;
  double result = 0.0;

  try {
    result = std::stod(text,&parsedChars);
  }
  catch (...) {
    std::ostringstream _exit_msg;
    _exit_msg << keyword << " at line " << lineNo
              << " must be a positive finite floating-point value; got '"
              << text << "'.";
    exit(__LINE__,__FILE__,_exit_msg.str().c_str());
  }

  if (parsedChars != text.size()) {
    std::ostringstream _exit_msg;
    _exit_msg << keyword << " at line " << lineNo
              << " has extra characters after the numeric value: '"
              << text.substr(parsedChars) << "'. Put units/comments after '!'.";
    exit(__LINE__,__FILE__,_exit_msg.str().c_str());
  }

  if (!std::isfinite(result) || result <= 0.0) {
    std::ostringstream _exit_msg;
    _exit_msg << keyword << " at line " << lineNo
              << " must be positive and finite; got " << text << ".";
    exit(__LINE__,__FILE__,_exit_msg.str().c_str());
  }

  return result;
}

static inline std::string StripComment(const std::string& line) {
  size_t p=line.find('!');
  if (p==std::string::npos) return line;
  return line.substr(0,p);
}

// Split an input line into the "code" part and the optional comment after '!'.
// We intentionally preserve the comment text because some AMPS_PARAM files carry
// UNITS in the comment, for example:
//   DOMAIN_X_MAX   20     ! Re
// In that case the numeric token is unitless in the code part, and the parser
// should use the unit hint from the comment.
static inline void SplitCodeAndComment(const std::string& line,std::string& code,std::string& comment) {
  size_t p=line.find('!');
  if (p==std::string::npos) {
    code=line;
    comment.clear();
  }
  else {
    code=line.substr(0,p);
    comment=line.substr(p+1);
  }
}

//======================================================================================
// Spectrum parsing
//======================================================================================
// The AMPS_PARAM.in format contains a #SPECTRUM section that defines the energy spectrum
// of the injected flux (typically differential flux in energy).
//
// We parse the spectrum into a strongly-typed object so downstream output code can
// reproduce the spectrum definition exactly (e.g., in Tecplot headers) and we can
// fail fast if the spectrum type is unknown.
//
// Supported spectrum types (as produced by the wizard website):
//   - POWER_LAW
//   - POWER_LAW_CUTOFF
//   - LIS_FORCE_FIELD
//   - BAND
//   - TABLE
//======================================================================================

/**
 * @brief Enumerates supported spectrum functional forms read from #SPECTRUM.
 *
 * These correspond 1:1 to the wizard-generated keywords in AMPS_PARAM.in.
 * Parsing into an enum avoids string comparisons during hot loops and allows us
 * to validate the input once at startup (fail-fast).
 */

// Parsed spectrum object.
// Note: this file was uploaded without amps_param_parser.h, so we keep this struct
// local. In the real codebase, move this to the header and add a field to AmpsParam.
/**
 * @brief Holds a parsed spectrum definition from AMPS_PARAM.in.
 *
 * This is a "data-only" representation produced by the parser.
 * - It is kept separate from any injection logic on purpose: the parser should
 *   not depend on physics modules, and injection should not depend on parsing.
 * - The Tecplot writer can use this object to emit metadata (AUXDATA) so the
 *   output file records exactly which spectrum was used.
 *
 * All energies are stored in MeV (typically MeV/n). Conversion to Joules is done
 * by downstream physics/injection code as needed.
 */

static inline double GetDoubleOrThrow(const std::map<std::string,std::string>& m,
                                      const std::string& k,
                                      const std::string& ctx) {
  auto it=m.find(k);
  if (it==m.end()) { std::ostringstream _exit_msg; _exit_msg << "Missing required spectrum key '"+k+"' in "+ctx; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  return std::stod(it->second);
}

static inline std::string GetStringOrThrow(const std::map<std::string,std::string>& m,
                                           const std::string& k,
                                           const std::string& ctx) {
  auto it=m.find(k);
  if (it==m.end()) { std::ostringstream _exit_msg; _exit_msg << "Missing required spectrum key '"+k+"' in "+ctx; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  return Trim(it->second);
}

static bool TryGetKey(const std::map<std::string,std::string>& kv,
                      const std::vector<std::string>& keys,
                      std::string& valueOut) {
  for (const auto& k : keys) {
    auto it = kv.find(k);
    if (it != kv.end()) {
      valueOut = Trim(it->second);
      return true;
    }
  }
  return false;
}

static bool TryGetDouble(const std::map<std::string,std::string>& kv,
                         const std::vector<std::string>& keys,
                         double& valueOut) {
  std::string s;
  if (!TryGetKey(kv, keys, s)) return false;
  valueOut = std::stod(s);
  return true;
}

static EarthUtil::ParticleSpectrum::Type ParseSpectrumTypeToken(const std::string& s) {
  const std::string t = ToUpper(Trim(s));
  if (t=="POWER_LAW") return EarthUtil::ParticleSpectrum::Type::POWER_LAW;
  if (t=="POWER_LAW_CUTOFF") return EarthUtil::ParticleSpectrum::Type::POWER_LAW_CUTOFF;
  if (t=="LIS_FORCE_FIELD") return EarthUtil::ParticleSpectrum::Type::LIS_FORCE_FIELD;
  if (t=="BAND") return EarthUtil::ParticleSpectrum::Type::BAND;
  if (t=="TABLE") return EarthUtil::ParticleSpectrum::Type::TABLE;
  return EarthUtil::ParticleSpectrum::Type::UNKNOWN;
}

EarthUtil::ParticleSpectrum ParseParticleSpectrum(const std::map<std::string,std::string>& kv) {
  EarthUtil::ParticleSpectrum spec;
  spec.raw = kv;
  if (kv.empty()) return spec;

  const std::string ctx = "#SPECTRUM";
  std::string typeToken;
  if (!TryGetKey(kv, {"SPECTRUM_TYPE", "TYPE"}, typeToken)) {
    exit(__LINE__,__FILE__,"Missing required key SPECTRUM_TYPE in #SPECTRUM");
  }

  spec.typeName = ToUpper(typeToken);
  spec.type = ParseSpectrumTypeToken(spec.typeName);
  spec.parsed = true;

  if (spec.type == EarthUtil::ParticleSpectrum::Type::UNKNOWN) {
    std::ostringstream _exit_msg;
    _exit_msg << "Unsupported SPECTRUM_TYPE='" << typeToken
              << "'. Supported tokens: POWER_LAW | POWER_LAW_CUTOFF | LIS_FORCE_FIELD | BAND | TABLE";
    exit(__LINE__,__FILE__,_exit_msg.str().c_str());
  }

  // Common aliases produced by different wizard / legacy inputs.
  const bool hasJ0    = TryGetDouble(kv, {"SPEC_J0", "J0", "J_REFERENCE", "SPEC_J_REFERENCE"}, spec.J0);
  const bool hasGamma = TryGetDouble(kv, {"SPEC_GAMMA", "GAMMA", "SPECTRAL_INDEX", "SPEC_SPECTRAL_INDEX"}, spec.gamma);
  const bool hasE0    = TryGetDouble(kv, {"SPEC_E0", "E0", "E0_MEV", "E_REFERENCE", "SPEC_E0_MEV", "SPEC_E_REFERENCE"}, spec.E0_MeV);
  TryGetDouble(kv, {"SPEC_EMIN", "EMIN", "ENERGY_MIN", "SPEC_ENERGY_MIN"}, spec.Emin_MeV);
  TryGetDouble(kv, {"SPEC_EMAX", "EMAX", "ENERGY_MAX", "SPEC_ENERGY_MAX"}, spec.Emax_MeV);
  TryGetDouble(kv, {"SPEC_ECUT", "ECUT", "E_CUTOFF", "SPEC_E_CUTOFF", "CUTOFF_ENERGY_MEV"}, spec.cutoffEnergy_MeV);
  TryGetDouble(kv, {"PHI_MV", "MODULATION_POTENTIAL_MV", "FORCE_FIELD_PHI_MV"}, spec.modulationPotential_MV);
  TryGetDouble(kv, {"ALPHA", "BAND_ALPHA"}, spec.alpha);
  TryGetDouble(kv, {"BETA", "BAND_BETA"}, spec.beta);
  TryGetDouble(kv, {"E_BREAK", "BREAK_ENERGY_MEV", "BAND_EBREAK"}, spec.breakEnergy_MeV);
  {
    std::string tableFile;
    if (TryGetKey(kv, {"SPEC_TABLE_FILE", "TABLE_FILE", "SPECTRUM_FILE", "FILE"}, tableFile)) spec.tableFile = tableFile;
  }

  // Strong validation for the currently used/tested spectrum form.
  if (spec.type == EarthUtil::ParticleSpectrum::Type::POWER_LAW) {
    if (!hasJ0 || !(spec.J0 > 0.0)) {
      exit(__LINE__,__FILE__,"POWER_LAW spectrum requires SPEC_J0/J0 > 0");
    }
    if (!hasE0 || !(spec.E0_MeV > 0.0)) {
      exit(__LINE__,__FILE__,"POWER_LAW spectrum requires SPEC_E0/E0 > 0 (MeV)");
    }
    if (!hasGamma) {
      exit(__LINE__,__FILE__,"POWER_LAW spectrum requires SPEC_GAMMA/GAMMA (or SPECTRAL_INDEX)");
    }
    if (spec.Emin_MeV > 0.0 && spec.Emax_MeV > 0.0 && !(spec.Emax_MeV > spec.Emin_MeV)) {
      exit(__LINE__,__FILE__,"POWER_LAW spectrum requires SPEC_EMAX > SPEC_EMIN when both are provided");
    }
  }

  return spec;
}


namespace {

enum class SpectrumTableFileFormat { UNKNOWN, TWO_COLUMN, TIME_DEPENDENT };

static inline bool TryParseScalarToken(const std::string& sIn, double& valueOut) {
  const std::string s = Trim(sIn);
  if (s.empty()) return false;
  char* end = nullptr;
  errno = 0;
  const double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || errno == ERANGE) return false;
  while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
  if (end && *end) return false;
  if (!std::isfinite(v)) return false;
  valueOut = v;
  return true;
}

static inline bool ParseUtcTimestampToUnixSeconds(const std::string& sIn, double& secondsOut) {
  std::string s = Trim(sIn);
  if (s.empty()) return false;
  if (!s.empty() && (s.back()=='Z' || s.back()=='z')) s.pop_back();

  int year=0, month=0, day=0, hour=0, minute=0;
  double second = 0.0;
  int consumed = 0;

  if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%lf%n", &year, &month, &day, &hour, &minute, &second, &consumed) < 6) {
    consumed = 0;
    second = 0.0;
    if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%lf%n", &year, &month, &day, &hour, &minute, &second, &consumed) < 6) {
      consumed = 0;
      if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d%n", &year, &month, &day, &hour, &minute, &consumed) < 5) {
        consumed = 0;
        if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d%n", &year, &month, &day, &hour, &minute, &consumed) < 5) {
          consumed = 0;
          if (std::sscanf(s.c_str(), "%d-%d-%d%n", &year, &month, &day, &consumed) < 3) return false;
          hour = 0; minute = 0; second = 0.0;
        }
        else {
          second = 0.0;
        }
      }
      else {
        second = 0.0;
      }
    }
  }

  while (consumed < static_cast<int>(s.size()) && std::isspace(static_cast<unsigned char>(s[consumed]))) ++consumed;
  if (consumed != static_cast<int>(s.size())) return false;

  if (month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
  if (!(second >= 0.0) || !(second < 61.0) || !std::isfinite(second)) return false;

  const int y = year - (month <= 2 ? 1 : 0);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned mp = static_cast<unsigned>(month + (month > 2 ? -3 : 9));
  const unsigned doy = (153u * mp + 2u) / 5u + static_cast<unsigned>(day - 1);
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  const long long days = static_cast<long long>(era) * 146097LL + static_cast<long long>(doe) - 719468LL;

  secondsOut = static_cast<double>(days) * 86400.0 + static_cast<double>(hour * 3600 + minute * 60) + second;
  return true;
}

static inline std::vector<std::string> SplitWhitespaceTokens(const std::string& line) {
  std::istringstream iss(line);
  std::vector<std::string> out;
  std::string tok;
  while (iss >> tok) out.push_back(tok);
  return out;
}

static inline std::string StripLeadingCommentMarkers(const std::string& line) {
  std::string s = Trim(line);
  while (!s.empty() && (s[0]=='#' || s[0]=='!')) s = Trim(s.substr(1));
  return s;
}

static SpectrumTableFileFormat DetectSpectrumTableFileFormat(const std::string& fileName) {
  std::ifstream in(fileName.c_str());
  if (!in) {
    std::ostringstream oss;
    oss << "Cannot open spectrum table file: " << fileName;
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  std::string line;
  std::size_t lineNo = 0;
  while (std::getline(in, line)) {
    ++lineNo;
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) continue;

    if (trimmed[0]=='#' || trimmed[0]=='!') {
      const std::string header = ToUpper(StripLeadingCommentMarkers(trimmed));
      if (header.find("ENERGY_MEV:") == 0 ||
          header.find("COLUMNS: TIMESTAMP") == 0 ||
          header.find("COLUMN 1") == 0 ||
          header.find("N_ENERGY_BINS") == 0) {
        return SpectrumTableFileFormat::TIME_DEPENDENT;
      }
      continue;
    }

    const std::vector<std::string> toks = SplitWhitespaceTokens(trimmed);
    if (toks.empty()) continue;

    double tSeconds = 0.0;
    if (ParseUtcTimestampToUnixSeconds(toks[0], tSeconds)) {
      return SpectrumTableFileFormat::TIME_DEPENDENT;
    }

    if (toks.size() == 2) {
      double e = 0.0, j = 0.0;
      if (TryParseScalarToken(toks[0], e) && TryParseScalarToken(toks[1], j)) {
        return SpectrumTableFileFormat::TWO_COLUMN;
      }
    }

    std::ostringstream oss;
    oss << "Cannot determine spectrum-table format for '" << fileName
        << "' from line " << lineNo << ": '" << trimmed << "'";
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  std::ostringstream oss;
  oss << "Spectrum table file '" << fileName << "' is empty";
  exit(__LINE__,__FILE__,oss.str().c_str());
  return SpectrumTableFileFormat::UNKNOWN;
}

static std::vector<double> ParseTimeDependentSpectrumEnergyGridOrThrow(const std::string& fileName) {
  std::ifstream in(fileName.c_str());
  if (!in) {
    std::ostringstream oss;
    oss << "Cannot open spectrum table file: " << fileName;
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  std::string line;
  while (std::getline(in, line)) {
    const std::string header = StripLeadingCommentMarkers(line);
    const std::string upper = ToUpper(header);
    if (upper.find("ENERGY_MEV:") != 0) continue;

    const std::string payload = Trim(header.substr(std::string("Energy_MeV:").size()));
    const std::vector<std::string> toks = SplitWhitespaceTokens(payload);
    std::vector<double> energyGrid;
    energyGrid.reserve(toks.size());
    for (const auto& tok : toks) {
      double e = 0.0;
      if (!TryParseScalarToken(tok, e) || !(e > 0.0)) {
        std::ostringstream oss;
        oss << "Invalid energy value '" << tok << "' in time-dependent spectrum header of '" << fileName << "'";
        exit(__LINE__,__FILE__,oss.str().c_str());
      }
      energyGrid.push_back(e);
    }
    if (energyGrid.size() < 2) {
      std::ostringstream oss;
      oss << "Time-dependent spectrum file '" << fileName << "' must define at least two energies in Energy_MeV header";
      exit(__LINE__,__FILE__,oss.str().c_str());
    }
    return energyGrid;
  }

  std::ostringstream oss;
  oss << "Time-dependent spectrum file '" << fileName << "' is missing the Energy_MeV header";
  exit(__LINE__,__FILE__,oss.str().c_str());
  return std::vector<double>();
}

static inline std::string BasenameOnly(const std::string& path) {
  const std::size_t pos = path.find_last_of("/\\");
  return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

static inline std::string SanitizeFileStem(std::string s) {
  for (char& c : s) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c=='.' || c=='_' || c=='-')) c = '_';
  }
  return s;
}

static std::string SelectSpectrumInitializationEpochUTC(const EarthUtil::AmpsParam& p) {
  const std::string mode = ToUpper(p.output.mode);
  if (mode == "TRAJECTORY") {
    for (const auto& tr : p.output.trajectories) {
      if (!tr.samples.empty()) return tr.samples.front().timeUTC;
    }
  }
  return p.field.epoch;
}

static std::string MaterializeTimeDependentSpectrumSnapshotOrThrow(const std::string& fileName,
                                                                   const std::string& refEpochUTC) {
  double refSeconds = 0.0;
  if (!ParseUtcTimestampToUnixSeconds(refEpochUTC, refSeconds)) {
    std::ostringstream oss;
    oss << "Cannot parse reference epoch for time-dependent spectrum selection: '" << refEpochUTC << "'";
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  const std::vector<double> energyGrid = ParseTimeDependentSpectrumEnergyGridOrThrow(fileName);

  std::ifstream in(fileName.c_str());
  if (!in) {
    std::ostringstream oss;
    oss << "Cannot open spectrum table file: " << fileName;
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  double bestDelta = std::numeric_limits<double>::infinity();
  std::string bestEpochUTC;
  std::vector<double> bestFlux;

  std::string line;
  std::size_t lineNo = 0;
  while (std::getline(in, line)) {
    ++lineNo;
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0]=='#' || trimmed[0]=='!') continue;

    const std::vector<std::string> toks = SplitWhitespaceTokens(trimmed);
    if (toks.size() < 2) continue;

    double rowSeconds = 0.0;
    if (!ParseUtcTimestampToUnixSeconds(toks[0], rowSeconds)) continue;

    if (toks.size() < 1 + energyGrid.size()) {
      std::ostringstream oss;
      oss << "Time-dependent spectrum row " << lineNo << " in '" << fileName
          << "' has " << (toks.size() - 1) << " flux columns but the Energy_MeV header declares "
          << energyGrid.size();
      exit(__LINE__,__FILE__,oss.str().c_str());
    }

    std::vector<double> flux(energyGrid.size(), std::numeric_limits<double>::quiet_NaN());
    for (std::size_t i=0; i<energyGrid.size(); ++i) {
      double v = 0.0;
      if (TryParseScalarToken(toks[1+i], v) && v > 0.0) flux[i] = v;
    }

    const double delta = std::fabs(rowSeconds - refSeconds);
    if (delta < bestDelta) {
      bestDelta = delta;
      bestEpochUTC = toks[0];
      bestFlux.swap(flux);
    }
  }

  if (bestFlux.empty()) {
    std::ostringstream oss;
    oss << "No timestamped spectrum rows were found in time-dependent spectrum file '" << fileName << "'";
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  std::ostringstream name;
  name << "spectrum_snapshot_" << SanitizeFileStem(BasenameOnly(fileName)) << ".dat";
  const std::string outFile = name.str();

  std::ofstream out(outFile.c_str());
  if (!out) {
    std::ostringstream oss;
    oss << "Cannot create snapshot spectrum file: " << outFile;
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  out << "# Extracted from time-dependent spectrum file\n";
  out << "# source_file: " << fileName << "\n";
  out << "# reference_epoch_utc: " << refEpochUTC << "\n";
  out << "# selected_epoch_utc: " << bestEpochUTC << "\n";
  out << "# selected_time_offset_s: " << bestDelta << "\n";
  out << "# columns: Energy_MeV  differential_flux_perMeV\n";

  std::size_t nValid = 0;
  out.setf(std::ios::scientific);
  out.precision(12);
  for (std::size_t i=0; i<energyGrid.size() && i<bestFlux.size(); ++i) {
    if (!(bestFlux[i] > 0.0) || !std::isfinite(bestFlux[i])) continue;
    out << energyGrid[i] << " " << bestFlux[i] << "\n";
    ++nValid;
  }

  if (nValid < 2) {
    std::ostringstream oss;
    oss << "Selected time-dependent spectrum snapshot from '" << fileName
        << "' at epoch '" << bestEpochUTC << "' contains fewer than two valid positive bins";
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  return outFile;
}

static void ResolveTableSpectrumFileForInitialization(EarthUtil::AmpsParam& p) {
  if (p.particleSpectrum.type != EarthUtil::ParticleSpectrum::Type::TABLE) return;

  std::string tableFile = Trim(p.particleSpectrum.tableFile);
  if (tableFile.empty()) {
    std::map<std::string,std::string>::const_iterator it = p.spectrum.find("SPEC_TABLE_FILE");
    if (it != p.spectrum.end()) tableFile = Trim(it->second);
  }
  if (tableFile.empty()) {
    std::map<std::string,std::string>::const_iterator it = p.spectrum.find("TABLE_FILE");
    if (it != p.spectrum.end()) tableFile = Trim(it->second);
  }
  if (tableFile.empty()) return;

  p.spectrum["SPEC_TABLE_FILE"] = tableFile;
  p.particleSpectrum.tableFile = tableFile;

  const SpectrumTableFileFormat fmt = DetectSpectrumTableFileFormat(tableFile);
  if (fmt == SpectrumTableFileFormat::TWO_COLUMN) {
    p.particleSpectrum.tableFormat = EarthUtil::ParticleSpectrum::TableFormat::TWO_COLUMN;
  }
  else if (fmt == SpectrumTableFileFormat::TIME_DEPENDENT) {
    p.particleSpectrum.tableFormat = EarthUtil::ParticleSpectrum::TableFormat::TIME_DEPENDENT;
  }
  else {
    std::ostringstream oss;
    oss << "Unsupported spectrum table format for file '" << tableFile << "'";
    exit(__LINE__,__FILE__,oss.str().c_str());
  }

  const std::string refEpochUTC = SelectSpectrumInitializationEpochUTC(p);
  p.spectrum["SPEC_TABLE_REFERENCE_EPOCH_UTC"] = refEpochUTC;
}


} // anonymous namespace

/**
 * @brief Parse and validate the #SPECTRUM section.
 *
 * @param kv  Key/value table collected by the generic AMPS_PARAM.in reader for the
 *            #SPECTRUM section only (keys already uppercased by the parser).
 *
 * @return SpectrumSpec with type and parameters filled in.
 *
 * Failure policy (important):
 * - Unknown SPECTRUM_TYPE => throw with a clear list of supported types.
 * - Missing required parameters for a known type => throw (fail-fast).
 * - Emin/Emax invalid => throw.
 *
 * Rationale:
 * Spectra define injected particle populations. If the spectrum is misread, the
 * resulting simulation can be scientifically invalid. Therefore we prefer explicit
 * errors over silent defaults.
 */

// Write spectrum definition as Tecplot AUXDATA entries.
// Typical usage: call this while writing the Tecplot header (before ZONE).
/**
 * @brief Emit spectrum parameters into Tecplot header as AUXDATA entries.
 *
 * This is meant to be called by the Tecplot writer (not by the parser itself)
 * so that each output file records the exact spectrum configuration used.
 *
 * Why AUXDATA:
 * - It is preserved with the dataset.
 * - It is easy to parse later for provenance.
 * - It does not interfere with VARIABLES/ZONE data.
 */

// Earth radius used for conversion of "Re" to kilometers in the parser.
// The parser stores all length-like quantities internally in km. Downstream code
// can convert km->Re (for Tsyganenko calls) or km->m (for particle pushers).
#ifndef AMPS_EARTH_RADIUS_KM
static const double kEarthRadiusKm = 6371.2;
#else
static const double kEarthRadiusKm = AMPS_EARTH_RADIUS_KM;
#endif

// Parse a single length token with optional inline unit suffix (e.g. "10Re",
// "2000km"). If no inline suffix is present, optionally use a unit hint taken
// from the trailing comment after '!'. If neither is present, default to km.
//
// IMPORTANT MODIFICATION (comment-embedded units):
//   Some AMPS_PARAM files document units as part of a human-readable comment,
//   e.g.
//      DOMAIN_X_MAX  15.0   ! RE dayside
//      R_INNER        2.0   ! RE inner loss sphere
//   In such cases the unit token is NOT equal to the entire comment string.
//   Therefore this parser now scans the comment text and extracts the first
//   recognized unit token ("km" or "Re", case-insensitive) from anywhere in
//   the comment.
//
// Supported units (case-insensitive): km, Re.
static double ParseLengthToKm(const std::string& token,const std::string& unitHintFromComment="") {
  std::string t=Trim(token);
  if (t.empty()) exit(__LINE__,__FILE__,"Empty length token");

  const char* begin=t.c_str();
  char* end=nullptr;
  errno=0;
  double value=std::strtod(begin,&end);
  if (begin==end) {
    { std::ostringstream _exit_msg; _exit_msg << "Cannot parse numeric length token: '"+t+"'"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  }
  if (errno==ERANGE) {
    { std::ostringstream _exit_msg; _exit_msg << "Length token is out of range: '"+t+"'"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  }

  std::string suffix=ToLower(Trim(std::string(end)));
  std::string hint=ToLower(Trim(unitHintFromComment));

  // Helper: normalize a potential unit token extracted from comments.
  // We strip common punctuation so comments like "! RE, dayside" or
  // "! (km) inner sphere" are accepted. We intentionally keep only alphabetic
  // characters because supported units are alphabetic (km, Re).
  auto NormalizeUnitToken = [](std::string x) {
    std::string y;
    y.reserve(x.size());
    for (char c : x) {
      unsigned char uc = static_cast<unsigned char>(c);
      if (std::isalpha(uc)) y.push_back(static_cast<char>(std::tolower(uc)));
    }
    return y;
  };

  // If the value token itself has an explicit suffix, it takes precedence over
  // any comment hint.
  if (!suffix.empty()) {
    if (suffix=="km") return value;
    if (suffix=="re") return value*kEarthRadiusKm;
    { std::ostringstream _exit_msg; _exit_msg << "Unsupported inline length unit '"+suffix+"' in token '"+t+"'"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  }

  // No inline suffix: use comment hint if present. The comment may be a pure
  // unit token ("Re") OR a longer descriptive phrase containing the unit
  // token ("RE dayside", "km inner loss sphere"). We scan tokens and use the
  // first recognized unit.
  if (!hint.empty()) {
    if (hint=="km") return value;
    if (hint=="re") return value*kEarthRadiusKm;

    // Scan the *entire* comment text token-by-token because the unit may be
    // embedded in a longer phrase, e.g. "RE dayside", "RE nightside",
    // "km inner loss sphere". The first recognized unit wins.
    std::istringstream hss(unitHintFromComment);
    std::string ht;
    while (hss >> ht) {
      std::string hu = NormalizeUnitToken(Trim(ht));
      if (hu=="km") return value;
      if (hu=="re") return value*kEarthRadiusKm;
    }

    // Free-text comment without a recognizable unit token: fall back to the
    // backward-compatible default (km) instead of throwing. This keeps comments
    // like "! rectangular box in GSM" harmless for length fields while still
    // supporting explicit unit hints embedded in prose.
    return value;
  }

  // Backward-compatible default when no units are provided anywhere.
  return value; // km
}

// Parse a Mode3D mesh-resolution length.  Canonical keys end in _RE or _KM;
// those suffixes are treated as explicit unit hints when the value itself does
// not carry an inline unit.  Generic aliases without a unit suffix may still use
// inline units (e.g., 0.05Re or 300km) or a comment unit hint; otherwise they
// fall back to ParseLengthToKm()'s historical km default.
static double ParseMode3DMeshLengthToKm(const std::string& key,
                                        const std::string& val,
                                        const std::string& commentText) {
  const std::string k=ToUpper(Trim(key));
  if (k.size()>=3 && k.compare(k.size()-3,3,"_RE")==0) {
    return ParseLengthToKm(val,"Re");
  }
  if (k.size()>=3 && k.compare(k.size()-3,3,"_KM")==0) {
    return ParseLengthToKm(val,"km");
  }
  return ParseLengthToKm(val,commentText);
}

static void ParseMode3DMeshKeyword(AmpsParam& p,
                                   const std::string& uKey,
                                   const std::string& val,
                                   const std::string& commentText) {
  if (uKey=="MODE3D_MESH_RES_EARTH_RE" ||
      uKey=="MODE3D_MESH_RESOLUTION_EARTH_RE" ||
      uKey=="MODE3D_MESH_EARTH_RES_RE" ||
      uKey=="MODE3D_MESH_RES_EARTH_KM" ||
      uKey=="MODE3D_MESH_RESOLUTION_EARTH_KM" ||
      uKey=="MODE3D_MESH_EARTH_RES_KM" ||
      uKey=="MODE3D_MESH_RES_EARTH" ||
      uKey=="MODE3D_MESH_RESOLUTION_EARTH") {
    p.mode3d.meshResolutionEarth_km=ParseMode3DMeshLengthToKm(uKey,val,commentText);
    p.mode3d.meshResolutionProfileActive=true;
  }
  else if (uKey=="MODE3D_MESH_RES_BOUNDARY_RE" ||
           uKey=="MODE3D_MESH_RESOLUTION_BOUNDARY_RE" ||
           uKey=="MODE3D_MESH_BOUNDARY_RES_RE" ||
           uKey=="MODE3D_MESH_RES_OUTER_RE" ||
           uKey=="MODE3D_MESH_RESOLUTION_OUTER_RE" ||
           uKey=="MODE3D_MESH_OUTER_RES_RE" ||
           uKey=="MODE3D_MESH_RES_BOUNDARY_KM" ||
           uKey=="MODE3D_MESH_RESOLUTION_BOUNDARY_KM" ||
           uKey=="MODE3D_MESH_BOUNDARY_RES_KM" ||
           uKey=="MODE3D_MESH_RES_OUTER_KM" ||
           uKey=="MODE3D_MESH_RESOLUTION_OUTER_KM" ||
           uKey=="MODE3D_MESH_OUTER_RES_KM" ||
           uKey=="MODE3D_MESH_RES_BOUNDARY" ||
           uKey=="MODE3D_MESH_RESOLUTION_BOUNDARY" ||
           uKey=="MODE3D_MESH_RES_OUTER" ||
           uKey=="MODE3D_MESH_RESOLUTION_OUTER") {
    p.mode3d.meshResolutionBoundary_km=ParseMode3DMeshLengthToKm(uKey,val,commentText);
    p.mode3d.meshResolutionProfileActive=true;
  }
  else if (uKey=="MODE3D_MESH_R_BOUNDARY_RE" ||
           uKey=="MODE3D_MESH_R_OUTER_RE" ||
           uKey=="MODE3D_MESH_OUTER_RADIUS_RE" ||
           uKey=="MODE3D_MESH_BOUNDARY_RADIUS_RE" ||
           uKey=="MODE3D_MESH_R_BOUNDARY_KM" ||
           uKey=="MODE3D_MESH_R_OUTER_KM" ||
           uKey=="MODE3D_MESH_OUTER_RADIUS_KM" ||
           uKey=="MODE3D_MESH_BOUNDARY_RADIUS_KM" ||
           uKey=="MODE3D_MESH_R_BOUNDARY" ||
           uKey=="MODE3D_MESH_R_OUTER" ||
           uKey=="MODE3D_MESH_OUTER_RADIUS" ||
           uKey=="MODE3D_MESH_BOUNDARY_RADIUS") {
    p.mode3d.meshResolutionOuterRadius_km=ParseMode3DMeshLengthToKm(uKey,val,commentText);
    p.mode3d.meshResolutionProfileActive=true;
  }
  else if (uKey=="MODE3D_MESH_COARSENING" ||
           uKey=="MODE3D_MESH_COARSENING_TYPE" ||
           uKey=="MODE3D_MESH_PROFILE" ||
           uKey=="MODE3D_MESH_RESOLUTION_PROFILE") {
    p.mode3d.meshResolutionCoarsening=ToUpper(Trim(val));
    p.mode3d.meshResolutionProfileActive=true;
  }
  else if (uKey=="MODE3D_MESH_EXPONENT" ||
           uKey=="MODE3D_MESH_COARSENING_EXPONENT" ||
           uKey=="MODE3D_MESH_POWER" ||
           uKey=="MODE3D_MESH_COARSENING_POWER") {
    p.mode3d.meshResolutionExponent=std::stod(val);
    p.mode3d.meshResolutionProfileActive=true;
  }
  else {
    std::ostringstream _exit_msg;
    _exit_msg << "Unknown #MODE3D_MESH keyword: " << uKey;
    exit(__LINE__,__FILE__,_exit_msg.str().c_str());
  }
}

// Parse a sequence of length values from a whitespace-separated string. Units may
// be provided inline (e.g. "1Re 200km") and/or in the comment after '!'.
// Supported comment conventions:
//   ! Re           -> apply one unit to all values that have no inline suffix
//   ! km km Re     -> per-value unit hints in order
// Any value with an inline suffix keeps that suffix and ignores the comment hint.
static std::vector<double> ParseLengthListToKm(const std::string& values,const std::string& comment) {
  std::vector<std::string> valueToks;
  {
    std::istringstream iss(values);
    std::string t;
    while (iss>>t) valueToks.push_back(t);
  }

  // IMPORTANT ROBUSTNESS FIX
  // ------------------------
  // A comment attached to a POINT line often contains *descriptive prose*, e.g.
  //
  //   POINT 50969.6 0 0   ! TC-EQ8: 8 Re equator (GSM x-axis)
  //
  // The earlier implementation scanned the full comment and collected every token that
  // looked like a supported unit.  In the example above it would find the word "Re"
  // in the prose fragment "8 Re equator" and incorrectly interpret that as a unit hint
  // for the coordinate list.  The result was catastrophic: a coordinate already given in
  // km (50969.6) was reinterpreted as 50969.6 Re, i.e. multiplied by Earth's radius.
  //
  // That bug affected any list-style length field parsed with ParseLengthListToKm(), but
  // it was most visible for OUTPUT_MODE=POINTS because the analytic dipole benchmark then
  // saw observation points absurdly far from Earth and returned near-zero/zero reference
  // values.  The numerical solver itself still used the same misparsed coordinates, which
  // is why the output X_km/Y_km/Z_km columns looked obviously wrong (orders of magnitude
  // too large).
  //
  // The corrected rule is intentionally strict:
  //   * a comment-derived unit hint is accepted only if the comment is *purely a unit
  //     specification*, either
  //         ! Re
  //     or  ! km km km
  //     i.e. one global unit token or one token per listed value;
  //   * as soon as the comment contains any non-unit text, it is treated as descriptive
  //     prose and contributes NO unit hint.
  //
  // This keeps explicit unit comments working, while preventing accidental capture of
  // words like "Re" in free-form descriptions.
  std::vector<std::string> rawCommentToks;
  {
    std::istringstream iss(comment);
    std::string t;
    while (iss>>t) rawCommentToks.push_back(t);
  }

  auto NormalizeUnitToken = [](std::string x) {
    std::string y;
    y.reserve(x.size());
    for (char c : x) {
      unsigned char uc = static_cast<unsigned char>(c);
      if (std::isalpha(uc)) y.push_back(static_cast<char>(std::tolower(uc)));
    }
    return y;
  };

  bool commentIsPureUnitList = !rawCommentToks.empty();
  std::vector<std::string> unitHints;
  unitHints.reserve(rawCommentToks.size());
  for (const auto& t : rawCommentToks) {
    const std::string u = NormalizeUnitToken(Trim(t));
    if (u=="km" || u=="re") {
      unitHints.push_back(u);
    }
    else {
      // Any non-unit token means the comment is descriptive prose rather than an
      // unambiguous unit declaration.  Discard *all* comment-derived hints.
      commentIsPureUnitList = false;
      unitHints.clear();
      break;
    }
  }

  std::vector<double> out;
  out.reserve(valueToks.size());

  const bool oneGlobalHint = commentIsPureUnitList && (unitHints.size()==1);
  const bool perValueHints = commentIsPureUnitList && (unitHints.size()==valueToks.size());

  for (size_t i=0;i<valueToks.size();++i) {
    std::string hint;
    if (perValueHints) hint=unitHints[i];
    else if (oneGlobalHint) hint=unitHints[0];
    out.push_back(ParseLengthToKm(valueToks[i],hint));
  }
  return out;
}


static inline bool ParseIsoUtcToEt(const std::string& timeUTC, double& et) {
#ifndef _NO_SPICE_CALLS_
  try {
    SpiceDouble etSpice = 0.0;
    str2et_c(timeUTC.c_str(), &etSpice);
    et = etSpice;
    return true;
  }
  catch (...) {
    return false;
  }
#else
  (void)timeUTC; (void)et;
  return false;
#endif
}

static inline EarthUtil::Vec3 RotateVectorWithSpiceOrThrow(const EarthUtil::Vec3& vin,
                                                           const char* fromFrame,
                                                           const char* toFrame,
                                                           const std::string& timeUTC,
                                                           const std::string& context) {
#ifndef _NO_SPICE_CALLS_
  SpiceDouble et = 0.0;
  if (!ParseIsoUtcToEt(timeUTC, et)) {
    { std::ostringstream _exit_msg; _exit_msg << "Cannot parse trajectory timestamp for SPICE transform in "
                                              << context << ": '" << timeUTC << "'"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  }

  SpiceDouble rot[3][3];
  pxform_c(fromFrame, toFrame, et, rot);

  SpiceDouble in[3]  = {vin.x, vin.y, vin.z};
  SpiceDouble out[3] = {0.0,0.0,0.0};
  mxv_c(rot, in, out);
  return {out[0], out[1], out[2]};
#else
  (void)vin; (void)fromFrame; (void)toFrame; (void)timeUTC; (void)context;
  exit(__LINE__,__FILE__,"Trajectory frame conversion requires SPICE, but the code was built with _NO_SPICE_CALLS_");
  return EarthUtil::Vec3{};
#endif
}

static inline EarthUtil::Vec3 GeoLatLonAltToCartesianKm(double lat_deg,double lon_deg,double alt_km) {
  const double r_km = kEarthRadiusKm + alt_km;
  const double pi = std::acos(-1.0);
  const double lat = lat_deg * pi / 180.0;
  const double lon = lon_deg * pi / 180.0;
  const double clat = std::cos(lat);
  return {r_km*clat*std::cos(lon), r_km*clat*std::sin(lon), r_km*std::sin(lat)};
}

static EarthUtil::Vec3 ConvertTrajectorySampleToGsmM(const std::string& frame,
                                                     const std::string& timeUTC,
                                                     double c1,double c2,double c3,
                                                     const std::string& fileName,
                                                     int lineNo) {
  const std::string f = ToUpper(Trim(frame));
  const std::string ctx = std::string("trajectory file '") + fileName + "' line " + std::to_string(lineNo);

  // All branches ultimately produce a GSM Cartesian vector in SI meters.
  if (f=="GSM") {
    // Input: Cartesian GSM in kilometers.  Convert km -> m.
    return {c1*1000.0, c2*1000.0, c3*1000.0};
  }
  else if (f=="SM") {
    // Input: Cartesian SM in kilometers.  Rotate SM -> GSM in km, then convert to m.
    const EarthUtil::Vec3 xSM_km{c1, c2, c3};
    const EarthUtil::Vec3 xGSM_km = RotateVectorWithSpiceOrThrow(xSM_km, "SM", "GSM", timeUTC, ctx);
    return {xGSM_km.x*1000.0, xGSM_km.y*1000.0, xGSM_km.z*1000.0};
  }
  else if (f=="GEO") {
    const EarthUtil::Vec3 xGeo_km = GeoLatLonAltToCartesianKm(c1, c2, c3);
    const EarthUtil::Vec3 xGSM_km = RotateVectorWithSpiceOrThrow(xGeo_km, "ITRF93", "GSM", timeUTC, ctx);
    return {xGSM_km.x*1000.0, xGSM_km.y*1000.0, xGSM_km.z*1000.0};
  }

  { std::ostringstream _exit_msg; _exit_msg << "Unsupported TRAJ_FRAME='" << frame
                                            << "' in " << ctx
                                            << ". Supported values: GEO | GSM | SM"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  return EarthUtil::Vec3{};
}

EarthUtil::SpacecraftTrajectory LoadTrajectoryFileAsGsm(const std::string& fileName,
                                                     const std::string& trajFrame) {
  std::ifstream fin(fileName);
  if (!fin.is_open()) {
    { std::ostringstream _exit_msg; _exit_msg << "Cannot open trajectory file: " << fileName; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  }

  EarthUtil::SpacecraftTrajectory tr;
  tr.name = fileName;
  tr.sourceFrame = ToUpper(Trim(trajFrame));

  std::string line;
  int lineNo = 0;
  while (std::getline(fin, line)) {
    ++lineNo;

    // Strip comments: in the trajectory file both '#' and '!' introduce a comment
    // that extends to the end of the line.  We strip whichever character comes
    // first so that lines like
    //   2017-09-10T00:00:00Z  4.716  -0.684  -0.415  # first point
    //   # full-line comment
    //   ! also a comment
    // are all handled correctly.
    // Note: '#' is NOT treated as a comment in the main AMPS_PARAM parser (it marks
    // section headers there), so this stripping is intentionally local to this
    // function and not applied via the global StripComment helper.
    {
      const std::size_t ph = line.find('#');
      const std::size_t pe = line.find('!');
      const std::size_t pcut = std::min(ph, pe);   // npos if both absent, else the earlier one
      if (pcut != std::string::npos) line = line.substr(0, pcut);
    }

    line = Trim(line);
    if (line.empty()) continue;   // blank or comment-only lines are skipped

    std::istringstream iss(line);
    std::string timeUTC;
    double c1=0.0, c2=0.0, c3=0.0;
    if (!(iss >> timeUTC >> c1 >> c2 >> c3)) {
      continue;  // fewer than 4 tokens on a non-blank line — skip gracefully
    }

    // Require an ISO-8601 'T' separator to distinguish data rows from any
    // residual header lines that slip through (e.g. column-name rows whose
    // first token is a plain string with no 'T').
    if (timeUTC.find('T') == std::string::npos) continue;

    // Trajectory file coordinates are interpreted in kilometers.
    // Position is stored internally in meters (SI) in xGSM_m.
    const EarthUtil::Vec3 xGSM_m = ConvertTrajectorySampleToGsmM(trajFrame, timeUTC, c1, c2, c3, fileName, lineNo);
    tr.AddSample(timeUTC, xGSM_m);
  }

  if (tr.samples.empty()) {
    { std::ostringstream _exit_msg; _exit_msg << "No trajectory samples were loaded from " << fileName; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  }

  return tr;
}

static void PackStandalonePointsIntoSyntheticTrajectories(EarthUtil::AmpsParam& p) {
  p.output.trajectories.clear();
  p.output.trajectories.reserve(p.output.points.size());

  for (size_t i=0; i<p.output.points.size(); ++i) {
    EarthUtil::SpacecraftTrajectory tr;
    tr.name = std::string("POINT_") + std::to_string(i);
    tr.sourceFrame = ToUpper(p.output.coords);
    // output.points is in km; trajectory samples are stored in meters.
    const EarthUtil::Vec3& pt_km = p.output.points[i];
    tr.AddSample(p.field.epoch, {pt_km.x*1000.0, pt_km.y*1000.0, pt_km.z*1000.0});
    p.output.trajectories.push_back(tr);
  }
}

void InitializePointLikeOutput(EarthUtil::AmpsParam& p) {
  const std::string mode = ToUpper(Trim(p.output.mode));

  if (mode=="TRAJECTORY") {
    if (Trim(p.output.trajFile).empty()) {
      exit(__LINE__,__FILE__,"OUTPUT_MODE=TRAJECTORY requires TRAJ_FILE in #OUTPUT_DOMAIN");
    }
    p.output.trajectories.clear();
    p.output.trajectories.push_back(LoadTrajectoryFileAsGsm(p.output.trajFile, p.output.trajFrame));
    p.output.RebuildFlattenedPointsFromTrajectories();
    return;
  }

  if (mode=="POINTS") {
    if (p.output.points.empty()) {
      exit(__LINE__,__FILE__,"OUTPUT_MODE=POINTS but no POINT entries were found in POINTS_BEGIN/END block");
    }
    PackStandalonePointsIntoSyntheticTrajectories(p);
    p.output.RebuildFlattenedPointsFromTrajectories();
    return;
  }

  if (mode=="SHELLS") return;

  {
    std::ostringstream _exit_msg;
    _exit_msg << "Unsupported OUTPUT_MODE='" << p.output.mode
              << "'. Supported values: POINTS | TRAJECTORY | SHELLS";
    exit(__LINE__,__FILE__,_exit_msg.str().c_str());
  }
}


//======================================================================================
// TsDriverTable — implementation
//======================================================================================

static inline double Lerp(double a, double b, double t) { return a + t*(b-a); }
static std::string NormalizeTsModelName(const std::string& raw);

void BuildTsParmod(const BackgroundField& field,
                   const std::string& model,
                   double parmod[11]) {
  for (int i = 0; i < 11; ++i) parmod[i] = 0.0;
  parmod[0] = field.pdyn_nPa;
  parmod[1] = field.dst_nT;
  parmod[2] = field.imfBy_nT;
  parmod[3] = field.imfBz_nT;

  const std::string m = NormalizeTsModelName(model);
  if (m == "T01") {
    for (int i = 0; i < 3; ++i) parmod[4 + i] = field.g[i];
  }
  else if (m == "T05") {
    for (int i = 0; i < 6; ++i) parmod[4 + i] = field.w[i];
  }
  else if (m == "TA16") {
    // TA16 (Tsyganenko-Andreeva 2016) PARMOD layout (as documented in TA16Interface.h):
    //   PARMOD(1) = PDYN   [nPa]           — already filled above from field.pdyn_nPa
    //   PARMOD(2) = SymHc  [nT]            — already filled above (field.dst_nT as SymHc proxy)
    //   PARMOD(3) = XIND   [dimensionless] — Newell optimal coupling index
    //   PARMOD(4) = BYIMF  [nT]
    //   PARMOD(5..10) = W1..W6             — storm-time history integrals (same as T05)
    parmod[2] = field.xind;
    parmod[3] = field.imfBy_nT;
    for (int i = 0; i < 6; ++i) parmod[4 + i] = field.w[i];
  }
  else if (m == "TA15N" || m == "TA15B") {
    // TA15 uses the direct Fortran parameter layout exposed by TA15Interface:
    //   PARMOD(1)=PDYN [nPa]
    //   PARMOD(2)=BYIMF [nT]
    //   PARMOD(3)=BZIMF [nT]
    //   PARMOD(4)=XIND  [dimensionless coupling index]
    // The remaining slots are left at zero for compatibility with the shared
    // 11-element AMPS-side PARMOD buffer.
    parmod[0] = field.pdyn_nPa;
    parmod[1] = field.imfBy_nT;
    parmod[2] = field.imfBz_nT;
    parmod[3] = field.xind;
  }
}

EarthUtil::TsDriverRecord EarthUtil::TsDriverTable::Lookup(double et) const {
  if (records_.empty())
    exit(__LINE__,__FILE__,"TsDriverTable::Lookup called on an empty table");

  if (et <= records_.front().et) {
    if (!clampWarnedLow_) {
      std::cerr << "[TsDriverTable] WARNING: time before table start ("
                << records_.front().timeUTC << "); clamping.\n";
      clampWarnedLow_ = true;
    }
    return records_.front();
  }
  if (et >= records_.back().et) {
    if (!clampWarnedHigh_) {
      std::cerr << "[TsDriverTable] WARNING: time after table end ("
                << records_.back().timeUTC << "); clamping.\n";
      clampWarnedHigh_ = true;
    }
    return records_.back();
  }

  // Binary search: records_[lo].et <= et < records_[lo+1].et
  std::size_t lo = 0, hi = records_.size() - 1;
  while (hi - lo > 1) {
    const std::size_t mid = (lo + hi) / 2;
    if (records_[mid].et <= et) lo = mid; else hi = mid;
  }

  const EarthUtil::TsDriverRecord& r0 = records_[lo];
  const EarthUtil::TsDriverRecord& r1 = records_[hi];
  const double dt = r1.et - r0.et;
  const double t  = (dt > 0.0) ? (et - r0.et) / dt : 0.0;

  EarthUtil::TsDriverRecord out;
  out.et       = et;
  out.timeUTC  = r0.timeUTC;
  out.imfBy_nT = Lerp(r0.imfBy_nT, r1.imfBy_nT, t);
  out.imfBz_nT = Lerp(r0.imfBz_nT, r1.imfBz_nT, t);
  out.swVx_kms = Lerp(r0.swVx_kms, r1.swVx_kms, t);
  out.swN_cm3  = Lerp(r0.swN_cm3,  r1.swN_cm3,  t);
  out.pdyn_nPa = Lerp(r0.pdyn_nPa, r1.pdyn_nPa, t);
  out.dst_nT   = Lerp(r0.dst_nT,   r1.dst_nT,   t);
  out.xind     = Lerp(r0.xind,     r1.xind,     t);
  for (int i = 0; i < 3; ++i) out.g[i] = Lerp(r0.g[i], r1.g[i], t);
  for (int i = 0; i < 6; ++i) out.w[i] = Lerp(r0.w[i], r1.w[i], t);
  for (int i = 0; i < 6; ++i) out.bzAvg[i] = Lerp(r0.bzAvg[i], r1.bzAvg[i], t);
  return out;
}

void EarthUtil::TsDriverTable::ApplyToField(const EarthUtil::TsDriverRecord& rec,
                                             EarthUtil::BackgroundField& field) {
  field.imfBy_nT = rec.imfBy_nT;
  field.imfBz_nT = rec.imfBz_nT;
  field.swVx_kms = rec.swVx_kms;
  field.swN_cm3  = rec.swN_cm3;
  field.pdyn_nPa = rec.pdyn_nPa;
  field.dst_nT   = rec.dst_nT;
  field.xind     = rec.xind;
  for (int i = 0; i < 3; ++i) field.g[i] = rec.g[i];
  for (int i = 0; i < 6; ++i) field.w[i] = rec.w[i];
  for (int i = 0; i < 6; ++i) field.bzAvg[i] = rec.bzAvg[i];
}

//======================================================================================
//======================================================================================
// NormalizeTsModelName
//======================================================================================
// PURPOSE
//   Map every known spelling of a Tsyganenko model name to the single canonical
//   string used throughout this codebase.  This centralises all alias handling
//   so that no downstream comparison needs to know about alternate spellings.
//
// WHY MULTIPLE SPELLINGS EXIST
//   The Tsyganenko models have been distributed with slightly different name
//   conventions over the years:
//     • CCMC Runs-on-Request uses "T96", "T05", "TS05" interchangeably depending
//       on the interface version.
//     • The ViRBO / Qin-Denton data portal labels the T05 model "TS05".
//     • Some older AMPS input files were generated with "T04S" (the Fortran
//       function name of the T05 external field is t04_s_).
//     • User-supplied input files may use any of the above, or variants like
//       "T05S", "TS04".
//   Rather than spreading if-chains throughout the code, we normalise once here
//   and everywhere else compares against the canonical form only.
//
// CANONICAL NAMES (matching the strings used in GetB_T / cFieldEvaluator)
//   "T96"   — Tsyganenko (1996)
//   "T05"   — Tsyganenko-Sitnov (2005); Fortran entry t04_s_
//   "T01"   — Tsyganenko (2001)
//   "TA15N" — Tsyganenko-Andreeva (2015), northward IMF variant
//   "TA15B" — Tsyganenko-Andreeva (2015), southward IMF variant
//   "TA16"  — Tsyganenko-Andreeva (2016) (future; Fortran not yet linked)
//   "IGRF"  — Geopack internal field only; no external-field call
//   "DIPOLE"— Analytic centred dipole; no external-field call
//
// EXTENSIBILITY
//   To add a new alias, append one line to the table below.
//   To add a new canonical model, add a new entry in GetB_T, RequiredColumnsForModel,
//   and cFieldEvaluator::ReinitGeopack, then optionally add aliases here.
static std::string NormalizeTsModelName(const std::string& raw) {
  const std::string u = ToUpper(Trim(raw));

  // T05 family — all refer to the Tsyganenko-Sitnov (2005) model whose
  // Fortran entry point is t04_s_.  "TS04" / "T04S" come from that name.
  if (u == "TS05" || u == "T05S" || u == "T04S" || u == "TS04") return "T05";

  // T96 family — alternate CCMC/ViRBO spellings.
  if (u == "TS96" || u == "T96S")                                 return "T96";

  // T01 family — Tsyganenko (2001).
  if (u == "TS01" || u == "T01S")                                 return "T01";

  // TA15 — two distinct sub-variants (northward / southward IMF orientation);
  // we keep the sub-variant suffix because they use different PARMOD layouts.
  if (u == "TA15N" || u == "TA15B")                               return u;

  // TA16 family — "TA16RBF" is the name used in AMPS_PARAM.in input files
  // to distinguish it from TA15; both map to the same canonical "TA16".
  if (u == "TA16" || u == "TA16RBF")                              return "TA16";

  // All other values (T96, T05, T01, DIPOLE, ...) are already canonical.
  return u;
}

//======================================================================================
//======================================================================================
// TsColMap — variable name (uppercase) -> 0-based column index
//======================================================================================
//
// WHY WE NEED THIS
//   Different versions of the Qin-Denton / ViRBO format, and driver files produced
//   by other sources (CCMC, custom scripts), may place the same physical quantity
//   in a different column.  Hardcoding column numbers (e.g. "Dst is always column
//   25") makes the loader fragile and produces silently wrong results whenever the
//   layout differs — which happens, for example, between the 1-minute and 5-minute
//   Qin-Denton products, or between different model driver formats (T96 vs T05 vs
//   TA15).
//
//   We avoid this entirely by parsing the JSON-like metadata block that every
//   ViRBO-format file embeds in its comment header.  That block explicitly maps
//   every variable NAME to its START_COLUMN, so the loader never has to guess.
//
// HEADER FORMAT (excerpt from qd_sep2017.txt)
//   The metadata block is delimited by comment lines:
//     # {
//     ...
//     # } End JSON
//
//   Inside, each variable is described by a JSON-like stanza, e.g.:
//
//     #  "NAME": "Pdyn",
//     #  "TITLE": "Solar Wind Dynamic Pressure",
//     #  "START_COLUMN": 11,
//     #  "UNITS": "nPa"
//
//   Vector (array) variables also carry an ELEMENT_NAMES list:
//
//     #  "NAME": "W",
//     #  "DIMENSION": [ 6 ],
//     #  "START_COLUMN": 32,
//     #  "ELEMENT_NAMES": [ "W1", "W2", "W3", "W4", "W5", "W6" ]
//
//   For vectors we expand the list into individual entries so that the caller can
//   look up any element by name: "W1"->32, "W2"->33, ..., "W6"->37.
//   The base name ("W") also gets an entry pointing at the first element.
//
// HOW PARSEDTSDRIVER HEADER BUILDS THE MAP
//   The function is a simple state machine:
//   • It accumulates (NAME, START_COLUMN, ELEMENT_NAMES) in three local variables.
//   • Whenever it sees a new NAME entry it "commits" the previous variable by
//     writing its entry (or entries, for vectors) into the map.
//   • At the end of the JSON block it commits whatever is still pending.
//   • Lines outside the JSON block and non-# data lines are ignored.
//
// THREAD SAFETY
//   TsColMap is a plain std::map<std::string,int>; it is built once by the parser
//   (single-threaded) and then used read-only by all MPI ranks/threads.
using TsColMap = std::map<std::string, int>;

//======================================================================================
// ParseTsDriverHeader
//======================================================================================
// Reads the JSON metadata block embedded in the comment header of a ViRBO-format
// driver file and returns a TsColMap mapping each variable name (uppercase) to its
// 0-based column index in the data rows.
//
// ARGUMENTS
//   fin      — open input stream positioned at the beginning of the file.
//   fileName — used only in warning messages.
//
// RETURN VALUE
//   A TsColMap with one entry per scalar variable and one entry per vector element
//   (plus one entry for the vector base name).  If no JSON block is found, an empty
//   map is returned and a warning is written to stderr.
//
// STATE MACHINE OVERVIEW
//   State variable: inJsonBlock (false until "# {" is seen)
//   Per-variable accumulator: currentName, currentStartCol, currentElements
//   The accumulator is committed to the map each time a new "NAME" line is seen,
//   ensuring that the final variable in the block is also written when "} End JSON"
//   is encountered.
//
// PARSING RULES
//   • Only '#'-prefixed lines are processed; the first non-# line ends the header.
//   • Three JSON keys are recognised (case-insensitive match on the uppercased line):
//       "NAME"          — begins a new variable entry; commits any pending entry first
//       "START_COLUMN"  — the 0-based column index of the first (or only) element
//       "ELEMENT_NAMES" — optional list of per-element names for array variables
//   • All other JSON keys (TITLE, LABEL, UNITS, DIMENSION, DESCRIPTION, ...) are
//     intentionally ignored; they carry no information needed by the loader.
//
// ROBUSTNESS
//   • The function does NOT require strict JSON: missing commas, extra whitespace,
//     and reordered keys within a variable block are all tolerated.
//   • If a variable block lacks a START_COLUMN it is silently skipped.
//   • If a variable block lacks ELEMENT_NAMES it is treated as a scalar.
static TsColMap ParseTsDriverHeader(std::istream& fin,
                                    const std::string& fileName) {
  TsColMap colMap;

  // ── Per-variable accumulator ──────────────────────────────────────────────
  // These three variables hold the state for the variable block currently being
  // parsed.  They are reset by commitCurrent() and updated incrementally as the
  // corresponding JSON keys are encountered.
  std::string currentName;             // text of the most recent "NAME" value
  int         currentStartCol = -1;    // value of "START_COLUMN" (-1 = not yet seen)
  std::vector<std::string> currentElements; // contents of "ELEMENT_NAMES" (empty = scalar)

  bool inJsonBlock = false; // true after "# {" has been seen
  bool headerDone  = false; // true once we have left the header section

  // ── Lambda: commitCurrent ─────────────────────────────────────────────────
  // Write the accumulated variable state into colMap and reset the accumulator.
  // Called each time a new "NAME" line is seen (before updating currentName) and
  // at the end of the JSON block.
  //
  // Scalar variable: one map entry   NAME -> startCol
  // Vector variable: one entry per   ELEMENT_NAME[k] -> (startCol + k)
  //                  plus a base     NAME -> startCol   (points at element 0)
  //
  // If currentName is empty or currentStartCol is -1 (incomplete block),
  // the function does nothing — this handles the first call before any NAME
  // has been seen, and any block whose START_COLUMN was missing.
  auto commitCurrent = [&]() {
    if (currentName.empty() || currentStartCol < 0) return;
    const std::string uName = ToUpper(currentName);
    if (currentElements.empty()) {
      // Scalar: single column entry.
      colMap[uName] = currentStartCol;
    } else {
      // Vector: one entry per named element at consecutive columns.
      for (int k = 0; k < static_cast<int>(currentElements.size()); ++k) {
        colMap[ToUpper(currentElements[k])] = currentStartCol + k;
      }
      // Base name also maps to the first element (column startCol + 0).
      colMap[uName] = currentStartCol;
    }
    // Reset for the next variable.
    currentName.clear();
    currentStartCol = -1;
    currentElements.clear();
  };

  // ── Lambda: extractQuotedValue ────────────────────────────────────────────
  // Extract the text between the first pair of double-quotes that follows the
  // colon on a line such as:
  //   #  "NAME": "ByIMF",
  // Returns "" if the expected pattern is not found.
  auto extractQuotedValue = [](const std::string& line) -> std::string {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) return "";
    const std::string after = line.substr(colon + 1);
    const std::size_t q1 = after.find('"');
    if (q1 == std::string::npos) return "";
    const std::size_t q2 = after.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";
    return after.substr(q1 + 1, q2 - q1 - 1);
  };

  // ── Lambda: extractIntValue ───────────────────────────────────────────────
  // Extract the integer that follows the colon on a line such as:
  //   #  "START_COLUMN": 7,
  // The trailing comma is stripped before parsing.  Returns -1 on failure.
  auto extractIntValue = [](const std::string& line) -> int {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) return -1;
    std::string after = line.substr(colon + 1);
    // Replace the first comma with a space so stoi/>> does not stumble on it.
    for (char& c : after) if (c == ',') { c = ' '; break; }
    std::istringstream iss(after);
    int v = -1;
    iss >> v;
    return v;
  };

  // ── Lambda: extractStringArray ────────────────────────────────────────────
  // Extract a JSON array of quoted strings from a line such as:
  //   #  "ELEMENT_NAMES": [ "W1", "W2", "W3", "W4", "W5", "W6" ],
  // Returns an empty vector if the '[' ... ']' delimiters are absent.
  // Scanning is character-by-character within the bracket content so that
  // element names containing digits (e.g. "G1") or underscores are preserved.
  auto extractStringArray = [](const std::string& line) -> std::vector<std::string> {
    std::vector<std::string> result;
    const std::size_t lb = line.find('[');
    const std::size_t rb = line.find(']');
    if (lb == std::string::npos || rb == std::string::npos || rb <= lb) return result;
    const std::string inner = line.substr(lb + 1, rb - lb - 1);
    std::size_t pos = 0;
    while (pos < inner.size()) {
      const std::size_t q1 = inner.find('"', pos);
      if (q1 == std::string::npos) break;
      const std::size_t q2 = inner.find('"', q1 + 1);
      if (q2 == std::string::npos) break;
      result.push_back(inner.substr(q1 + 1, q2 - q1 - 1));
      pos = q2 + 1;
    }
    return result;
  };

  // ── Main scan loop ────────────────────────────────────────────────────────
  // We read line-by-line.  Only '#'-prefixed lines are part of the header;
  // the first non-# line means we have left the header section entirely.
  std::string line;
  while (!headerDone && std::getline(fin, line)) {
    if (line.empty() || line[0] != '#') {
      // This is the first data row (or a blank non-comment line).
      // We cannot "put back" a line with std::istream, so we simply mark
      // the header as done.  The caller opens a fresh stream for the data pass.
      headerDone = true;
      break;
    }

    // Strip the leading '#' and trim whitespace.
    const std::string stripped = Trim(line.substr(1));

    // Detect the JSON block boundaries.
    if (stripped == "{") { inJsonBlock = true; continue; }
    if (!inJsonBlock) continue;  // outside JSON block — skip source-file comments etc.
    if (stripped.find("} End JSON") != std::string::npos ||
        stripped.find("}") == 0) {
      // End of JSON block.  Commit the last pending variable and stop.
      commitCurrent();
      headerDone = true;
      break;
    }

    // Inside the JSON block: inspect this line for the three keys we care about.
    // We uppercase the line for the key search so the match is case-insensitive,
    // but pass the original 'stripped' to the extraction helpers so quoted values
    // are not accidentally uppercased.
    const std::string ustripped = ToUpper(stripped);

    if (ustripped.find("\"NAME\"") != std::string::npos) {
      // A new variable is starting.  Commit whatever was being accumulated for
      // the previous variable before resetting the accumulator.
      commitCurrent();
      currentName = extractQuotedValue(stripped);
    }
    else if (ustripped.find("\"START_COLUMN\"") != std::string::npos) {
      // The 0-based column index of this variable's first (or only) element.
      currentStartCol = extractIntValue(stripped);
    }
    else if (ustripped.find("\"ELEMENT_NAMES\"") != std::string::npos) {
      // Optional: names for each component of a vector variable.
      // If present, commitCurrent will create one map entry per element.
      currentElements = extractStringArray(stripped);
    }
    // All other JSON keys (TITLE, LABEL, UNITS, DIMENSION, DESCRIPTION,
    // VALUES, ...) are ignored.
  }

  // Commit any variable whose closing brace may have been the end-of-file.
  commitCurrent();

  if (colMap.empty() && !fileName.empty()) {
    std::cerr << "[TsDriverTable] WARNING: no column-map entries found in header of '"
              << fileName << "'. Proceeding without header-driven column detection.\n";
  }
  return colMap;
}

//======================================================================================
// ParseSimpleTsDriverHeader
//======================================================================================
// Fallback for AMPS wizard style plain-text tables whose second comment line is a
// whitespace-separated list of column names, for example:
//   # YYYY-MM-DDTHH:MM:SS Bx By Bz Vx Vy Vz Np Temp SYM-H IMFflag SWflag Tilt Pdyn W1 ...
static TsColMap ParseSimpleTsDriverHeader(const std::string& fileName) {
  std::ifstream fin(fileName);
  if (!fin.is_open()) return {};

  std::string line;
  while (std::getline(fin, line)) {
    const std::string t = Trim(line);
    if (t.empty() || t[0] != '#') continue;
    const std::string body = Trim(t.substr(1));
    if (body.find("YYYY-MM-DDTHH:MM:SS") == std::string::npos) continue;

    std::istringstream iss(body);
    std::vector<std::string> hdr;
    for (std::string tok; iss >> tok; ) hdr.push_back(tok);
    if (hdr.empty()) return {};

    TsColMap colMap;
    for (int i = 0; i < static_cast<int>(hdr.size()); ++i) {
      // Strip any trailing unit suffix of the form "[...]" that the AMPS wizard
      // appends to column names (e.g. "By[nT]" -> "By", "Vsw[km/s]" -> "Vsw",
      // "Pdyn[nPa]" -> "Pdyn", "Dst[nT]" -> "Dst").  Tokens without a bracket
      // (e.g. "G1", "YYYY-MM-DDTHH:MM:SS") are unaffected.
      const std::string raw  = hdr[i];
      const std::size_t brk  = raw.find('[');
      const std::string base = (brk != std::string::npos) ? raw.substr(0, brk) : raw;
      const std::string u    = ToUpper(base);

      if (u == "YYYY-MM-DDTHH:MM:SS") {
        colMap["DATETIME"] = i;
        colMap["ISODATETIME"] = i;
      }
      // IMF components — ViRBO alias (By/Bz) and AMPS wizard direct names (ByIMF/BzIMF).
      else if (u == "BY" || u == "BYIMF") colMap["BYIMF"] = i;
      else if (u == "BZ" || u == "BZIMF") colMap["BZIMF"] = i;
      // Solar wind speed — ViRBO uses the x-component "Vx"; AMPS wizard writes
      // the scalar magnitude "Vsw".  Both map to the internal key VSW.
      else if (u == "VX" || u == "VSW") colMap["VSW"] = i;
      // Proton density.
      else if (u == "NP") colMap["DEN_P"] = i;
      // Dst index — AMPS wizard writes "Dst" directly; ViRBO files use SYM-H/SYMH.
      // Plain SYM-H is mapped to DST.  The corrected SymH value (SymHc) that
      // TA16 needs as PARMOD(2) is kept under its own key so the loader can
      // prefer it over the raw DST when both are present.
      else if (u == "DST" || u == "SYM-H" || u == "SYMH") colMap["DST"] = i;
      else if (u == "SYMHC") colMap["SYMHC"] = i;
      else if (u == "PDYN") colMap["PDYN"] = i;
      // N-index is the Newell optimal solar-wind coupling index, written as
      // "N-index" by the AMPS wizard.  It maps to XIND (TA15/TA16 PARMOD slot).
      // The hyphen in "N-index" survives ToUpper as "N-INDEX".
      else if (u == "N-INDEX" || u == "NINDEX" || u == "XIND") colMap["XIND"] = i;
      else if (u == "G1" || u == "G2" || u == "G3" ||
               u == "W1" || u == "W2" || u == "W3" || u == "W4" || u == "W5" || u == "W6" ||
               u == "BZ1" || u == "BZ2" || u == "BZ3" || u == "BZ4" || u == "BZ5" || u == "BZ6" ||
               u == "XIND") {
        colMap[u] = i;
      }
    }
    return colMap;
  }
  return {};
}

//======================================================================================
// RequiredColumnsForModel
//======================================================================================
// PURPOSE
//   Returns the set of TsColMap variable names (uppercase) that must be present
//   in the driver file to drive the named Tsyganenko model.  Called by
//   ValidateTsDriverColumns to produce a clear, model-specific error message when
//   a required column is absent.
//
// ARGUMENT
//   model — canonical model name (output of NormalizeTsModelName, already uppercase).
//
// VARIABLE NAME CONVENTIONS
//   The names in the returned vector must match the keys in the TsColMap produced
//   by ParseTsDriverHeader, which are the uppercase "NAME" values from the JSON
//   metadata block.  Standard Qin-Denton / ViRBO mappings:
//
//     ViRBO NAME   TsColMap key   Physical meaning
//     ----------   ------------   -----------------------------------------
//     ByIMF        BYIMF          IMF Y-component [nT]
//     BzIMF        BZIMF          IMF Z-component [nT]
//     Vsw          VSW            Solar wind speed magnitude [km/s]
//     Den_P        DEN_P          Solar wind proton density [cm^-3]
//     Pdyn         PDYN           Solar wind dynamic pressure [nPa]
//     Dst          DST            Dst index [nT]
//     G1..G3       G1..G3         T01 storm-activity G-indices
//     W1..W6       W1..W6         T05/TA15/TA16 storm-time history integrals
//     Bz1..Bz6     BZ1..BZ6       TA15 |Bz_south| running averages [nT]
//
// HOW TO ADD A NEW MODEL
//   1. Add an `if (m == "MODELNAME")` block.
//   2. Build the required list from swBase and any additional variable names.
//   3. Optionally register aliases in NormalizeTsModelName.
//   4. Add the Fortran call in cFieldEvaluator::GetB_T.
//   5. Update PARMOD loading in cFieldEvaluator constructor and ReinitGeopack.
static std::vector<std::string> RequiredColumnsForModel(const std::string& model) {
  // Shared by all external Tsyganenko models: IMF, solar wind, Dst.
  // These are the minimum inputs needed to call any T96/T01/T05/TA15/TA16
  // Fortran routine.
  static const std::vector<std::string> swBase = {
    "BYIMF",  // IMF By [nT]
    "BZIMF",  // IMF Bz [nT]
    "VSW",    // Solar wind speed [km/s] (positive magnitude in file)
    "DEN_P",  // Solar wind proton density [cm^-3]
    "PDYN",   // Solar wind dynamic pressure [nPa]
    "DST"     // Dst index [nT]
  };

  // W1..W6: storm-time history integrals introduced by T05.
  // They encode the time-integrated energy injection into each internal current
  // system (ring current, partial ring current, field-aligned, Chapman-Ferraro,
  // tail) and are also required by TA15 and TA16.
  static const std::vector<std::string> wIntegrals = {
    "W1", "W2", "W3", "W4", "W5", "W6"
  };

  const std::string m = ToUpper(Trim(model));

  // DIPOLE and IGRF are internal-field-only models.  Neither consumes a
  // time-dependent solar-wind/Tsyganenko driver table.
  if (m == "DIPOLE" || m == "IGRF") return {};

  // T96: driven by [Pdyn, Dst, By, Bz] only.
  // PARMOD: [0]=Pdyn [1]=Dst [2]=By [3]=Bz [4..10]=0
  // VSW and DEN_P are NOT arguments to the T96 Fortran routine; do not include
  // them in the required set.  AMPS wizard T96 driver files legitimately omit
  // those columns, and requiring them produces a spurious fatal validation error.
  if (m == "T96") return {"BYIMF", "BZIMF", "PDYN", "DST"};

  // T01: same as T96 plus the G-indices G1, G2, G3 that capture the recent
  // history of strong southward Bz intervals and their effect on the ring current.
  // PARMOD: [0]=Pdyn [1]=Dst [2]=By [3]=Bz [4]=G1 [5]=G2 [6]=G3 [7..10]=0
  if (m == "T01") {
    std::vector<std::string> req = swBase;
    req.insert(req.end(), {"G1", "G2", "G3"});
    return req;
  }

  // T05: adds the six W storm-time integrals to the base set.
  // PARMOD: [0]=Pdyn [1]=Dst [2]=By [3]=Bz [4]=W1 .. [9]=W6 [10]=0
  if (m == "T05") {
    std::vector<std::string> req = swBase;
    req.insert(req.end(), wIntegrals.begin(), wIntegrals.end());
    return req;
  }

  // TA15 (Tsyganenko-Andreeva 2015): two sub-variants (northward / southward IMF).
  // In addition to the W-integrals it needs BZ1..BZ6 — the mean |Bz_south| over
  // 1, 2, 3, 4, 5, 6 hours before the observation epoch — and XIND, the
  // Newell-type dimensionless coupling index that maps directly to PARMOD(4) in
  // the TA15 Fortran call (see BuildTsParmod: parmod[3] = field.xind).
  //
  // WHY XIND MUST BE VALIDATED HERE
  // ---------------------------------
  // ExtractColumn() returns 0.0 (its default) whenever a column is absent from
  // the column map.  For XIND this means a missing column silently produces
  // parmod[3] = 0 for every time step -- a physically wrong value for any
  // geomagnetically active interval, with no diagnostic to alert the user.
  // Including XIND in the required set converts this silent wrong-value condition
  // into a descriptive fatal error at load time, consistent with how all other
  // required columns are handled throughout this function.
  if (m == "TA15N" || m == "TA15B") {
    std::vector<std::string> req = swBase;
    req.insert(req.end(), wIntegrals.begin(), wIntegrals.end());
    req.insert(req.end(), {"BZ1", "BZ2", "BZ3", "BZ4", "BZ5", "BZ6"});
    req.push_back("XIND");   // Newell coupling index -> parmod[3] in BuildTsParmod
    return req;
  }

  // TA16 (Tsyganenko-Andreeva 2016).
  // PARMOD layout: [PDYN, SymHc, XIND, BYIMF].  Only the first 4 PARMOD slots
  // are used — W1..W6 (PARMOD 5..10) are a T05 concept and are NOT read by TA16.
  // Required: PDYN, BYIMF, and either SYMHC (preferred, corrected SymH) or DST
  //   (raw SYM-H, acceptable proxy).
  // Optional: XIND (Newell coupling index) — defaults to 0.0 if the column is
  //   absent, which is physically reasonable for quiet-time runs.
  if (m == "TA16") {
    // We cannot express "SYMHC or DST" as a hard requirement in this function,
    // so we require DST which covers both: ParseSimpleTsDriverHeader maps both
    // the raw SYM-H and SymHc columns to usable keys, and the loader prefers
    // SYMHC when present (see LoadTsDriverFile).
    return {"PDYN", "BYIMF", "DST"};
  }

  // Unknown model — skip validation and let the solver report the problem.
  std::cerr << "[TsDriverTable] WARNING: no required-column definition for model '"
            << model << "'. Column validation will be skipped.\n";
  return {};
}
//======================================================================================
// ValidateTsDriverColumns
//======================================================================================
// PURPOSE
//   Verify that every column required by `model` is present in `colMap`.
//   Called once after ParseTsDriverHeader, before any data rows are read, so
//   the user gets a clear error message at load time rather than a silent wrong
//   value during the trajectory run.
//
// ERROR MESSAGE
//   On failure the function lists:
//   • each missing column by name, and
//   • all available column names (from the file header) so the user can see
//     whether the file uses a different spelling or simply lacks the variable.
//
// DESIGN NOTE
//   We exit rather than throw because a missing required column is always a
//   configuration error that cannot be recovered from at runtime.  Continuing
//   with a default of zero for, say, Dst or W1 would produce physically wrong
//   cutoff rigidities with no diagnostic.
static void ValidateTsDriverColumns(const TsColMap& colMap,
                                    const std::string& model,
                                    const std::string& fileName) {
  const std::vector<std::string> required = RequiredColumnsForModel(model);

  // Collect all missing names in one pass so the error message is complete.
  // Special case: "DST" is satisfied by either the "DST" key (raw SYM-H) or
  // the "SYMHC" key (corrected SymH used by TA16), because the loader already
  // prefers SYMHC over DST when both are present.
  std::vector<std::string> missing;
  for (const auto& name : required) {
    if (colMap.find(name) != colMap.end()) continue;
    if (name == "DST" && colMap.find("SYMHC") != colMap.end()) continue;
    missing.push_back(name);
  }
  if (missing.empty()) return;  // all required columns present — OK

  std::ostringstream _m;
  _m << "TS_INPUT_FILE '" << fileName << "' is missing columns required by "
     << "FIELD_MODEL=" << model << ":\n";
  for (const auto& n : missing) _m << "  " << n << "\n";
  _m << "Available columns detected in file header:";
  for (const auto& kv : colMap) _m << " " << kv.first;
  exit(__LINE__,__FILE__,_m.str().c_str());
}
//======================================================================================
// ExtractColumn
//======================================================================================
// PURPOSE
//   Safely retrieve one double value from a pre-tokenised data row using the
//   variable name as a lookup key in the TsColMap.
//
// ARGUMENTS
//   toks       — whitespace-split tokens of the current data line (0-based).
//   colMap     — column map produced by ParseTsDriverHeader.
//   varName    — variable name to look up (case-insensitive; ToUpper applied).
//   defaultVal — value returned when the variable is absent from the map or the
//                token at the mapped column cannot be converted to double.
//
// WHEN DEFAULTVAL IS RETURNED
//   1. varName not in colMap — column does not exist in this file.
//   2. Mapped column index is out of range for this row — row is shorter than
//      expected (can happen for the last line if the file is truncated).
//   3. std::stod throws — the token is not a valid floating-point number.
//
//   Cases (1) and (2) are expected only for optional variables (e.g. G1..G3 in
//   a T96 driver file that was also run through a T01 validation check).  For
//   required columns, ValidateTsDriverColumns has already verified their presence,
//   so case (1) should never occur for those variables at this point.
//
// THREAD SAFETY
//   colMap is read-only; toks is a local copy.  Safe to call concurrently.
static double ExtractColumn(const std::vector<std::string>& toks,
                            const TsColMap& colMap,
                            const std::string& varName,
                            double defaultVal = 0.0) {
  const auto it = colMap.find(ToUpper(varName));
  if (it == colMap.end()) return defaultVal;           // variable not in this file
  const int col = it->second;
  if (col < 0 || col >= static_cast<int>(toks.size())) return defaultVal; // row too short
  try { return std::stod(toks[static_cast<std::size_t>(col)]); }
  catch (...) { return defaultVal; }                   // non-numeric token
}
//======================================================================================
// LoadTsDriverFile
//======================================================================================
// PURPOSE
//   Parse a Qin-Denton / ViRBO formatted driver file and return a populated
//   TsDriverTable suitable for per-trajectory-point interpolation of Tsyganenko
//   model driving parameters.
//
// TWO-PASS ALGORITHM
//
//   Pass 1 — Header scan
//     Open the file once and call ParseTsDriverHeader to build a TsColMap.
//     This maps every variable name (uppercase) to its 0-based column index in
//     the data rows.  Close the stream immediately after the header scan.
//
//   Pass 2 — Data rows
//     Re-open the file and iterate over all lines.  For each non-comment,
//     non-empty line:
//       • Strip '#' and '!' comment characters.
//       • Split into whitespace-separated tokens.
//       • Extract the ISO-8601 timestamp from the column identified in Pass 1.
//       • Reject lines whose timestamp token contains no 'T' (ISO-8601 separator),
//         which filters out residual header lines that survived comment stripping.
//       • Convert the timestamp to SPICE ET (exits if SPICE is unavailable).
//       • Use ExtractColumn to read each physics parameter by name from the TsColMap.
//       • Construct a TsDriverRecord and append it to the table.
//
//   After Pass 2, ValidateTsDriverColumns is called between the passes to ensure
//   that every column required by `modelName` is present before any data is read.
//
// ARGUMENTS
//   fileName  — path to the driver file (from TS_INPUT_FILE in #TEMPORAL).
//   modelName — canonical model name (from NormalizeTsModelName); used only to
//               select the correct required-column set for validation.
//
// COLUMN EXTRACTION DETAILS
//   BYIMF / BZIMF     — IMF components [nT]; stored directly.
//   VSW               — solar wind speed [km/s]; the file stores a positive
//                       magnitude, but the solver convention for swVx_kms is
//                       negative (sunward = -X_GSM direction).  We apply
//                       -abs() so the sign is always correct regardless of
//                       whether a particular file happens to include a sign.
//   DEN_P             — proton density [cm^-3].
//   PDYN              — dynamic pressure [nPa].
//   DST               — Dst index [nT].
//   W1..W6            — T05 storm-time integrals; default to 0.0 if not present
//                       in the file (relevant for T96 driver files used with
//                       a T05 run — caught by ValidateTsDriverColumns, but
//                       ExtractColumn's default provides a safe fallback).
//
// TIMESTAMP DETECTION
//   The TsColMap lookup for the timestamp column tries the following variable
//   names in order: ISODATETIME, DATETIME, TIME, DATE_TIME.  If none are found
//   in the header, column 0 is used as a fallback.
//
// THREAD SAFETY
//   This function is called once from ParseAmpsParamFile (single-threaded) and
//   the resulting TsDriverTable is then used read-only by MPI workers.
static EarthUtil::TsDriverTable LoadTsDriverFile(const std::string& fileName,
                                                  const std::string& modelName) {
  // -----------------------------------------------------------------------
  // Pass 1: Build the column map from the file header.
  // -----------------------------------------------------------------------
  TsColMap colMap;
  {
    std::ifstream hdr(fileName);
    if (!hdr.is_open()) {
      std::ostringstream _m; _m << "Cannot open TS_INPUT_FILE: " << fileName;
      exit(__LINE__,__FILE__,_m.str().c_str());
    }
    colMap = ParseTsDriverHeader(hdr, fileName);
    // hdr is closed here (RAII).
  }
  if (colMap.empty()) colMap = ParseSimpleTsDriverHeader(fileName);

  // Validate that every column required by this model is present before
  // reading a single data row.  Fails with a descriptive error listing the
  // missing columns and all available columns from the header.
  ValidateTsDriverColumns(colMap, modelName, fileName);

  // Resolve the 0-based column index for the ISO-8601 timestamp.
  // Try standard ViRBO name variants in priority order.
  int colTime = -1;
  for (const auto& candidate : {"ISODATETIME", "DATETIME", "TIME", "DATE_TIME"}) {
    const auto it = colMap.find(std::string(candidate));
    if (it != colMap.end()) { colTime = it->second; break; }
  }
  if (colTime < 0) {
    // No timestamp column found in header — fall back to column 0 (the standard
    // first column in all known ViRBO files).
    colTime = 0;
    std::cerr << "[TsDriverTable] WARNING: no timestamp column identified in '"
              << fileName << "' header; defaulting to column 0.\n";
  }

  // -----------------------------------------------------------------------
  // Pass 2: Read data rows.
  // -----------------------------------------------------------------------
  std::ifstream fin(fileName);
  if (!fin.is_open()) {
    std::ostringstream _m; _m << "Cannot open TS_INPUT_FILE: " << fileName;
    exit(__LINE__,__FILE__,_m.str().c_str());
  }

  EarthUtil::TsDriverTable table;
  std::string line;
  int lineNo = 0, loaded = 0, skipped = 0;

  while (std::getline(fin, line)) {
    ++lineNo;

    // Strip comment text.  In trajectory / driver files both '#' and '!'
    // introduce comments to end-of-line.  We take the earlier delimiter.
    {
      const std::size_t ph = line.find('#');
      const std::size_t pe = line.find('!');
      const std::size_t pcut = std::min(ph, pe);
      if (pcut != std::string::npos) line = line.substr(0, pcut);
    }
    line = Trim(line);
    if (line.empty()) continue;  // blank or comment-only line

    // Tokenise the remaining text on whitespace.
    std::vector<std::string> toks;
    {
      std::istringstream iss(line);
      std::string t;
      while (iss >> t) toks.push_back(t);
    }
    if (toks.empty()) continue;

    // Extract the timestamp from the column identified above.
    const std::string timeUTC =
        (colTime < static_cast<int>(toks.size()))
          ? toks[static_cast<std::size_t>(colTime)]
          : "";

    // Reject lines that do not look like ISO-8601 timestamps (e.g. column-name
    // rows that survived comment stripping because they had no comment prefix).
    // ISO-8601 requires a 'T' separator between date and time.
    if (timeUTC.find('T') == std::string::npos) { ++skipped; continue; }

    // Convert ISO-8601 to SPICE ET (seconds past J2000).
    // We need ET for two purposes:
    //   (a) storing a monotone numeric time for binary-search in TsDriverTable::Lookup,
    //   (b) potential future use in coordinate transforms.
    double et = 0.0;
    if (!ParseIsoUtcToEt(timeUTC, et)) {
#ifdef _NO_SPICE_CALLS_
      // SPICE is required for timestamp conversion.  Without it the table
      // would have et=0 for every record, making Lookup useless.
      exit(__LINE__,__FILE__,
           "LoadTsDriverFile requires SPICE for timestamp conversion "
           "but the build has _NO_SPICE_CALLS_ defined");
#else
      // SPICE is present but this specific string failed.  Skip the row with
      // a warning; do not abort the entire load.
      std::cerr << "[TsDriverTable] WARNING: cannot parse timestamp '"
                << timeUTC << "' at line " << lineNo
                << " of " << fileName << "; skipping.\n";
      ++skipped; continue;
#endif
    }

    // Extract each physics parameter by variable name.
    // ExtractColumn returns defaultVal (0.0) for any column absent from the
    // TsColMap.  For required columns this should never occur here because
    // ValidateTsDriverColumns has already guaranteed their presence.
    EarthUtil::TsDriverRecord rec;
    rec.et       = et;
    rec.timeUTC  = timeUTC;
    rec.imfBy_nT = ExtractColumn(toks, colMap, "BYIMF");
    rec.imfBz_nT = ExtractColumn(toks, colMap, "BZIMF");
    // Vsw in the file is always a positive speed magnitude (e.g. 587.5 km/s).
    // The solver stores swVx_kms as a signed velocity in GSM X: negative
    // because the solar wind flows anti-sunward (-X_GSM).
    rec.swVx_kms = -std::abs(ExtractColumn(toks, colMap, "VSW"));
    rec.swN_cm3  = ExtractColumn(toks, colMap, "DEN_P");
    rec.pdyn_nPa = ExtractColumn(toks, colMap, "PDYN");
    // Prefer the corrected SymH index (SymHc) over the raw Dst/SYM-H when both
    // columns are present.  TA16 uses SymHc as PARMOD(2); T96/T05 use plain Dst.
    // Files that have only one of the two still work correctly via the fallback.
    rec.dst_nT   = colMap.count("SYMHC")
                     ? ExtractColumn(toks, colMap, "SYMHC")
                     : ExtractColumn(toks, colMap, "DST");
    rec.xind     = ExtractColumn(toks, colMap, "XIND");
    for (int i = 0; i < 3; ++i) {
      const std::string gKey = "G" + std::to_string(i + 1);
      rec.g[i] = ExtractColumn(toks, colMap, gKey);
    }
    // W1..W6 are stored in the array rec.w[0..5].
    for (int i = 0; i < 6; ++i) {
      const std::string wKey = "W" + std::to_string(i + 1);
      rec.w[i] = ExtractColumn(toks, colMap, wKey);
      const std::string bzKey = "BZ" + std::to_string(i + 1);
      rec.bzAvg[i] = ExtractColumn(toks, colMap, bzKey);
    }

    table.push_back(rec);
    ++loaded;
  }

  if (table.empty()) {
    std::ostringstream _m;
    _m << "LoadTsDriverFile: no valid records in '" << fileName
       << "' (lines=" << lineNo << ", skipped=" << skipped << ")";
    exit(__LINE__,__FILE__,_m.str().c_str());
  }

  std::cout << "[TsDriverTable] Loaded " << loaded << " records from '"
            << fileName << "'"
            << (skipped > 0 ? " (" + std::to_string(skipped) + " lines skipped)" : "")
            << "\n";
  return table;
}


DirectionalAperture ParseDirectionalApertureSpec(const std::string& text) {
  // Keep this parser deliberately independent of GOES/C19 naming.  A specification
  // describes one arbitrary finite-FOV instrument look direction in a frame understood
  // by the cutoff solver.  Commas are accepted as whitespace so the same compact form
  // is convenient in hand-written input files, generated files, and shell arguments.
  std::string normalized=text;
  for (char& c : normalized) if (c==',') c=' ';
  std::istringstream iss(normalized);
  DirectionalAperture a;
  if (!(iss >> a.name >> a.frame
            >> a.boresight.x >> a.boresight.y >> a.boresight.z
            >> a.up.x >> a.up.y >> a.up.z
            >> a.horizontalHalfAngle_deg >> a.verticalHalfAngle_deg)) {
    throw std::runtime_error(
        "Invalid DIRMAP_APERTURE specification. Expected: "
        "<name> <SM|GSM|LOCAL_SM> bx by bz ux uy uz hHalfDeg vHalfDeg; got '"+
        Trim(text)+"'");
  }
  // Optional location association for batched trajectory runs.  Requiring the
  // self-describing LOCATION=<index> spelling prevents an accidental extra numeric
  // column in a legacy file from silently changing pruning semantics.
  std::string extra;
  if (iss >> extra) {
    const std::string prefix="LOCATION=";
    const std::string upper=ToUpper(extra);
    if (upper.find(prefix)!=0 || extra.size()<=prefix.size()) {
      throw std::runtime_error("Unexpected extra token '"+extra+
                               "' in DIRMAP_APERTURE specification '"+Trim(text)+
                               "'. The only optional token is LOCATION=<zero-based-index>");
    }
    const std::string indexText=extra.substr(prefix.size());
    std::size_t used=0;
    try {
      a.locationIndex=std::stoi(indexText,&used);
    }
    catch (...) {
      throw std::runtime_error("Invalid DIRMAP_APERTURE location selector '"+extra+"'");
    }
    if (used!=indexText.size() || a.locationIndex<0)
      throw std::runtime_error("DIRMAP_APERTURE LOCATION must be a non-negative integer: '"+extra+"'");
    if (iss >> extra)
      throw std::runtime_error("Unexpected extra token '"+extra+
                               "' in DIRMAP_APERTURE specification '"+Trim(text)+"'");
  }

  a.name=Trim(a.name);
  a.frame=ToUpper(Trim(a.frame));
  if (a.name.empty()) throw std::runtime_error("DIRMAP_APERTURE name must not be empty");
  if (!(a.frame=="SM" || a.frame=="GSM" || a.frame=="LOCAL_SM" || a.frame=="LOCAL"))
    throw std::runtime_error("DIRMAP_APERTURE frame must be SM, GSM, or LOCAL_SM");
  if (a.frame=="LOCAL") a.frame="LOCAL_SM";

  auto norm3=[](const Vec3& v) {
    return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
  };
  if (!(norm3(a.boresight)>0.0) || !std::isfinite(norm3(a.boresight)))
    throw std::runtime_error("DIRMAP_APERTURE boresight must be finite and non-zero");
  if (!(norm3(a.up)>0.0) || !std::isfinite(norm3(a.up)))
    throw std::runtime_error("DIRMAP_APERTURE up vector must be finite and non-zero");
  if (!(a.horizontalHalfAngle_deg>0.0 && a.horizontalHalfAngle_deg<=90.0 &&
        a.verticalHalfAngle_deg>0.0 && a.verticalHalfAngle_deg<=90.0 &&
        std::isfinite(a.horizontalHalfAngle_deg) &&
        std::isfinite(a.verticalHalfAngle_deg)))
    throw std::runtime_error("DIRMAP_APERTURE half-angles must be finite and in (0,90] degrees");

  // The solvers intentionally perform the final projection/orthogonalization because
  // GSM and LOCAL_SM vectors must first be transformed into the map-label frame.
  return a;
}

std::vector<DirectionalAperture> LoadDirectionalApertureFile(const std::string& fileName) {
  std::ifstream fin(fileName);
  if (!fin.is_open())
    throw std::runtime_error("Cannot open DIRMAP_APERTURE_FILE: "+fileName);

  std::vector<DirectionalAperture> result;
  std::string line;
  int lineNo=0;
  while (std::getline(fin,line)) {
    ++lineNo;
    // Both '#' and '!' start comments in the standalone aperture-list format.
    std::size_t cut=std::string::npos;
    const std::size_t h=line.find('#');
    const std::size_t b=line.find('!');
    if (h!=std::string::npos) cut=h;
    if (b!=std::string::npos) cut=(cut==std::string::npos ? b : std::min(cut,b));
    if (cut!=std::string::npos) line=line.substr(0,cut);
    line=Trim(line);
    if (line.empty()) continue;
    try {
      result.push_back(ParseDirectionalApertureSpec(line));
    }
    catch (const std::exception& e) {
      std::ostringstream out;
      out << "DIRMAP_APERTURE_FILE " << fileName << " line " << lineNo
          << ": " << e.what();
      throw std::runtime_error(out.str());
    }
  }
  if (result.empty())
    throw std::runtime_error("DIRMAP_APERTURE_FILE contains no aperture definitions: "+fileName);
  return result;
}

AmpsParam ParseAmpsParamFile(const std::string& fileName) {
  std::ifstream fin(fileName);
  if (!fin.is_open()) {
    { std::ostringstream _exit_msg; _exit_msg << "Cannot open input file: "+fileName; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
  }

  AmpsParam p;

  std::string section;
  bool inPointsBlock=false;
  bool inChannelsBlock=false;

  std::string line;
  int lineNo=0;
  while (std::getline(fin,line)) {
    ++lineNo;

    // Preserve the original line because unit hints may be encoded in the
    // trailing comment after '!'. Example: DOMAIN_X_MAX 20 ! Re
    const std::string rawLine=line;
    std::string commentText;
    SplitCodeAndComment(rawLine,line,commentText);
    line=Trim(line);
    commentText=Trim(commentText);
    if (line.empty()) continue;

    // Section header.  Section names are keywords too; reject misspelled or
    // unsupported section-keywords immediately instead of silently collecting
    // ignored keys.  Lines such as "# ----" or "# explanatory comment" are
    // comments, not section keywords, and are skipped.
    if (!line.empty() && line[0]=='#') {
      if (line.size()==1) continue;
      const std::string candidateSection=ToUpper(Trim(line));
      if (IsRecognizedAmpsParamSection(candidateSection)) {
        section=candidateSection;
        continue;
      }

      // Treat common hash-comment forms as comments.  This preserves the many
      // explanatory banner/comment lines in existing AMPS_PARAM.in files while
      // still catching typo-like section names such as #PARTICLE_TRAJECTORYS.
      const char c1 = line[1];
      const bool hashComment =
          std::isspace(static_cast<unsigned char>(c1)) ||
          c1=='=' || c1=='-' || c1=='!' || c1=='#';
      if (hashComment) continue;

      FailUnknownSection(fileName,lineNo,candidateSection,rawLine);
    }

    // Block delimiters for POINTS
    if (ToUpper(line)=="POINTS_BEGIN") { inPointsBlock=true; continue; }
    if (ToUpper(line)=="POINTS_END") { inPointsBlock=false; continue; }

    // Block delimiters for ENERGY_CHANNELS
    if (ToUpper(line)=="CH_BEGIN") { inChannelsBlock=true; continue; }
    if (ToUpper(line)=="CH_END")   { inChannelsBlock=false; continue; }

    if (inPointsBlock) {
      // Accept both the original bare format "x y z" and the newer "POINT x y z"
      // form (which was added to allow inline units on each token, e.g. POINT 1Re 0 0).
      // The presence of the "POINT" keyword is detected by trying to read the first
      // token: if it converts to a double it is the x-coordinate; otherwise it must
      // be the literal string "POINT" (anything else is an error).
      std::istringstream iss(line);
      std::string firstTok;
      iss >> firstTok;

      std::string xTok, yTok, zTok;
      if (ToUpper(firstTok) == "POINT") {
        // New format: "POINT x y z" — first token is the keyword.
        // Coordinates may carry inline units (e.g. POINT 1Re 0 0) or inherit
        // units from the trailing comment (e.g. POINT 1 0 0 ! Re).
        if (!(iss >> xTok >> yTok >> zTok)) {
          { std::ostringstream _exit_msg; _exit_msg << "Cannot parse POINT coordinates at line "+std::to_string(lineNo)+": "+line; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
        }
      } else {
        // Original format: "x y z" — first token is already the x-coordinate.
        xTok = firstTok;
        if (!(iss >> yTok >> zTok)) {
          { std::ostringstream _exit_msg; _exit_msg << "Cannot parse point coordinates at line "+std::to_string(lineNo)+": "+line; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
        }
      }

      Vec3 v;
      {
        std::vector<double> xyz = ParseLengthListToKm(xTok+" "+yTok+" "+zTok, commentText);
        if (xyz.size()!=3) { std::ostringstream _exit_msg; _exit_msg << "Internal error parsing POINT at line "+std::to_string(lineNo); exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
        v.x=xyz[0]; v.y=xyz[1]; v.z=xyz[2];
      }
      p.output.points.push_back(v);
      continue;
    }

    // ----- ENERGY_CHANNELS block: each line is  NAME  E1_MeV  E2_MeV  -----
    // PHYSICS:
    //   Each channel defines an energy sub-range for integral-flux output.
    //   The solver will integrate  F_ch = 4pi * integral[E1,E2] T(E)*Jb(E) dE.
    // VALIDATION:
    //   E1 and E2 are checked here; cross-validation against DS_EMIN/DS_EMAX
    //   is left to the solver (channels that don't overlap the grid give F=0).
    if (inChannelsBlock) {
      std::istringstream iss(line);
      EnergyChannel ch;
      if (!(iss >> ch.name >> ch.E1_MeV >> ch.E2_MeV)) {
        { std::ostringstream _exit_msg; _exit_msg << "Malformed CH line at line "
            +std::to_string(lineNo)+": expected NAME E1_MeV E2_MeV, got: "+line;
          exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
      }
      if (ch.E1_MeV <= 0.0) {
        { std::ostringstream _exit_msg; _exit_msg << "Energy channel '"+ch.name
            +"' at line "+std::to_string(lineNo)+": E1_MeV must be > 0";
          exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
      }
      if (ch.E2_MeV <= ch.E1_MeV) {
        { std::ostringstream _exit_msg; _exit_msg << "Energy channel '"+ch.name
            +"' at line "+std::to_string(lineNo)+": E2_MeV must be > E1_MeV";
          exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
      }
      p.fluxChannels.push_back(ch);
      continue;
    }

    // Generic KEY VALUE line
    std::string key,val;
    SplitKV(line,key,val);
    if (key.empty()) continue;
    std::string uKey=ToUpper(key);

    auto rejectUnknownKeyword=[&](){
      FailUnknownKeyword(fileName,lineNo,section,uKey,rawLine);
    };

    if (section=="#RUN_INFO") {
      if (uKey=="RUN_ID") p.runId=val;
      else if (uKey=="PI_NAME" || uKey=="PI_EMAIL" ||
               uKey=="INSTITUTION" || uKey=="SCIENCE_GOAL") {
        // CCMC/Runs-on-Request files often include provenance metadata in
        // #RUN_INFO. These values do not affect the physics or numerics, but
        // they are legitimate input-file metadata. Store them rather than
        // rejecting the file under strict validation.
        p.runInfo[uKey]=val;
      }
      else rejectUnknownKeyword();
    }
    else if (section=="#CALCULATION_MODE") {
      if (uKey=="CALC_TARGET") {
        p.calc.target=ToUpper(val);
        p.calc.targetExplicit=true;
      }
      else if (uKey=="FIELD_EVAL_METHOD") p.calc.fieldEvalMethod=ToUpper(val);
      else rejectUnknownKeyword();
    }
    else if (section=="#CUTOFF_RIGIDITY") {
      if (uKey=="CUTOFF_EMIN") p.cutoff.eMin_MeV=std::stod(val);
      else if (uKey=="CUTOFF_EMAX") p.cutoff.eMax_MeV=std::stod(val);
      else if (uKey=="CUTOFF_NENERGY") p.cutoff.nEnergy=std::stoi(val);
      else if (uKey=="CUTOFF_MAX_PARTICLES") p.cutoff.maxParticlesPerPoint=std::stoi(val);

      // Optional per-trajectory integration cap for the cutoff search.
      // If omitted, the solver will fall back to #NUMERICAL MAX_TRACE_TIME.
      else if (uKey=="CUTOFF_MAX_TRAJ_TIME") p.cutoff.maxTrajTime_s=std::stod(val);

      // Unresolved-only time-convergence extension.  These controls never remap a
      // trace-limit outcome to a physical state.  Instead, TIME_LIMIT/STEP_LIMIT
      // samples are retraced from the same initial condition with successively larger
      // total-time budgets.  A sample that still hits a configured limit after the
      // last pass remains UNRESOLVED under the normal trace-limit policy.
      else if (uKey=="CUTOFF_UNRESOLVED_EXTENSION_PASSES" ||
               uKey=="CUTOFF_TRACE_EXTENSION_PASSES")
        p.cutoff.unresolvedExtensionPasses=std::stoi(val);
      else if (uKey=="CUTOFF_UNRESOLVED_EXTENSION_FACTOR" ||
               uKey=="CUTOFF_TRACE_EXTENSION_FACTOR")
        p.cutoff.unresolvedExtensionFactor=std::stod(val);

      // Rigidity-search strategy.  UPPER_SCAN is the penumbra-safe scalar
      // default; PENUMBRA_SCAN evaluates one full grid and reports both lower
      // and upper cutoff; BINARY is the legacy endpoint-only method.
      else if (uKey=="CUTOFF_SEARCH_ALGORITHM" ||
               uKey=="CUTOFF_SEARCH" ||
               uKey=="CUTOFF_RIGIDITY_SEARCH") {
        p.cutoff.searchAlgorithm=ToUpper(Trim(val));
      }
      else if (uKey=="CUTOFF_UPPER_SCAN_N" ||
               uKey=="CUTOFF_SEARCH_N" ||
               uKey=="CUTOFF_SCAN_N") {
        p.cutoff.upperScanN=std::stoi(val);
      }
      else if (uKey=="CUTOFF_RIGIDITY_LIST_GV" ||
               uKey=="CUTOFF_FIXED_RIGIDITIES_GV" ||
               uKey=="CUTOFF_ACCESS_RIGIDITIES_GV") {
        p.cutoff.rigidityList_GV=ParseStrictDoubleList_(val,uKey);
      }
      else if (uKey=="CUTOFF_DIRECT_ACCESS_ADAPTIVE" ||
               uKey=="DIRECT_ACCESS_ADAPTIVE") {
        p.cutoff.directAccessAdaptive=ToBool(val);
      }
      else if (uKey=="CUTOFF_DIRECT_ACCESS_ADAPTIVE_MAX_DEPTH" ||
               uKey=="DIRECT_ACCESS_ADAPTIVE_MAX_DEPTH") {
        p.cutoff.directAccessAdaptiveMaxDepth=std::stoi(val);
      }
      else if (uKey=="CUTOFF_DIRECT_ACCESS_ADAPTIVE_GUARD_DEPTH" ||
               uKey=="DIRECT_ACCESS_ADAPTIVE_GUARD_DEPTH") {
        p.cutoff.directAccessAdaptiveGuardDepth=std::stoi(val);
      }
      else if (uKey=="CUTOFF_ACCESS_ABS_LAT_MIN" ||
               uKey=="CUTOFF_ACCESS_LAT_ABS_MIN") {
        p.cutoff.accessAbsLatMin_deg=std::stod(val);
      }
      else if (uKey=="CUTOFF_ACCESS_ABS_LAT_MAX" ||
               uKey=="CUTOFF_ACCESS_LAT_ABS_MAX") {
        p.cutoff.accessAbsLatMax_deg=std::stod(val);
      }

      // Select the numerical integration policy used by the Boolean cutoff
      // classifier.  This is deliberately separate from the structured F3
      // trajectory API, which always uses the corrected accurate policy.
      else if (uKey=="CUTOFF_TRACE_POLICY" ||
               uKey=="CUTOFF_INTEGRATION_POLICY" ||
               uKey=="CUTOFF_TRAJECTORY_POLICY") {
        p.cutoff.traceIntegrationPolicy=ToUpper(Trim(val));
      }

      // Charge convention for the outward reverse trajectory.  Keep this separate
      // from PARTICLE_SPECIES/CHARGE: the latter describes the physical incoming
      // particle, whereas this option controls the numerically integrated
      // time-reversed trajectory.
      else if (uKey=="CUTOFF_BACKTRACE_CHARGE" ||
               uKey=="CUTOFF_BACKTRACE_CHARGE_CONVENTION" ||
               uKey=="BACKTRACE_CHARGE_CONVENTION") {
        p.cutoff.backtraceChargeConvention=ToUpper(Trim(val));
      }

      // Policy for finite integration caps in the three-state PENUMBRA_SCAN.
      // Numerical/field failures are not affected; only TIME_LIMIT, STEP_LIMIT, and
      // DISTANCE_LIMIT may be mapped to PHYSICAL_FORBIDDEN for reference-table
      // compatibility.
      else if (uKey=="CUTOFF_TRACE_LIMIT_POLICY" ||
               uKey=="CUTOFF_LIMIT_POLICY" ||
               uKey=="CUTOFF_LIMIT_CLASSIFICATION") {
        p.cutoff.traceLimitPolicy=ToUpper(Trim(val));
      }

      // Cutoff sampling strategy: VERTICAL or ISOTROPIC.
      // We store the string in uppercase for robust comparisons later.
      else if (uKey=="CUTOFF_SAMPLING") p.cutoff.sampling=ToUpper(val);
      else if (uKey=="CUTOFF_SCAN_SPACING" || uKey=="CUTOFF_RIGIDITY_SPACING")
        p.cutoff.scanSpacing=ToUpper(Trim(val));

      // Optional Mode3D rigidity-classification diagnostic.
      // This writes TraceAllowed3D(R) at one spherical-shell location before the
      // full cutoff map is computed.  It is used to diagnose endpoint/bracketing
      // problems in dipole/Störmer validation runs.
      else if (uKey=="CUTOFF_DEBUG_RIGIDITY_SCAN" ||
               uKey=="DEBUG_RIGIDITY_SCAN" ||
               uKey=="CUTOFF_RIGIDITY_SCAN_TEST" ||
               uKey=="CUTOFF_DEBUG_SCAN") {
        p.cutoff.debugRigidityScan=ToBool(val);
      }
      else if (uKey=="CUTOFF_DEBUG_SCAN_LON" ||
               uKey=="DEBUG_SCAN_LON" ||
               uKey=="DEBUG_SCAN_LON_DEG") {
        p.cutoff.debugScanLon_deg=std::stod(val);
      }
      else if (uKey=="CUTOFF_DEBUG_SCAN_LAT" ||
               uKey=="DEBUG_SCAN_LAT" ||
               uKey=="DEBUG_SCAN_LAT_DEG") {
        p.cutoff.debugScanLat_deg=std::stod(val);
      }
      else if (uKey=="CUTOFF_DEBUG_SCAN_ALT" ||
               uKey=="CUTOFF_DEBUG_SCAN_ALT_KM" ||
               uKey=="DEBUG_SCAN_ALT" ||
               uKey=="DEBUG_SCAN_ALT_KM") {
        p.cutoff.debugScanAlt_km=std::stod(val);
      }
      else if (uKey=="CUTOFF_DEBUG_SCAN_N" ||
               uKey=="DEBUG_SCAN_N" ||
               uKey=="DEBUG_SCAN_NPOINTS") {
        p.cutoff.debugScanN=std::stoi(val);
      }
      else if (uKey=="CUTOFF_DEBUG_SCAN_FILE" ||
               uKey=="DEBUG_SCAN_FILE") {
        p.cutoff.debugScanFile=Trim(val);
      }

      // Optional trajectory-exit diagnostic.  This uses the same selected point style
      // as the rigidity scan but writes the terminal state/reason for each traced R.
      else if (uKey=="CUTOFF_DEBUG_EXIT_TRACE" ||
               uKey=="DEBUG_EXIT_TRACE" ||
               uKey=="CUTOFF_EXIT_TRACE_TEST") {
        p.cutoff.debugExitTrace=ToBool(val);
      }
      else if (uKey=="CUTOFF_DEBUG_EXIT_LON" ||
               uKey=="DEBUG_EXIT_LON" ||
               uKey=="DEBUG_EXIT_LON_DEG") {
        p.cutoff.debugExitLon_deg=std::stod(val);
      }
      else if (uKey=="CUTOFF_DEBUG_EXIT_LAT" ||
               uKey=="DEBUG_EXIT_LAT" ||
               uKey=="DEBUG_EXIT_LAT_DEG") {
        p.cutoff.debugExitLat_deg=std::stod(val);
      }
      else if (uKey=="CUTOFF_DEBUG_EXIT_ALT" ||
               uKey=="CUTOFF_DEBUG_EXIT_ALT_KM" ||
               uKey=="DEBUG_EXIT_ALT" ||
               uKey=="DEBUG_EXIT_ALT_KM") {
        p.cutoff.debugExitAlt_km=std::stod(val);
      }
      else if (uKey=="CUTOFF_DEBUG_EXIT_R_GV" ||
               uKey=="CUTOFF_DEBUG_EXIT_R" ||
               uKey=="DEBUG_EXIT_R_GV") {
        p.cutoff.debugExitR_GV=std::stod(val);
      }
      else if (uKey=="CUTOFF_DEBUG_EXIT_N" ||
               uKey=="DEBUG_EXIT_N" ||
               uKey=="DEBUG_EXIT_NPOINTS") {
        p.cutoff.debugExitN=std::stoi(val);
      }
      else if (uKey=="CUTOFF_DEBUG_EXIT_LIST_FILE" ||
               uKey=="CUTOFF_DEBUG_EXIT_TRAJECTORY_LIST" ||
               uKey=="DEBUG_EXIT_LIST_FILE") {
        p.cutoff.debugExitListFile=Trim(val);
        if (!p.cutoff.debugExitListFile.empty()) p.cutoff.debugExitTrace=true;
      }
      else if (uKey=="CUTOFF_DEBUG_EXIT_FILE" ||
               uKey=="DEBUG_EXIT_FILE") {
        p.cutoff.debugExitFile=Trim(val);
      }

      // Directional Rc sky-map controls.
      // Notes:
      //   - These controls are shared by the gridless and standalone Mode3D cutoff solvers.
      //   - A directional map/direct-access cube can be expensive; DIRMAP_COVERAGE
      //     can retain only cells inside the configured instrument look apertures.
      else if (uKey=="DIRECTIONAL_MAP" || uKey=="CUTOFF_DIRECTIONAL_MAP") p.cutoff.directionalMap=ToBool(val);
      else if (uKey=="DIRMAP_LON_RES") p.cutoff.dirMapLonRes_deg=std::stod(val);
      else if (uKey=="DIRMAP_LAT_RES") p.cutoff.dirMapLatRes_deg=std::stod(val);
      // Optional angular-work reduction for directional products.  FULL_SPHERE is
      // backward compatible.  VECTOR_APERTURES retains only cells inside arbitrary
      // finite-FOV look directions supplied by vector definitions.  LOCATION-qualified
      // apertures are evaluated only at their associated observation location; Mode3D
      // may use a compact union for storage, but does not trace another location's cells.
      // The solver never assumes that the apertures are east/west or antipodal.
      else if (uKey=="DIRMAP_COVERAGE" || uKey=="CUTOFF_DIRMAP_COVERAGE")
        p.cutoff.dirMapCoverage=ToUpper(Trim(val));
      else if (uKey=="DIRMAP_APERTURE_FILE" || uKey=="CUTOFF_DIRMAP_APERTURE_FILE")
        p.cutoff.dirMapApertureFile=Trim(val);
      else if (uKey=="DIRMAP_APERTURE" || uKey=="CUTOFF_DIRMAP_APERTURE")
        p.cutoff.dirMapApertures.push_back(ParseDirectionalApertureSpec(val));
      else rejectUnknownKeyword();
    }
    else if (section=="#PARTICLE_SPECIES") {
      if (uKey=="SPECIES" || uKey=="SPECIES_NAME") p.species.name=ToUpper(val);
      else if (uKey=="CHARGE" || uKey=="SPECIES_CHARGE") p.species.charge_e=std::stoi(val);
      else if (uKey=="MASS_AMU" || uKey=="SPECIES_MASS_AMU") p.species.mass_amu=std::stod(val);
      else rejectUnknownKeyword();
    }
    else if (section=="#BACKGROUND_FIELD") {
      if (uKey=="FIELD_MODEL") {
        // Normalize alternate spellings to the canonical model name
        // used throughout the codebase (e.g. TS05 -> T05).
        p.field.model = NormalizeTsModelName(val);
      }
      else if (uKey=="DIPOLE_MOMENT") p.field.dipoleMoment_Me=std::stod(val);
      else if (uKey=="DIPOLE_TILT" || uKey=="DIPOLE_TILT_DEG") p.field.dipoleTilt_deg=std::stod(val);
      // Magnetic-driver parameters.
      //
      // The internal names are intentionally model-neutral (DST, PDYN, IMF_BZ,
      // ...), but CCMC/Runs-on-Request style files often prefix the same keys
      // with the selected Tsyganenko model name, for example TS05_DST or
      // TS05_BZ. Accept both styles and map them to the same BackgroundField
      // storage. FIELD_MODEL=TS05 is normalized to canonical model name T05;
      // the TS05_* aliases are only input-file spelling compatibility.
      else if (uKey=="DST" || uKey=="SYM_H" || uKey=="SYMH" ||
               uKey=="T05_DST" || uKey=="TS05_DST" ||
               uKey=="T05_SYMH" || uKey=="TS05_SYMH") {
        p.field.dst_nT=std::stod(val);
      }
      else if (uKey=="PDYN" || uKey=="P_DYN" || uKey=="SW_PDYNP" ||
               uKey=="T05_PDYN" || uKey=="TS05_PDYN") {
        p.field.pdyn_nPa=std::stod(val);
      }
      else if (uKey=="IMF_BY" || uKey=="BYIMF" || uKey=="BY" ||
               uKey=="T05_BY" || uKey=="TS05_BY") {
        p.field.imfBy_nT=std::stod(val);
      }
      else if (uKey=="IMF_BZ" || uKey=="BZIMF" || uKey=="BZ" ||
               uKey=="T05_BZ" || uKey=="TS05_BZ") {
        p.field.imfBz_nT=std::stod(val);
      }
      else if (uKey=="IMF_BX" || uKey=="BXIMF" || uKey=="BX" ||
               uKey=="T05_BX" || uKey=="TS05_BX") {
        p.field.imfBx_nT=std::stod(val);
      }
      else if (uKey=="SW_VX" || uKey=="VX" || uKey=="VSW" ||
               uKey=="T05_VX" || uKey=="TS05_VX") {
        p.field.swVx_kms=std::stod(val);
      }
      else if (uKey=="SW_N" || uKey=="NSW" || uKey=="N_SW" ||
               uKey=="DENSITY" || uKey=="DEN_P" ||
               uKey=="T05_NSW" || uKey=="TS05_NSW") {
        p.field.swN_cm3=std::stod(val);
      }
      else if (uKey=="G1") p.field.g[0]=std::stod(val);
      else if (uKey=="G2") p.field.g[1]=std::stod(val);
      else if (uKey=="G3") p.field.g[2]=std::stod(val);
      else if (uKey=="W1" || uKey=="T05_W1" || uKey=="TS05_W1") p.field.w[0]=std::stod(val);
      else if (uKey=="W2" || uKey=="T05_W2" || uKey=="TS05_W2") p.field.w[1]=std::stod(val);
      else if (uKey=="W3" || uKey=="T05_W3" || uKey=="TS05_W3") p.field.w[2]=std::stod(val);
      else if (uKey=="W4" || uKey=="T05_W4" || uKey=="TS05_W4") p.field.w[3]=std::stod(val);
      else if (uKey=="W5" || uKey=="T05_W5" || uKey=="TS05_W5") p.field.w[4]=std::stod(val);
      else if (uKey=="W6" || uKey=="T05_W6" || uKey=="TS05_W6") p.field.w[5]=std::stod(val);
      else if (uKey=="BZ1" || uKey=="TA15_BZ1" || uKey=="TA16_BZ1") p.field.bzAvg[0]=std::stod(val);
      else if (uKey=="BZ2" || uKey=="TA15_BZ2" || uKey=="TA16_BZ2") p.field.bzAvg[1]=std::stod(val);
      else if (uKey=="BZ3" || uKey=="TA15_BZ3" || uKey=="TA16_BZ3") p.field.bzAvg[2]=std::stod(val);
      else if (uKey=="BZ4" || uKey=="TA15_BZ4" || uKey=="TA16_BZ4") p.field.bzAvg[3]=std::stod(val);
      else if (uKey=="BZ5" || uKey=="TA15_BZ5" || uKey=="TA16_BZ5") p.field.bzAvg[4]=std::stod(val);
      else if (uKey=="BZ6" || uKey=="TA15_BZ6" || uKey=="TA16_BZ6") p.field.bzAvg[5]=std::stod(val);
      else if (uKey=="XIND" || uKey=="TA15_XIND") p.field.xind=std::stod(val);
      else if (uKey=="TA16_COEFF_FILE") p.field.ta16CoeffFile=Trim(val);
      else if (uKey=="EPOCH") {
        // Store the complete value after the EPOCH keyword rather than taking only
        // the first whitespace-delimited token.  This intentionally accepts both
        //
        //   EPOCH  2010-01-01T00:00:00
        //   EPOCH  2010-01-01 00:00:00
        //
        // because the general parser tokenization keeps all text after the key in
        // `val`.  No Geopack/SPICE initialization is performed while parsing: the
        // final value may still be overridden by CLI --epoch in main.cpp.  Solver
        // startup later passes the merged p.field.epoch to the model and coordinate
        // initialization routines.
        p.field.epoch=Trim(val);
        if (p.field.epoch.empty()) {
          std::ostringstream msg;
          msg << "#BACKGROUND_FIELD EPOCH requires a non-empty UTC timestamp";
          exit(__LINE__,__FILE__,msg.str().c_str());
        }
      }
      else if (uKey=="DRIVER_FILE" || uKey=="MAGNETIC_DRIVER_FILE") {
        // Allow the magnetic-field driver table to be declared next to the
        // Tsyganenko model selection inside #BACKGROUND_FIELD, e.g.
        //
        //   #BACKGROUND_FIELD
        //   FIELD_MODEL   TS05
        //   DRIVER_FILE   ts05_driving_2017-09-07T000000Z.txt
        //
        // This is intentionally stored on p.field first, because semantically the
        // file belongs to the magnetic-field model configuration.  The actual load
        // is deferred until the entire input file has been parsed so that:
        //   1) FIELD_MODEL has already been normalized,
        //   2) later sections still have a chance to override the same driver via
        //      #TEMPORAL TS_INPUT_FILE if desired, and
        //   3) all parser errors are reported from a single finalization stage.
        p.field.driverFile = Trim(val);
      }
      else {
        p.field.raw[uKey]=val;
        rejectUnknownKeyword();
      }
    }
    else if (section=="#ELECTRIC_FIELD") {
      if (uKey=="EFIELD_MODEL") p.efield.model=ToUpper(val);
      else if (uKey=="COROTATION_SCALE") p.efield.corotationScale=std::stod(val);
      else if (uKey=="VS_POTENTIAL_KV") p.efield.vsPotential_kV=std::stod(val);
      else if (uKey=="VS_GAMMA") p.efield.vsGamma=std::stod(val);
      else if (uKey=="VS_REFERENCE_L") p.efield.vsReferenceL=std::stod(val);
      else if (uKey=="VS_SCALE") p.efield.vsScale=std::stod(val);
      else if (uKey=="EFIELD_RMIN" || uKey=="EFIELD_RMIN_KM") p.efield.rMin_km=ParseLengthToKm(val,commentText);
      else if (uKey=="EFIELD_LMIN") p.efield.lMin=std::stod(val);
      else {
        p.efield.raw[uKey]=val;
        rejectUnknownKeyword();
      }
    }
    else if (section=="#DOMAIN_BOUNDARY") {
      if (uKey=="DOMAIN_X_MAX" || uKey=="DOMAIN_XMAX") p.domain.xMax=ParseLengthToKm(val,commentText);
      else if (uKey=="DOMAIN_X_MIN" || uKey=="DOMAIN_XMIN") p.domain.xMin=ParseLengthToKm(val,commentText);
      else if (uKey=="DOMAIN_Y_MAX" || uKey=="DOMAIN_YMAX") p.domain.yMax=ParseLengthToKm(val,commentText);
      else if (uKey=="DOMAIN_Y_MIN" || uKey=="DOMAIN_YMIN") p.domain.yMin=ParseLengthToKm(val,commentText);
      else if (uKey=="DOMAIN_Z_MAX" || uKey=="DOMAIN_ZMAX") p.domain.zMax=ParseLengthToKm(val,commentText);
      else if (uKey=="DOMAIN_Z_MIN" || uKey=="DOMAIN_ZMIN") p.domain.zMin=ParseLengthToKm(val,commentText);
      else if (uKey=="R_INNER") p.domain.rInner=ParseLengthToKm(val,commentText);
      else if (uKey=="DOMAIN_X_TAIL" || uKey=="X_TAIL") {
        // RoR inputs may specify a flat nightside cap as a positive or negative
        // tail distance in Re/km. The rectangular Mode3D domain represents this
        // as the minimum GSM X boundary; enforce the expected nightside sign.
        const double xTail_km = ParseLengthToKm(val,commentText);
        p.domain.xMin = (xTail_km <= 0.0) ? xTail_km : -xTail_km;
      }
      else if (uKey=="BOUNDARY_TYPE") {
        // Recognize SHUE/BOX declarations. SHUE is stored for provenance and
        // future use; current standalone Mode3D cutoff still uses the parsed
        // rectangular/capped bounds for trajectory classification.
        p.domain.boundaryType=ToUpper(Trim(val));
      }
      else if (uKey=="SHUE_R0") {
        // May be a number or AUTO. Keep as a token because the present domain
        // object has no Shue implementation yet.
        p.domain.shueR0Token=Trim(val);
      }
      else if (uKey=="SHUE_ALPHA") {
        // May be a number or AUTO. Keep as a token because the present domain
        // object has no Shue implementation yet.
        p.domain.shueAlphaToken=Trim(val);
      }
      else {
        p.domain.raw[uKey]=val;
        rejectUnknownKeyword();
      }
    }
    else if (section=="#OUTPUT_DOMAIN") {
      if (uKey=="OUTPUT_MODE") p.output.mode=ToUpper(val);
      else if (uKey=="OUTPUT_COORDS" || uKey=="COORDS") p.output.coords=ToUpper(val);
      else if (uKey=="TRAJ_FRAME") p.output.trajFrame=ToUpper(val);
      else if (uKey=="TRAJ_FILE") p.output.trajFile=Trim(val);
      else if (uKey=="FLUX_DT") p.output.fluxDt_min=std::stod(val);
      else if (uKey=="SHELL_ALTS_KM" || uKey=="SHELL_ALTITUDES") {
        // Although the key name contains _KM (historical), accept explicit Re
        // units inline or in the trailing comment. If no units are provided, km
        // remains the default for backward compatibility.
	auto t=ParseLengthListToKm(val,commentText);
        p.output.shellAlt_km.insert(p.output.shellAlt_km.end(),t.begin(),t.end());  
      }
      else if (uKey=="SHELL_RES_DEG" || uKey=="SHELL_RES") p.output.shellRes_deg=std::stod(val);
      else if (uKey=="SHELL_LON_RES_DEG" || uKey=="SHELL_LON_RES") p.output.shellLonRes_deg=std::stod(val);
      else if (uKey=="SHELL_LAT_RES_DEG" || uKey=="SHELL_LAT_RES") p.output.shellLatRes_deg=std::stod(val);
      else if (uKey=="SHELL_GEOMETRY") p.output.shellGeometry=ToUpper(Trim(val));
      else if (uKey=="SHELL_COUNT") {
        // Legacy/self-documenting input files sometimes specify SHELL_COUNT next to
        // SHELL_ALTS_KM.  The actual number of shells is determined by the number
        // of altitudes parsed above, but recognizing this key avoids treating a
        // harmless consistency annotation as an unknown option.
        const int nShellDeclared = std::stoi(val);
        if (nShellDeclared < 0) {
          exit(__LINE__,__FILE__,"SHELL_COUNT must be >= 0");
        }
        p.output.raw[uKey]=val;
      }
      else {
        p.output.raw[uKey]=val;
        rejectUnknownKeyword();
      }
    }
    else if (section=="#MODE3D_MESH") {
      // Optional standalone Mode3D AMR resolution profile.  If no keywords from
      // this section are present, main_lib.cpp keeps using the historical hard-coded
      // localResolution() function.
      ParseMode3DMeshKeyword(p,uKey,val,commentText);
    }
    else if (section=="#NUMERICAL") {
      if (uKey=="DT_TRACE") p.numerics.dtTrace_s=std::stod(val);
      else if (uKey=="ADAPTIVE_DT" || uKey=="TRACE_ADAPTIVE_DT" ||
               uKey=="USE_ADAPTIVE_DT" || uKey=="AUTO_DT") {
        p.numerics.adaptiveDt=ToBool(val);
      }
      else if (uKey=="MAX_STEPS") p.numerics.maxSteps=std::stoi(val);
      else if (uKey=="MAX_TRACE_TIME") p.numerics.maxTraceTime_s=std::stod(val);
      else if (uKey=="BOUNDARY_EVENT_TOL_M" || uKey=="BOUNDARY_REFINE_TOL_M")
        p.numerics.boundaryEventTolerance_m=std::stod(val);
      else if (uKey=="BOUNDARY_EVENT_MAX_ITER" || uKey=="BOUNDARY_REFINE_MAX_ITER")
        p.numerics.boundaryEventMaxIterations=std::stoi(val);
      else if (uKey=="TRAP_DETECTION" || uKey=="TRACE_TRAP_DETECTION")
        p.numerics.trapDetection=ToBool(val);
      else if (uKey=="TRAP_MIN_MIRROR_POINTS")
        p.numerics.trapMinMirrorPoints=std::stoi(val);
      else if (uKey=="TRAP_MIN_BOUNCES" || uKey=="TRAP_MIN_BOUNCE_CYCLES")
        p.numerics.trapMinBounceCycles=std::stoi(val);
      else if (uKey=="TRAP_OUTER_MARGIN_RE")
        p.numerics.trapOuterMargin_Re=std::stod(val);
      else if (uKey=="TRAP_RADIAL_GROWTH_TOL_RE" || uKey=="TRAP_RADIAL_ENVELOPE_TOL_RE")
        p.numerics.trapRadialGrowthTolerance_Re=std::stod(val);
      else if (uKey=="TRAP_ENERGY_REL_TOL")
        p.numerics.trapEnergyRelativeTolerance=std::stod(val);
      else if (uKey=="TRAP_PARALLEL_DEADBAND")
        p.numerics.trapParallelDeadband=std::stod(val);
      else if (uKey=="TRAP_DRIFT_DETECTION" || uKey=="TRAP_DRIFT_SHELL_DETECTION")
        p.numerics.trapDriftDetection=ToBool(val);
      else if (uKey=="TRAP_MIN_DRIFT_REVOLUTIONS")
        p.numerics.trapMinDriftRevolutions=std::stoi(val);
      else if (uKey=="TRAP_DRIFT_RADIAL_GROWTH_TOL_RE" ||
               uKey=="TRAP_DRIFT_RADIAL_ENVELOPE_TOL_RE")
        p.numerics.trapDriftRadialGrowthTolerance_Re=std::stod(val);
      else if (uKey=="TRAP_DRIFT_RADIAL_REL_TOL" ||
               uKey=="TRAP_DRIFT_RADIAL_RELATIVE_TOL")
        p.numerics.trapDriftRadialRelativeTolerance=std::stod(val);
      else if (uKey=="TRAP_DRIFT_LATITUDE_TOL")
        p.numerics.trapDriftLatitudeTolerance=std::stod(val);
      else if (uKey=="TRAP_DRIFT_PITCH_COS2_TOL")
        p.numerics.trapDriftPitchCos2Tolerance=std::stod(val);
      else if (uKey=="TRAP_DRIFT_MAX_MEAN_RADIUS_CHANGE_RE" ||
               uKey=="TRAP_DRIFT_SECULAR_RADIAL_TOL_RE")
        p.numerics.trapDriftMaxMeanRadiusChange_Re=std::stod(val);
      else if (uKey=="TRAP_DRIFT_PROFILE_BINS")
        p.numerics.trapDriftProfileBins=std::stoi(val);
      else if (uKey=="TRAP_DRIFT_MIN_PROFILE_COVERAGE")
        p.numerics.trapDriftMinProfileCoverage=std::stod(val);
      else if (uKey=="TRAP_DRIFT_MIN_MATCHED_BIN_FRACTION")
        p.numerics.trapDriftMinMatchedBinFraction=std::stod(val);
      else if (uKey=="MAX_TRACE_DISTANCE") p.numerics.maxTraceDistance_Re=std::stod(val);
      else if (uKey=="N_PARTICLES") {
        // RoR-style generic particle count. For the forward-mode injector this
        // is equivalent to FORWARD_N_PARTICLES; cutoff/density backtracking use
        // their own CUTOFF_* / DS_* counters.
        p.numerics.nParticles=std::stoi(val);
        p.mode3dForward.nParticlesPerIter=p.numerics.nParticles;
      }
      else if (uKey=="MAX_BOUNCE") {
        // Reserved compatibility keyword. The present backtracing termination
        // controls are MAX_STEPS, MAX_TRACE_TIME, MAX_TRACE_DISTANCE, and optional
        // cutoff/density-specific time caps.
        p.numerics.maxBounce=std::stoi(val);
      }
      else if (uKey=="PITCH_ISOTROPIC") {
        // Generic RoR boundary-pitch flag. Store it for diagnostics/provenance;
        // Mode3D density/cutoff backtracking uses CUTOFF_SAMPLING, DS_BOUNDARY_MODE,
        // and #BOUNDARY_ANISOTROPY for its active angular controls.
        p.numerics.pitchIsotropic=ToBool(val);
      }
      else if (uKey=="DENSITY_PARALLEL" || uKey=="DENSITY_BACKEND" ||
               uKey=="MODE3D_DENSITY_PARALLEL" || uKey=="MODE3D_DENSITY_BACKEND" ||
               uKey=="MODE3D_PARALLEL" || uKey=="MODE3D_BACKEND" ||
               uKey=="GRIDLESS_PARALLEL" || uKey=="GRIDLESS_BACKEND" ||
               uKey=="BACKTRACK_PARALLEL" || uKey=="BACKTRACK_BACKEND" ||
               uKey=="BACKWARD_PARALLEL" || uKey=="BACKWARD_BACKEND") {
        p.mode3d.densityParallelBackend=ToUpper(Trim(val));
      }
      else if (uKey=="DENSITY_THREADS" || uKey=="DENSITY_NTHREADS" ||
               uKey=="MODE3D_DENSITY_THREADS" || uKey=="MODE3D_DENSITY_NTHREADS" ||
               uKey=="MODE3D_THREADS" || uKey=="MODE3D_NTHREADS" ||
               uKey=="GRIDLESS_THREADS" || uKey=="GRIDLESS_NTHREADS" ||
               uKey=="BACKTRACK_THREADS" || uKey=="BACKTRACK_NTHREADS" ||
               uKey=="BACKWARD_THREADS" || uKey=="BACKWARD_NTHREADS") {
        p.mode3d.densityThreads=std::stoi(val);
      }
      else if (uKey=="MODE3D_MPI_SCHEDULER" || uKey=="MODE3D_MPI_BACKEND" ||
               uKey=="GRIDLESS_MPI_SCHEDULER" || uKey=="GRIDLESS_MPI_BACKEND" ||
               uKey=="BACKTRACK_MPI_SCHEDULER" || uKey=="BACKTRACK_MPI_BACKEND" ||
               uKey=="BACKWARD_MPI_SCHEDULER" || uKey=="BACKWARD_MPI_BACKEND") {
        p.mode3d.mpiScheduler=ToUpper(Trim(val));
      }
      else if (uKey=="MODE3D_MPI_DYNAMIC_CHUNK" || uKey=="MODE3D_MPI_CHUNK" ||
               uKey=="GRIDLESS_MPI_DYNAMIC_CHUNK" || uKey=="GRIDLESS_MPI_CHUNK" ||
               uKey=="BACKTRACK_MPI_DYNAMIC_CHUNK" || uKey=="BACKTRACK_MPI_CHUNK" ||
               uKey=="BACKWARD_MPI_DYNAMIC_CHUNK" || uKey=="BACKWARD_MPI_CHUNK") {
        p.mode3d.mpiDynamicChunk=std::stoi(val);
      }
      else if (uKey.rfind("MODE3D_MESH_",0)==0) {
        // Allow the same mesh-resolution controls in #NUMERICAL for compact test
        // inputs, but keep #MODE3D_MESH as the preferred/documented section.
        ParseMode3DMeshKeyword(p,uKey,val,commentText);
      }
      else if (uKey=="DS_BOUNDARY_MODE") {
        // Legacy inputs placed this density/spectrum setting in #NUMERICAL.
        // Keep it recognized, but route it to the same destination as the
        // canonical #DENSITY_SPECTRUM / DS_BOUNDARY_MODE keyword.
        p.densitySpectrum.boundaryMode=ToUpper(val);
      }
      // 3d_forward: simulation particles injected from domain boundary per iteration.
      else if (uKey=="FORWARD_N_PARTICLES")  p.mode3dForward.nParticlesPerIter=std::stoi(val);
      // 3d_forward: total number of forward integration iterations.
      else if (uKey=="FORWARD_N_ITERATIONS") p.mode3dForward.nIterations=std::stoi(val);
      // 3d_forward: energy proposal distribution for boundary injection.
      // Supported by Mode3DForward.cpp: SPECTRUM (legacy/default) and LOG_UNIFORM.
      // Multiple key names are accepted so the future input-file keyword can be
      // settled without touching the injection implementation.
      else if (uKey=="FORWARD_INJECTION_ENERGY" ||
               uKey=="FORWARD_INJECTION_ENERGY_DISTRIBUTION" ||
               uKey=="FORWARD_ENERGY_SAMPLING" ||
               uKey=="FORWARD_INJECTION_SCHEME") {
        p.mode3dForward.injectionEnergyDistribution=ToUpper(Trim(val));
      }
      // 3d_forward mover keywords are reserved for a future input-file selection
      // feature.  Store the requested value for diagnostics/future use, but do NOT
      // apply it to p.mode3dForward.particleMover yet.  At present the active mover is
      // selected by the CLI (-mover) or falls back to BORIS.
      else if (uKey=="FORWARD_MOVER" ||
               uKey=="FORWARD_PARTICLE_MOVER" ||
               uKey=="FORWARD_INTEGRATOR" ||
               uKey=="PARTICLE_MOVER") {
        p.mode3dForward.reservedInputFileParticleMover=ToUpper(Trim(val));
        p.mode3dForward.hasReservedInputFileParticleMover=true;
      }
      else rejectUnknownKeyword();
    }
    else if (section=="#DENSITY_3D") {
      // Volumetric 3D density sampling controls for Mode3DForward.
      // All energies are in MeV/n (same convention as #DENSITY_SPECTRUM).
      if      (uKey=="FORWARD_MOVER" || uKey=="FORWARD_PARTICLE_MOVER" ||
               uKey=="FORWARD_INTEGRATOR" || uKey=="PARTICLE_MOVER" ||
               uKey=="DENS_MOVER") {
        // Reserved for future input-file-driven 3d_forward mover selection.  Keep the
        // value but do not make it active; CLI selection remains the only active path.
        p.mode3dForward.reservedInputFileParticleMover=ToUpper(Trim(val));
        p.mode3dForward.hasReservedInputFileParticleMover=true;
      }
      else if (uKey=="DENS_EMIN")    p.density3d.Emin_MeV    = std::stod(val);
      else if (uKey=="DENS_EMAX")    p.density3d.Emax_MeV    = std::stod(val);
      else if (uKey=="DENS_NENERGY") p.density3d.nEnergyBins  = std::stoi(val);
      else if (uKey=="DENS_ENERGY_SPACING") {
        const std::string sp = ToUpper(Trim(val));
        if      (sp=="LOG")    p.density3d.spacing = EarthUtil::Density3DParam::Spacing::LOG;
        else if (sp=="LINEAR") p.density3d.spacing = EarthUtil::Density3DParam::Spacing::LINEAR;
        else { std::ostringstream _m; _m << "DENS_ENERGY_SPACING must be LOG or LINEAR (got '" << Trim(val) << "')"; exit(__LINE__,__FILE__,_m.str().c_str()); }
      }
      else rejectUnknownKeyword();
    }
    else if (section=="#PARTICLE_TRAJECTORY") {
      // Runtime controls for AMPS particle-trajectory records in 3d_forward.
      // These settings do not replace the AMPS compile-time switch: the tracker
      // must still be built with _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_.
      // They only decide whether 3d_forward injected particles request tracking
      // and how many such trajectory records may be initialized.
      if (uKey=="INITIALIZE_TRAJECTORIES" ||
          uKey=="INITIALIZE_PARTICLE_TRAJECTORIES" ||
          uKey=="TRACK_TRAJECTORIES" ||
          uKey=="ENABLE_TRAJECTORY_TRACKING" ||
          uKey=="PARTICLE_TRAJECTORY_TRACKING") {
        p.mode3dForward.initializeParticleTrajectories = ToBool(val);
      }
      else if (uKey=="N_TRAJECTORIES" ||
               uKey=="N_PARTICLE_TRAJECTORIES" ||
               uKey=="MAX_TRAJECTORIES" ||
               uKey=="MAX_PARTICLE_TRAJECTORIES") {
        p.mode3dForward.nParticleTrajectories = std::stoi(val);

        // Compact input such as:
        //   #PARTICLE_TRAJECTORY
        //   N_TRAJECTORIES 1000
        // must be sufficient to change the runtime cap and enable trajectory
        // initialization.  This is the bug fix: before this section was parsed,
        // pt.cpp could fall back to an internal hard-coded trajectory cap.
        // A zero value is treated as an explicit disable switch.
        p.mode3dForward.initializeParticleTrajectories =
            (p.mode3dForward.nParticleTrajectories > 0);
      }
      else rejectUnknownKeyword();
    }
    else if (section=="#DENSITY_SPECTRUM") {
      // Density/spectrum workflow controls.
      // All energies are read in MeV (commonly MeV/n in CCMC inputs).
      if (uKey=="DS_EMIN") p.densitySpectrum.Emin_MeV=std::stod(val);
      else if (uKey=="DS_EMAX") p.densitySpectrum.Emax_MeV=std::stod(val);
      else if (uKey=="DS_NINTERVALS") p.densitySpectrum.nIntervals=std::stoi(val);
      else if (uKey=="DS_MAX_PARTICLES") p.densitySpectrum.maxParticlesPerPoint=std::stoi(val);
      else if (uKey=="DS_MAX_TRAJ_TIME") p.densitySpectrum.maxTrajTime_s=std::stod(val);
      else if (uKey=="DS_ENERGY_SPACING") p.densitySpectrum.spacing=ParseEnergySpacingToken(val);
      else if (uKey=="DS_BOUNDARY_MODE") p.densitySpectrum.boundaryMode=ToUpper(val);
      else if (uKey=="DS_TRANSMISSION_MODE" || uKey=="DENSITY_TRANSMISSION_MODE" ||
               uKey=="DS_TRANSMISSION" || uKey=="DENSITY_TRANSMISSION") {
        // Density/flux is controlled by a transmission function T(E,Omega).  This
        // token selects whether the solver evaluates T on the legacy user energy
        // grid (DIRECT) or on a rigidity scan better suited for cutoff/penumbra
        // structure (SCAN/ADAPTIVE).  Validation is done after parsing so aliases
        // can be accepted uniformly.
        p.densitySpectrum.transmissionMode=ToUpper(Trim(val));
      }
      else if (uKey=="DS_TRANSMISSION_SCAN_N" || uKey=="DENSITY_TRANSMISSION_SCAN_N" ||
               uKey=="DS_TRANSMISSION_N" || uKey=="DENSITY_TRANSMISSION_N") {
        p.densitySpectrum.transmissionScanN=std::stoi(val);
      }
      else if (uKey=="DS_TRANSMISSION_REFINE_N" || uKey=="DENSITY_TRANSMISSION_REFINE_N") {
        p.densitySpectrum.transmissionRefineN=std::stoi(val);
      }
      else if (uKey=="DS_TRANSMISSION_MAX_N" || uKey=="DENSITY_TRANSMISSION_MAX_N") {
        p.densitySpectrum.transmissionMaxN=std::stoi(val);
      }
      else if (uKey=="DS_TRANSMISSION_SAVE" || uKey=="DENSITY_TRANSMISSION_SAVE") {
        p.densitySpectrum.transmissionSave=ToBool(val);
      }
      else if (uKey=="DS_UNRESOLVED_TOL" || uKey=="DENSITY_UNRESOLVED_TOL") {
        p.densitySpectrum.unresolvedTolerance=std::stod(val);
      }
      else if (uKey=="DS_RETRY_UNRESOLVED" || uKey=="DENSITY_RETRY_UNRESOLVED") {
        p.densitySpectrum.retryUnresolved=ToBool(val);
      }
      else if (uKey=="DS_SAVE_TERMINATION_SUMMARY" ||
               uKey=="DENSITY_SAVE_TERMINATION_SUMMARY") {
        p.densitySpectrum.saveTerminationSummary=ToBool(val);
      }
      else if (uKey=="DS_PARALLEL" || uKey=="DS_BACKEND" ||
               uKey=="DENSITY_PARALLEL" || uKey=="DENSITY_BACKEND" ||
               uKey=="MODE3D_DENSITY_PARALLEL" || uKey=="MODE3D_DENSITY_BACKEND" ||
               uKey=="MODE3D_PARALLEL" || uKey=="MODE3D_BACKEND" ||
               uKey=="GRIDLESS_PARALLEL" || uKey=="GRIDLESS_BACKEND" ||
               uKey=="BACKTRACK_PARALLEL" || uKey=="BACKTRACK_BACKEND" ||
               uKey=="BACKWARD_PARALLEL" || uKey=="BACKWARD_BACKEND") {
        p.mode3d.densityParallelBackend=ToUpper(Trim(val));
      }
      else if (uKey=="DS_THREADS" || uKey=="DS_NTHREADS" ||
               uKey=="DENSITY_THREADS" || uKey=="DENSITY_NTHREADS" ||
               uKey=="MODE3D_DENSITY_THREADS" || uKey=="MODE3D_DENSITY_NTHREADS" ||
               uKey=="MODE3D_THREADS" || uKey=="MODE3D_NTHREADS" ||
               uKey=="GRIDLESS_THREADS" || uKey=="GRIDLESS_NTHREADS" ||
               uKey=="BACKTRACK_THREADS" || uKey=="BACKTRACK_NTHREADS" ||
               uKey=="BACKWARD_THREADS" || uKey=="BACKWARD_NTHREADS") {
        p.mode3d.densityThreads=std::stoi(val);
      }
      else if (uKey=="MODE3D_MPI_SCHEDULER" || uKey=="MODE3D_MPI_BACKEND" ||
               uKey=="GRIDLESS_MPI_SCHEDULER" || uKey=="GRIDLESS_MPI_BACKEND" ||
               uKey=="BACKTRACK_MPI_SCHEDULER" || uKey=="BACKTRACK_MPI_BACKEND" ||
               uKey=="BACKWARD_MPI_SCHEDULER" || uKey=="BACKWARD_MPI_BACKEND") {
        p.mode3d.mpiScheduler=ToUpper(Trim(val));
      }
      else if (uKey=="MODE3D_MPI_DYNAMIC_CHUNK" || uKey=="MODE3D_MPI_CHUNK" ||
               uKey=="GRIDLESS_MPI_DYNAMIC_CHUNK" || uKey=="GRIDLESS_MPI_CHUNK" ||
               uKey=="BACKTRACK_MPI_DYNAMIC_CHUNK" || uKey=="BACKTRACK_MPI_CHUNK" ||
               uKey=="BACKWARD_MPI_DYNAMIC_CHUNK" || uKey=="BACKWARD_MPI_CHUNK") {
        p.mode3d.mpiDynamicChunk=std::stoi(val);
      }
      else rejectUnknownKeyword();
    }
    else if (section=="#SPECTRUM") {
      if (IsRecognizedSpectrumKey(uKey)) p.spectrum[uKey]=val;
      else rejectUnknownKeyword();
    }
    else if (section=="#BOUNDARY_ANISOTROPY") {
      if      (uKey=="BA_PAD_MODEL")        p.anisotropy.padModel        = ToUpper(val);
      else if (uKey=="BA_PAD_EXPONENT")     p.anisotropy.padExponent     = std::stod(val);
      else if (uKey=="BA_SPATIAL_MODEL")    p.anisotropy.spatialModel    = ToUpper(val);
      else if (uKey=="BA_DAYSIDE_FACTOR")   p.anisotropy.daysideFactor   = std::stod(val);
      else if (uKey=="BA_NIGHTSIDE_FACTOR") p.anisotropy.nightsideFactor = std::stod(val);
      else rejectUnknownKeyword();
    }
    else if (section=="#OUTPUT_OPTIONS") {
      if (uKey=="OUTPUT_COORDS" || uKey=="COORDS") {
        // Some RoR files place the coordinate selector in #OUTPUT_OPTIONS rather
        // than #OUTPUT_DOMAIN. Route it to the same destination.
        p.output.coords=ToUpper(Trim(val));
        p.outputOptions[uKey]=val;
      }
      else if (uKey=="FLUX_TYPE" || uKey=="OUTPUT_CUTOFF" ||
               uKey=="OUTPUT_PITCH" || uKey=="OUTPUT_FORMAT" ||
               uKey=="ENERGY_BINS") {
        // Recognized RoR output-product metadata. These options are not all
        // consumed by the current standalone Mode3D implementation, but accepting
        // and storing them avoids rejecting otherwise valid generated inputs.
        p.outputOptions[uKey]=val;
      }
      else {
        // Keep strict validation for genuinely unknown output options.
        rejectUnknownKeyword();
      }
    }
    else if (section=="#TEMPORAL") {
      if      (uKey=="TEMPORAL_MODE")    p.temporal.mode=ToUpper(val);
      else if (uKey=="EVENT_START")      p.temporal.eventStart=Trim(val);
      else if (uKey=="EVENT_END")        p.temporal.eventEnd=Trim(val);
      else if (uKey=="FIELD_UPDATE_DT")  p.temporal.fieldUpdateDt_min=ParsePositiveFiniteDouble(val,"#TEMPORAL/FIELD_UPDATE_DT",lineNo);
      else if (uKey=="INJECT_DT")        p.temporal.injectDt_min=std::stod(val);
      else if (uKey=="TS_INPUT_MODE")    p.temporal.tsInputMode=ToUpper(val);
      else if (uKey=="TS_INPUT_FILE")    p.temporal.tsInputFile=Trim(val);
      else if (uKey=="SNAPSHOT_LIST_FILE") p.temporal.snapshotListFile=Trim(val);
      else rejectUnknownKeyword();
    }
    else {
      rejectUnknownKeyword();
    }
  }

  

// Validate dipole-specific background field settings if requested.
if (ToUpper(p.field.model)=="DIPOLE") {
  // Moment scaling must be positive.
  if (!(p.field.dipoleMoment_Me>0.0)) {
    exit(__LINE__,__FILE__,"DIPOLE_MOMENT must be > 0 (multiples of M_E)");
  }
  // Keep tilt range conservative; values outside +/-90 deg usually indicate
  // a coordinate/sign convention mistake in the input.
  if (p.field.dipoleTilt_deg < -90.0 || p.field.dipoleTilt_deg > 90.0) {
    exit(__LINE__,__FILE__,"DIPOLE_TILT must be in [-90, 90] degrees");
  }
}

  // Finalize the point-like initialization path.  This is the key step that makes
  // POINTS and TRAJECTORY reuse the same downstream solver kernels:
  //   POINTS     -> one synthetic one-sample trajectory per point
  //   TRAJECTORY -> real multi-sample trajectory loaded from file
  // In both cases the result is a flattened output.points array plus the unified
  // output.trajectories container used for per-sample timestamps.
  InitializePointLikeOutput(p);

  // Finalize loading of a time-dependent Tsyganenko driver table.
  //
  // Supported user-facing ways to request the same capability:
  //
  //   (A) Legacy / explicit temporal syntax
  //       #TEMPORAL
  //       TS_INPUT_MODE  FILE
  //       TS_INPUT_FILE  ...
  //
  //   (B) Magnetic-model-local syntax
  //       #BACKGROUND_FIELD
  //       FIELD_MODEL    TS05
  //       DRIVER_FILE    ...
  //
  // The solver uses only one shared object at runtime: p.temporal.driverTable.
  // Therefore the parser resolves both input styles here and loads exactly one
  // file into that table.
  //
  // Precedence rule:
  //   #TEMPORAL TS_INPUT_FILE wins over #BACKGROUND_FIELD DRIVER_FILE when both
  //   are present, because #TEMPORAL is the more explicit request for
  //   time-dependent forcing.  This keeps backward compatibility with inputs that
  //   already relied on TS_INPUT_FILE while still allowing the simpler
  //   BACKGROUND_FIELD-only syntax requested for Tsyganenko runs.
  //
  // Validation rule:
  //   If TS_INPUT_MODE=FILE is requested, a filename must be available from
  //   either TS_INPUT_FILE or DRIVER_FILE.  If neither is provided, abort with a
  //   clear message instead of silently falling back to static parameters.
  {
    const std::string temporalMode = ToUpper(p.temporal.tsInputMode);
    const std::string temporalDriverFile = Trim(p.temporal.tsInputFile);
    const std::string backgroundDriverFile = Trim(p.field.driverFile);

    // Determine whether the run explicitly requests a file-backed driver table
    // through either configuration path.
    const bool temporalRequestsFile = (temporalMode == "FILE");
    const bool backgroundProvidesFile = !backgroundDriverFile.empty();

    // Pick the actual file to load.  Prefer #TEMPORAL TS_INPUT_FILE when set;
    // otherwise fall back to #BACKGROUND_FIELD DRIVER_FILE.
    std::string resolvedDriverFile;
    if (!temporalDriverFile.empty()) resolvedDriverFile = temporalDriverFile;
    else if (backgroundProvidesFile) resolvedDriverFile = backgroundDriverFile;

    // When the input explicitly says TS_INPUT_MODE=FILE, insist that a filename
    // exists somewhere in the input.  This preserves the old safety check while
    // broadening where the filename may come from.
    if (temporalRequestsFile && resolvedDriverFile.empty()) {
      exit(__LINE__,__FILE__,
           "TS_INPUT_MODE=FILE requires TS_INPUT_FILE in #TEMPORAL or DRIVER_FILE in #BACKGROUND_FIELD");
    }

    // Load the table whenever either syntax supplied a filename.  The loaded
    // table is attached to p.temporal.driverTable because the runtime solvers
    // already look there for per-epoch interpolation.
    if (!resolvedDriverFile.empty()) {
      p.temporal.driverTable = LoadTsDriverFile(
          resolvedDriverFile,
          p.field.model);  // model name drives per-model column validation

      // Keep the resolved filename mirrored in the temporal structure so any
      // later diagnostics or dumps that inspect TS_INPUT_FILE still see the
      // actual source used for the run, even when it originally came from
      // #BACKGROUND_FIELD DRIVER_FILE.
      p.temporal.tsInputFile = resolvedDriverFile;

      // If the user supplied DRIVER_FILE through #BACKGROUND_FIELD while leaving
      // #TEMPORAL in its default PARAMS mode, promote the effective mode to FILE
      // so downstream diagnostics and solver branches accurately reflect that the
      // magnetic field is being driven by an external time table.
      //
      // We intentionally do not override an explicit user request such as
      // TS_INPUT_MODE=FILE (already consistent) or any future nonstandard mode
      // token that the caller may want to inspect later.
      if (temporalMode.empty() || temporalMode == "PARAMS") p.temporal.tsInputMode = "FILE";
    }
  }


  // Normalize generic directional-aperture coverage after the complete AMPS_PARAM
  // file has been read.  IMPORTANT: do *not* open DIRMAP_APERTURE_FILE here.
  //
  // Aperture-file loading is deliberately deferred until main.cpp has merged command-
  // line overrides.  Otherwise an input deck that names a stale/nonexistent aperture
  // file would fail before a valid `-cutoff-dirmap-aperture-file` CLI override had a
  // chance to replace it.  Inline DIRMAP_APERTURE records are parsed immediately and
  // retained; the final file + inline list is assembled once, after CLI precedence has
  // been resolved.
  {
    std::string coverage=ToUpper(Trim(p.cutoff.dirMapCoverage));
    if (coverage=="FULL" || coverage=="SPHERE") coverage="FULL_SPHERE";
    if (coverage=="APERTURES" || coverage=="INSTRUMENT_APERTURES" ||
        coverage=="VECTOR" || coverage=="VECTORS") coverage="VECTOR_APERTURES";
    p.cutoff.dirMapCoverage=coverage;

    if (!(coverage=="FULL_SPHERE" || coverage=="VECTOR_APERTURES"))
      exit(__LINE__,__FILE__,"DIRMAP_COVERAGE must be FULL_SPHERE or VECTOR_APERTURES");
    if (coverage=="VECTOR_APERTURES" && p.cutoff.dirMapApertures.empty() &&
        p.cutoff.dirMapApertureFile.empty())
      exit(__LINE__,__FILE__,
           "DIRMAP_COVERAGE VECTOR_APERTURES requires DIRMAP_APERTURE_FILE or at least one DIRMAP_APERTURE");
  }


  // Validate global trajectory-control settings used by both cutoff and density/flux
  // backtracing.  ADAPTIVE_DT only changes how DT_TRACE is interpreted; the remaining
  // hard limits keep the same meaning in fixed-step and adaptive-step modes.
  if (!(p.numerics.dtTrace_s > 0.0)) {
    exit(__LINE__,__FILE__,"DT_TRACE must be > 0 s");
  }
  if (p.numerics.maxSteps <= 0) {
    exit(__LINE__,__FILE__,"MAX_STEPS must be > 0");
  }
  if (!(p.numerics.maxTraceTime_s > 0.0)) {
    exit(__LINE__,__FILE__,"MAX_TRACE_TIME must be > 0 s");
  }
  if (p.numerics.maxTraceDistance_Re < 0.0) {
    exit(__LINE__,__FILE__,"MAX_TRACE_DISTANCE must be >= 0 (0 means: disabled)");
  }
  if (!(p.numerics.boundaryEventTolerance_m >= 0.0) ||
      !std::isfinite(p.numerics.boundaryEventTolerance_m)) {
    exit(__LINE__,__FILE__,"BOUNDARY_EVENT_TOL_M must be finite and >= 0");
  }
  if (p.numerics.boundaryEventMaxIterations < 1) {
    exit(__LINE__,__FILE__,"BOUNDARY_EVENT_MAX_ITER must be >= 1");
  }
  if (p.numerics.trapMinMirrorPoints < 2) {
    exit(__LINE__,__FILE__,"TRAP_MIN_MIRROR_POINTS must be >= 2");
  }
  if (p.numerics.trapMinBounceCycles < 1) {
    exit(__LINE__,__FILE__,"TRAP_MIN_BOUNCES must be >= 1");
  }
  if (!(p.numerics.trapOuterMargin_Re >= 0.0) ||
      !std::isfinite(p.numerics.trapOuterMargin_Re)) {
    exit(__LINE__,__FILE__,"TRAP_OUTER_MARGIN_RE must be finite and >= 0");
  }
  if (!(p.numerics.trapRadialGrowthTolerance_Re >= 0.0) ||
      !std::isfinite(p.numerics.trapRadialGrowthTolerance_Re)) {
    exit(__LINE__,__FILE__,"TRAP_RADIAL_GROWTH_TOL_RE must be finite and >= 0");
  }
  if (!(p.numerics.trapEnergyRelativeTolerance >= 0.0) ||
      !std::isfinite(p.numerics.trapEnergyRelativeTolerance)) {
    exit(__LINE__,__FILE__,"TRAP_ENERGY_REL_TOL must be finite and >= 0");
  }
  if (!(p.numerics.trapParallelDeadband >= 0.0 &&
        p.numerics.trapParallelDeadband < 1.0) ||
      !std::isfinite(p.numerics.trapParallelDeadband)) {
    exit(__LINE__,__FILE__,"TRAP_PARALLEL_DEADBAND must be finite and in [0,1)");
  }
  if (p.numerics.trapMinDriftRevolutions < 2) {
    exit(__LINE__,__FILE__,"TRAP_MIN_DRIFT_REVOLUTIONS must be >= 2 so full-orbit recurrence can be compared between complete drift turns");
  }
  if (!(p.numerics.trapDriftRadialGrowthTolerance_Re >= 0.0) ||
      !std::isfinite(p.numerics.trapDriftRadialGrowthTolerance_Re)) {
    exit(__LINE__,__FILE__,
         "TRAP_DRIFT_RADIAL_GROWTH_TOL_RE must be finite and >= 0");
  }
  if (!(p.numerics.trapDriftRadialRelativeTolerance >= 0.0) ||
      !std::isfinite(p.numerics.trapDriftRadialRelativeTolerance))
    exit(__LINE__,__FILE__,"TRAP_DRIFT_RADIAL_REL_TOL must be finite and >= 0");
  if (!(p.numerics.trapDriftLatitudeTolerance >= 0.0) ||
      !std::isfinite(p.numerics.trapDriftLatitudeTolerance))
    exit(__LINE__,__FILE__,"TRAP_DRIFT_LATITUDE_TOL must be finite and >= 0");
  if (!(p.numerics.trapDriftPitchCos2Tolerance >= 0.0) ||
      !std::isfinite(p.numerics.trapDriftPitchCos2Tolerance))
    exit(__LINE__,__FILE__,"TRAP_DRIFT_PITCH_COS2_TOL must be finite and >= 0");
  if (!(p.numerics.trapDriftMaxMeanRadiusChange_Re >= 0.0) ||
      !std::isfinite(p.numerics.trapDriftMaxMeanRadiusChange_Re))
    exit(__LINE__,__FILE__,
         "TRAP_DRIFT_MAX_MEAN_RADIUS_CHANGE_RE must be finite and >= 0 (0 disables the veto)");
  if (p.numerics.trapDriftProfileBins < 8 || p.numerics.trapDriftProfileBins > 360)
    exit(__LINE__,__FILE__,"TRAP_DRIFT_PROFILE_BINS must be in [8,360]");
  if (!(p.numerics.trapDriftMinProfileCoverage > 0.0 &&
        p.numerics.trapDriftMinProfileCoverage <= 1.0) ||
      !std::isfinite(p.numerics.trapDriftMinProfileCoverage))
    exit(__LINE__,__FILE__,"TRAP_DRIFT_MIN_PROFILE_COVERAGE must be finite and in (0,1]");
  if (!(p.numerics.trapDriftMinMatchedBinFraction > 0.0 &&
        p.numerics.trapDriftMinMatchedBinFraction <= 1.0) ||
      !std::isfinite(p.numerics.trapDriftMinMatchedBinFraction))
    exit(__LINE__,__FILE__,"TRAP_DRIFT_MIN_MATCHED_BIN_FRACTION must be finite and in (0,1]");

  // Resolve product-selection requests from CALC_TARGET.
  //
  // Historically CALC_TARGET was a single exact token.  Mode3D now supports combined
  // products such as CUTOFF_RIGIDITY+DENSITY_SPECTRUM so cutoff, density, and flux can
  // be generated from the same magnetic-field snapshot.  The parser therefore validates
  // each product if its name appears anywhere in the target string; the runtime mode
  // (-mode gridless, -mode 3d, or SWMF-coupled 3d) decides which field backend to use.
  const std::string calcTargetUpper = ToUpper(p.calc.target);
  const bool calcRequestsCutoff = (calcTargetUpper.find("CUTOFF") != std::string::npos) ||
                                  calcTargetUpper=="ALL" || calcTargetUpper=="BOTH";
  const bool calcRequestsDensityFlux = (calcTargetUpper.find("DENSITY") != std::string::npos) ||
                                       (calcTargetUpper.find("FLUX")    != std::string::npos) ||
                                       calcTargetUpper=="ALL" || calcTargetUpper=="BOTH";

  // Validate cutoff controls if requested.
  if (calcRequestsCutoff) {
    if (!(p.cutoff.eMin_MeV>0.0)) {
      exit(__LINE__,__FILE__,"CUTOFF_EMIN must be > 0 (MeV/n)");
    }
    if (!(p.cutoff.eMax_MeV>p.cutoff.eMin_MeV)) {
      exit(__LINE__,__FILE__,"CUTOFF_EMAX must be > CUTOFF_EMIN (MeV/n)");
    }
    if (!(p.cutoff.nEnergy>=1)) {
      exit(__LINE__,__FILE__,"CUTOFF_NENERGY must be >= 1");
    }
    if (p.cutoff.maxTrajTime_s < 0.0 || !std::isfinite(p.cutoff.maxTrajTime_s)) {
      exit(__LINE__,__FILE__,
           "CUTOFF_MAX_TRAJ_TIME must be finite and >= 0 (0 means: use MAX_TRACE_TIME)");
    }
    if (p.cutoff.unresolvedExtensionPasses < 0 ||
        p.cutoff.unresolvedExtensionPasses > 8) {
      exit(__LINE__,__FILE__,
           "CUTOFF_UNRESOLVED_EXTENSION_PASSES must be in [0,8]");
    }
    if (!(p.cutoff.unresolvedExtensionFactor > 1.0) ||
        !std::isfinite(p.cutoff.unresolvedExtensionFactor)) {
      exit(__LINE__,__FILE__,
           "CUTOFF_UNRESOLVED_EXTENSION_FACTOR must be finite and > 1");
    }
    {
      const std::string cutoffSearch = ToUpper(p.cutoff.searchAlgorithm);
      if (!(cutoffSearch=="UPPER_SCAN" || cutoffSearch=="UPPER" ||
            cutoffSearch=="SCAN" ||
            cutoffSearch=="PENUMBRA_SCAN" || cutoffSearch=="BOTH" ||
            cutoffSearch=="FULL_SCAN" || cutoffSearch=="CUTOFF_BAND" ||
            cutoffSearch=="RIGIDITY_LIST" || cutoffSearch=="FIXED_RIGIDITY" ||
            cutoffSearch=="FIXED_RIGIDITIES" || cutoffSearch=="ACCESS_LIST" ||
            cutoffSearch=="DIRECT_ACCESS" ||
            cutoffSearch=="BINARY" || cutoffSearch=="ENDPOINT_BINARY" ||
            cutoffSearch=="LEGACY_BINARY")) {
        exit(__LINE__,__FILE__,
             "CUTOFF_SEARCH_ALGORITHM must be UPPER_SCAN, PENUMBRA_SCAN, "
             "DIRECT_ACCESS, RIGIDITY_LIST, or BINARY");
      }
      if (cutoffSearch=="UPPER" || cutoffSearch=="SCAN")
        p.cutoff.searchAlgorithm="UPPER_SCAN";
      else if (cutoffSearch=="BOTH" || cutoffSearch=="FULL_SCAN" ||
               cutoffSearch=="CUTOFF_BAND")
        p.cutoff.searchAlgorithm="PENUMBRA_SCAN";
      else if (cutoffSearch=="FIXED_RIGIDITY" ||
               cutoffSearch=="FIXED_RIGIDITIES" || cutoffSearch=="ACCESS_LIST")
        p.cutoff.searchAlgorithm="RIGIDITY_LIST";
      else if (cutoffSearch=="DIRECT_ACCESS")
        // Preserve DIRECT_ACCESS as a distinct directional point/trajectory product.
        // RIGIDITY_LIST remains the historical shell-oriented vertical access mode.
        p.cutoff.searchAlgorithm="DIRECT_ACCESS";
      else if (cutoffSearch=="ENDPOINT_BINARY" || cutoffSearch=="LEGACY_BINARY")
        p.cutoff.searchAlgorithm="BINARY";
      else
        p.cutoff.searchAlgorithm=cutoffSearch;
    }
    if ((p.cutoff.searchAlgorithm=="PENUMBRA_SCAN" ||
         p.cutoff.searchAlgorithm=="RIGIDITY_LIST" ||
         p.cutoff.searchAlgorithm=="DIRECT_ACCESS") &&
        ToUpper(p.cutoff.sampling)!="VERTICAL") {
      exit(__LINE__,__FILE__,
           "PENUMBRA_SCAN, RIGIDITY_LIST, and DIRECT_ACCESS require CUTOFF_SAMPLING VERTICAL");
    }
    // A rigidity list is mandatory for RIGIDITY_LIST and optional for
    // PENUMBRA_SCAN.  In the latter case it requests exact fixed-rigidity access
    // states as a companion diagnostic (used by C9/PAMELA_T50) without changing
    // the regular penumbra grid or effective-cutoff integration.
    if ((p.cutoff.searchAlgorithm=="RIGIDITY_LIST" ||
         p.cutoff.searchAlgorithm=="DIRECT_ACCESS") &&
        p.cutoff.rigidityList_GV.empty()) {
      exit(__LINE__,__FILE__,
           "RIGIDITY_LIST and DIRECT_ACCESS require a non-empty CUTOFF_RIGIDITY_LIST_GV");
    }
    if (p.cutoff.directAccessAdaptiveMaxDepth < 0 ||
        p.cutoff.directAccessAdaptiveMaxDepth > 20) {
      exit(__LINE__,__FILE__,
           "CUTOFF_DIRECT_ACCESS_ADAPTIVE_MAX_DEPTH must be in [0,20]");
    }
    if (p.cutoff.directAccessAdaptiveGuardDepth < 0 ||
        p.cutoff.directAccessAdaptiveGuardDepth > p.cutoff.directAccessAdaptiveMaxDepth) {
      exit(__LINE__,__FILE__,
           "CUTOFF_DIRECT_ACCESS_ADAPTIVE_GUARD_DEPTH must be in [0,max depth]");
    }
    if (p.cutoff.directAccessAdaptive &&
        p.cutoff.searchAlgorithm != "DIRECT_ACCESS") {
      exit(__LINE__,__FILE__,
           "CUTOFF_DIRECT_ACCESS_ADAPTIVE=T is valid only with CUTOFF_SEARCH_ALGORITHM DIRECT_ACCESS");
    }
    if (!p.cutoff.rigidityList_GV.empty()) {
      for (std::size_t i=0;i<p.cutoff.rigidityList_GV.size();++i) {
        const double value=p.cutoff.rigidityList_GV[i];
        if (!(value>0.0) || !std::isfinite(value)) {
          exit(__LINE__,__FILE__,
               "CUTOFF_RIGIDITY_LIST_GV values must be finite and > 0 GV");
        }
        if (i>0 && !(value>p.cutoff.rigidityList_GV[i-1])) {
          exit(__LINE__,__FILE__,
               "CUTOFF_RIGIDITY_LIST_GV values must be strictly increasing");
        }
      }
    }
    if (p.cutoff.searchAlgorithm=="RIGIDITY_LIST" &&
        !(p.cutoff.accessAbsLatMin_deg>=0.0 &&
          p.cutoff.accessAbsLatMax_deg<=90.0 &&
          p.cutoff.accessAbsLatMax_deg>p.cutoff.accessAbsLatMin_deg)) {
      exit(__LINE__,__FILE__,
           "RIGIDITY_LIST requires 0 <= CUTOFF_ACCESS_ABS_LAT_MIN < "
           "CUTOFF_ACCESS_ABS_LAT_MAX <= 90 degrees");
    }
    if (p.cutoff.upperScanN < 0) {
      exit(__LINE__,__FILE__,"CUTOFF_UPPER_SCAN_N must be >= 0 (0 means: use CUTOFF_NENERGY)");
    }
    if (p.cutoff.upperScanN > 0 && p.cutoff.upperScanN < 2) {
      exit(__LINE__,__FILE__,"CUTOFF_UPPER_SCAN_N must be >= 2 when specified");
    }
    {
      const std::string policy=ToUpper(p.cutoff.traceIntegrationPolicy);
      if (policy=="LEGACY" || policy=="COMPATIBLE" ||
          policy=="LEGACY_CUTOFF" || policy=="C_SERIES") {
        p.cutoff.traceIntegrationPolicy="LEGACY";
      }
      else if (policy=="ACCURATE" || policy=="STRUCTURED" ||
               policy=="F3" || policy=="UPPER_BOUNDED") {
        p.cutoff.traceIntegrationPolicy="ACCURATE";
      }
      else {
        exit(__LINE__,__FILE__,
             "CUTOFF_TRACE_POLICY must be LEGACY or ACCURATE");
      }
    }
    {
      const std::string convention=ToUpper(Trim(p.cutoff.backtraceChargeConvention));
      if (convention=="SAME" || convention=="SAME_SIGN" ||
          convention=="LEGACY" || convention=="HISTORICAL") {
        p.cutoff.backtraceChargeConvention="SAME";
      }
      else if (convention=="REVERSED" || convention=="REVERSE" ||
               convention=="OPPOSITE" || convention=="ANTIPARTICLE" ||
               convention=="PHYSICAL") {
        p.cutoff.backtraceChargeConvention="REVERSED";
      }
      else {
        exit(__LINE__,__FILE__,
             "CUTOFF_BACKTRACE_CHARGE must be SAME or REVERSED");
      }
    }
    {
      const std::string policy=ToUpper(Trim(p.cutoff.traceLimitPolicy));
      if (policy=="UNRESOLVED" || policy=="STRICT" ||
          policy=="THREE_STATE") {
        p.cutoff.traceLimitPolicy="UNRESOLVED";
      }
      else if (policy=="FORBIDDEN" || policy=="LEGACY" ||
               policy=="REFERENCE" || policy=="CUTOFF_TABLE") {
        p.cutoff.traceLimitPolicy="FORBIDDEN";
      }
      else {
        exit(__LINE__,__FILE__,
             "CUTOFF_TRACE_LIMIT_POLICY must be UNRESOLVED or FORBIDDEN");
      }
    }
    if (p.cutoff.unresolvedExtensionPasses>0 &&
        p.cutoff.traceLimitPolicy!="UNRESOLVED") {
      exit(__LINE__,__FILE__,
           "CUTOFF_UNRESOLVED_EXTENSION_PASSES requires CUTOFF_TRACE_LIMIT_POLICY UNRESOLVED");
    }
    if (p.cutoff.maxParticlesPerPoint < 1) {
      exit(__LINE__,__FILE__,"CUTOFF_MAX_PARTICLES must be >= 1");
    }
    if (p.cutoff.debugRigidityScan) {
      if (p.cutoff.debugScanN < 2) {
        exit(__LINE__,__FILE__,"CUTOFF_DEBUG_SCAN_N must be >= 2");
      }
      if (p.cutoff.debugScanFile.empty()) {
        exit(__LINE__,__FILE__,"CUTOFF_DEBUG_SCAN_FILE must not be empty");
      }
      if (!std::isfinite(p.cutoff.debugScanLon_deg) ||
          !std::isfinite(p.cutoff.debugScanLat_deg) ||
          !std::isfinite(p.cutoff.debugScanAlt_km)) {
        exit(__LINE__,__FILE__,"CUTOFF_DEBUG_SCAN_LON/LAT/ALT must be finite numbers");
      }
      if (p.cutoff.debugScanLat_deg < -90.0 || p.cutoff.debugScanLat_deg > 90.0) {
        exit(__LINE__,__FILE__,"CUTOFF_DEBUG_SCAN_LAT must be in [-90,90] degrees");
      }
    }
    if (p.cutoff.debugExitTrace) {
      if (p.cutoff.debugExitN < 2) {
        exit(__LINE__,__FILE__,"CUTOFF_DEBUG_EXIT_N must be >= 2");
      }
      if (p.cutoff.debugExitFile.empty()) {
        exit(__LINE__,__FILE__,"CUTOFF_DEBUG_EXIT_FILE must not be empty");
      }
      if (!p.cutoff.debugExitListFile.empty()) {
        // The list file is opened by the solver in the run directory.  It contains
        // all lon/lat/alt/R validation trajectories, so no single-point coordinate
        // validation is needed here.
      }
      else {
        if (!std::isfinite(p.cutoff.debugExitLon_deg) ||
            !std::isfinite(p.cutoff.debugExitLat_deg) ||
            !std::isfinite(p.cutoff.debugExitAlt_km) ||
            !std::isfinite(p.cutoff.debugExitR_GV)) {
          exit(__LINE__,__FILE__,"CUTOFF_DEBUG_EXIT_LON/LAT/ALT/R_GV must be finite numbers");
        }
        if (p.cutoff.debugExitLat_deg < -90.0 || p.cutoff.debugExitLat_deg > 90.0) {
          exit(__LINE__,__FILE__,"CUTOFF_DEBUG_EXIT_LAT must be in [-90,90] degrees");
        }
      }
    }
  }

  // Validate shared Mode3D backward-product parallel controls if cutoff or density/flux
  // is requested.  The DENSITY_* names are historical; in Mode3D they control the
  // generic intra-rank backend for both cutoff rigidity and density/flux.  Therefore
  // cutoff-only inputs should catch invalid backend/thread settings during parsing too.
  if (calcRequestsCutoff || calcRequestsDensityFlux) {
    if (p.mode3d.densityThreads < 0) {
      exit(__LINE__,__FILE__,"DENSITY_THREADS/MODE3D_THREADS/GRIDLESS_THREADS/BACKTRACK_THREADS must be >= 0 (0 means: automatic)");
    }
    if (!p.mode3d.densityParallelBackend.empty()) {
      const std::string db = ToUpper(p.mode3d.densityParallelBackend);
      if (db!="OPENMP" && db!="OMP" && db!="THREAD" && db!="THREADS" &&
          db!="STD_THREAD" && db!="STD_THREADS" && db!="PTHREAD" && db!="PTHREADS" &&
          db!="SERIAL" && db!="NONE" && db!="OFF") {
        std::ostringstream _exit_msg;
        _exit_msg << "DENSITY_PARALLEL/MODE3D_PARALLEL/GRIDLESS_PARALLEL/BACKTRACK_PARALLEL must be OPENMP, THREADS, or SERIAL (got '"
                  << p.mode3d.densityParallelBackend << "')";
        exit(__LINE__,__FILE__,_exit_msg.str().c_str());
      }
      p.mode3d.densityParallelBackend = db;
    }

    if (!p.mode3d.mpiScheduler.empty()) {
      const std::string ms = ToUpper(p.mode3d.mpiScheduler);
      if (ms!="DYNAMIC" && ms!="DYN" && ms!="QUEUE" &&
          ms!="WORK_QUEUE" && ms!="WORKQUEUE" &&
          ms!="BLOCK_CYCLIC" && ms!="BLOCKCYCLIC" && ms!="CYCLIC" &&
          ms!="ROUND_ROBIN" && ms!="ROUNDROBIN" &&
          ms!="STATIC" && ms!="CONTIGUOUS" && ms!="BLOCK" && ms!="SLAB") {
        std::ostringstream _exit_msg;
        _exit_msg << "MODE3D_MPI_SCHEDULER/GRIDLESS_MPI_SCHEDULER/BACKTRACK_MPI_SCHEDULER must be DYNAMIC, BLOCK_CYCLIC, or STATIC (got '"
                  << p.mode3d.mpiScheduler << "')";
        exit(__LINE__,__FILE__,_exit_msg.str().c_str());
      }
      p.mode3d.mpiScheduler = ms;
    }

    if (p.mode3d.mpiDynamicChunk < 0) {
      exit(__LINE__,__FILE__,
           "MODE3D_MPI_DYNAMIC_CHUNK/GRIDLESS_MPI_DYNAMIC_CHUNK/BACKTRACK_MPI_DYNAMIC_CHUNK must be >= 0 (0 means: automatic)");
    }
  }

  if (p.mode3d.meshResolutionProfileActive) {
    if (!(p.mode3d.meshResolutionEarth_km > 0.0)) {
      exit(__LINE__,__FILE__,
           "MODE3D_MESH_RES_EARTH_RE/KM must be specified and > 0 when using a user-defined Mode3D mesh profile");
    }
    if (!(p.mode3d.meshResolutionBoundary_km > 0.0)) {
      exit(__LINE__,__FILE__,
           "MODE3D_MESH_RES_BOUNDARY_RE/KM must be specified and > 0 when using a user-defined Mode3D mesh profile");
    }
    if (p.mode3d.meshResolutionOuterRadius_km < 0.0) {
      exit(__LINE__,__FILE__,
           "MODE3D_MESH_R_BOUNDARY_RE/KM must be >= 0 when specified (0 means: infer from domain)");
    }
    const std::string mc = ToUpper(Trim(p.mode3d.meshResolutionCoarsening));
    if (mc=="LINEAR" || mc=="LIN") p.mode3d.meshResolutionCoarsening="LINEAR";
    else if (mc=="LOG" || mc=="LOGARITHMIC" || mc=="GEOMETRIC" ||
             mc=="EXP" || mc=="EXPONENTIAL") p.mode3d.meshResolutionCoarsening="LOG";
    else if (mc=="POWER" || mc=="POW" || mc=="EXPONENT" || mc=="POLYNOMIAL") p.mode3d.meshResolutionCoarsening="POWER";
    else if (mc=="CONSTANT" || mc=="CONST" || mc=="UNIFORM") p.mode3d.meshResolutionCoarsening="CONSTANT";
    else {
      std::ostringstream _exit_msg;
      _exit_msg << "MODE3D_MESH_COARSENING must be LINEAR, LOG/EXPONENTIAL, POWER, or CONSTANT (got '"
                << p.mode3d.meshResolutionCoarsening << "')";
      exit(__LINE__,__FILE__,_exit_msg.str().c_str());
    }
    if (!(p.mode3d.meshResolutionExponent > 0.0) || !std::isfinite(p.mode3d.meshResolutionExponent)) {
      exit(__LINE__,__FILE__,"MODE3D_MESH_EXPONENT must be finite and > 0");
    }
  }

  // Validate 3d_forward density/energy-grid controls if requested.
  if (ToUpper(p.calc.target)=="DENSITY_3D") {
    if (!(p.density3d.Emin_MeV > 0.0)) {
      exit(__LINE__,__FILE__,"DENS_EMIN must be > 0 (MeV/n)");
    }
    if (!(p.density3d.Emax_MeV > p.density3d.Emin_MeV)) {
      exit(__LINE__,__FILE__,"DENS_EMAX must be > DENS_EMIN (MeV/n)");
    }
    if (!(p.density3d.nEnergyBins >= 1)) {
      exit(__LINE__,__FILE__,"DENS_NENERGY must be >= 1");
    }
    if (p.mode3dForward.nParticleTrajectories < 0) {
      exit(__LINE__,__FILE__,
           "N_TRAJECTORIES must be >= 0 (0 disables particle trajectory initialization)");
    }
    if (p.mode3dForward.initializeParticleTrajectories &&
        p.mode3dForward.nParticleTrajectories <= 0) {
      exit(__LINE__,__FILE__,
           "INITIALIZE_TRAJECTORIES requires N_TRAJECTORIES > 0 in #PARTICLE_TRAJECTORY");
    }
  }

  // Validate density/spectrum controls if requested.  Do not require
  // FIELD_EVAL_METHOD=GRIDLESS here: in -mode 3d the same DensitySpectrumParam block is
  // deliberately consumed by the mesh-field density/flux backend, and in SWMF-coupled
  // mode the field method is set to SWMF by the coupling bridge.
  if (calcRequestsDensityFlux) {
    if (!(p.densitySpectrum.Emin_MeV>0.0)) {
      exit(__LINE__,__FILE__,"DS_EMIN must be > 0 (MeV)");
    }
    if (!(p.densitySpectrum.Emax_MeV>p.densitySpectrum.Emin_MeV)) {
      exit(__LINE__,__FILE__,"DS_EMAX must be > DS_EMIN (MeV)");
    }
    if (!(p.densitySpectrum.nIntervals>=1)) {
      exit(__LINE__,__FILE__,"DS_NINTERVALS must be >= 1");
    }
    if (p.densitySpectrum.maxParticlesPerPoint < 0) {
      exit(__LINE__,__FILE__,"DS_MAX_PARTICLES must be >= 0 (0 means: no cap)");
    }
    if (p.densitySpectrum.maxTrajTime_s < 0.0) {
      exit(__LINE__,__FILE__,"DS_MAX_TRAJ_TIME must be >= 0 (0 means: use MAX_TRACE_TIME)");
    }
    if (p.numerics.maxTraceDistance_Re < 0.0) {
      exit(__LINE__,__FILE__,"MAX_TRACE_DISTANCE must be >= 0 (0 means: disabled)");
    }
    if (!(p.densitySpectrum.unresolvedTolerance >= 0.0 &&
          p.densitySpectrum.unresolvedTolerance <= 1.0) ||
        !std::isfinite(p.densitySpectrum.unresolvedTolerance)) {
      exit(__LINE__,__FILE__,"DS_UNRESOLVED_TOL must be finite and in [0,1]");
    }
    // Validate transmission-function controls.
    {
      const std::string tm = ToUpper(p.densitySpectrum.transmissionMode);
      if (tm=="DIRECT" || tm=="LEGACY") p.densitySpectrum.transmissionMode="DIRECT";
      else if (tm=="SCAN" || tm=="RIGIDITY_SCAN" || tm=="RIGIDITY") p.densitySpectrum.transmissionMode="SCAN";
      else if (tm=="ADAPTIVE" || tm=="ADAPT") p.densitySpectrum.transmissionMode="ADAPTIVE";
      else {
        std::ostringstream _exit_msg;
        _exit_msg << "DS_TRANSMISSION_MODE must be DIRECT, SCAN, or ADAPTIVE (got '"
                  << p.densitySpectrum.transmissionMode << "')";
        exit(__LINE__,__FILE__,_exit_msg.str().c_str());
      }
      if (p.densitySpectrum.transmissionScanN < 0)
        exit(__LINE__,__FILE__,"DS_TRANSMISSION_SCAN_N must be >= 0 (0 means automatic)");
      if (p.densitySpectrum.transmissionRefineN < 0)
        exit(__LINE__,__FILE__,"DS_TRANSMISSION_REFINE_N must be >= 0");
      if (p.densitySpectrum.transmissionMaxN < 0)
        exit(__LINE__,__FILE__,"DS_TRANSMISSION_MAX_N must be >= 0");
      if (p.densitySpectrum.transmissionMaxN > 0 &&
          p.densitySpectrum.transmissionScanN > p.densitySpectrum.transmissionMaxN) {
        exit(__LINE__,__FILE__,
             "DS_TRANSMISSION_SCAN_N must not exceed DS_TRANSMISSION_MAX_N when the max is set");
      }
    }

    // Validate DS_BOUNDARY_MODE token.
    const std::string bm = ToUpper(p.densitySpectrum.boundaryMode);
    if (bm!="ISOTROPIC" && bm!="ANISOTROPIC") {
      { std::ostringstream _exit_msg; _exit_msg << "DS_BOUNDARY_MODE must be ISOTROPIC or ANISOTROPIC (got '"+p.densitySpectrum.boundaryMode+"')"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
    }
    // When ANISOTROPIC is requested, validate the anisotropy sub-parameters.
    if (bm=="ANISOTROPIC") {
      const std::string pm = ToUpper(p.anisotropy.padModel);
      if (pm!="ISOTROPIC" && pm!="SINALPHA_N" && pm!="COSALPHA_N" && pm!="BIDIRECTIONAL") {
        { std::ostringstream _exit_msg; _exit_msg << "BA_PAD_MODEL must be ISOTROPIC|SINALPHA_N|COSALPHA_N|BIDIRECTIONAL (got '"+p.anisotropy.padModel+"')"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
      }
      if (p.anisotropy.padExponent < 0.0) {
        exit(__LINE__,__FILE__,"BA_PAD_EXPONENT must be >= 0");
      }
      const std::string sm = ToUpper(p.anisotropy.spatialModel);
      if (sm!="UNIFORM" && sm!="DAYSIDE_NIGHTSIDE") {
        { std::ostringstream _exit_msg; _exit_msg << "BA_SPATIAL_MODEL must be UNIFORM|DAYSIDE_NIGHTSIDE (got '"+p.anisotropy.spatialModel+"')"; exit(__LINE__,__FILE__,_exit_msg.str().c_str()); }
      }
      if (p.anisotropy.daysideFactor < 0.0 || p.anisotropy.nightsideFactor < 0.0) {
        exit(__LINE__,__FILE__,"BA_DAYSIDE_FACTOR and BA_NIGHTSIDE_FACTOR must be >= 0");
      }
    }
  }

  // Parse the particle spectrum into a typed metadata object first.  The legacy
  // boundary/spectrum initializer still consumes the raw key/value table, but by
  // storing the typed view in AmpsParam we make the selected spectrum explicit in
  // the model initialization sequence.
  p.particleSpectrum = ParseParticleSpectrum(p.spectrum);

  // TABLE spectra support two on-disk formats:
  //   (1) a legacy 2-column table (Energy_MeV, differential flux)
  //   (2) a time-dependent table with one UTC timestamp per row and many energy columns
  // The parser determines the file type here and passes the reference epoch through
  // #SPECTRUM so cSpectrum can call the appropriate loader directly.
  ResolveTableSpectrumFileForInitialization(p);

  // Build/validate the global spectrum object from the prepared #SPECTRUM table because
  // the downstream density/cutoff code still evaluates spectra through ::gSpectrum.
  InitGlobalSpectrumFromKeyValueMap(p.spectrum);

  // Diagnostic output: record the parsed spectrum in a standalone Tecplot file.
  // This is intentionally written immediately after spectrum initialization so the
  // output reflects exactly what injection will use.
  //
  // File: spectrum_input.dat
  //  - Energy is written in MeV
  //  - TABLE spectra preserve the user table points
  //  - Analytic spectra are sampled with log-spaced energies (200 points by default)
  ::WriteSpectrumInputTecplot("spectrum_input.dat", ::gSpectrum);

  {
    const std::string spacing=ToUpper(Trim(p.cutoff.scanSpacing));
    if (spacing!="LOG" && spacing!="LINEAR")
      exit(__LINE__,__FILE__,"CUTOFF_SCAN_SPACING must be LOG or LINEAR");
    p.cutoff.scanSpacing=spacing;

    const std::string geometry=ToUpper(Trim(p.output.shellGeometry));
    if (geometry!="SPHERICAL" && geometry!="GEODETIC")
      exit(__LINE__,__FILE__,"SHELL_GEOMETRY must be SPHERICAL or GEODETIC");
    p.output.shellGeometry=geometry;
    if (!(p.output.shellRes_deg>0.0))
      exit(__LINE__,__FILE__,"SHELL_RES_DEG must be positive");
    if (p.output.shellLonRes_deg==0.0 || p.output.shellLatRes_deg==0.0)
      exit(__LINE__,__FILE__,"SHELL_LON_RES_DEG and SHELL_LAT_RES_DEG must be positive when specified");
  }

  return p;
}

}
