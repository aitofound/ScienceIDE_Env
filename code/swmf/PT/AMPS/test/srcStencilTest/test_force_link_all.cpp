/**
 * @file   test_force_link_all.cpp
 * @brief  Central "force link" hook called by main() to keep test TUs.
 *
 * See header for a detailed rationale. This TU references per-suite shims,
 * which forces the linker to retain those translation units so their static
 * AutoRegister objects run and their tests appear in the registry.
 */

#include "test_force_link_all.h"

// Declare per-suite shims (each defined in its corresponding test TU)
namespace CurlB    { void ForceLinkAllTests(); }
namespace GradDivE { void ForceLinkAllTests(); }
namespace LaplacianE { void ForceLinkAllTests(); }
namespace CurlCurlE { void ForceLinkAllTests(); }
namespace CurlCurlMetricsDebug { void ForceLinkAllTests(); }

namespace StencilTests {
  void ForceLinkAllTests() {
    // Create concrete references to each suite’s TU.
    CurlB::ForceLinkAllTests();
    GradDivE::ForceLinkAllTests();
    LaplacianE::ForceLinkAllTests();
    CurlCurlE::ForceLinkAllTests();
    CurlCurlMetricsDebug::ForceLinkAllTests();
  }
}

