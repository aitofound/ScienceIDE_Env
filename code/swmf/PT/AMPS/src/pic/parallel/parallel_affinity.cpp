/*
 * parallel_affinity.cpp
 *
 * CPU affinity helpers for MPI + std::thread hybrid calculations in AMPS/PIC.
 *
 * Motivation
 * ----------
 * Some MPI launchers and batch-system integrations pin each MPI rank to a
 * single CPU core. That behavior is often useful for pure-MPI jobs, but it can
 * severely limit a hybrid calculation in which each MPI rank creates additional
 * std::thread workers. The worker threads inherit the CPU affinity mask of the
 * parent MPI rank. If the rank is allowed to run only on CPU 4, then all worker
 * threads are also allowed to run only on CPU 4, even if many threads are
 * created successfully.
 *
 * A manual command such as:
 *
 *   taskset -apc 0-39 <PID>
 *
 * can widen the affinity mask of a running rank and all of its existing
 * threads. The functions below automate that operation from inside each MPI
 * rank. This is useful on multi-node jobs because every rank can adjust its own
 * process affinity locally without needing to log into each allocated node.
 *
 * Design
 * ------
 * All public functions are placed in namespace PIC::Parallel.
 *
 * Two affinity strategies are provided:
 *
 *   1. Wide scheduler-controlled affinity
 *
 *      PIC::Parallel::SetWideAffinityForScheduler()
 *
 *      This gives the rank a broad CPU mask, usually all online CPUs on the
 *      node, and leaves individual worker-thread placement to the Linux
 *      scheduler. This is the preferred mode when the goal is: "do not pin
 *      threads to fixed cores; just stop the MPI runtime from restricting every
 *      thread to one core."
 *
 *   2. Local-rank partitioned affinity
 *
 *      PIC::Parallel::SetAffinityByLocalRank(threadsPerRank)
 *
 *      This assigns a non-overlapping CPU range to each MPI rank on the same
 *      node. This can reduce competition between ranks on a node, but assumes a
 *      simple contiguous CPU numbering scheme.
 *
 * Important limitations
 * ---------------------
 * These helpers cannot escape restrictions imposed by the scheduler or Linux
 * cgroups. If a job allocation genuinely allows a rank to use only one CPU,
 * then requesting a wider CPU set will fail or the resulting affinity mask will
 * remain narrow. Use PrintCurrentAffinity() to verify the actual mask after the
 * affinity operation.
 *
 * These functions intentionally use the external taskset command because that
 * exactly reproduces the manual command that was observed to work on the target
 * system. A future implementation could replace taskset with direct
 * sched_setaffinity()/pthread_setaffinity_np() calls.
 */

#include "parallel_affinity.h"

#include <mpi.h>
#include <unistd.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace {

/*
 * Trim_()
 *
 * Remove leading and trailing ASCII whitespace from a string. This is used when
 * parsing CPU-list fragments such as " 0-7" or "8-15 ".
 */
std::string Trim_(const std::string& s) {
  std::size_t first = 0;
  while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) {
    ++first;
  }

  std::size_t last = s.size();
  while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) {
    --last;
  }

  return s.substr(first, last - first);
}

/*
 * IsSafeCpuList_()
 *
 * Basic validation for a CPU-list string before passing it to a shell command.
 *
 * Allowed characters are digits, comma, dash, colon, and whitespace. The colon
 * is accepted because some taskset implementations support stride notation such
 * as "0-15:2". The parser used by CountCpusInList() does not fully count stride
 * notation, but taskset can still receive it safely.
 *
 * This is not meant to be a complete taskset grammar validator. It is a safety
 * check to avoid accidental shell metacharacters in environment variables.
 */
bool IsSafeCpuList_(const std::string& cpuList) {
  if (cpuList.empty()) return false;

  for (std::size_t i = 0; i < cpuList.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(cpuList[i]);

    if (std::isdigit(c) || c == ',' || c == '-' || c == ':' || std::isspace(c)) {
      continue;
    }

    return false;
  }

  return true;
}

/*
 * MpiIsInitialized_()
 *
 * Query whether MPI_Init() has already been called. The affinity helpers are
 * intended to be called after MPI_Init(), but this guard allows diagnostics to
 * still work in a non-MPI or pre-MPI context.
 */
bool MpiIsInitialized_() {
  int initialized = 0;
  MPI_Initialized(&initialized);
  return initialized != 0;
}

/*
 * GetGlobalMpiRank_()
 *
 * Return the global MPI rank for diagnostic messages. If MPI has not been
 * initialized, return -1.
 */
int GetGlobalMpiRank_() {
  if (!MpiIsInitialized_()) return -1;

  int rank = -1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return rank;
}

/*
 * RunTasksetForCurrentProcess_()
 *
 * Apply taskset -apc <cpuSet> <pid> to the current process.
 *
 * The -a option is important: it applies the affinity change to all existing
 * tasks/threads in the process. Worker threads created after this call should
 * inherit the current process affinity mask.
 *
 * Return value:
 *   The return value from std::system(). A value of 0 normally means the taskset
 *   command succeeded.
 */
int RunTasksetForCurrentProcess_(const std::string& cpuSet) {
  const pid_t pid = getpid();

  char cmd[512];
  std::snprintf(cmd, sizeof(cmd),
                "taskset -apc %s %ld >/dev/null 2>&1",
                cpuSet.c_str(), static_cast<long>(pid));

  return std::system(cmd);
}

}  // anonymous namespace

namespace PIC {
namespace Parallel {

std::string GetNodeOnlineCpuList() {
  /*
   * Preferred Linux mechanism.
   *
   * The file /sys/devices/system/cpu/online contains a CPU-list string in a
   * format already understood by taskset. Examples include:
   *
   *   0-39
   *   0-127
   *   0-31,64-95
   */
  std::ifstream f("/sys/devices/system/cpu/online");

  if (f.good()) {
    std::string s;
    std::getline(f, s);
    s = Trim_(s);

    if (!s.empty()) return s;
  }

  /*
   * Fallback: ask POSIX for the number of online logical processors.
   *
   * This only gives a count, not actual CPU IDs, so we assume IDs are
   * contiguous from 0 to ncpu-1.
   */
  const long ncpu = sysconf(_SC_NPROCESSORS_ONLN);

  if (ncpu > 0) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "0-%ld", ncpu - 1);
    return std::string(buf);
  }

  /*
   * Empty string means the CPU list could not be determined.
   */
  return std::string();
}

int CountCpusInList(const std::string& cpuList) {
  int count = 0;

  std::stringstream ss(cpuList);
  std::string item;

  while (std::getline(ss, item, ',')) {
    item = Trim_(item);
    if (item.empty()) continue;

    /*
     * Ignore optional stride suffix in diagnostic counting. For example,
     * "0-15:2" is counted as 16 here rather than 8. This function is only for
     * logging, not for affinity correctness. The full string is still passed to
     * taskset unchanged.
     */
    const std::size_t colon = item.find(':');
    if (colon != std::string::npos) item = item.substr(0, colon);

    const std::size_t dash = item.find('-');

    if (dash == std::string::npos) {
      ++count;
    }
    else {
      const int first = std::atoi(item.substr(0, dash).c_str());
      const int last  = std::atoi(item.substr(dash + 1).c_str());

      if (last >= first) count += last - first + 1;
    }
  }

  return count;
}

void PrintCurrentAffinity(const char* label) {
  const int rank = GetGlobalMpiRank_();
  const pid_t pid = getpid();

  if (label != nullptr && label[0] != '\0') {
    std::fprintf(stderr, "[rank %d pid %ld] affinity %s\n",
                 rank, static_cast<long>(pid), label);
  }

  /*
   * Read /proc/self/status directly instead of invoking grep. This avoids an
   * extra shell command and works even if grep is unavailable in a restricted
   * runtime environment.
   */
  std::ifstream f("/proc/self/status");

  if (!f.good()) {
    std::fprintf(stderr,
                 "[rank %d pid %ld] affinity: cannot read /proc/self/status\n",
                 rank, static_cast<long>(pid));
    return;
  }

  std::string line;
  while (std::getline(f, line)) {
    if (line.find("Cpus_allowed_list:") == 0) {
      std::fprintf(stderr, "[rank %d pid %ld] %s\n",
                   rank, static_cast<long>(pid), line.c_str());
      return;
    }
  }

  std::fprintf(stderr,
               "[rank %d pid %ld] affinity: Cpus_allowed_list not found\n",
               rank, static_cast<long>(pid));
}

void SetWideAffinityForScheduler() {
  /*
   * First allow explicit runtime control with environment variables.
   *
   * PIC_PARALLEL_CPUSET is generic and should be preferred for shared PIC
   * infrastructure. AMPS_MODE3D_DENSITY_CPUSET is kept for compatibility with
   * density-backtracking runs that may already use that name.
   */
  std::string cpuSet;

  const char* envCpuSet = std::getenv("PIC_PARALLEL_CPUSET");
  if (envCpuSet != nullptr && envCpuSet[0] != '\0') {
    cpuSet = envCpuSet;
  }
  else {
    envCpuSet = std::getenv("AMPS_MODE3D_DENSITY_CPUSET");
    if (envCpuSet != nullptr && envCpuSet[0] != '\0') {
      cpuSet = envCpuSet;
    }
  }

  /*
   * If no explicit CPU set is provided, use the online CPU list for this node.
   */
  if (cpuSet.empty()) {
    cpuSet = GetNodeOnlineCpuList();
  }

  SetWideAffinityForScheduler(cpuSet);
}

void SetWideAffinityForScheduler(const std::string& cpuSetIn) {
  const int rank = GetGlobalMpiRank_();
  const pid_t pid = getpid();

  const std::string cpuSet = Trim_(cpuSetIn);

  if (cpuSet.empty()) {
    std::fprintf(stderr,
                 "[rank %d pid %ld] wide affinity: empty CPU set; affinity unchanged\n",
                 rank, static_cast<long>(pid));
    return;
  }

  if (!IsSafeCpuList_(cpuSet)) {
    std::fprintf(stderr,
                 "[rank %d pid %ld] wide affinity: unsafe CPU set string '%s'; affinity unchanged\n",
                 rank, static_cast<long>(pid), cpuSet.c_str());
    return;
  }

  const int nCpu = CountCpusInList(cpuSet);

  /*
   * Apply the same operation that was verified manually:
   *
   *   taskset -apc <CPUSET> <PID>
   */
  const int ret = RunTasksetForCurrentProcess_(cpuSet);

  std::fprintf(stderr,
               "[rank %d pid %ld] wide affinity: CPUSET=%s, nCPU=%d, taskset ret=%d\n",
               rank, static_cast<long>(pid), cpuSet.c_str(), nCpu, ret);

  /*
   * Print the resulting mask. This is the most important diagnostic because a
   * scheduler or cgroup may prevent the requested mask from taking effect.
   */
  PrintCurrentAffinity("after SetWideAffinityForScheduler");
}

void SetAffinityByLocalRank(int threadsPerRank) {
  const int globalRank = GetGlobalMpiRank_();
  const pid_t pid = getpid();

  if (threadsPerRank <= 0) {
    std::fprintf(stderr,
                 "[rank %d pid %ld] local-rank affinity: invalid threadsPerRank=%d; affinity unchanged\n",
                 globalRank, static_cast<long>(pid), threadsPerRank);
    return;
  }

  if (!MpiIsInitialized_()) {
    std::fprintf(stderr,
                 "[rank %d pid %ld] local-rank affinity: MPI is not initialized; affinity unchanged\n",
                 globalRank, static_cast<long>(pid));
    return;
  }

  /*
   * Create a communicator containing only MPI ranks that share memory. On a
   * normal cluster, this means all ranks on the same compute node.
   *
   * CPU IDs are local to each node, so we need a node-local rank index rather
   * than the global MPI rank.
   */
  MPI_Comm localComm;
  MPI_Comm_split_type(MPI_COMM_WORLD,
                      MPI_COMM_TYPE_SHARED,
                      globalRank,
                      MPI_INFO_NULL,
                      &localComm);

  int localRank = 0;
  int localSize = 1;
  MPI_Comm_rank(localComm, &localRank);
  MPI_Comm_size(localComm, &localSize);
  MPI_Comm_free(&localComm);

  /*
   * Assign a contiguous CPU block to this local rank.
   *
   * Example for threadsPerRank=8:
   *
   *   localRank=0 -> 0-7
   *   localRank=1 -> 8-15
   *   localRank=2 -> 16-23
   */
  const int firstCpu = localRank * threadsPerRank;
  const int lastCpu  = firstCpu + threadsPerRank - 1;

  char cpuSet[128];
  std::snprintf(cpuSet, sizeof(cpuSet), "%d-%d", firstCpu, lastCpu);

  const int ret = RunTasksetForCurrentProcess_(cpuSet);

  std::fprintf(stderr,
               "[rank %d local_rank %d local_size %d pid %ld] local-rank affinity: CPUSET=%s, threadsPerRank=%d, taskset ret=%d\n",
               globalRank, localRank, localSize, static_cast<long>(pid),
               cpuSet, threadsPerRank, ret);

  PrintCurrentAffinity("after SetAffinityByLocalRank");
}

}  // namespace Parallel
}  // namespace PIC
