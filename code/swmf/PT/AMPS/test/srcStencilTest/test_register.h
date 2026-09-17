/**
 * =============================================================================
 *  AMPS Stencil Test Framework — Design Overview & How-To Add New Tests
 * =============================================================================
 *
 * 1) PURPOSE
 * ----------
 * This lightweight framework lets you add self-contained numerical tests
 * (e.g., curl(B), grad·div(E), etc.) as individual translation units (TUs)
 * that register themselves at program start. You can run any test by name:
 *
 *    ./stencil_tests curl_b
 *    ./stencil_tests grad_div_e
 *
 * The framework:
 *   • decouples test discovery/registration from the main program,
 *   • supports passing argv-like arguments to tests,
 *   • is robust to link-time dead-stripping (via an explicit force-link hook).
 *
 *
 * 2) MAJOR PIECES
 * ---------------
 * (A) TestHarness (test_harness.h/.cpp)
 *     - Owns a global registry:  std::map<std::string, Runner>
 *     - Runner = std::function<int(const std::vector<std::string>&)>
 *     - Provides a small RAII helper:
 *           struct AutoRegister {
 *             AutoRegister(const std::string& name, Runner fn, const std::string& blurb);
 *           };
 *       Constructing an AutoRegister adds (name → fn) into the registry.
 *
 * (B) Registration Macro (test_register.h)
 *     - One-line macro a test TU uses to:
 *         1. declare a standard entry point:  int NS::Run(const std::vector<std::string>&)
 *         2. declare a per-suite force-link shim:  void NS::ForceLinkAllTests();
 *         3. register NS::Run under a public test name ("curl_b", "grad_div_e", ...)
 *     - The TU then defines NS::Run(...) and an out-of-line NS::ForceLinkAllTests() {}.
 *
 * (C) Central Force-Link Hook (test_force_link_all.{h,cpp})
 *     - main() calls:  StencilTests::ForceLinkAllTests();
 *     - That function makes *one call per test suite*:
 *           CurlB::ForceLinkAllTests();
 *           GradDivE::ForceLinkAllTests();
 *       The sole purpose is to create concrete symbol references so the linker
 *       must pull in those test TUs. This ensures their static AutoRegister
 *       objects run (and the tests appear in the registry).
 *
 * (D) The Test Runner (main.cpp)
 *     - Parses argv; if a test name is provided, runs that test:
 *           int rc = TestHarness::RunByName(test_name, args);
 *     - If no name is provided, prints the list of registered tests.
 *
 *
 * 3) LINKING & DEAD-STRIPPING: WHY "FORCE LINK" EXISTS
 * ----------------------------------------------------
 * If your tests are compiled into a static library (e.g., libtests.a) or your
 * link uses GC of sections, the linker only extracts objects needed to resolve
 * already-seen symbols. Tests that register themselves via static initializers
 * can be dropped before their AutoRegister constructors run → test "disappears".
 *
 * To defeat that:
 *   - Each test TU defines  NS::ForceLinkAllTests()  (empty function).
 *   - The central hook calls each suite’s ForceLinkAllTests() exactly once.
 *   - This creates a real, external symbol reference to the test TU, forcing
 *     the linker to include it; its static AutoRegister executes; the test is
 *     discoverable at runtime.
 *
 * Build-system alternative (no code): link the tests library as whole-archive:
 *   • GNU/Clang:  -Wl,--whole-archive libtests.a -Wl,--no-whole-archive
 *   • MSVC:       /WHOLEARCHIVE:tests.lib
 * You can use that instead of (or in addition to) the force-link hook.
 *
 *
 * 4) STANDARD TEST ENTRY POINT
 * ----------------------------
 * Every test implements:
 *
 *     namespace MySuite {
 *       int Run(const std::vector<std::string>& args);
 *     }
 *
 * The test can parse its own args (e.g., grid sizes, flags) and should return
 * 0 on success; nonzero on failure.
 *
 *
 * 5) HOW TO ADD A NEW TEST (STEP-BY-STEP)
 * ---------------------------------------
 * Suppose you’re adding a test "foo_bar".
 *
 * (A) Create `foo_bar.cpp`:
 *
 *     #include <vector>
 *     #include <string>
 *     #include <iostream>
 *     #include "test_register.h"  // <-- provides REGISTER_STENCIL_TEST
 *
 *     namespace FooBar {
 *       // 1) Implement the standard entry point
 *       int Run(const std::vector<std::string>& args) {
 *         // parse args if needed
 *         // set up domain, fields, stencils, etc.
 *         // compute numeric result(s), compare to analytic
 *         // print norms, orders, point-wise diagnostics
 *         // return 0 on success, nonzero on failure
 *         return 0;
 *       }
 *     }
 *
 *     // 2) Register with the harness (name must be unique)
 *     REGISTER_STENCIL_TEST(FooBar,
 *       "foo_bar",
 *       "Short blurb: what this test verifies.");
 *
 *     // 3) Provide an out-of-line force-link shim (non-inline, external linkage)
 *     namespace FooBar {
 *       void ForceLinkAllTests() {}
 *     }
 *
 * (B) Tell the central hook to retain it:
 *     In `test_force_link_all.cpp`, add:
 *
 *       namespace FooBar { void ForceLinkAllTests(); }
 *       // ...
 *       void StencilTests::ForceLinkAllTests() {
 *         // existing calls...
 *         FooBar::ForceLinkAllTests();   // <-- add one line
 *       }
 *
 * (C) Build. Now you can run:
 *       ./stencil_tests foo_bar  [optional args...]
 *
 * Notes:
 *   • If your existing driver was a legacy name (e.g., `int test_foo_bar()`),
 *     either move its body into `FooBar::Run(args)`, or call it from there.
 *   • Do not mark `Run(...)` or `ForceLinkAllTests()` as `static`; they need
 *     external linkage to be visible across TUs.
 *
 *
 * 6) COMMON PITFALLS & HOW TO AVOID THEM
 * --------------------------------------
 * • Undefined reference to `NS::ForceLinkAllTests()`:
 *     You declared it (via REGISTER_STENCIL_TEST) but didn’t define it in the
 *     test TU. Add:
 *         namespace NS { void ForceLinkAllTests() {} }
 *     at the end of your test .cpp.
 *
 * • Redefinition of `NS::ForceLinkAllTests()`:
 *     You defined it in both the header (inline) and the .cpp. The header
 *     must only DECLARE it; the .cpp provides the single definition.
 *
 * • Test name is “Unknown test: 'X'” at runtime:
 *     The TU that contains the test wasn’t linked (dead-stripped). Ensure:
 *       - The central hook calls  NS::ForceLinkAllTests();
 *       - The test TU defines  NS::ForceLinkAllTests() {}  (out-of-line);
 *       - Link order: the tests archive appears AFTER objects that reference it,
 *         or use whole-archive for the tests library.
 *
 * • Lambda won’t convert to std::function in AutoRegister:
 *     Use a plain function pointer (or the `REGISTER_STENCIL_TEST` macro),
 *     which passes `NS::Run` directly and avoids conversion issues.
 *
 * • Mismatched entry point:
 *     The registrar expects `NS::Run(const std::vector<std::string>&)`.
 *     If your file defines `::test_xxx()` instead, either rename it or call
 *     it from inside `NS::Run(...)`. Avoid declaring `NS::test_xxx()` unless
 *     the function is actually defined inside that namespace.
 *
 *
 * 7) RUNTIME USAGE & CONVENTIONS
 * ------------------------------
 * • List tests:
 *     ./stencil_tests
 *   (The harness prints all registered names with blurbs.)
 *
 * • Run a test:
 *     ./stencil_tests curl_b [--no-tecplot] [other test-specific flags]
 *
 * • Typical output:
 *   - summary of domain/params,
 *   - L_inf and relative L2 over refinement set,
 *   - computed order p = log(E_{k-1}/E_k) / log(N_k/N_{k-1}),
 *   - optional interior-point component-wise comparisons,
 *   - optional Tecplot dump on the finest grid.
 *
 *
 * 8) OPTIONAL: BUILD-SYSTEM SHORTCUTS
 * -----------------------------------
 * If you don’t want to maintain the central hook, you can whole-archive the
 * tests library so every test TU is kept:
 *   GNU/Clang:   -Wl,--whole-archive libtests.a -Wl,--no-whole-archive
 *   MSVC:        /WHOLEARCHIVE:tests.lib
 * That removes the need to add a new ForceLink call per test, but may increase
 * binary size. The macro pattern still works unchanged.
 *
 *
 * 9) QUICK CHECKLIST WHEN ADDING A TEST
 * -------------------------------------
 * [ ] Implement  NS::Run(const std::vector<std::string>&)
 * [ ] Add  REGISTER_STENCIL_TEST(NS, "name", "blurb")
 * [ ] Define  namespace NS { void ForceLinkAllTests() {} }  (in that TU)
 * [ ] Declare & call `NS::ForceLinkAllTests()` in test_force_link_all.cpp
 * [ ] Confirm the test appears in `./stencil_tests` (no args)
 * [ ] Run it: `./stencil_tests name`
 *
 * =============================================================================
 */

#ifndef STENCIL_TESTS_REGISTER_H
#define STENCIL_TESTS_REGISTER_H

#include <vector>
#include <string>
#include "test_harness.h"

/**
 * REGISTER_STENCIL_TEST(NS, NAME, BLURB)
 *
 * This registers NS::Run(...) and DECLARES NS::ForceLinkAllTests() so
 * the central hook can call it. Each test TU must provide a DEFINITION
 * of NS::ForceLinkAllTests() at file scope (non-static).
 */
#define REGISTER_STENCIL_TEST(NS, NAME, BLURB)                                   \
  namespace NS {                                                                  \
    int Run(const std::vector<std::string>&);                                     \
    void ForceLinkAllTests();   /* declaration only: definition in this TU */     \
  }                                                                               \
  static TestHarness::AutoRegister _auto_reg_##NS(                                \
      NAME,                                                                       \
      NS::Run,                                                                    \
      BLURB)

#endif // STENCIL_TESTS_REGISTER_H

