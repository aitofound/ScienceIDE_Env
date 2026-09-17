#ifndef _DOMAINPARAMETERS_H_
#define _DOMAINPARAMETERS_H_

#include "FleksDistributionMap.h"

// Configuration for a FLEKS Domain, owned by Domain and shared
// with its child components.
struct DomainParameters {
  // Restart control.
  bool doRestart = false;
  bool doRestartPT = false;
  bool doRestartFIOnly = false;

  // Coupling / initialization mode.
  bool isStandalone = false;
  bool initFromSWMF = true;
  bool receiveICOnly = false;

  // Component toggles; gate child construction in Domain::init().
  bool usePT = false;
  bool useSource = false;

  // Number of files per AMReX output.
  int nFileField = 64;
  int nFileParticle = 256;

  // Load-balancing strategy.
  BalanceStrategy balanceStrategy = BalanceStrategy::Cell;
  int cellWeight = 10;
};

#endif // _DOMAINPARAMETERS_H_
