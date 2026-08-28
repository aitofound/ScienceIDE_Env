#include "pluto.h"

/* Direct probe for the vendored mutually-exclusive H+ PVTE law.  This law
 * supplies its own LTE mean molecular weight and does not provide the
 * chemistry InternalEnergyBracket hook expected by the generic PVTE wrapper. */
int main(void)
{
  double v[NVAR] = {0.0};
  double T = 10000.0, mu = 0.0, e, p, recovered = 0.0;
  v[RHO] = 1.0;
  GetMu(T, v[RHO], &mu);
  e = InternalEnergyFunc(v, T);
  p = Pressure(v, T);
  v[PRS] = p;
  if (!isfinite(mu) || mu <= 0.0 || !isfinite(e) || e <= 0.0 || !isfinite(p) || p <= 0.0)
    return 1;
  if (GetPV_Temperature(v, &recovered) != 0 || !isfinite(recovered) || fabs(recovered - T) > 1.e-10*T)
    return 1;
  if (!isfinite(Gamma1(v)) || Gamma1(v) <= 1.0)
    return 1;
  printf("EOS=%d HPLUS mu=%0.17g energy=%0.17g pressure=%0.17g recovered_T=%0.17g gamma1=%0.17g\n", EOS, mu, e, p, recovered, Gamma1(v));
  return 0;
}
