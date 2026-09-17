#ifndef STENCIL_TESTS_FORCE_LINK_ALL_H
#define STENCIL_TESTS_FORCE_LINK_ALL_H

/**
 * @file   test_force_link_all.h
 * @brief  Declaration of a tiny, linker-facing hook used by the test runner:
 *         `StencilTests::ForceLinkAllTests()`.
 *
 * WHY THIS HEADER EXISTS
 * ----------------------
 * Your test runner's `main.cpp` explicitly calls a function named
 * `StencilTests::ForceLinkAllTests()` before running the registered tests.
 * The *sole* purpose of that call is to ensure that the linker includes at least
 * one symbol from the unit that defines this function, which in turn (depending
 * on your build setup) can cause additional test translation units (TUs) to be
 * retained in the final binary.
 *
 * WHY A "FORCE LINK" HOOK IS OFTEN NEEDED
 * ---------------------------------------
 * Modern linkers aggressively drop "unused" object files and library members
 * during dead-stripping / garbage collection of sections. This is helpful for
 * release builds but can inadvertently remove test object files when:
 *
 *   1) Tests live in separate object files bundled into a *static library*
 *      (e.g., `libtests.a`). If no symbol from a given object is directly
 *      referenced, the linker won't pull that object from the archive.
 *
 *   2) Your tests rely on *static registration* (e.g., a global object whose
 *      constructor self-registers the test in a registry). If the entire TU
 *      is discarded before the static object is "seen," the registrar never
 *      runs, and the test silently vanishes.
 *
 *   3) Your link step uses flags like `-Wl,--gc-sections` or platform defaults
 *      that aggressively remove unreferenced sections/functions.
 *
 * WHAT THIS HOOK DOES (AND DOESN'T DO)
 * ------------------------------------
 * - DOES: Provide a *concrete, referenced* symbol so the TU defining it is
 *   undeniably needed by the final link. Main calls it, so the linker must
 *   keep the object that defines it.
 *
 * - MAY DO: Optionally call *other* per-suite "force link" shims (if you add
 *   them later), each living in a TU that you'd like to keep. Calling them
 *   creates a reference chain that prevents their TUs from being dropped.
 *
 * - DOES NOT: Enumerate or register tests by itself. It's simply a hook.
 *
 * WHEN YOU MIGHT *NOT* NEED THIS
 * ------------------------------
 * - If your build directly compiles and links all test `.o` files into the
 *   test binary (no static archives), and you don't use section GC, or your
 *   tests are otherwise referenced—this hook may be redundant.
 * - Some frameworks avoid this problem by using linker "whole archive" flags
 *   (see below), but that’s a build-system choice.
 *
 * ALTERNATIVES (BUILD-SYSTEM-ONLY)
 * --------------------------------
 * - GNU/Clang: wrap your test archive with:
 *     `-Wl,--whole-archive libtests.a -Wl,--no-whole-archive`
 *   so *all* members are linked regardless of references.
 * - MSVC: use `/WHOLEARCHIVE:libtests.lib`.
 * These remove the need for a code-level "force link" symbol, but they also
 * increase binary size by keeping everything.
 *
 * MAINTENANCE TIPS
 * ----------------
 * - Namespace **must** match the declaration used by `main.cpp`. If `main` calls
 *   `StencilTests::ForceLinkAllTests()`, define it in **that exact** namespace.
 * - Define this function **once** in the entire test build (otherwise you'll get
 *   multiple-definition link errors).
 * - If you later add per-suite shims (e.g., `namespace CurlB { void ForceLinkAllTests(); }`),
 *   call them from the implementation in `test_force_link_all.cpp`.
 */

namespace StencilTests {
  // Defined in test_force_link_all.cpp
  void ForceLinkAllTests();
}

#endif // STENCIL_TESTS_FORCE_LINK_ALL_H

