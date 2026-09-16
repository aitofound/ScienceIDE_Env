//
// Copyright (c) 2015-2019 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/timer.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/timer.hpp"
#endif // PINOCCHIO_LSP

#define SMOOTH(s) for (size_t _smooth = 0; _smooth < s; ++_smooth)

struct PinocchioTicToc
{
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

  enum Unit
  {
    S = 1,
    MS = 1000,
    US = 1000000,
    NS = 1000000000
  };
  Unit DEFAULT_UNIT;

  static std::string unitName(Unit u)
  {
    switch (u)
    {
    case S:
      return "s";
    case MS:
      return "ms";
    case US:
      return "us";
    case NS:
      return "ns";
    }
    return "";
  }

  std::stack<time_point> stack;

  PinocchioTicToc(Unit def = MS)
  : DEFAULT_UNIT(def)
  {
  }

  inline void tic()
  {
    stack.push(clock::now());
  }

  inline double toc()
  {
    return toc(DEFAULT_UNIT);
  }

  inline double toc(const Unit factor)
  {
    std::chrono::duration<double> duration = (clock::now() - stack.top());
    stack.pop();
    return duration.count() * static_cast<double>(factor);
  }

  inline void toc(std::ostream & os, double SMOOTH = 1)
  {
    os << toc(DEFAULT_UNIT) / SMOOTH << " " << unitName(DEFAULT_UNIT) << std::endl;
  }
};
