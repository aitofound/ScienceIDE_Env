// mc_fieldline.cpp
// =====================================================================================
// Monte-Carlo guiding-center test-particle model for electrons moving along a magnetic
// field line, with time-integrated sampling of pitch-angle, kinetic energy, and
// parallel velocity distributions as a function of distance along the line.
//
// This code is intentionally self-contained (single .cpp + Makefile) so it can be
// dropped into small AMPS-side utilities or used standalone for quick experiments.
//
// -------------------------------------------------------------------------------------
// 1) WHAT THIS PROGRAM DOES
// -------------------------------------------------------------------------------------
// (A) FIELD-LINE I/O
//   * Reads a field-line file that contains a sequence of points:
//       - position  (x,y,z) in meters
//       - magnetic field information either as magnitude |B| [T] or components (Bx,By,Bz)
//   * Computes arc-length coordinate s along the field line:
//       s[0]=0,  s[i+1]=s[i] + |r[i+1]-r[i]|
//   * Builds a piecewise-linear interpolation of:
//       r(s),  |B|(s),  and an approximate tangent direction t_hat(s)=dr/ds
//
//   IMPORTANT FOR 4-COLUMN INPUT (x y z Bmag):
//     The file does not provide B-direction. In that case we reconstruct a consistent
//     vector field using the curve tangent:
//       Bvec(s) = |B|(s) * t_hat(s)
//     This makes Tecplot output (Bx,By,Bz) and the local b-hat meaningful, and avoids
//     the common mistake of interpreting the 4th column as Bx.
//
//   * Writes a Tecplot ASCII “XY” file where X is distance along the line (s), and
//     Y includes abs(x), abs(y), abs(z), (Bx,By,Bz), and |B|.
//     (You can plot any of these as an XY curve in Tecplot.)
//
// (B) GUIDING-CENTER PARTICLE TRANSPORT (1D ALONG s)
//   Each particle is advanced in time using a guiding-center approximation along the
//   field-line coordinate s. We evolve:
//     - s            : arc-length position
//     - v_parallel   : velocity component parallel to the magnetic field
//     - mu_mag       : magnetic moment (adiabatic invariant), held constant unless you
//                      add additional physics
//
//   Equations (SI units):
//     ds/dt        = v_parallel
//     dv_par/dt    = (q/m) * E_par(s)  -  (mu_mag/m) * d|B|/ds
//
//   where:
//     * q = electron charge (-e), m = electron mass
//     * E_par is a user-specified constant parallel electric field (CLI option
//       -Epar_Vm). If E_par=0, no electrostatic acceleration is applied.
//     * The mirror force term uses d|B|/ds computed from the piecewise-linear |B|(s).
//
//   Pitch-angle cosine is derived from mu_mag and v_parallel:
//     v_perp^2   = 2 * mu_mag * |B| / m
//     v^2        = v_parallel^2 + v_perp^2
//     mu         = cos(alpha) = v_parallel / sqrt(v^2)   (clamped to [-1,1])
//
//   Optional pitch-angle diffusion (stochastic scattering) can be enabled with -D:
//     dmu = sqrt(2 D dt) * N(0,1), then clamp mu to [-1,1], and recompute v_par and
//     mu_mag consistently at the local |B|.
//
//   BOUNDARIES (IMPORTANT):
//     If a particle leaves the field line (s < 0 or s > L), it is DELETED (marked dead)
//     and no longer contributes to transport or sampling. There is NO reflection at
//     endpoints (per your requirement).
//
// (C) TIME-INTEGRATED SAMPLING DURING TRANSPORT
//   At EACH TIME STEP (not only at the end), each alive particle contributes one sample
//   to histograms binned by:
//     - s-bin   : particle location along the line (0..L)
//     - mu-bin  : pitch-angle cosine mu in [-1,1]
//     - E-bin   : kinetic energy in eV
//     - vpar-bin: parallel velocity in m/s
//
//   This produces time-integrated distributions f(s,mu), f(s,E), f(s,vpar), normalized
//   PER s-bin at output time:
//     sum_j pdf(s,x_j) * dx  = 1
//
//   Output Tecplot ordered-zone files:
//     <prefix>_mc_mu_s_2d.dat    : pdf(s, mu)
//     <prefix>_mc_E_s_2d.dat     : pdf(s, E_eV)
//     <prefix>_mc_vpar_s_2d.dat  : pdf(s, vpar)
//
// (D) MPI PARALLELIZATION + PROGRESS OUTPUT
//   * When launched under MPI, the total particle population is partitioned across
//     ranks. Each rank advances only its local subset of particles.
//   * Time-integrated histograms are reduced across ranks with MPI_Reduce, and rank 0
//     gathers the surviving-particle diagnostics needed for the final 1D outputs.
//   * Progress information is printed by rank 0 at a user-controlled step interval and
//     reports completed steps, percent complete, elapsed wall time, ETA, and the
//     current number of globally alive particles.
//   * The deterministic guiding-center mover can be selected at runtime:
//       -gc_mover euler   : keep the original first-order update
//       -gc_mover rk4     : use a 4th-order Runge-Kutta update for (s, v_parallel)
//     The stochastic pitch-angle diffusion step, if enabled, is still applied as a
//     separate operator before the deterministic mover.
//
// -------------------------------------------------------------------------------------
// 2) INPUT FILE FORMAT (FIELD LINE)
// -------------------------------------------------------------------------------------
// The loader is designed to be tolerant of comment/header lines.
// It supports rows with:
//
//   (i) 4 columns:   x  y  z  Bmag
//  (ii) 6 columns:   x  y  z  Bx  By  Bz     (|B| computed)
// (iii) 7 columns:   x  y  z  Bx  By  Bz  Bmag (|B| may be redundant)
//
// Your example file uses case (i): “# variables: X,Y,Z [m]; B[T]” followed by the point
// count and then x y z B rows.
//
// -------------------------------------------------------------------------------------
// 3) EXAMPLES
// -------------------------------------------------------------------------------------
// Build:
//   make
//
// Convert field line to Tecplot XY curves:
//   ./mc_fieldline -fieldline /mnt/data/fieldline_32.txt -out demo -npart 0
//
// Run Monte Carlo GC transport, inject in the middle, sample f(s,mu), f(s,E), f(s,vpar):
//   ./mc_fieldline -fieldline /mnt/data/fieldline_32.txt -out run 
//       -inj_sfrac 0.5 -T_eV 200 -npart 200000 -dt 1e-3 -tmax 1.0 
//       -D 0.05 -nbins_s 100 -nbins_mu 100 -nbins_E 120 -Emax_eV 3000 
//       -nbins_vpar 120 -seed 42
//
// Add constant parallel acceleration (uniform E_par):
//   ./mc_fieldline -fieldline /mnt/data/fieldline_32.txt -out accel 
//       -inj_sfrac 0.25 -T_eV 100 -Epar_Vm 1e-3 -npart 100000 -dt 1e-3 -tmax 2.0
//
// Use the 4th-order deterministic mover for the guiding-center update:
//   ./mc_fieldline -fieldline /mnt/data/fieldline_32.txt -out rk4_run
//       -inj_sfrac 0.5 -T_eV 200 -npart 200000 -dt 1e-3 -tmax 1.0 -gc_mover rk4
//
// -------------------------------------------------------------------------------------
// 4) IMPLEMENTATION NOTES / ASSUMPTIONS
// -------------------------------------------------------------------------------------
// * This is a 1D-along-the-line guiding-center model intended for algorithm development
//   and diagnostics, not a full 3D GC integrator.
// * Spatial interpolation is piecewise linear in s.
// * d|B|/ds is taken as a piecewise constant slope between nodes.
// * If B-direction is not provided, b-hat is approximated by the curve tangent.
// * All dynamics are SI; temperature is specified in eV for convenience.
// * The deterministic part of the particle advance can use either the original
//   first-order mover or a classical 4th-order Runge-Kutta (RK4) mover.
// * The RK4 mover integrates only the deterministic guiding-center equations.
//   Pitch-angle diffusion remains operator-split and is applied separately.
// * For reproducibility, use -seed.
//
// =====================================================================================

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <mpi.h>

static constexpr double kElectronMass   = 9.1093837015e-31; // kg
static constexpr double kElectronCharge = -1.602176634e-19; // C (electron charge is negative)
static constexpr double kBoltzmann      = 1.380649e-23;     // J/K
static constexpr double kEvToJ          = 1.602176634e-19;  // J/eV

struct Vec3 { double x=0, y=0, z=0; };

static inline Vec3 add(const Vec3& a, const Vec3& b){ return {a.x+b.x, a.y+b.y, a.z+b.z}; }
static inline Vec3 sub(const Vec3& a, const Vec3& b){ return {a.x-b.x, a.y-b.y, a.z-b.z}; }
static inline Vec3 mul(const Vec3& a, double s){ return {a.x*s, a.y*s, a.z*s}; }
static inline double dot(const Vec3& a, const Vec3& b){ return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline double norm(const Vec3& a){ return std::sqrt(dot(a,a)); }
static inline Vec3 unit_or_zero(const Vec3& a){
  double n = norm(a);
  if(n<=0.0) return {0,0,0};
  return mul(a, 1.0/n);
}
static inline double clamp(double x, double lo, double hi){ return std::min(hi, std::max(lo, x)); }

// -------------------------------------------------------------------------------------
// GuidingCenterMover
//   Runtime-selectable deterministic particle mover for the guiding-center advance.
//   We keep the original first-order method for backward compatibility and add a
//   classical 4th-order Runge-Kutta option for improved accuracy in the deterministic
//   (s, v_parallel) integration.
// -------------------------------------------------------------------------------------
enum class GuidingCenterMover {
  Euler = 0,  // original first-order mover used by earlier versions of the code
  RK4   = 1   // classical 4th-order Runge-Kutta for the deterministic GC equations
};

static inline const char* guiding_center_mover_name(GuidingCenterMover mover){
  switch(mover){
    case GuidingCenterMover::Euler: return "euler";
    case GuidingCenterMover::RK4:   return "rk4";
    default:                        return "unknown";
  }
}

static inline bool parse_guiding_center_mover(const std::string& s, GuidingCenterMover& mover){
  if(s=="euler" || s=="Euler" || s=="EULER" || s=="1"){
    mover = GuidingCenterMover::Euler;
    return true;
  }
  if(s=="rk4" || s=="RK4" || s=="Rk4" || s=="4"){
    mover = GuidingCenterMover::RK4;
    return true;
  }
  return false;
}


// -------------------------------------------------------------------------------------
// local_particle_count()
//   Static block partition of the global particle count across MPI ranks. The first
//   (npart % nranks) ranks get one extra particle so that the load stays balanced to
//   within one particle.
// -------------------------------------------------------------------------------------
static inline int local_particle_count(int npart_global, int rank, int nranks){
  const int base = npart_global / nranks;
  const int rem  = npart_global % nranks;
  return base + ((rank < rem) ? 1 : 0);
}

// -------------------------------------------------------------------------------------
// reduce_sum_vector_to_root()
//   Sum a double vector across all MPI ranks and store the global result on root.
//   Non-root ranks keep an empty vector because only rank 0 writes the output files.
// -------------------------------------------------------------------------------------
static inline std::vector<double> reduce_sum_vector_to_root(const std::vector<double>& local,
                                                            int root,
                                                            MPI_Comm comm)
{
  int rank = 0;
  MPI_Comm_rank(comm, &rank);

  std::vector<double> global;
  if(rank == root) global.resize(local.size(), 0.0);

  MPI_Reduce(local.empty() ? nullptr : local.data(),
             (rank == root && !global.empty()) ? global.data() : nullptr,
             static_cast<int>(local.size()), MPI_DOUBLE, MPI_SUM, root, comm);

  return global;
}

// -------------------------------------------------------------------------------------
// gather_double_vector_to_root()
//   Gather a variable-length double vector from each rank onto root. This is used for
//   the final surviving-particle diagnostics (s_final and mu_final), which are much
//   more convenient to write on rank 0 after a single gather.
// -------------------------------------------------------------------------------------
static inline std::vector<double> gather_double_vector_to_root(const std::vector<double>& local,
                                                               int root,
                                                               MPI_Comm comm)
{
  int rank = 0, nranks = 1;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &nranks);

  const int local_n = static_cast<int>(local.size());
  std::vector<int> counts;
  if(rank == root) counts.resize(nranks, 0);

  MPI_Gather(&local_n, 1, MPI_INT,
             (rank == root) ? counts.data() : nullptr, 1, MPI_INT,
             root, comm);

  std::vector<double> global;
  std::vector<int> displs;
  if(rank == root){
    displs.resize(nranks, 0);
    int total = 0;
    for(int r=0;r<nranks;r++){
      displs[r] = total;
      total += counts[r];
    }
    global.resize(static_cast<size_t>(total));
  }

  MPI_Gatherv(local.empty() ? nullptr : local.data(), local_n, MPI_DOUBLE,
              (rank == root && !global.empty()) ? global.data() : nullptr,
              (rank == root) ? counts.data() : nullptr,
              (rank == root) ? displs.data() : nullptr,
              MPI_DOUBLE, root, comm);

  return global;
}

struct FieldLinePoint {
  Vec3 r;          // position [m]
  Vec3 Bvec;       // [T] optional; may be (0,0,0) if not present
  double Bmag=0.0; // |B| [T]
  double s=0.0;    // arc-length from start [m]
};

struct InterpSample {
  Vec3 r;      // position at s
  Vec3 t_hat;  // tangent unit vector dr/ds at s (used as local b-hat if B direction absent)
  Vec3 Bvec;   // interpolated Bvec (may be 0)
  double Bmag; // interpolated |B|
  double dBds; // piecewise derivative d|B|/ds
};

struct FieldLine {
  std::vector<FieldLinePoint> p;

  double length() const { return p.empty() ? 0.0 : p.back().s; }

// -------------------------------------------------------------------------------------
// FieldLine::load()
//   Robust parser for field-line text files with optional header/comments.
//   Supports 4/6/7-column rows described in the file header.
//   After reading points, we compute arc-length s[i] and (if needed) reconstruct Bvec
//   from tangent direction.
// -------------------------------------------------------------------------------------

  bool load(const std::string& path, std::string* err=nullptr){
    std::ifstream in(path);
    if(!in){
      if(err) *err = "Cannot open file: " + path;
      return false;
    }

    std::string line;
    std::vector<std::array<double,7>> rows; // max columns we care about

    while(std::getline(in, line)){
      // trim leading spaces
      size_t i=0;
      while(i<line.size() && std::isspace(static_cast<unsigned char>(line[i]))) i++;
      if(i==line.size()) continue;
      if(line[i]=='#') continue;

      // skip a bare integer line (N points)
      {
        std::istringstream iss(line);
        long long maybeN;
        if((iss >> maybeN) && iss.eof()) continue;
      }

      std::istringstream iss(line);
      std::vector<double> cols;
      double v;
      while(iss >> v) cols.push_back(v);

      if(cols.size()==4){
        // x y z B
        std::array<double,7> r{};
        r[0]=cols[0]; r[1]=cols[1]; r[2]=cols[2];
        r[3]=0; r[4]=0; r[5]=0;
        r[6]=cols[3];
        rows.push_back(r);
      } else if(cols.size()>=6){
        // x y z Bx By Bz [B]
        std::array<double,7> r{};
        r[0]=cols[0]; r[1]=cols[1]; r[2]=cols[2];
        r[3]=cols[3]; r[4]=cols[4]; r[5]=cols[5];
        if(cols.size()>=7) r[6]=cols[6];
        else r[6]=std::sqrt(r[3]*r[3]+r[4]*r[4]+r[5]*r[5]);
        rows.push_back(r);
      } else {
        // ignore malformed
        continue;
      }
    }

    if(rows.size()<2){
      if(err) *err = "Parsed <2 data rows from file: " + path;
      return false;
    }

    p.clear();
    p.reserve(rows.size());
    for(const auto& r: rows){
      FieldLinePoint q;
      q.r    = {r[0], r[1], r[2]};
      q.Bvec = {r[3], r[4], r[5]};
      q.Bmag = r[6];
      p.push_back(q);
    }

    // arc-length
    p[0].s = 0.0;
    for(size_t k=1;k<p.size();k++){
      double ds = norm(sub(p[k].r, p[k-1].r));
      p[k].s = p[k-1].s + ds;
    }

    // If Bmag looks unusable, compute it from Bvec
    bool allZero=true;
    for(const auto& q: p){ if(q.Bmag!=0.0){ allZero=false; break; } }
    if(allZero){
      for(auto& q: p) q.Bmag = norm(q.Bvec);
    }

    // If the file provides only |B| (Bvec == 0 everywhere) but |B| is present,
    // reconstruct a consistent vector field direction using the geometric tangent:
    //   Bvec = |B| * t_hat, where t_hat ~ dr/ds.
    bool bvecAllZero = true;
    bool bmagAnyPos  = false;
    for(const auto& q: p){
      if(norm(q.Bvec) > 0.0) bvecAllZero = false;
      if(q.Bmag > 0.0) bmagAnyPos = true;
    }

    if(bvecAllZero && bmagAnyPos){
      const size_t n = p.size();
      for(size_t k=0;k<n;k++){
        Vec3 t{0,0,0};
        if(n>=2){
          if(k==0)          t = sub(p[1].r, p[0].r);
          else if(k==n-1)   t = sub(p[n-1].r, p[n-2].r);
          else {
            // centered difference for smoother direction
            t = sub(p[k+1].r, p[k-1].r);
          }
        }
        Vec3 th = unit_or_zero(t);
        p[k].Bvec = mul(th, p[k].Bmag);
      }
    }

    // Guard against zeros
    for(auto& q: p){
      if(!(q.Bmag>0.0)) q.Bmag = 0.0;
    }

    return true;
  }

  // Find bracket segment for s_query (returns i such that p[i].s <= s < p[i+1].s)
  size_t bracket(double s_query) const {
    if(s_query<=p.front().s) return 0;
    if(s_query>=p.back().s) return p.size()-2;
    auto it = std::lower_bound(p.begin(), p.end(), s_query,
      [](const FieldLinePoint& a, double s){ return a.s < s; });
    size_t j = static_cast<size_t>(it - p.begin());
    if(j==0) return 0;
    if(j>=p.size()) return p.size()-2;
    return j-1;
  }

// -------------------------------------------------------------------------------------
// FieldLine::sample(s_query)
//   Piecewise-linear interpolation in arc-length coordinate s.
//   Returns:
//     r(s), |B|(s), Bvec(s), tangent t_hat(s), and d|B|/ds for mirror force.
//   Notes:
//     * For 4-column inputs, Bvec(s)=|B|(s)*t_hat(s).
//     * d|B|/ds is piecewise constant between nodes.
// -------------------------------------------------------------------------------------

  InterpSample sample(double s_query) const {
    InterpSample out{};
    if(p.size()<2){
      return out;
    }

    // Clamp s into range
    double s = clamp(s_query, p.front().s, p.back().s);
    size_t i = bracket(s);
    size_t j = i+1;

    double s0=p[i].s, s1=p[j].s;
    double t = (s1>s0) ? (s - s0)/(s1 - s0) : 0.0;

    out.r    = add(mul(p[i].r, 1.0-t), mul(p[j].r, t));
    out.Bvec = add(mul(p[i].Bvec,1.0-t), mul(p[j].Bvec,t));
    out.Bmag = p[i].Bmag*(1.0-t) + p[j].Bmag*t;

    // Tangent along the segment
    Vec3 dr = sub(p[j].r, p[i].r);
    double ds = (s1 - s0);
    out.t_hat = (ds>0.0) ? unit_or_zero(dr) : Vec3{0,0,0};

    // Piecewise dB/ds on the same segment
    if(ds>0.0) out.dBds = (p[j].Bmag - p[i].Bmag)/ds;
    else out.dBds = 0.0;

    // Robustness: ensure Bmag positive-ish
    if(!(out.Bmag>0.0)) out.Bmag = 0.0;

    return out;
  }

  // Tecplot XY exporter: X axis = s (relative distance), Y variables = abs(x,y,z,Bx,By,Bz,B)
  bool write_tecplot_xy(const std::string& out_path, std::string* err=nullptr) const {
    std::ofstream out(out_path);
    if(!out){
      if(err) *err = "Cannot write: " + out_path;
      return false;
    }

    out << "TITLE = \"Field line XY export\"\n";
    out << "VARIABLES = \"s_m\" \"absX_m\" \"absY_m\" \"absZ_m\" \"Bx_T\" \"By_T\" \"Bz_T\" \"B_T\"\n";
    out << "ZONE T=\"fieldline\", I=" << p.size() << ", F=POINT\n";
    out << std::setprecision(12) << std::scientific;

    for(const auto& q: p){
      out << q.s << " "
          << std::fabs(q.r.x) << " " << std::fabs(q.r.y) << " " << std::fabs(q.r.z) << " "
          << q.Bvec.x << " " << q.Bvec.y << " " << q.Bvec.z << " "
          << q.Bmag << "\n";
    }
    return true;
  }
};

struct Cli {
  std::string fieldline_path;
  std::string out_prefix = "out";
  bool help_requested = false;

  // Injection / plasma parameters
  double inj_sfrac = 0.0;     // fraction of field line length [0..1]
  double T_eV = 100.0;        // injected electron temperature [eV]
  double Epar_Vm = 0.0;       // constant parallel electric field [V/m]
  GuidingCenterMover gc_mover = GuidingCenterMover::Euler; // deterministic GC mover

  // Monte Carlo / numerics
  int npart = 100000;
  double dt = 1e-3;           // [s]
  double tmax = 1.0;          // [s]
  double D = 0.0;             // [1/s] pitch-angle diffusion coefficient

  bool mu0_provided = false;
  double mu0 = 0.0;           // fixed initial mu_cos = cos(alpha)
  bool forward_only = false;

  uint64_t seed = 1;

  // Progress output during the MC loop. The reporting itself is done only on rank 0.
  bool progress = true;
  int progress_steps = 0;   // <=0 => auto-select based on total number of time steps

  int nbins_mu = 80;
  int nbins_s  = 80;


  // Additional time-integrated sampling (energy and v_parallel)
  int nbins_E = 80;
  int nbins_vpar = 80;
  double Emin_eV = 0.0;
  double Emax_eV = -1.0;      // if <= Emin_eV => auto-set
  double vpar_min = 0.0;
  double vpar_max = 0.0;      // if <= vpar_min => auto-set

  static void print_help(const char* exe){
    std::cout
      << "Usage:\n"
      << "  " << exe << " -fieldline <path> [options]\n\n"
      << "Required:\n"
      << "  -fieldline <path>         Input field line file.\n"
      << "                            Supports columns:\n"
      << "                              x y z B\n"
      << "                              x y z Bx By Bz\n"
      << "                              x y z Bx By Bz B\n\n"
      << "Key options (requested enhancements):\n"
      << "  -inj_sfrac <0..1>          Injection location as fraction of total field-line length.\n"
      << "                            0.0=start, 0.5=middle, 1.0=end (default: 0.0)\n"
      << "  -T_eV <eV>                 Injected electron temperature (Maxwellian) (default: 100 eV)\n\n"
      << "Guiding-center / acceleration:\n"
      << "  -Epar_Vm <V/m>             Constant parallel electric field along +s (default: 0)\n"
      << "  -gc_mover <euler|rk4>      Deterministic guiding-center mover\n"
      << "                            euler = original first-order update (default)\n"
      << "                            rk4   = classical 4th-order Runge-Kutta update\n\n"
      << "Monte Carlo / numerics:\n"
      << "  -npart <int>               Number of particles (default: 100000)\n"
      << "  -dt <s>                    Time step (default: 1e-3)\n"
      << "  -tmax <s>                  Total time (default: 1.0)\n"
      << "  -D <1/s>                   Pitch-angle diffusion coeff for mu=cos(alpha) (default: 0)\n"
      << "  -mu0 <value>               Fix initial mu for all particles (overrides isotropic Maxwellian angles)\n"
      << "  -forward_only <0|1>         Force initial v_par >= 0 (default: 0)\n"
      << "  -seed <int>                RNG seed (default: 1)\n"
      << "  -progress_steps <int>      Rank-0 progress report interval in time steps\n"
      << "                            (default: auto; about 100 reports per run)\n"
      << "  -no_progress               Disable rank-0 progress output\n"
      << "  -nbins_mu <int>            mu histogram bins (default: 80)\n"
      << "  -nbins_s <int>             s bins for statistics and 2D distribution (default: 80)\n"
      << "  -nbins_E <int>             kinetic energy bins for time-integrated f(s,E) (default: 80)\n"
      << "  -Emin_eV <eV>              min kinetic energy for binning (default: 0)\n"
      << "  -Emax_eV <eV>              max kinetic energy for binning (default: auto = 10*T_eV)\n"
      << "  -nbins_vpar <int>          v_par bins for time-integrated f(s,v_par) (default: 80)\n"
      << "  -vpar_min <m/s>            min v_par for binning (default: auto = -6*v_th)\n"
      << "  -vpar_max <m/s>            max v_par for binning (default: auto = +6*v_th)\n"
      << "  -out <prefix>              Output prefix (default: out)\n"
      << "  -help                      Show this help and exit\n\n"
      << "Outputs (Tecplot ASCII):\n"
      << "  <prefix>_fieldline.dat     Field line XY: s vs abs(x,y,z), B components and |B|\n"
      << "  <prefix>_mc_mu_hist.dat    Final mu histogram (PDF)\n"
      << "  <prefix>_mc_mu_vs_s.dat    mean/var(mu) binned vs s\n"
      << "  <prefix>_mc_mu_s_2d.dat    2D distribution f(s,mu) sampled each time step; normalized per s-bin\n"
      << "  <prefix>_mc_E_s_2d.dat     2D distribution f(s,E_kin) sampled each time step; normalized per s-bin\n"
      << "  <prefix>_mc_vpar_s_2d.dat  2D distribution f(s,v_par) sampled each time step; normalized per s-bin\n"
      << "\nMPI notes:\n"
      << "  Run in parallel, for example:\n"
      << "    mpirun -np 8 " << exe << " -fieldline fieldline.txt -out run -npart 200000\n"
      << "  Particles are distributed across ranks, but only rank 0 writes output files.\n";
  }

  bool parse(int argc, char** argv, std::string* err=nullptr){
    if(argc<=1){
      if(err) *err = "No arguments. Use -help.";
      return false;
    }

    for(int i=1;i<argc;i++){
      std::string a = argv[i];
      auto need = [&](const std::string& opt)->std::string{
        if(i+1>=argc) throw std::runtime_error("Missing value after " + opt);
        return std::string(argv[++i]);
      };

      try{
        if(a=="-help" || a=="--help" || a=="-h"){
          help_requested = true;
        } else if(a=="-fieldline"){
          fieldline_path = need(a);
        } else if(a=="-out"){
          out_prefix = need(a);
        } else if(a=="-inj_sfrac"){
          inj_sfrac = std::stod(need(a));
        } else if(a=="-T_eV"){
          T_eV = std::stod(need(a));
        } else if(a=="-Epar_Vm"){
          Epar_Vm = std::stod(need(a));
        } else if(a=="-gc_mover"){
          const std::string mover_name = need(a);
          if(!parse_guiding_center_mover(mover_name, gc_mover)){
            throw std::runtime_error("Unknown -gc_mover value: " + mover_name +
                                     " (expected euler or rk4)");
          }
        } else if(a=="-npart"){
          npart = std::stoi(need(a));
        } else if(a=="-dt"){
          dt = std::stod(need(a));
        } else if(a=="-tmax"){
          tmax = std::stod(need(a));
        } else if(a=="-D"){
          D = std::stod(need(a));
        } else if(a=="-mu0"){
          mu0 = std::stod(need(a));
          mu0_provided = true;
        } else if(a=="-forward_only"){
          forward_only = (std::stoi(need(a))!=0);
        } else if(a=="-seed"){
          seed = static_cast<uint64_t>(std::stoull(need(a)));
        } else if(a=="-progress_steps"){
          progress_steps = std::stoi(need(a));
        } else if(a=="-no_progress"){
          progress = false;
        } else if(a=="-nbins_mu"){
          nbins_mu = std::max(4, std::stoi(need(a)));
        } else if(a=="-nbins_s"){
          nbins_s = std::max(4, std::stoi(need(a)));
        } else if(a=="-nbins_E"){
          nbins_E = std::max(4, std::stoi(need(a)));
        } else if(a=="-Emin_eV"){
          Emin_eV = std::stod(need(a));
        } else if(a=="-Emax_eV"){
          Emax_eV = std::stod(need(a));
        } else if(a=="-nbins_vpar"){
          nbins_vpar = std::max(4, std::stoi(need(a)));
        } else if(a=="-vpar_min"){
          vpar_min = std::stod(need(a));
        } else if(a=="-vpar_max"){
          vpar_max = std::stod(need(a));
        } else {
          throw std::runtime_error("Unknown option: " + a);
        }
      } catch(const std::exception& e){
        if(err) *err = e.what();
        return false;
      }
    }

    if(help_requested) return true;

    if(fieldline_path.empty()){
      if(err) *err = "Missing -fieldline <path>.";
      return false;
    }
    if(!(inj_sfrac>=0.0 && inj_sfrac<=1.0)){
      if(err) *err = "-inj_sfrac must be in [0,1].";
      return false;
    }
    if(!(T_eV>0.0)){
      if(err) *err = "-T_eV must be > 0.";
      return false;
    }
    if(npart<0){
      if(err) *err = "-npart must be >= 0.";
      return false;
    }
    if(!(dt>0.0 && tmax>0.0)){
      if(err) *err = "-dt and -tmax must be > 0.";
      return false;
    }
    if(D<0.0){
      if(err) *err = "-D must be >= 0.";
      return false;
    }
    if(mu0_provided && (mu0<-1.0 || mu0>1.0)){
      if(err) *err = "-mu0 must be in [-1,1].";
      return false;
    }

    // Derived/default binning ranges for energy and v_par sampling
    if(Emin_eV < 0.0){
      if(err) *err = "-Emin_eV must be >= 0.";
      return false;
    }
    if(Emax_eV <= Emin_eV){
      // Auto: cover a broad thermal tail by default
      Emax_eV = std::max(Emin_eV + 1e-6, 10.0*T_eV);
    }

    if(vpar_max <= vpar_min){
      // Auto: +/- 6 thermal speeds based on injected temperature
      const double kT = T_eV * kEvToJ; // J
      const double vth = std::sqrt(2.0*kT / kElectronMass);
      vpar_min = -6.0*vth;
      vpar_max = +6.0*vth;
    }
    if(vpar_max <= vpar_min){
      if(err) *err = "Invalid vpar range (vpar_max must be > vpar_min).";
      return false;
    }
    if(progress_steps < 0){
      if(err) *err = "-progress_steps must be >= 0.";
      return false;
    }

    return true;
  }
};

struct Particle {
  double s=0.0;       // [m]
  double vpar=0.0;    // [m/s]
  double mu_mag=0.0;  // [J/T] = m v_perp^2 /(2B)
  bool alive=true;    // particle is on the field line
};

struct MCResult {
  std::vector<double> s_final;
  std::vector<double> mu_final; // mu_cos

  // Time-integrated (sampled every time step) 2D distribution counts.
  // Layout: counts[ sbin * nbins_mu + mubin ]
  std::vector<double> mu_s_counts;
  std::vector<double> mu_s_count_s;


  // Additional time-integrated sampling (same per-s-bin totals as mu sampling).
  // Layouts:
  //   E_s_counts   [ sbin * nbins_E   + ebin ]
  //   vpar_s_counts[ sbin * nbins_vpar+ vbin ]
  std::vector<double> E_s_counts;
  std::vector<double> vpar_s_counts;
};

static inline double vth_sigma_from_Te_eV(double T_eV){
  // For Maxwellian, each velocity component is N(0, sigma^2) with sigma^2 = kT/m.
  // Convert eV -> Joules using kT = T_eV * e (since 1 eV = e Joules).
  const double kT = T_eV * kEvToJ; // J
  return std::sqrt(kT / kElectronMass);
}

static inline Vec3 sample_maxwellian_velocity(std::mt19937_64& rng, double sigma){
  std::normal_distribution<double> gauss(0.0, sigma);
  return {gauss(rng), gauss(rng), gauss(rng)};
}

// For mu diffusion, keep speed constant (during scattering), update (vpar, mu_mag) at local B.

// -------------------------------------------------------------------------------------
// apply_pitch_angle_diffusion()
//   Adds stochastic pitch-angle scattering in mu=cos(alpha):
//       mu <- mu + sqrt(2 D dt) * N(0,1)
//   Then reprojects the state onto consistent (v_par, mu_mag) at the local |B|.
//   This keeps the model internally consistent with the guiding-center invariants.
// -------------------------------------------------------------------------------------
static inline void apply_pitch_angle_diffusion(Particle& part,
                                               double B_local,
                                               double dt,
                                               double D_mumu,
                                               std::mt19937_64& rng)
{
  if(D_mumu<=0.0) return;

  // Compute current v_perp from mu_mag and B
  const double vperp2 = std::max(0.0, 2.0*part.mu_mag*B_local / kElectronMass);
  const double v2 = part.vpar*part.vpar + vperp2;
  const double v  = std::sqrt(std::max(0.0, v2));
  if(v<=0.0) return;

  double mu_cos = part.vpar / v;
  mu_cos = clamp(mu_cos, -1.0, 1.0);

  std::normal_distribution<double> gauss01(0.0, 1.0);
  const double sigma = std::sqrt(2.0*D_mumu*dt);
  mu_cos = clamp(mu_cos + sigma*gauss01(rng), -1.0, 1.0);

  // Re-split speed between parallel/perp at *fixed* |v|
  part.vpar = mu_cos * v;
  const double vperp2_new = (1.0 - mu_cos*mu_cos) * v2;

  // Update mu_mag consistent with new v_perp at the current B.
  const double B = std::max(B_local, 1e-30);
  part.mu_mag = 0.5*kElectronMass*vperp2_new / B;
}

// -------------------------------------------------------------------------------------
// gc_rhs_deterministic()
//   Right-hand side of the deterministic 1D guiding-center equations:
//
//     ds/dt     = v_parallel
//     dv_par/dt = (q/m) E_par - (mu_mag/m) d|B|/ds
//
//   The magnetic moment mu_mag is treated as constant during this deterministic
//   substep. The field-line interpolation is queried at the supplied s position, so the
//   same helper can be used by both the original first-order mover and the RK4 mover.
// -------------------------------------------------------------------------------------
struct GCRhsState {
  double s = 0.0;
  double vpar = 0.0;
};

static inline GCRhsState gc_rhs_deterministic(const FieldLine& fl,
                                              double s,
                                              double vpar,
                                              double mu_mag,
                                              double q_over_m,
                                              double Epar_Vm)
{
  const InterpSample sp = fl.sample(s);
  GCRhsState rhs;
  rhs.s = vpar;
  rhs.vpar = q_over_m*Epar_Vm - (mu_mag/kElectronMass)*sp.dBds;
  return rhs;
}

// -------------------------------------------------------------------------------------
// advance_guiding_center_euler()
//   Original first-order deterministic mover retained for backward compatibility.
//   This reproduces the legacy update order used by the code before the RK4 option
//   was added:
//     1) evaluate d|B|/ds at the old position
//     2) update v_parallel
//     3) update s using the updated v_parallel
//
//   Because many users may rely on the exact behavior of the existing mover, we keep
//   this implementation separate rather than rewriting it in a more abstract form.
// -------------------------------------------------------------------------------------
static inline void advance_guiding_center_euler(Particle& p,
                                                const FieldLine& fl,
                                                double dt,
                                                double q_over_m,
                                                double Epar_Vm)
{
  const InterpSample sOld = fl.sample(p.s);
  const double dBds = sOld.dBds;
  const double dvpar = dt * ( q_over_m*Epar_Vm - (p.mu_mag/kElectronMass)*dBds );
  p.vpar += dvpar;
  p.s += dt * p.vpar;
}

// -------------------------------------------------------------------------------------
// advance_guiding_center_rk4()
//   Classical Runge-Kutta 4th-order deterministic mover for the 1D guiding-center
//   system in (s, v_parallel). The magnetic moment mu_mag is held fixed during the
//   deterministic substep, exactly as in the legacy mover.
//
//   Boundary note:
//     The no-reflection / delete-on-exit rule is still enforced outside this helper
//     after the full time step is completed. Intermediate RK stages are allowed to
//     probe the field-line interpolation at clamped endpoints through FieldLine::sample.
//     This keeps the implementation simple and robust while preserving the original
//     endpoint-deletion semantics at the outer time-step level.
// -------------------------------------------------------------------------------------
static inline void advance_guiding_center_rk4(Particle& p,
                                              const FieldLine& fl,
                                              double dt,
                                              double q_over_m,
                                              double Epar_Vm)
{
  const GCRhsState k1 = gc_rhs_deterministic(fl, p.s, p.vpar, p.mu_mag, q_over_m, Epar_Vm);
  const GCRhsState k2 = gc_rhs_deterministic(fl,
                                             p.s + 0.5*dt*k1.s,
                                             p.vpar + 0.5*dt*k1.vpar,
                                             p.mu_mag, q_over_m, Epar_Vm);
  const GCRhsState k3 = gc_rhs_deterministic(fl,
                                             p.s + 0.5*dt*k2.s,
                                             p.vpar + 0.5*dt*k2.vpar,
                                             p.mu_mag, q_over_m, Epar_Vm);
  const GCRhsState k4 = gc_rhs_deterministic(fl,
                                             p.s + dt*k3.s,
                                             p.vpar + dt*k3.vpar,
                                             p.mu_mag, q_over_m, Epar_Vm);

  p.s += (dt/6.0) * (k1.s + 2.0*k2.s + 2.0*k3.s + k4.s);
  p.vpar += (dt/6.0) * (k1.vpar + 2.0*k2.vpar + 2.0*k3.vpar + k4.vpar);
}

// -------------------------------------------------------------------------------------
// advance_guiding_center_particle()
//   Thin dispatch wrapper that selects the deterministic mover requested on the CLI.
//   Keeping the selection in one place makes it easier to add additional movers later
//   without cluttering the Monte Carlo loop below.
// -------------------------------------------------------------------------------------
static inline void advance_guiding_center_particle(Particle& p,
                                                   const FieldLine& fl,
                                                   GuidingCenterMover mover,
                                                   double dt,
                                                   double q_over_m,
                                                   double Epar_Vm)
{
  switch(mover){
    case GuidingCenterMover::Euler:
      advance_guiding_center_euler(p, fl, dt, q_over_m, Epar_Vm);
      break;
    case GuidingCenterMover::RK4:
      advance_guiding_center_rk4(p, fl, dt, q_over_m, Epar_Vm);
      break;
    default:
      advance_guiding_center_euler(p, fl, dt, q_over_m, Epar_Vm);
      break;
  }
}

// -------------------------------------------------------------------------------------
// run_guiding_center_mc()
//   Main Monte Carlo loop:
//     1) Initialize particles at s = inj_sfrac*L with Maxwellian velocities from T_eV.
//     2) For each time step:
//         - interpolate |B|(s) as needed for scattering
//         - optionally apply pitch-angle diffusion
//         - advance the deterministic GC equations with the selected mover
//           (original first-order Euler-like update or 4th-order RK4)
//         - delete particles leaving [0,L]
//         - sample time-integrated histograms in (s,mu), (s,E), (s,v_par)
//     3) After the loop, reduce/gather distributed diagnostics onto rank 0 and
//        write Tecplot files with per-s-bin normalization there.
//
//   MPI strategy:
//     * The particle population is decomposed across ranks using a static block split.
//     * Histograms are reduced with MPI_Reduce.
//     * Final surviving-particle arrays are gathered to rank 0 with MPI_Gatherv.
//
//   Mover strategy:
//     * The stochastic pitch-angle diffusion, if enabled, is operator-split from the
//       deterministic guiding-center advance.
//     * The deterministic substep is selected by cli.gc_mover.
// -------------------------------------------------------------------------------------

static MCResult run_guiding_center_mc(const FieldLine& fl, const Cli& cli, MPI_Comm comm){
  int mpi_rank = 0, mpi_size = 1;
  MPI_Comm_rank(comm, &mpi_rank);
  MPI_Comm_size(comm, &mpi_size);

  // Make the per-rank random streams deterministic but distinct. This preserves
  // reproducibility for a fixed MPI layout while avoiding identical streams on all ranks.
  const uint64_t rank_seed = cli.seed + 0x9E3779B97F4A7C15ull * static_cast<uint64_t>(mpi_rank + 1);
  std::mt19937_64 rng(rank_seed);

  const double L = fl.length();
  const double s0 = cli.inj_sfrac * L;

  const int nsteps = static_cast<int>(std::ceil(cli.tmax / cli.dt));

  // Initialize only the local subset of particles assigned to this rank.
  const int npart_local = local_particle_count(cli.npart, mpi_rank, mpi_size);
  std::vector<Particle> part(static_cast<size_t>(npart_local));

  // Local tangent direction (used as b-hat for defining pitch angle at injection)
  InterpSample samp0 = fl.sample(s0);
  Vec3 bhat = samp0.t_hat;
  if(norm(bhat)<=0.0){
    // fallback if tangent is degenerate
    bhat = {1,0,0};
  }
  (void)bhat; // retained for documentation / future extensions that use an explicit b-hat basis

  const double sigma = vth_sigma_from_Te_eV(cli.T_eV); // std of each component

  // Uniform distribution for mu if mu0 not specified (isotropic in mu)
  std::uniform_real_distribution<double> uni01(0.0, 1.0);

  for(int i=0;i<npart_local;i++){
    Particle p;
    p.s = s0;

    // Sample thermal velocity vector
    Vec3 v = sample_maxwellian_velocity(rng, sigma);
    double vmag = norm(v);

    // If user fixes mu0, reorient the parallel/perp split while keeping |v|
    double mu_cos;
    if(cli.mu0_provided){
      mu_cos = clamp(cli.mu0, -1.0, 1.0);
    } else {
      if(cli.forward_only){
        mu_cos = uni01(rng);           // [0,1]
      } else {
        mu_cos = 2.0*uni01(rng) - 1.0; // [-1,1]
      }
    }

    // Define v_par from mu_cos and |v|, independent of sampled direction
    // (this avoids needing full gyro-phase and perpendicular basis).
    double vpar = mu_cos * vmag;
    if(cli.forward_only && vpar<0.0) vpar = -vpar;

    // Perpendicular speed
    double vperp2 = std::max(0.0, vmag*vmag - vpar*vpar);

    // Magnetic moment
    double B = std::max(samp0.Bmag, 1e-30);
    p.mu_mag = 0.5*kElectronMass*vperp2 / B;
    p.vpar = vpar;

    part[static_cast<size_t>(i)] = p;
  }

  // Time advance
  const double q_over_m = kElectronCharge / kElectronMass;

  // ---------------------------------------------------------------------------
  // Time-integrated sampling of pitch-angle distribution f(s,mu)
  // Sample at each time step based on the particle's *current* s (after the move)
  // and its mu_cos computed from (v_par, mu_mag, B(s)).
  // ---------------------------------------------------------------------------

  std::vector<double> mu_s_counts(static_cast<size_t>(cli.nbins_s * cli.nbins_mu), 0.0);
  std::vector<double> mu_s_count_s(static_cast<size_t>(cli.nbins_s), 0.0);

  std::vector<double> E_s_counts(static_cast<size_t>(cli.nbins_s * cli.nbins_E), 0.0);
  std::vector<double> vpar_s_counts(static_cast<size_t>(cli.nbins_s * cli.nbins_vpar), 0.0);

  auto idx2d = [&](int bs, int b, int stride)->size_t{
    return static_cast<size_t>(bs * stride + b);
  };

  auto s_to_bin = [&](double s)->int{
    double xs = (L>0.0) ? clamp(s / L, 0.0, 1.0) : 0.0;
    int bs = static_cast<int>(std::floor(xs * cli.nbins_s));
    if(bs < 0) bs = 0;
    if(bs >= cli.nbins_s) bs = cli.nbins_s - 1;
    return bs;
  };

  auto mu_to_bin = [&](double mu_cos)->int{
    double xm = (clamp(mu_cos, -1.0, 1.0) + 1.0) * 0.5; // [-1,1] -> [0,1]
    int bm = static_cast<int>(std::floor(xm * cli.nbins_mu));
    if(bm < 0) bm = 0;
    if(bm >= cli.nbins_mu) bm = cli.nbins_mu - 1;
    return bm;
  };

  auto E_to_bin = [&](double E_eV)->int{
    const double Emin = cli.Emin_eV;
    const double Emax = cli.Emax_eV;
    double x = (Emax > Emin) ? (E_eV - Emin)/(Emax - Emin) : 0.0;
    x = clamp(x, 0.0, 1.0);
    int be = static_cast<int>(std::floor(x * cli.nbins_E));
    if(be < 0) be = 0;
    if(be >= cli.nbins_E) be = cli.nbins_E - 1;
    return be;
  };

  auto vpar_to_bin = [&](double vpar)->int{
    const double vmin = cli.vpar_min;
    const double vmax = cli.vpar_max;
    double x = (vmax > vmin) ? (vpar - vmin)/(vmax - vmin) : 0.0;
    x = clamp(x, 0.0, 1.0);
    int bv = static_cast<int>(std::floor(x * cli.nbins_vpar));
    if(bv < 0) bv = 0;
    if(bv >= cli.nbins_vpar) bv = cli.nbins_vpar - 1;
    return bv;
  };

  // ---------------------------------------------------------------------------
  // sample_particle_into_hist(p)
  //   This lambda performs the "time-integrated sampling" requested:
  //     * It is called once per particle per time step (after the deterministic move).
  //     * It finds the particle's s-bin and variable bins (mu, E, v_par).
  //     * It increments the corresponding 2D histogram counters.
  //
  //   Notes:
  //     - Sampling uses the *current* local |B|(s) to reconstruct v_perp from mu_mag.
  //     - Energy is computed from v^2 = v_par^2 + v_perp^2.
  //     - Per-s-bin total counts are stored in mu_s_count_s[bs] and used later for
  //       per-s normalization when writing PDFs.
  // ---------------------------------------------------------------------------


  auto sample_particle_into_hist = [&](const Particle& p){
    // Defensive: only sample inside the domain
    if(p.s < 0.0 || p.s > L) return;

    InterpSample sp = fl.sample(p.s);
    const double B = std::max(sp.Bmag, 1e-30);
    const double vperp2 = std::max(0.0, 2.0*p.mu_mag*B / kElectronMass);
    const double v2 = p.vpar*p.vpar + vperp2;
    const double v  = std::sqrt(std::max(0.0, v2));
    double mu_cos = (v>0.0) ? (p.vpar / v) : 0.0;
    mu_cos = clamp(mu_cos, -1.0, 1.0);

    const int bs = s_to_bin(p.s);
    const int bm = mu_to_bin(mu_cos);

    // Kinetic energy [eV] and v_par [m/s]
    const double E_eV = 0.5*kElectronMass*v2 / kEvToJ;
    const int be = E_to_bin(E_eV);
    const int bv = vpar_to_bin(p.vpar);

    mu_s_counts[idx2d(bs,bm,cli.nbins_mu)] += 1.0;
    E_s_counts[idx2d(bs,be,cli.nbins_E)] += 1.0;
    vpar_s_counts[idx2d(bs,bv,cli.nbins_vpar)] += 1.0;

    mu_s_count_s[static_cast<size_t>(bs)] += 1.0;
  };

  // Progress-report cadence. By default, aim for about 100 updates per run, but
  // never more frequent than once per step. Progress output is emitted only by rank 0.
  const int progress_stride = (cli.progress_steps > 0) ? cli.progress_steps
                                                       : std::max(1, nsteps / 100);
  const auto wall_t0 = std::chrono::steady_clock::now();

  long long local_alive = static_cast<long long>(part.size());

  for(int n=0;n<nsteps;n++){
    for(auto& p : part){
      if(!p.alive) continue;

      // Sample local field at the old position. This is used only for the stochastic
      // pitch-angle diffusion operator. The deterministic advance itself is handled
      // below by the user-selected guiding-center mover.
      InterpSample sOld = fl.sample(p.s);
      const double B_old = std::max(sOld.Bmag, 1e-30);

      // Optional scattering (acts at old position)
      apply_pitch_angle_diffusion(p, B_old, cli.dt, cli.D, rng);

      // Deterministic guiding-center update. The selected mover advances (s, v_par)
      // over one full time step while keeping mu_mag fixed during this substep.
      advance_guiding_center_particle(p, fl, cli.gc_mover, cli.dt, q_over_m, cli.Epar_Vm);

      // Delete particle if it leaves the field line (no reflection)
      if(p.s < 0.0 || p.s > L){
        p.alive = false;
        local_alive--;
        continue;
      }

      // Sample distribution after the move at this time step
      sample_particle_into_hist(p);

      // Update mu_mag is constant in deterministic step (already stored),
      // but B changes => v_perp changes implicitly via mu_mag = m v_perp^2 /(2B).
      // We do not need to explicitly store v_perp, but it enters mu_cos at output.
    }

    // Progress output: reduce the currently alive particle count across all ranks and
    // let rank 0 print a compact status line. The reduction is done only at the chosen
    // stride so that the reporting overhead stays small even for long runs.
    const bool report_progress = cli.progress &&
                                 ((n==0) || ((n+1)%progress_stride==0) || (n+1==nsteps));
    if(report_progress){
      long long global_alive = 0;
      MPI_Reduce(&local_alive, &global_alive, 1, MPI_LONG_LONG, MPI_SUM, 0, comm);

      if(mpi_rank == 0){
        const double frac = (nsteps>0) ? static_cast<double>(n+1)/static_cast<double>(nsteps) : 1.0;
        const auto now = std::chrono::steady_clock::now();
        const double elapsed_s = std::chrono::duration<double>(now - wall_t0).count();
        const double eta_s = (frac > 0.0 && frac < 1.0) ? elapsed_s*(1.0 - frac)/frac : 0.0;

        std::cout << std::fixed << std::setprecision(1)
                  << "[progress] step " << (n+1) << "/" << nsteps
                  << " (" << (100.0*frac) << "%), alive=" << global_alive
                  << ", elapsed=" << elapsed_s << " s"
                  << ", eta=" << eta_s << " s"
                  << std::defaultfloat << "\n";
      }
    }
  }

  // Collect local surviving-particle outputs first. These are later gathered to rank 0,
  // while the histogram-style diagnostics are globally reduced with MPI_Reduce.
  std::vector<double> s_final_local;
  std::vector<double> mu_final_local;
  s_final_local.reserve(part.size());
  mu_final_local.reserve(part.size());

  for(const Particle& p : part){
    if(!p.alive) continue;
    if(p.s < 0.0 || p.s > L) continue; // should not happen, but keep it robust

    InterpSample sf = fl.sample(p.s);
    const double B = std::max(sf.Bmag, 1e-30);
    const double vperp2 = std::max(0.0, 2.0*p.mu_mag*B / kElectronMass);
    const double v2 = p.vpar*p.vpar + vperp2;
    const double v  = std::sqrt(std::max(0.0, v2));
    double mu_cos = (v>0.0) ? (p.vpar / v) : 0.0;
    mu_cos = clamp(mu_cos, -1.0, 1.0);

    s_final_local.push_back(p.s);
    mu_final_local.push_back(mu_cos);
  }

  MCResult out;
  out.mu_s_counts   = reduce_sum_vector_to_root(mu_s_counts, 0, comm);
  out.mu_s_count_s  = reduce_sum_vector_to_root(mu_s_count_s, 0, comm);
  out.E_s_counts    = reduce_sum_vector_to_root(E_s_counts, 0, comm);
  out.vpar_s_counts = reduce_sum_vector_to_root(vpar_s_counts, 0, comm);
  out.s_final       = gather_double_vector_to_root(s_final_local, 0, comm);
  out.mu_final      = gather_double_vector_to_root(mu_final_local, 0, comm);

  return out;
}

static bool write_mu_hist_tecplot(const std::string& out_path,
                                  const std::vector<double>& mu,
                                  int nbins,
                                  std::string* err=nullptr)
{
  std::vector<double> counts(static_cast<size_t>(nbins), 0.0);

  for(double m : mu){
    double x = (m + 1.0) * 0.5; // [-1,1] -> [0,1]
    int b = static_cast<int>(std::floor(x * nbins));
    if(b<0) b=0;
    if(b>=nbins) b=nbins-1;
    counts[static_cast<size_t>(b)] += 1.0;
  }

  const double N  = static_cast<double>(mu.size());
  const double dm = 2.0 / nbins;

  std::ofstream out(out_path);
  if(!out){
    if(err) *err = "Cannot write: " + out_path;
    return false;
  }

  out << "TITLE = \"Pitch-angle distribution (mu histogram)\"\n";
  out << "VARIABLES = \"mu\" \"pdf\"\n";
  out << "ZONE T=\"mu_hist\", I=" << nbins << ", F=POINT\n";
  out << std::setprecision(12) << std::scientific;

  for(int b=0;b<nbins;b++){
    const double mu_center = -1.0 + (b + 0.5)*dm;
    const double pdf = (N>0.0) ? ((counts[static_cast<size_t>(b)]/N)/dm) : 0.0;
    out << mu_center << " " << pdf << "\n";
  }
  return true;
}

static bool write_mu_vs_s_tecplot(const std::string& out_path,
                                  const std::vector<double>& s,
                                  const std::vector<double>& mu,
                                  double smax,
                                  int nbins_s,
                                  std::string* err=nullptr)
{
  std::vector<double> sum_mu(static_cast<size_t>(nbins_s),0.0);
  std::vector<double> sum_mu2(static_cast<size_t>(nbins_s),0.0);
  std::vector<double> count(static_cast<size_t>(nbins_s),0.0);

  for(size_t i=0;i<s.size();i++){
    double x = clamp(s[i]/smax, 0.0, 1.0);
    int b = static_cast<int>(std::floor(x*nbins_s));
    if(b>=nbins_s) b=nbins_s-1;
    size_t bi = static_cast<size_t>(b);
    sum_mu[bi] += mu[i];
    sum_mu2[bi] += mu[i]*mu[i];
    count[bi] += 1.0;
  }

  std::ofstream out(out_path);
  if(!out){
    if(err) *err = "Cannot write: " + out_path;
    return false;
  }

  out << "TITLE = \"Pitch-angle moments vs distance\"\n";
  out << "VARIABLES = \"s_m\" \"mu_mean\" \"mu_var\" \"count\"\n";
  out << "ZONE T=\"mu_vs_s\", I=" << nbins_s << ", F=POINT\n";
  out << std::setprecision(12) << std::scientific;

  const double ds = smax/nbins_s;
  for(int b=0;b<nbins_s;b++){
    const double s_center = (b + 0.5)*ds;
    size_t bi = static_cast<size_t>(b);
    if(count[bi]>0.0){
      const double m = sum_mu[bi]/count[bi];
      const double v = sum_mu2[bi]/count[bi] - m*m;
      out << s_center << " " << m << " " << v << " " << count[bi] << "\n";
    } else {
      out << s_center << " " << 0.0 << " " << 0.0 << " " << 0.0 << "\n";
    }
  }
  return true;
}

// Write a 2D distribution f(s,mu) from pre-accumulated counts.
// counts layout: counts[ sbin * nbins_mu + mubin ]
// count_s layout: per-sbin totals, same sampling scheme used for counts.

// -------------------------------------------------------------------------------------
// write_mu_s_2d_tecplot_from_counts()
//   Writes time-integrated pitch-angle PDF per s-bin as a Tecplot ordered zone.
//   Input:
//     - mu_s_counts[bs, bm] : raw sample counts accumulated during the run
//     - mu_s_count_s[bs]    : total samples in each s-bin (normalization denominator)
//   Output variables:
//     s_m, mu_cos, pdf
//   Normalization:
//     For each s-bin independently, pdf is scaled so that sum_j pdf*DeltaMu = 1.
// -------------------------------------------------------------------------------------
static bool write_mu_s_2d_tecplot_from_counts(const std::string& out_path,
                                              const std::vector<double>& counts,
                                              const std::vector<double>& count_s,
                                              double smax,
                                              int nbins_s,
                                              int nbins_mu,
                                              std::string* err=nullptr)
{
  if(static_cast<int>(count_s.size())!=nbins_s || static_cast<int>(counts.size())!=nbins_s*nbins_mu){
    if(err) *err = "write_mu_s_2d_tecplot_from_counts: size mismatch";
    return false;
  }

  const double ds = smax/nbins_s;
  const double dm = 2.0/nbins_mu;

  std::ofstream out(out_path);
  if(!out){
    if(err) *err = "Cannot write: " + out_path;
    return false;
  }

  out << "TITLE = \"2D distribution f(s,mu) (time-integrated)\"\n";
  out << "VARIABLES = \"s_m\" \"mu\" \"pdf\"\n";
  // Tecplot ordered zone: I varies fastest, then J.
  // We'll use I=nbins_s (s) and J=nbins_mu (mu) so that rows correspond to mu bins.
  out << "ZONE T=\"mu_s_2d\", I=" << nbins_s << ", J=" << nbins_mu << ", F=POINT\n";
  out << std::setprecision(12) << std::scientific;

  for(int jm=0;jm<nbins_mu;jm++){
    const double mu_center = -1.0 + (jm + 0.5)*dm;
    for(int is=0;is<nbins_s;is++){
      const double s_center = (is + 0.5)*ds;

      const double cs = count_s[static_cast<size_t>(is)];
      // Normalize per s-bin so that for each s-bin: sum_j pdf(s,mu_j)*dm = 1
      double pdf = 0.0;
      if(cs>0.0){
        pdf = (counts[static_cast<size_t>(is*nbins_mu + jm)]/cs)/dm;
      }
      out << s_center << " " << mu_center << " " << pdf << "\n";
    }
  }

  return true;
}

// Write a 2D distribution f(s,E_kin) from pre-accumulated counts.
// counts layout: counts[ sbin * nbins_E + ebin ]
// count_s layout: per-sbin totals (same sampling cadence as counts).

// -------------------------------------------------------------------------------------
// write_E_s_2d_tecplot_from_counts()
//   Writes time-integrated kinetic-energy PDF per s-bin.
//   Energy is binned linearly in [Emin_eV, Emax_eV].
//   Per-s normalization: sum_j pdf*DeltaE = 1.
// -------------------------------------------------------------------------------------
static bool write_E_s_2d_tecplot_from_counts(const std::string& out_path,
                                             const std::vector<double>& counts,
                                             const std::vector<double>& count_s,
                                             double smax,
                                             int nbins_s,
                                             int nbins_E,
                                             double Emin_eV,
                                             double Emax_eV,
                                             std::string* err=nullptr)
{
  if(static_cast<int>(count_s.size())!=nbins_s || static_cast<int>(counts.size())!=nbins_s*nbins_E){
    if(err) *err = "write_E_s_2d_tecplot_from_counts: size mismatch";
    return false;
  }
  const double ds = smax/nbins_s;
  const double dE = (Emax_eV - Emin_eV)/nbins_E;

  std::ofstream out(out_path);
  if(!out){
    if(err) *err = "Cannot write: " + out_path;
    return false;
  }

  out << "TITLE = \"2D distribution f(s,E_kin) (time-integrated)\"\n";
  out << "VARIABLES = \"s_m\" \"E_eV\" \"pdf\"\n";
  out << "ZONE T=\"E_s_2d\", I=" << nbins_s << ", J=" << nbins_E << ", F=POINT\n";
  out << std::setprecision(12) << std::scientific;

  for(int jE=0;jE<nbins_E;jE++){
    const double E_center = Emin_eV + (jE + 0.5)*dE;
    for(int is=0;is<nbins_s;is++){
      const double s_center = (is + 0.5)*ds;
      const double cs = count_s[static_cast<size_t>(is)];
      double pdf = 0.0;
      if(cs>0.0 && dE>0.0){
        pdf = (counts[static_cast<size_t>(is*nbins_E + jE)]/cs)/dE;
      }
      out << s_center << " " << E_center << " " << pdf << "\n";
    }
  }
  return true;
}

// Write a 2D distribution f(s,v_par) from pre-accumulated counts.
// counts layout: counts[ sbin * nbins_vpar + vbin ]
// count_s layout: per-sbin totals (same sampling cadence as counts).

// -------------------------------------------------------------------------------------
// write_vpar_s_2d_tecplot_from_counts()
//   Writes time-integrated parallel-velocity PDF per s-bin.
//   v_par is binned linearly in [vpar_min, vpar_max].
//   Per-s normalization: sum_j pdf*DeltaV = 1.
// -------------------------------------------------------------------------------------
static bool write_vpar_s_2d_tecplot_from_counts(const std::string& out_path,
                                                const std::vector<double>& counts,
                                                const std::vector<double>& count_s,
                                                double smax,
                                                int nbins_s,
                                                int nbins_vpar,
                                                double vpar_min,
                                                double vpar_max,
                                                std::string* err=nullptr)
{
  if(static_cast<int>(count_s.size())!=nbins_s || static_cast<int>(counts.size())!=nbins_s*nbins_vpar){
    if(err) *err = "write_vpar_s_2d_tecplot_from_counts: size mismatch";
    return false;
  }
  const double ds = smax/nbins_s;
  const double dv = (vpar_max - vpar_min)/nbins_vpar;

  std::ofstream out(out_path);
  if(!out){
    if(err) *err = "Cannot write: " + out_path;
    return false;
  }

  out << "TITLE = \"2D distribution f(s,v_par) (time-integrated)\"\n";
  out << "VARIABLES = \"s_m\" \"vpar_mps\" \"pdf\"\n";
  out << "ZONE T=\"vpar_s_2d\", I=" << nbins_s << ", J=" << nbins_vpar << ", F=POINT\n";
  out << std::setprecision(12) << std::scientific;

  for(int jv=0;jv<nbins_vpar;jv++){
    const double v_center = vpar_min + (jv + 0.5)*dv;
    for(int is=0;is<nbins_s;is++){
      const double s_center = (is + 0.5)*ds;
      const double cs = count_s[static_cast<size_t>(is)];
      double pdf = 0.0;
      if(cs>0.0 && dv>0.0){
        pdf = (counts[static_cast<size_t>(is*nbins_vpar + jv)]/cs)/dv;
      }
      out << s_center << " " << v_center << " " << pdf << "\n";
    }
  }
  return true;
}



int main(int argc, char** argv){
  MPI_Init(&argc, &argv);

  int mpi_rank = 0, mpi_size = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  Cli cli;
  std::string err;

  if(!cli.parse(argc, argv, &err)){
    if(mpi_rank == 0){
      std::cerr << "Error: " << err << "\n\n";
      Cli::print_help(argv[0]);
    }
    MPI_Finalize();
    return 1;
  }

  if(cli.help_requested){
    if(mpi_rank == 0) Cli::print_help(argv[0]);
    MPI_Finalize();
    return 0;
  }

  FieldLine fl;
  if(!fl.load(cli.fieldline_path, &err)){
    if(mpi_rank == 0) std::cerr << "Error: " << err << "\n";
    MPI_Finalize();
    return 2;
  }

  if(fl.p.size()<2 || !(fl.length()>0.0)){
    if(mpi_rank == 0) std::cerr << "Error: field line is too short or invalid.\n";
    MPI_Finalize();
    return 3;
  }

  // 1) Field line Tecplot output. Only rank 0 writes files so that output naming stays
  //    unchanged relative to the serial code.
  if(mpi_rank == 0){
    const std::string out_path = cli.out_prefix + "_fieldline.dat";
    if(!fl.write_tecplot_xy(out_path, &err)){
      std::cerr << "Error: " << err << "\n";
      MPI_Finalize();
      return 4;
    }
    std::cout << "Wrote: " << out_path << "\n";
    std::cout << "Running with " << mpi_size << " MPI rank(s) and "
              << cli.npart << " total particle(s).\n";
    std::cout << "Deterministic guiding-center mover: "
              << guiding_center_mover_name(cli.gc_mover) << "\n";
    if(cli.npart == 0){
      std::cout << "-npart 0: field-line export only; Monte Carlo outputs will be empty.\n";
    }
  }

  // Keep all ranks synchronized before entering the collective Monte Carlo section.
  MPI_Barrier(MPI_COMM_WORLD);

  // 2) Guiding-center Monte Carlo
  MCResult res = run_guiding_center_mc(fl, cli, MPI_COMM_WORLD);

  // 3) Final mu histogram
  if(mpi_rank == 0){
    const std::string out_path = cli.out_prefix + "_mc_mu_hist.dat";
    if(!write_mu_hist_tecplot(out_path, res.mu_final, cli.nbins_mu, &err)){
      std::cerr << "Error: " << err << "\n";
      MPI_Finalize();
      return 5;
    }
    std::cout << "Wrote: " << out_path << "\n";
  }

  // 4) mu moments vs s
  if(mpi_rank == 0){
    const std::string out_path = cli.out_prefix + "_mc_mu_vs_s.dat";
    if(!write_mu_vs_s_tecplot(out_path, res.s_final, res.mu_final, fl.length(), cli.nbins_s, &err)){
      std::cerr << "Error: " << err << "\n";
      MPI_Finalize();
      return 6;
    }
    std::cout << "Wrote: " << out_path << "\n";
  }

  // 5) 2D distribution f(s,mu) (time-integrated sampling, normalized per s-bin)
  if(mpi_rank == 0){
    const std::string out_path = cli.out_prefix + "_mc_mu_s_2d.dat";
    if(!write_mu_s_2d_tecplot_from_counts(out_path, res.mu_s_counts, res.mu_s_count_s,
                                          fl.length(), cli.nbins_s, cli.nbins_mu, &err)){
      std::cerr << "Error: " << err << "\n";
      MPI_Finalize();
      return 7;
    }
    std::cout << "Wrote: " << out_path << "\n";
  }

  // 6) 2D distribution f(s,E_kin) (time-integrated sampling, normalized per s-bin)
  if(mpi_rank == 0){
    const std::string out_path = cli.out_prefix + "_mc_E_s_2d.dat";
    if(!write_E_s_2d_tecplot_from_counts(out_path, res.E_s_counts, res.mu_s_count_s,
                                         fl.length(), cli.nbins_s, cli.nbins_E,
                                         cli.Emin_eV, cli.Emax_eV, &err)){
      std::cerr << "Error: " << err << "\n";
      MPI_Finalize();
      return 8;
    }
    std::cout << "Wrote: " << out_path << "\n";
  }

  // 7) 2D distribution f(s,v_par) (time-integrated sampling, normalized per s-bin)
  if(mpi_rank == 0){
    const std::string out_path = cli.out_prefix + "_mc_vpar_s_2d.dat";
    if(!write_vpar_s_2d_tecplot_from_counts(out_path, res.vpar_s_counts, res.mu_s_count_s,
                                            fl.length(), cli.nbins_s, cli.nbins_vpar,
                                            cli.vpar_min, cli.vpar_max, &err)){
      std::cerr << "Error: " << err << "\n";
      MPI_Finalize();
      return 9;
    }
    std::cout << "Wrote: " << out_path << "\n";
    std::cout << "Done.\n";
  }

  MPI_Finalize();
  return 0;
}
