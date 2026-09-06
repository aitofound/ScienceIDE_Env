#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unistd.h>

#include <AMReX_MultiFab.H>
#include <AMReX_REAL.H>
#include <AMReX_iMultiFab.H>
#include <mpi.h>

#include "Constants.h"
#include "Timer.h"

// Only works for x>-8;
inline int fastfloor(amrex::Real x) { return (int)(x + 8) - 8; }

inline amrex::Real median(amrex::Real a, amrex::Real b, amrex::Real c) {
  return std::clamp(a, std::min(b, c), std::max(b, c));
}

inline amrex::Real limiter_theta(amrex::Real theta, amrex::Real u0,
                                 amrex::Real u1, amrex::Real u2) {
  amrex::Real du21 = u2 - u1;
  if (du21 == 0)
    du21 = 1e-99;

  amrex::Real r = (u1 - u0) / du21;

  amrex::Real phi =
      amrex::max(0.0, amrex::min(theta * r, 0.5 * (1 + r), theta));

  return 1 - phi;
}

inline int product(const amrex::IntVect& vect) {
  return AMREX_D_TERM(vect[0], *vect[1], *vect[2]);
}

inline amrex::Dim3 init_dim3(const int i) {
  amrex::Dim3 dim;
  dim.x = i;
  dim.y = i;
  dim.z = nDim > 2 ? i : 0;
  return dim;
}

// Return the first integer in the string.
// Example: "abc123def345b" -> 123
inline int extract_int(std::string_view s) {
  std::size_t i0 = s.find_first_of("0123456789");
  std::size_t i1 = s.find_last_of("0123456789");
  if (i0 == std::string_view::npos || i1 == std::string_view::npos)
    return 0;
  return std::stoi(std::string(s.substr(i0, i1 - i0 + 1)));
};

// Normalize option strings before comparing user-facing mode names.
// Examples with the default removed characters:
//   "NewtonKrylov"   -> "newtonkrylov"
//   "newton-krylov"  -> "newtonkrylov"
//   "Newton_Krylov"  -> "newtonkrylov"
//   "GMRES"          -> "gmres"
// The second argument can choose a different removal set:
//   normalize_string_token("A+B-C", "+-") -> "abc"
inline std::string normalize_string_token(std::string name,
                                          std::string_view removeChars = "-_") {
  name.erase(std::remove_if(name.begin(), name.end(),
                            [removeChars](char c) {
                              return removeChars.find(c) !=
                                     std::string_view::npos;
                            }),
             name.end());
  std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return name;
}

// Local dot product followed by an optional communicator-wide sum.
inline double dot_product_mpi(const double* a, const double* b, const int n,
                              const MPI_Comm iComm) {
  double c = 0.0;
  for (int i = 0; i < n; ++i) {
    c += a[i] * b[i];
  }

  if (iComm == MPI_COMM_SELF) {
    return c;
  }

  double cMpi = 0.0;
  MPI_Allreduce(&c, &cMpi, 1, MPI_DOUBLE, MPI_SUM, iComm);
  return cMpi;
}

inline int shift_periodic_index(int idx, int lo, int hi) {
  if (idx > hi)
    idx -= hi - lo;

  if (idx < lo)
    idx += hi - lo;

  return idx;
};

template <class T> inline void a_cross_b(T (&a)[3], T (&b)[3], T (&c)[3]) {
  // c = a x b

  c[0] = a[1] * b[2] - a[2] * b[1];
  c[1] = a[2] * b[0] - a[0] * b[2];
  c[2] = a[0] * b[1] - a[1] * b[0];

  return;
}

template <class T> inline void zero_array(T* arr, int nSize) {
  for (int i = 0; i < nSize; ++i)
    arr[i] = 0;
}

// rand1, rand2 are random numbers in [0,1].
inline void random_vector(const amrex::Real rand1, const amrex::Real rand2,
                          amrex::Real (&vec)[nDim3]) {
  amrex::Real lon = 2 * M_PI * rand1;
  amrex::Real costheta = 2 * rand2 - 1;
  amrex::Real sintheta = sqrt(1 - costheta * costheta);

  vec[ix_] = sintheta * cos(lon);
  vec[iy_] = sintheta * sin(lon);
  vec[iz_] = costheta;
}

inline amrex::Real l2_norm(amrex::Real* vec, int n) {
  amrex::Real sum = 0;
  for (int i = 0; i < n; ++i)
    sum += vec[i] * vec[i];
  return sqrt(sum);
}

inline void linear_interpolation_coef(const amrex::RealVect& dx,
                                      amrex::Real (&coef)[2][2][2]) {
  amrex::Real interpX[2] = { dx[0], 1 - dx[0] };
  amrex::Real interpY[2] = { dx[1], 1 - dx[1] };
  amrex::Real interpZ[2] = { nDim > 2 ? dx[2] : 0, nDim > 2 ? 1 - dx[2] : 1 };

  amrex::Real xy[2][2] = { { interpX[0] * interpY[0], interpX[0] * interpY[1] },
                           { interpX[1] * interpY[0],
                             interpX[1] * interpY[1] } };

  coef[0][0][0] = xy[1][1] * interpZ[1];
  coef[0][0][1] = xy[1][1] * interpZ[0];
  coef[0][1][0] = xy[1][0] * interpZ[1];
  coef[0][1][1] = xy[1][0] * interpZ[0];
  coef[1][0][0] = xy[0][1] * interpZ[1];
  coef[1][0][1] = xy[0][1] * interpZ[0];
  coef[1][1][0] = xy[0][0] * interpZ[1];
  coef[1][1][1] = xy[0][0] * interpZ[0];
}

inline void linear_interpolation_coef_finer(const amrex::RealVect& dx,
                                            amrex::Real (&coef)[2][2][2]) {
  amrex::Real interpX[2] = { dx[0], 1 - dx[0] };
  amrex::Real interpY[2] = { dx[1], 1 - dx[1] };
  amrex::Real interpZ[2] = { nDim > 2 ? dx[2] : 0, nDim > 2 ? 1 - dx[2] : 1 };

  // Shape fix
  for (int i = 0; i <= 1; i++) {
    interpX[i] = 2.0 * ((2.0 * interpX[i]) - 1.0);
    if (interpX[i] < 0.0)
      interpX[i] = 0.0;

    interpY[i] = 2.0 * ((2.0 * interpY[i]) - 1.0);
    if (interpY[i] < 0.0)
      interpY[i] = 0.0;

    interpZ[i] = nDim > 2 ? 2.0 * ((2.0 * interpZ[i]) - 1.0)
                          : ((2.0 * interpZ[i]) - 1.0);
    if (interpZ[i] < 0.0)
      interpZ[i] = 0.0;
  }

  amrex::Real xy[2][2] = { { interpX[0] * interpY[0], interpX[0] * interpY[1] },
                           { interpX[1] * interpY[0],
                             interpX[1] * interpY[1] } };

  coef[0][0][0] = xy[1][1] * interpZ[1];
  coef[0][0][1] = xy[1][1] * interpZ[0];
  coef[0][1][0] = xy[1][0] * interpZ[1];
  coef[0][1][1] = xy[1][0] * interpZ[0];
  coef[1][0][0] = xy[0][1] * interpZ[1];
  coef[1][0][1] = xy[0][1] * interpZ[0];
  coef[1][1][0] = xy[0][0] * interpZ[1];
  coef[1][1][1] = xy[0][0] * interpZ[0];
}

template <typename T, int nRow, int nCol>
bool linear_solver_Gauss_Elimination(
    int m, int n, amrex::Array2D<T, 0, nRow - 1, 0, nCol - 1>& a,
    amrex::Vector<T>& x, amrex::Vector<T>& ref) {
  // a(m,n)
  if (m > nRow) {
    amrex::Abort("Error: m>nRow in linear_solver_Gauss_Elimination");
  }
  if (n > nCol) {
    amrex::Abort("Error: n>nCol in linear_solver_Gauss_Elimination");
  }

  for (int i = 0; i < m - 1; ++i) {
    // Partial Pivoting
    for (int k = i + 1; k < m; ++k) {
      // If diagonal element(absolute vallue) is smaller than
      // any of the terms below it
      if (fabs(a(i, i)) < fabs(a(k, i))) {
        // Swap the rows
        for (int j = 0; j < n; ++j) {
          std::swap(a(i, j), a(k, j));
        }
      }
    }
    // Begin Gauss Elimination
    for (int k = i + 1; k < m; ++k) {
      // Guard against singular, zero, or NaN pivots.
      if (!(fabs(a(i, i)) > ref[i])) {
        return false;
      }
      double term = a(k, i) / a(i, i);
      for (int j = 0; j < n; ++j) {
        a(k, j) = a(k, j) - term * a(i, j);
      }
    }
  }
  // Begin Back-substitution
  for (int i = m - 1; i >= 0; i--) {
    x[i] = a(i, n - 1);
    for (int j = i + 1; j < n - 1; ++j) {
      x[i] = x[i] - a(i, j) * x[j];
    }

    if (!(fabs(a(i, i)) > ref[i])) {
      return false;
    }

    x[i] = x[i] / a(i, i);
  }

  for (int i = 0; i < m; ++i) {
    if (!std::isfinite(x[i])) {
      return false;
    }
  }
  return true;
};

// Fisher-Yates shuffle algorithm
template <class Vec, class Rand> void shuffle_fish_yates(Vec& arr, Rand& rd) {
  for (int i = arr.size() - 1; i > 0; --i) {
    int j = floor(rd() * i);
    if (i != j)
      std::swap(arr[i], arr[j]);
  }
};

// Randomly select m elements from an array with weights. These m elements can
// not repeat.
template <class Vec, class Rand>
std::vector<int> random_select_weighted_n(Vec arr, int m, Rand& rd) {

  const int n = arr.size();
  std::vector<int> idx;

  if (m >= n) {
    for (int i = 0; i < n; ++i)
      idx.push_back(i);
    return idx;
  }

  amrex::Vector<double> w1;
  w1.resize(n);

  double wt = 0;
  for (int i = 0; i < arr.size(); ++i) {
    wt += arr[i];
    w1[i] = wt;
  }

  const int iSelected = 1, iNotSelected = 0;
  amrex::Vector<int> selected(n, iNotSelected);

  while (idx.size() < size_t(m)) {
    double r = rd() * wt;
    int i = 0;
    while (w1[i] < r)
      ++i;

    if (selected[i] == iNotSelected) {
      idx.push_back(i);
      selected[i] = iSelected;

      { // 'Remove' arr[i] so that it will not be selected again.
        arr[i] = 0;
        wt = 0;
        for (int j = 0; j < arr.size(); ++j) {
          wt += arr[j];
          w1[j] = wt;
        }
      }
    }
  }

  return idx;
};

inline double read_mem_usage() {
  // This function returns the resident set size (RSS) of
  // this processor in unit MB.

  // From wiki:
  // RSS is the portion of memory occupied by a process that is
  // held in main memory (RAM).

  double rssMB = 0.0;

  std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);

  if (!stat_stream.fail()) {
    // Dummy vars for leading entries in stat that we don't care about
    std::string pid, comm, state, ppid, pgrp, session, tty_nr;
    std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    std::string utime, stime, cutime, cstime, priority, nice;
    std::string O, itrealvalue, starttime;

    // Two values we want
    unsigned long vsize;
    unsigned long rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >>
        tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >>
        stime >> cutime >> cstime >> priority >> nice >> O >> itrealvalue >>
        starttime >> vsize >> rss; // Ignore the rest

    rssMB = rss * sysconf(_SC_PAGE_SIZE) / 1024.0 / 1024.0;
  }

  return rssMB;
}

#endif
