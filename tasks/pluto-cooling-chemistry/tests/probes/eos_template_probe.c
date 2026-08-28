#include "pluto.h"

/* The vendored PVTE template is intentionally a user-supplied skeleton.  Its
 * executable contract is the supplied Gamma1 default; the empty hooks are
 * compiled and linked, but are not mistaken for a physical oracle. */
int main(void)
{
  double v[NVAR] = {0.0};
  double gamma1 = Gamma1(v);
  if (!isfinite(gamma1) || fabs(gamma1 - 1.6667) > 1.e-12) return 1;
  printf("PVTE_TEMPLATE gamma1=%0.17g hooks=compiled\n", gamma1);
  return 0;
}
