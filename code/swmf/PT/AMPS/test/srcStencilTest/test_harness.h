/***************************************************************************************
 * test_harness.h
 * -------------------------------------------------------------------------------------
 * PURPOSE
 *   A tiny, dependency-free test registry used to run multiple stencil validation
 *   programs from a single executable. This file deliberately avoids depending on any
 *   production PIC headers or types so it cannot introduce namespace pollution or
 *   link-order surprises.
 *
 * WHAT THIS HEADER PROVIDES
 *   • TestHarness::Registry
 *       - Global singleton that maps a string name → a test runner function.
 *       - Methods:
 *           Add(name, runner, blurb)   Register a test once, at static init time.
 *           Has(name)                  Returns true if a test name exists.
 *           Run(name, args)            Invoke a test by name with arbitrary args.
 *           List()                     Print all registered tests with blurbs.
 *   • TestHarness::AutoRegister
 *       - Helper that you construct as a static in each test translation unit (TU).
 *         Its constructor inserts your test runner into the registry.
 *
 * DESIGN PRINCIPLES
 *   • Non-intrusive: keeps test code outside PIC namespaces and production files.
 *   • Link-safe: test TUs define a symbol (ForceLinkAllTests) that is referenced from
 *     the main dispatcher, preventing aggressive linkers from dead-stripping tests.
 *   • Header-only: easy to drop into any project without extra build steps.
 *
 * TYPICAL USAGE
 *   In a test TU (e.g., curl_b.cpp):
 *
 *     static int Run(const std::vector<std::string>& args) {
 *       // ... do test work ...
 *       return 0; // 0 = success; non-zero = failure
 *     }
 *
 *     static TestHarness::AutoRegister _auto_reg(
 *       "curl_b",
 *       [](const std::vector<std::string>& a){ return Run(a); },
 *       "Corner curl(B) stencil initialization smoke test");
 *
 *   Then the manager (main.cpp) can dispatch it by name:
 *     ./stencil_tests curl_b
 *
 * EXTENDING
 *   - Add new tests with unique names (e.g., "div_b", "curl_e"), each in its own TU.
 *   - Tests may parse arbitrary key=value args (e.g., N=16,24,32) to alter behavior.
 *
 * LICENSE / SCOPE
 *   Provided as-is; minimal, robust, and intentionally simple.
 ***************************************************************************************/

#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <cstdio>

#include "pic.h"

namespace TestHarness {

/// Function type for a test runner: takes argv-like string vector, returns exit code.
using Runner = std::function<int(const std::vector<std::string>& args)>;

/**
 * @class Registry
 * @brief Central registry of tests keyed by short string names.
 *
 * The registry is a Meyers singleton. Tests register themselves during static
 * initialization using AutoRegister. The manager (main.cpp) queries and runs them.
 */
class Registry {
public:
  /// Access the global registry instance.
  static Registry& Get() { static Registry R; return R; }

  /// Register a test runner with a human-readable blurb.
  void Add(const std::string& name, Runner fn, const std::string& blurb) {
    tests_[name]  = std::move(fn);
    blurbs_[name] = blurb;
  }

  /// True if a test with @p name has been registered.
  bool Has(const std::string& name) const { return tests_.count(name) != 0; }

  /// Run the test @p name. Returns -1 if not found, else the runner's return code.
  int Run(const std::string& name, const std::vector<std::string>& args) const {
    auto it = tests_.find(name);
    if (it == tests_.end()) return -1;
    return it->second(args);
  }

  /// Print all registered tests and their descriptive blurbs.
  void List() const {
    std::puts("Available tests:");
    for (const auto& kv : tests_) {
      const auto& k   = kv.first;
      const auto  itb = blurbs_.find(k);
      std::printf("  %-16s  %s\n", k.c_str(),
                  (itb==blurbs_.end() ? "" : itb->second.c_str()));
    }
  }

private:
  std::map<std::string, Runner> tests_;   ///< map: test name → runner
  std::map<std::string, std::string> blurbs_; ///< map: test name → description
};

/**
 * @struct AutoRegister
 * @brief Helper object to self-register tests at static initialization.
 *
 * Usage:
 *   static TestHarness::AutoRegister _ar("name", runner, "blurb");
 */
struct AutoRegister {
  AutoRegister(const std::string& name, Runner fn, const std::string& blurb) {
    Registry::Get().Add(name, std::move(fn), blurb);
  }
};

} // namespace TestHarness

