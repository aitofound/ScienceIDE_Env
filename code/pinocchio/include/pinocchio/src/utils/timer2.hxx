//
// Copyright (c) 2021 LAAS-CNRS
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/timer2.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/timer2.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  class Timer
  {
  public:
    Timer()
    {
      clock_gettime(CLOCK_MONOTONIC, &start_);
    }

    inline void reset()
    {
      clock_gettime(CLOCK_MONOTONIC, &start_);
    }

    inline double get_duration()
    {
      clock_gettime(CLOCK_MONOTONIC, &finish_);
      duration_ = static_cast<double>(finish_.tv_sec - start_.tv_sec) * 1000000;
      duration_ += static_cast<double>(finish_.tv_nsec - start_.tv_nsec) / 1000;
      return duration_ / 1000.;
    }

    inline double get_us_duration()
    {
      clock_gettime(CLOCK_MONOTONIC, &finish_);
      duration_ = static_cast<double>(finish_.tv_sec - start_.tv_sec) * 1000000;
      duration_ += static_cast<double>(finish_.tv_nsec - start_.tv_nsec) / 1000;
      return duration_;
    }

  private:
    struct timespec start_;
    struct timespec finish_;
    double duration_;
  };
} // namespace pinocchio
