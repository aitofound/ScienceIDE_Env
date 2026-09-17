#ifndef SHIELDSIM_CLI_HH
#define SHIELDSIM_CLI_HH

/* ============================================================================
 * CLI.hh
 *
 * Command-line parsing and help text for shieldSim.
 *
 * All options use --key=value syntax except boolean flags such as --sweep and
 * --sweep-log.  Length values are specified in mm.  Energy values are specified
 * in MeV total kinetic energy per particle.  --quantities controls which
 * post-processed radiation-effect outputs are written.
 * ========================================================================== */

#include "ShieldSimConfig.hh"

void PrintHelp();
Options ParseArguments(int argc, char** argv);

#endif // SHIELDSIM_CLI_HH
