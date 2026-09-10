#ifndef _PIC_PARALLEL_AFFINITY_H_
#define _PIC_PARALLEL_AFFINITY_H_

#include <string>

namespace PIC {
namespace Parallel {

/*
 * Return a taskset-compatible list of logical CPUs that are online on the
 * current compute node.
 *
 * The implementation first reads:
 *
 *   /sys/devices/system/cpu/online
 *
 * which usually returns strings such as:
 *
 *   0-39
 *   0-63
 *   0-31,64-95
 *
 * If that Linux sysfs file is not available, the implementation falls back to
 * sysconf(_SC_NPROCESSORS_ONLN) and assumes contiguous CPU IDs from 0.
 *
 * Return value:
 *   Non-empty taskset-compatible CPU list on success.
 *   Empty string if the online CPU list cannot be determined.
 *
 * Important:
 *   This reports CPUs online on the node. It does not necessarily report the
 *   subset allocated by the batch scheduler or cgroup for the current job.
 */
std::string GetNodeOnlineCpuList();

/*
 * Count how many logical CPUs are represented by a taskset-style CPU list.
 *
 * Supported examples:
 *
 *   4             -> 1
 *   0-7           -> 8
 *   0-31,64-95    -> 64
 *
 * This is mainly a diagnostic helper for log messages. taskset itself does not
 * need this count.
 */
int CountCpusInList(const std::string& cpuList);

/*
 * Print the current CPU affinity mask of this process to stderr.
 *
 * The implementation prints the Cpus_allowed_list entry from:
 *
 *   /proc/self/status
 *
 * Example useful output:
 *
 *   Cpus_allowed_list: 0-39
 *
 * Problematic output for std::thread parallelism:
 *
 *   Cpus_allowed_list: 4
 *
 * because all worker threads created by this process would be restricted to one
 * CPU. The optional label is printed before the affinity line to identify the
 * call site.
 */
void PrintCurrentAffinity(const char* label = nullptr);

/*
 * Set a wide CPU affinity mask for the current MPI rank/process and let the
 * Linux scheduler decide which allowed CPU runs each worker thread.
 *
 * This function does not pin individual std::thread workers to fixed CPUs.
 * It only widens the process-level allowed CPU mask by running the equivalent
 * of:
 *
 *   taskset -apc <CPUSET> <PID>
 *
 * where <PID> is getpid() for the current MPI rank.
 *
 * CPU set selection:
 *
 *   1. If PIC_PARALLEL_CPUSET is set, use it.
 *   2. Else if AMPS_MODE3D_DENSITY_CPUSET is set, use it.
 *   3. Else automatically use GetNodeOnlineCpuList().
 *
 * Recommended call location:
 *   After MPI_Init() and before creating std::thread workers.
 *
 * Limitation:
 *   This cannot escape scheduler/cgroup restrictions. If the job allocation
 *   allows only CPU 4, requesting CPUSET=0-39 will fail or remain narrow.
 */
void SetWideAffinityForScheduler();

/*
 * Same as SetWideAffinityForScheduler(), but use the explicitly provided CPU
 * list instead of checking environment variables or auto-detecting the node CPU
 * list.
 *
 * Example:
 *
 *   PIC::Parallel::SetWideAffinityForScheduler("0-39");
 */
void SetWideAffinityForScheduler(const std::string& cpuSet);

/*
 * Assign each MPI rank on the same physical node a separate CPU range.
 *
 * This is not the same as wide affinity.
 *
 * Wide affinity:
 *   Every rank may use the same broad mask, and Linux decides where each
 *   std::thread worker runs.
 *
 * Local-rank partitioned affinity:
 *   local rank 0 -> CPUs 0 ... threadsPerRank-1
 *   local rank 1 -> CPUs threadsPerRank ... 2*threadsPerRank-1
 *   local rank 2 -> CPUs 2*threadsPerRank ... 3*threadsPerRank-1
 *
 * Example:
 *
 *   PIC::Parallel::SetAffinityByLocalRank(8);
 *
 * gives, on each node:
 *
 *   local rank 0 -> CPUs 0-7
 *   local rank 1 -> CPUs 8-15
 *   local rank 2 -> CPUs 16-23
 *
 * Recommended call location:
 *   After MPI_Init() and before creating std::thread workers.
 *
 * Limitation:
 *   This assumes CPU IDs are contiguous from zero on each node. For unusual
 *   cpuset layouts, prefer SetWideAffinityForScheduler() with an explicit CPU
 *   set.
 */
void SetAffinityByLocalRank(int threadsPerRank);

}  // namespace Parallel
}  // namespace PIC

#endif  // _PIC_PARALLEL_AFFINITY_H_
