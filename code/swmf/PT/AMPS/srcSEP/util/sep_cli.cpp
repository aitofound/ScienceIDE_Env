#include "sep_cli.h"
#include "../sep.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace SEP {
namespace Util {
namespace CLI {

namespace {

// Convert a string to lower case for case-insensitive command-line values.  The
// unsigned-char cast is intentional; std::tolower has undefined behavior for
// negative signed char values.
std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

// Parse a boolean switch value.  Accepting several common spellings makes the
// CLI convenient for shell scripts and interactive use while still rejecting
// ambiguous inputs with a clear error message.
bool ParseBoolValue(const std::string& raw_value, bool& value) {
  const std::string v = ToLower(raw_value);

  if (v == "on" || v == "true" || v == "yes" || v == "1" || v == "enable" || v == "enabled") {
    value = true;
    return true;
  }

  if (v == "off" || v == "false" || v == "no" || v == "0" || v == "disable" || v == "disabled") {
    value = false;
    return true;
  }

  return false;
}


// Parse the turbulence model selector.  This deliberately accepts several
// spellings used in notes/scripts so users do not need to remember one exact
// token.  The model name controls only the representation of wave energy; the
// independent switches --coupling, --cascade, and --reflection still turn the
// corresponding physics on/off.
bool ParseTurbulenceModelValue(const std::string& raw_value, Options::TurbulenceModel& value) {
  const std::string v = ToLower(raw_value);

  if (v == "integrated" || v == "legacy" || v == "old" || v == "branch-integrated") {
    value = Options::TurbulenceModel::Integrated;
    return true;
  }

  if (v == "wave-number-resolved" || v == "wavenumber-resolved" ||
      v == "k-resolved" || v == "k" || v == "spectral" || v == "new") {
    value = Options::TurbulenceModel::WaveNumberResolved;
    return true;
  }

  return false;
}

// Parse the particle-mover selector.  The strings below intentionally include
// several aliases because the mover names are long and have evolved during
// development.  Only the selected function pointer is changed here; the physics
// flags such as --particle-coupling and --turbulence-model remain independent.
// This is important because the same mover can be used in diagnostic runs with
// coupling disabled, while the wave-number-resolved model specifically benefits
// from the event-driven FTE mover when coupling is enabled.
bool ParseParticleMoverValue(const std::string& raw_value, Options::ParticleMover& value) {
  const std::string v = ToLower(raw_value);

  if (v == "fte" || v == "default" || v == "focused-transport" ||
      v == "focused-transport-equation" || v == "legacy-fte") {
    value = Options::ParticleMover::FTE;
    return true;
  }

  if (v == "focused-transport-event-driven" || v == "event-driven" ||
      v == "event-driven-fte" || v == "fte-event-driven" ||
      v == "coupled-fte" || v == "coupling-fte") {
    value = Options::ParticleMover::FocusedTransportEventDriven;
    return true;
  }

  if (v == "focused-transport-wave-scattering" || v == "wave-scattering" ||
      v == "fte-wave-scattering" || v == "direct-wave-scattering") {
    value = Options::ParticleMover::FocusedTransportWaveScattering;
    return true;
  }

  if (v == "parker-dxx" || v == "parker-diffusion" || v == "dxx") {
    value = Options::ParticleMover::ParkerDxx;
    return true;
  }

  if (v == "parker-mean-free-path" || v == "parker-mfp" ||
      v == "parker-meanfreepath") {
    value = Options::ParticleMover::ParkerMeanFreePath;
    return true;
  }

  if (v == "mean-free-path-scattering" || v == "mfp-scattering" ||
      v == "meanfreepath-scattering") {
    value = Options::ParticleMover::MeanFreePathScattering;
    return true;
  }

  if (v == "tenishev-2005-fl" || v == "tenishev-2005" ||
      v == "tenishev-field-line" || v == "tenishev-fl") {
    value = Options::ParticleMover::Tenishev2005FieldLine;
    return true;
  }

  return false;
}

// Return a stable, human-readable name for the selected particle mover.  The
// name is printed in batch logs and in the help text, making it easier to
// confirm whether the simulation is using the mover that actually supplies
// particle-streaming terms for wave/turbulence coupling.
const char* ParticleMoverName(Options::ParticleMover value) {
  switch (value) {
  case Options::ParticleMover::FTE:
    return "fte";
  case Options::ParticleMover::FocusedTransportEventDriven:
    return "focused-transport-event-driven";
  case Options::ParticleMover::FocusedTransportWaveScattering:
    return "focused-transport-wave-scattering";
  case Options::ParticleMover::ParkerDxx:
    return "parker-dxx";
  case Options::ParticleMover::ParkerMeanFreePath:
    return "parker-mean-free-path";
  case Options::ParticleMover::MeanFreePathScattering:
    return "mean-free-path-scattering";
  case Options::ParticleMover::Tenishev2005FieldLine:
    return "tenishev-2005-fl";
  }

  return "unknown";
}

// Split options of the form "--option=value".  If there is no '=' character,
// option_name receives the complete argument and option_value is left empty.
void SplitOption(const std::string& arg, std::string& option_name, std::string& option_value) {
  const std::string::size_type pos = arg.find('=');
  if (pos == std::string::npos) {
    option_name = arg;
    option_value.clear();
  }
  else {
    option_name = arg.substr(0, pos);
    option_value = arg.substr(pos + 1);
  }
}

// Read an option value either from "--option=value" or from the following
// command-line token in "--option value" form.  The index i is advanced only
// when the following token is consumed.
bool GetOptionValue(int argc, char** argv, int& i,
                    const std::string& option_name,
                    const std::string& value_from_equals,
                    std::string& value,
                    std::ostream& err) {
  if (!value_from_equals.empty()) {
    value = value_from_equals;
    return true;
  }

  if (i + 1 >= argc) {
    err << "ERROR: option '" << option_name << "' requires a value.\n";
    return false;
  }

  value = argv[++i];
  return true;
}

// Shared implementation for switches that set one boolean model flag.  The
// function handles both --option=value and --option value syntaxes and reports a
// model-specific error if the value cannot be interpreted as boolean.
bool ParseBooleanOption(int argc, char** argv, int& i,
                        const std::string& option_name,
                        const std::string& value_from_equals,
                        const char* description,
                        bool& destination,
                        std::ostream& err) {
  std::string raw_value;
  if (!GetOptionValue(argc, argv, i, option_name, value_from_equals, raw_value, err)) return false;

  bool parsed_value = false;
  if (!ParseBoolValue(raw_value, parsed_value)) {
    err << "ERROR: invalid value '" << raw_value << "' for " << description
        << ". Use on/off, true/false, yes/no, or 1/0.\n";
    return false;
  }

  destination = parsed_value;
  return true;
}

// Parse a non-negative integer option.  This is used for diagnostic-output
// cadence controls where zero has a useful meaning: disable that diagnostic.
// strtol is used instead of atoi so malformed values such as "10abc" are
// rejected rather than silently truncated.
bool ParseNonNegativeIntegerOption(int argc, char** argv, int& i,
                                   const std::string& option_name,
                                   const std::string& value_from_equals,
                                   const char* description,
                                   int& destination,
                                   std::ostream& err) {
  std::string raw_value;
  if (!GetOptionValue(argc, argv, i, option_name, value_from_equals, raw_value, err)) return false;

  errno = 0;
  char* end_ptr = nullptr;
  const long parsed_value = std::strtol(raw_value.c_str(), &end_ptr, 10);

  if (errno != 0 || end_ptr == raw_value.c_str() || (end_ptr && *end_ptr != '\0') ||
      parsed_value < 0 || parsed_value > std::numeric_limits<int>::max()) {
    err << "ERROR: invalid value '" << raw_value << "' for " << description
        << ". Use a non-negative integer; 0 disables the diagnostic.\n";
    return false;
  }

  destination = static_cast<int>(parsed_value);
  return true;
}

// Parse a strictly positive integer option.  This is separate from the
// non-negative diagnostic-cadence parser above because injection-particle counts
// are used in denominators in field_line.cpp when computing particle-weight
// correction factors.  Accepting zero would therefore create either a singular
// weight or a run with a silently disabled injection source.
bool ParsePositiveIntegerOption(int argc, char** argv, int& i,
                                const std::string& option_name,
                                const std::string& value_from_equals,
                                const char* description,
                                int& destination,
                                std::ostream& err) {
  std::string raw_value;
  if (!GetOptionValue(argc, argv, i, option_name, value_from_equals, raw_value, err)) return false;

  errno = 0;
  char* end_ptr = nullptr;
  const long parsed_value = std::strtol(raw_value.c_str(), &end_ptr, 10);

  if (errno != 0 || end_ptr == raw_value.c_str() || (end_ptr && *end_ptr != '\0') ||
      parsed_value <= 0 || parsed_value > std::numeric_limits<int>::max()) {
    err << "ERROR: invalid value '" << raw_value << "' for " << description
        << ". Use a positive integer greater than zero.\n";
    return false;
  }

  destination = static_cast<int>(parsed_value);
  return true;
}

} // anonymous namespace

void PrintHelp(const char* program_name, std::ostream& out) {
  const char* exe = (program_name && program_name[0] != '\0') ? program_name : "amps";

  out
      << "SEP standalone driver command-line options\n"
      << "\n"
      << "Usage:\n"
      << "  " << exe << " [options]\n"
      << "\n"
      << "Turbulence physics switches:\n"
      << "  --coupling <on|off>          Enable/disable SEP particle coupling to the\n"
      << "                               Alfven turbulence wave energy.\n"
      << "  --coupling-mode <on|off>     Alias for --coupling.\n"
      << "  --particle-coupling <on|off> Alias for --coupling.\n"
      << "  --no-coupling                Shortcut for --coupling off.\n"
      << "\n"
      << "  --cascade <on|off>           Enable/disable nonlinear turbulence cascade.\n"
      << "  --cascase <on|off>           Accepted alias for --cascade.\n"
      << "  --no-cascade                 Shortcut for --cascade off.\n"
      << "  --no-cascase                 Accepted alias for --no-cascade.\n"
      << "\n"
      << "  --reflection <on|off>        Enable/disable large-scale-gradient reflection\n"
      << "                               between W+ and W- waves.\n"
      << "  --no-reflection              Shortcut for --reflection off.\n"
      << "\n"
      << "Turbulence representation:\n"
      << "  --turbulence-model <integrated|wave-number-resolved>\n"
      << "                               Select the turbulence-energy representation.\n"
      << "                               integrated: legacy/default E+,E- only.\n"
      << "                               wave-number-resolved: store and advect E±(k_j);\n"
      << "                               particle coupling modifies the resonant k-bin.\n"
      << "  --turbulence-model=...      Same option using --option=value syntax.\n"
      << "  --wave-number-resolved      Shortcut for --turbulence-model wave-number-resolved.\n"
      << "  --integrated-turbulence     Shortcut for --turbulence-model integrated.\n"
      << "\n"
      << "Particle mover selection:\n"
      << "  --particle-mover <name>     Select the SEP particle mover used by\n"
      << "                               SEP::ParticleMoverPtr.  Default: fte.\n"
      << "                               Important for turbulence coupling: the\n"
      << "                               wave-number-resolved model needs a mover that\n"
      << "                               fills G_+(k),G_-(k); use\n"
      << "                               focused-transport-event-driven for that path.\n"
      << "  --particle-mover=...        Same option using --option=value syntax.\n"
      << "  --mover <name>              Alias for --particle-mover.\n"
      << "  --sep-mover <name>          Alias for --particle-mover.\n"
      << "                               Accepted names/aliases include:\n"
      << "                                 fte, default\n"
      << "                                 focused-transport-event-driven, event-driven-fte\n"
      << "                                 focused-transport-wave-scattering, wave-scattering\n"
      << "                                 parker-dxx\n"
      << "                                 parker-mean-free-path\n"
      << "                                 mean-free-path-scattering\n"
      << "                                 tenishev-2005-fl\n"
      << "\n"
      << "Field-line SEP injection controls:\n"
      << "  --particles-per-iteration <N>\n"
      << "                               Set SEP::FieldLine::InjectionParameters::\n"
      << "                               nParticlesPerIteration.  Default: N=300.\n"
      << "                               This controls the target number of injected\n"
      << "                               SEP macroparticles per active injection event;\n"
      << "                               the code adjusts statistical weights when the\n"
      << "                               physical injection estimate differs from N.\n"
      << "                               N must be a positive integer.\n"
      << "  --particles-per-iteration=... Same option using --option=value syntax.\n"
      << "  --n-particles-per-iteration <N>\n"
      << "                               Alias for --particles-per-iteration.\n"
      << "  --injection-particles-per-iteration <N>\n"
      << "                               Alias for --particles-per-iteration.\n"
      << "\n"
      << "Wave-number-resolved spectrum output:\n"
      << "  --spectrum-output-interval <N>\n"
      << "                               Write the Tecplot 2-D W+/- (s,k), sigma_c(s,k)\n"
      << "                               diagnostic every N main-loop iterations when\n"
      << "                               --turbulence-model wave-number-resolved is active.\n"
      << "                               Default: N=100.  Use N=0 to disable.\n"
      << "  --spectrum-output-interval=... Same option using --option=value syntax.\n"
      << "  --wave-number-output-interval <N>\n"
      << "                               Alias for --spectrum-output-interval.\n"
      << "  --turbulence-spectrum-output-interval <N>\n"
      << "                               Alias for --spectrum-output-interval.\n"
      << "\n"
      << "Diagnostics and development tests:\n"
      << "  --test-manager <on|off>      Enable/disable the standalone SEP TestManager()\n"
      << "                               diagnostics after AMPS/field-line initialization.\n"
      << "  --testmanager <on|off>       Alias for --test-manager.\n"
      << "  --run-test-manager           Shortcut for --test-manager on.\n"
      << "  --no-test-manager            Shortcut for --test-manager off.\n"
      << "\n"
      << "General:\n"
      << "  -h, --help                   Print this help message and exit before AMPS\n"
      << "                               initialization.\n"
      << "\n"
      << "Accepted boolean values are: on/off, true/false, yes/no, 1/0,\n"
      << "enable/disable, and enabled/disabled.\n"
      << "\n"
      << "Defaults:\n"
      << "  coupling=on, cascade=on, reflection=on, turbulence-model=integrated, particle-mover=fte,\n"
      << "  particles-per-iteration=300, test-manager=off, spectrum-output-interval=100.\n"
      << "\n"
      << "Examples:\n"
      << "  " << exe << " --coupling off --cascade off --reflection off\n"
      << "  " << exe << " --coupling-mode=on --no-cascade --reflection=on\n"
      << "  " << exe << " --run-test-manager\n"
      << "  " << exe << " --turbulence-model wave-number-resolved --coupling on\n"
      << "  " << exe << " --wave-number-resolved --particle-mover focused-transport-event-driven --coupling on\n"
      << "  " << exe << " --particles-per-iteration 1000\n"
      << "  " << exe << " --wave-number-resolved --spectrum-output-interval 25\n";
}

bool ParseCommandLine(int argc, char** argv, Options& options,
                      std::ostream& out, std::ostream& err) {
  (void)out; // Reserved for future informational parse-time messages.

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i] ? argv[i] : "";

    if (arg == "-h" || arg == "--help") {
      options.printHelp = true;
      return true;
    }

    std::string option_name, value_from_equals;
    SplitOption(arg, option_name, value_from_equals);

    if (option_name == "--coupling" || option_name == "--coupling-mode" ||
        option_name == "--particle-coupling") {
      if (!ParseBooleanOption(argc, argv, i, option_name, value_from_equals,
                              "particle/turbulence coupling", options.particleCouplingMode, err)) {
        return false;
      }
      continue;
    }

    if (option_name == "--cascade" || option_name == "--cascase") {
      if (!ParseBooleanOption(argc, argv, i, option_name, value_from_equals,
                              "nonlinear turbulence cascade", options.cascadeActive, err)) {
        return false;
      }
      continue;
    }

    if (option_name == "--reflection") {
      if (!ParseBooleanOption(argc, argv, i, option_name, value_from_equals,
                              "turbulence reflection", options.reflectionActive, err)) {
        return false;
      }
      continue;
    }

    if (option_name == "--turbulence-model") {
      std::string raw_value;
      if (!GetOptionValue(argc, argv, i, option_name, value_from_equals, raw_value, err)) return false;

      Options::TurbulenceModel parsed_model = Options::TurbulenceModel::Integrated;
      if (!ParseTurbulenceModelValue(raw_value, parsed_model)) {
        err << "ERROR: invalid value '" << raw_value << "' for --turbulence-model. "
            << "Use integrated or wave-number-resolved.\n";
        return false;
      }

      options.turbulenceModel = parsed_model;
      continue;
    }

    if (option_name == "--particle-mover" || option_name == "--mover" ||
        option_name == "--sep-mover") {
      std::string raw_value;
      if (!GetOptionValue(argc, argv, i, option_name, value_from_equals, raw_value, err)) return false;

      Options::ParticleMover parsed_mover = Options::ParticleMover::FTE;
      if (!ParseParticleMoverValue(raw_value, parsed_mover)) {
        err << "ERROR: invalid value '" << raw_value << "' for " << option_name << ".\n"
            << "       Accepted movers: fte, focused-transport-event-driven, "
            << "focused-transport-wave-scattering, parker-dxx, parker-mean-free-path, "
            << "mean-free-path-scattering, tenishev-2005-fl.\n";
        return false;
      }

      options.particleMover = parsed_mover;
      continue;
    }

    if (option_name == "--particles-per-iteration" ||
        option_name == "--n-particles-per-iteration" ||
        option_name == "--injection-particles-per-iteration" ||
        option_name == "--field-line-injection-particles") {
      if (!ParsePositiveIntegerOption(argc, argv, i, option_name, value_from_equals,
                                      "field-line injection particles per iteration",
                                      options.injectionParticlesPerIteration, err)) {
        return false;
      }
      continue;
    }

    if (option_name == "--spectrum-output-interval" ||
        option_name == "--wave-number-output-interval" ||
        option_name == "--turbulence-spectrum-output-interval" ||
        option_name == "--spectral-output-interval") {
      if (!ParseNonNegativeIntegerOption(argc, argv, i, option_name, value_from_equals,
                                         "wave-number-resolved spectrum output interval",
                                         options.spectralOutputInterval, err)) {
        return false;
      }
      continue;
    }

    if (option_name == "--test-manager" || option_name == "--testmanager") {
      if (!ParseBooleanOption(argc, argv, i, option_name, value_from_equals,
                              "SEP TestManager diagnostics", options.runTestManager, err)) {
        return false;
      }
      continue;
    }

    if (option_name == "--wave-number-resolved" || option_name == "--k-resolved" || option_name == "--spectral-turbulence") {
      if (!value_from_equals.empty()) {
        err << "ERROR: option '" << option_name << "' does not take a value.\n";
        return false;
      }
      options.turbulenceModel = Options::TurbulenceModel::WaveNumberResolved;
      continue;
    }

    if (option_name == "--integrated-turbulence" || option_name == "--legacy-turbulence") {
      if (!value_from_equals.empty()) {
        err << "ERROR: option '" << option_name << "' does not take a value.\n";
        return false;
      }
      options.turbulenceModel = Options::TurbulenceModel::Integrated;
      continue;
    }

    if (option_name == "--no-coupling" || option_name == "--no-particle-coupling") {
      if (!value_from_equals.empty()) {
        err << "ERROR: option '" << option_name << "' does not take a value.\n";
        return false;
      }
      options.particleCouplingMode = false;
      continue;
    }

    if (option_name == "--no-cascade" || option_name == "--no-cascase") {
      if (!value_from_equals.empty()) {
        err << "ERROR: option '" << option_name << "' does not take a value.\n";
        return false;
      }
      options.cascadeActive = false;
      continue;
    }

    if (option_name == "--no-reflection") {
      if (!value_from_equals.empty()) {
        err << "ERROR: option '" << option_name << "' does not take a value.\n";
        return false;
      }
      options.reflectionActive = false;
      continue;
    }

    if (option_name == "--run-test-manager") {
      if (!value_from_equals.empty()) {
        err << "ERROR: option '" << option_name << "' does not take a value.\n";
        return false;
      }
      options.runTestManager = true;
      continue;
    }

    if (option_name == "--no-test-manager") {
      if (!value_from_equals.empty()) {
        err << "ERROR: option '" << option_name << "' does not take a value.\n";
        return false;
      }
      options.runTestManager = false;
      continue;
    }

    err << "ERROR: unknown SEP command-line option '" << arg << "'.\n"
        << "Run with -h or --help to see available options.\n";
    return false;
  }

  return true;
}

void ApplyTurbulenceOptions(const Options& options) {
  // These are the three runtime switches used by the turbulence operators.  The
  // assignment is centralized here so the standalone driver no longer hard-codes
  // the values in main.cpp; future input-file parsing can reuse this function or
  // the same Options structure.
  SEP::AlfvenTurbulence_Kolmogorov::ParticleCouplingMode = options.particleCouplingMode;
  SEP::AlfvenTurbulence_Kolmogorov::Cascade::active = options.cascadeActive;
  SEP::AlfvenTurbulence_Kolmogorov::Reflection::active = options.reflectionActive;

  // Configure the field-line SEP injection macroparticle count.
  //
  // This option writes the variable defined in field_line.cpp:
  //   SEP::FieldLine::InjectionParameters::nParticlesPerIteration
  //
  // The injection routine uses this number as the target Monte-Carlo particle
  // count and compensates differences between the physically estimated source
  // strength and the requested macroparticle count by adjusting statistical
  // weights.  Applying the CLI value here, after the optional post-compile
  // input file is parsed in main.cpp and before particle injection starts, lets
  // command-line runs override the hard-coded default without recompilation.
  SEP::FieldLine::InjectionParameters::nParticlesPerIteration =
      options.injectionParticlesPerIteration;

  // Select the particle mover.
  //
  // The mover choice is kept in the same CLI utility as the turbulence options
  // because the wave/turbulence coupling path depends on the mover: the coupling
  // manager can update the wave energy only if the mover accumulates the
  // particle-streaming source terms G_+(k) and G_-(k).  In particular,
  // ParticleMover_FTE and ParticleMover_FocusedTransport_EventDriven call
  // AccumulateParticleFluxForWaveCoupling() and therefore fill the particle
  // streaming source terms G_+(k),G_-(k) used by the wave-number-resolved
  // turbulence model.  The historical default remains ParticleMover_FTE.
  switch (options.particleMover) {
  case Options::ParticleMover::FTE:
    SEP::ParticleMoverPtr = SEP::ParticleMover_FTE;
    break;
  case Options::ParticleMover::FocusedTransportEventDriven:
    SEP::ParticleMoverPtr = SEP::ParticleMover_FocusedTransport_EventDriven;
    break;
  case Options::ParticleMover::FocusedTransportWaveScattering:
    SEP::ParticleMoverPtr = SEP::ParticleMover_FocusedTransport_WaveScattering;
    break;
  case Options::ParticleMover::ParkerDxx:
    SEP::ParticleMoverPtr = SEP::ParticleMover_Parker_Dxx;
    break;
  case Options::ParticleMover::ParkerMeanFreePath:
    SEP::ParticleMoverPtr = SEP::ParticleMover_Parker_MeanFreePath;
    break;
  case Options::ParticleMover::MeanFreePathScattering:
    SEP::ParticleMoverPtr = SEP::ParticleMover_MeanFreePathScattering;
    break;
  case Options::ParticleMover::Tenishev2005FieldLine:
    SEP::ParticleMoverPtr = SEP::ParticleMover_Tenishev_2005_FL;
    break;
  }

  // Select the wave-energy representation.  This affects only the turbulence
  // energy transport/coupling kernels.  All existing output and scattering code
  // still sees the integrated CellIntegratedWaveEnergy datum, which the new
  // spectral model keeps synchronized by summing over k-bins.
  SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::TurbulenceModelMode =
      (options.turbulenceModel == Options::TurbulenceModel::WaveNumberResolved)
          ? SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::ModelMode::WaveNumberResolved
          : SEP::AlfvenTurbulence_Kolmogorov::WaveNumberResolved::ModelMode::Integrated;
}

void PrintTurbulenceOptions(const Options& options, std::ostream& out) {
  out << "SEP turbulence CLI configuration:\n"
      << "  particle/turbulence coupling: " << (options.particleCouplingMode ? "on" : "off") << "\n"
      << "  nonlinear cascade:            " << (options.cascadeActive ? "on" : "off") << "\n"
      << "  wave reflection:               " << (options.reflectionActive ? "on" : "off") << "\n"
      << "  turbulence model:              "
      << (options.turbulenceModel == Options::TurbulenceModel::WaveNumberResolved
              ? "wave-number-resolved" : "integrated") << "\n"
      << "  particle mover:                " << ParticleMoverName(options.particleMover) << "\n"
      << "  injected particles/iteration:  " << options.injectionParticlesPerIteration << "\n"
      << "  spectrum output interval:      " << options.spectralOutputInterval
      << " iteration(s)" << (options.spectralOutputInterval == 0 ? " (disabled)" : "") << "\n"
      << "  TestManager diagnostics:       " << (options.runTestManager ? "on" : "off") << "\n";

  // Practical warning for the most common pitfall found during testing: the
  // wave-number-resolved coupling manager can be enabled, but it receives zero
  // particle source terms unless the mover calls
  // AccumulateParticleFluxForWaveCoupling().  The default FTE mover, the
  // event-driven FTE mover, and the Parker-Dxx mover fill those arrays.
  if (options.particleCouplingMode &&
      options.turbulenceModel == Options::TurbulenceModel::WaveNumberResolved &&
      options.particleMover != Options::ParticleMover::FTE &&
      options.particleMover != Options::ParticleMover::FocusedTransportEventDriven &&
      options.particleMover != Options::ParticleMover::ParkerDxx) {
    out << "  WARNING: wave-number-resolved particle coupling needs a mover that fills "
        << "G_+(k),G_-(k).\n"
        << "           Recommended: --particle-mover fte or --particle-mover focused-transport-event-driven.\n";
  }
}

} // namespace CLI
} // namespace Util
} // namespace SEP
