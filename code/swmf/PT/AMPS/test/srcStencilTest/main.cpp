/***************************************************************************************
 * main.cpp — Stencil test manager / dispatcher
 * -------------------------------------------------------------------------------------
 * ROLE
 *   Small front-end that lists registered tests and dispatches by name. The tests
 *   self-register via TestHarness::AutoRegister in their own translation units.
 *
 * USAGE
 *   ./stencil_tests                 # list available tests
 *   ./stencil_tests curl_b          # run the curl-B stencil initialization test
 *   ./stencil_tests curl_b N=16,24  # example of passing arbitrary args to the test
 *
 * DESIGN NOTES
 *   • References StencilTests::ForceLinkAllTests() (provided by each test TU) so
 *     linkers do not dead-strip the test objects under -O3 / LTO.
 *   • No dependency on PIC headers; only the harness is included here.
 ***************************************************************************************/

#include <string>
#include <vector>
#include <cstdio>
#include "test_harness.h"


// Force-link hook implemented in each test TU group.
// The manager calls it once to ensure tests aren't dead-stripped by the linker.
namespace StencilTests { void ForceLinkAllTests(); }

/// Convert raw argc/argv into a std::vector<std::string> for convenience.
static std::vector<std::string> ParseArgs(int argc, char** argv) {
  std::vector<std::string> out;
  out.reserve(argc);
  for (int i=0;i<argc;i++) out.emplace_back(argv[i]);
  return out;
}

int main(int argc, char** argv) {
  // Ensure the tests' translation units remain linked-in even with aggressive LTO.
  StencilTests::ForceLinkAllTests();

  {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::SecondOrder;

PIC::FieldSolver::Electromagnetic::ECSIM::Stencil::cGradDivEStencil Gc[3], Gw[3];
double dx,dy,dz;

InitGradDivEBStencils_compact(Gc, dx, dy, dz);
InitGradDivEBStencils_wide   (Gw, dx, dy, dz);

//test_grad_div_e();
  }

  auto args = ParseArgs(argc, argv);

  // No test name? Print list and a short usage hint.
  if (args.size() < 2) {
    std::puts("stencil_tests: no test specified.\n");
    TestHarness::Registry::Get().List();
    std::puts("\nUse: ./stencil_tests <test_name> [key=value,...]");
    return 0;
  }

  // The first positional argument selects the test; the rest are passed through.
  const std::string test = args[1];
  std::vector<std::string> subargs(args.begin()+2, args.end());

  if (!TestHarness::Registry::Get().Has(test)) {
    std::fprintf(stderr, "Unknown test: '%s'\n\n", test.c_str());
    TestHarness::Registry::Get().List();
    return 2; // non-zero exit marks this as an invocation error
  }

  // Delegate to the test's runner function; propagate its return code.
  return TestHarness::Registry::Get().Run(test, subargs);
}

