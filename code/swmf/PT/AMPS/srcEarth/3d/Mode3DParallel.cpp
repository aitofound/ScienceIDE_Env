//======================================================================================
// Mode3DParallel.cpp
//======================================================================================
//
// Shared parallel-backend selection for mesh-backed Mode3D backward products.
//
// Once the compact global field arrays have been assembled, Mode3D backward products
// operate on independent work items.  Density/flux uses observation locations; cutoff
// rigidity now flattens each location into independent trajectory tasks so directional
// maps and explicit rigidity lists can use all MPI ranks and shared-memory workers even
// when the run contains only one observation location.  The common backend-resolution
// logic here guarantees consistent DENSITY_PARALLEL / DENSITY_THREADS handling.
//======================================================================================

#include "Mode3DParallel.h"

#include "pic.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <cstdlib>
#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <thread>

namespace Earth {
namespace Mode3D {
namespace {

static int ParsePositiveIntEnv_(const char* name) {
  const char* v = std::getenv(name);
  if (v == nullptr || *v == '\0') return 0;

  try {
    const int n = std::stoi(std::string(v));
    return (n > 0) ? n : 0;
  }
  catch (...) {
    return 0;
  }
}

} // namespace

const char* MpiSchedulerName(MpiScheduler scheduler) {
  switch (scheduler) {
  case MpiScheduler::BLOCK_CYCLIC: return "BLOCK_CYCLIC";
  case MpiScheduler::STATIC:       return "STATIC";
  case MpiScheduler::DYNAMIC:      return "DYNAMIC";
  }
  return "UNKNOWN";
}

MpiScheduler ResolveMpiScheduler(const EarthUtil::AmpsParam& prm,
                                 const char* diagnosticContext) {
  // The user-facing keyword is deliberately generic: the same inter-rank scheduler
  // infrastructure applies to cutoff rigidity and density/flux, although the actual
  // work unit differs (flattened trajectory task for cutoff, location for density).
  // Keep environment fallback support for
  // quick tests on HPC systems where editing the input deck is inconvenient.
  std::string token = prm.mode3d.mpiScheduler;
  if (token.empty()) {
    // Gridless and Mode3D share the same scheduler resolver.  Keep the original
    // AMPS_MODE3D_* environment names and also accept explicit AMPS_GRIDLESS_*
    // aliases for clarity in gridless-only batch scripts.
    const char* env = std::getenv("AMPS_GRIDLESS_MPI_SCHEDULER");
    if (env != nullptr) token = env;
  }
  if (token.empty()) {
    const char* env = std::getenv("AMPS_MODE3D_MPI_SCHEDULER");
    if (env != nullptr) token = env;
  }

  token = EarthUtil::ToUpper(token);

  // DYNAMIC is the new default because it is the only scheduler that can rebalance
  // work between MPI ranks when trajectory cost is strongly nonuniform over a shell.
  // BLOCK_CYCLIC remains available for deterministic regression runs and for MPI
  // implementations where RMA atomics are known to be problematic.
  if (token.empty() || token == "DYNAMIC" || token == "DYN" || token == "QUEUE" ||
      token == "WORK_QUEUE" || token == "WORKQUEUE") {
    return MpiScheduler::DYNAMIC;
  }

  if (token == "BLOCK_CYCLIC" || token == "BLOCKCYCLIC" || token == "CYCLIC" ||
      token == "ROUND_ROBIN" || token == "ROUNDROBIN") {
    return MpiScheduler::BLOCK_CYCLIC;
  }

  if (token == "STATIC" || token == "CONTIGUOUS" || token == "BLOCK" ||
      token == "SLAB") {
    return MpiScheduler::STATIC;
  }

  std::ostringstream msg;
  msg << diagnosticContext
      << ": unknown MODE3D_MPI_SCHEDULER/GRIDLESS_MPI_SCHEDULER/BACKTRACK_MPI_SCHEDULER value '"
      << token << "'. Valid values: DYNAMIC, BLOCK_CYCLIC, STATIC.";
  exit(__LINE__,__FILE__,msg.str().c_str());
  return MpiScheduler::DYNAMIC;
}

long long ResolveMpiDynamicChunk(const EarthUtil::AmpsParam& prm,
                                 int workerCount,
                                 long long nGlobalWorkItems) {
  long long chunk = static_cast<long long>(prm.mode3d.mpiDynamicChunk);

  if (chunk <= 0) {
    const char* env = std::getenv("AMPS_GRIDLESS_MPI_DYNAMIC_CHUNK");
    if (env != nullptr && *env != '\0') {
      try { chunk = std::stoll(std::string(env)); }
      catch (...) { chunk = 0; }
    }
  }
  if (chunk <= 0) {
    const char* env = std::getenv("AMPS_MODE3D_MPI_DYNAMIC_CHUNK");
    if (env != nullptr && *env != '\0') {
      try { chunk = std::stoll(std::string(env)); }
      catch (...) { chunk = 0; }
    }
  }

  if (chunk <= 0) {
    // Automatic chunk-size heuristic:
    //   * at least one work item per worker so all direct threads can participate;
    //   * normally four work items per worker to amortize MPI_Fetch_and_op overhead;
    //   * never larger than the whole job.
    // This is intentionally conservative. Users with very expensive/variable traces
    // can reduce MODE3D_MPI_DYNAMIC_CHUNK/GRIDLESS_MPI_DYNAMIC_CHUNK; users with
    // tiny/cheap work items can increase it to reduce scheduler traffic.
    const long long workers = static_cast<long long>(std::max(1,workerCount));
    chunk = std::max(1LL,4LL*workers);
  }

  if (nGlobalWorkItems > 0) chunk = std::min(chunk,nGlobalWorkItems);
  return std::max(1LL,chunk);
}

DynamicMpiLocationScheduler::DynamicMpiLocationScheduler(
    MPI_Comm comm,
    long long nGlobalLocations,
    long long chunkSize,
    const char* diagnosticContext)
  : comm_(comm),
    nGlobalLocations_(nGlobalLocations),
    chunkSize_(std::max(1LL,chunkSize)),
    exposedCounter_(0) {

  if (comm_ == MPI_COMM_NULL) {
    exit(__LINE__,__FILE__,"DynamicMpiLocationScheduler: invalid MPI communicator");
  }

  MPI_Comm_rank(comm_,&rank_);

  if (nGlobalLocations_ < 0) {
    std::ostringstream msg;
    msg << diagnosticContext << ": negative number of global locations in dynamic MPI scheduler.";
    exit(__LINE__,__FILE__,msg.str().c_str());
  }

  // All ranks must collectively create the RMA window.  Rank 0 exposes one 64-bit
  // counter.  Non-root ranks expose zero bytes; they still participate in the same
  // window and can atomically update rank 0's counter.
  void* base = (rank_ == 0) ? static_cast<void*>(&exposedCounter_) : nullptr;
  MPI_Aint nBytes = (rank_ == 0) ? static_cast<MPI_Aint>(sizeof(long long)) : 0;

  const int ierr = MPI_Win_create(base,
                                  nBytes,
                                  static_cast<int>(sizeof(long long)),
                                  MPI_INFO_NULL,
                                  comm_,
                                  &win_);
  if (ierr != MPI_SUCCESS) {
    std::ostringstream msg;
    msg << diagnosticContext << ": MPI_Win_create failed in dynamic Mode3D scheduler.";
    exit(__LINE__,__FILE__,msg.str().c_str());
  }

  // The window creation is collective, but an explicit barrier makes the ordering
  // self-documenting: no rank can fetch from the counter before rank 0 has initialized
  // and exposed it.  This is a setup-only synchronization, not inside the work loop.
  MPI_Barrier(comm_);
}

DynamicMpiLocationScheduler::~DynamicMpiLocationScheduler() {
  if (win_ != MPI_WIN_NULL) {
    MPI_Win_free(&win_);
    win_ = MPI_WIN_NULL;
  }
}

long long DynamicMpiLocationScheduler::FetchNextChunkStart() {
  long long start = 0;
  const long long increment = chunkSize_;

  // MPI_Fetch_and_op with MPI_SUM is an atomic fetch-add on rank 0's exposed counter.
  // The returned value is unique for each caller and is the first global location in
  // that caller's chunk.  Exclusive lock is conservative and widely supported.  This
  // function is intentionally called only by the rank/main thread, not by std::thread
  // workers, so the code does not require MPI_THREAD_MULTIPLE.
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE,0,0,win_);
  MPI_Fetch_and_op(&increment,
                   &start,
                   MPI_LONG_LONG,
                   0,
                   0,
                   MPI_SUM,
                   win_);
  MPI_Win_flush(0,win_);
  MPI_Win_unlock(0,win_);

  return start;
}

DynamicMpiProgressCounter::DynamicMpiProgressCounter(
    MPI_Comm comm,
    long long totalWork,
    const char* diagnosticContext)
  : comm_(comm),
    totalWork_(std::max(0LL,totalWork)),
    exposedCounter_(0) {

  if (comm_ == MPI_COMM_NULL) {
    exit(__LINE__,__FILE__,"DynamicMpiProgressCounter: invalid MPI communicator");
  }

  MPI_Comm_rank(comm_,&rank_);

  // All ranks collectively create the progress-counter window.  Rank 0 exposes one
  // 64-bit integer; other ranks expose zero bytes but can still atomically update
  // rank 0's counter.  This is exactly the same RMA pattern used by the dynamic work
  // scheduler, but the semantic meaning is different: this counter records completed
  // work, not assigned work.
  void* base = (rank_ == 0) ? const_cast<long long*>(&exposedCounter_) : nullptr;
  MPI_Aint nBytes = (rank_ == 0) ? static_cast<MPI_Aint>(sizeof(long long)) : 0;

  const int ierr = MPI_Win_create(base,
                                  nBytes,
                                  static_cast<int>(sizeof(long long)),
                                  MPI_INFO_NULL,
                                  comm_,
                                  &win_);
  if (ierr != MPI_SUCCESS) {
    std::ostringstream msg;
    msg << diagnosticContext << ": MPI_Win_create failed for progress counter.";
    exit(__LINE__,__FILE__,msg.str().c_str());
  }

  // Make construction ordering explicit.  This is outside the expensive trajectory
  // loop and therefore has negligible cost.
  MPI_Barrier(comm_);
}

DynamicMpiProgressCounter::~DynamicMpiProgressCounter() {
  if (win_ != MPI_WIN_NULL) {
    MPI_Win_free(&win_);
    win_ = MPI_WIN_NULL;
  }
}

void DynamicMpiProgressCounter::Add(long long delta) {
  if (delta <= 0 || totalWork_ <= 0) return;

  // Atomic add to rank 0's exposed counter.  The progress counter is updated only
  // by MPI rank/main threads.  Callers may publish a whole completed chunk or
  // periodically publish newly completed tasks while a threaded chunk is still
  // running.  Worker threads do not call MPI, so this remains compatible with MPI
  // implementations initialized below MPI_THREAD_MULTIPLE.
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE,0,0,win_);
  MPI_Accumulate(&delta,
                 1,
                 MPI_LONG_LONG,
                 0,
                 0,
                 1,
                 MPI_LONG_LONG,
                 MPI_SUM,
                 win_);
  MPI_Win_flush(0,win_);
  MPI_Win_unlock(0,win_);
}

long long DynamicMpiProgressCounter::Get() const {
  if (totalWork_ <= 0) return 0;

  long long value = 0;
  const long long zero = 0;

  // Fetch-and-add with zero is an atomic read that is safe even while other ranks are
  // concurrently adding completed chunks.  We use an RMA read instead of directly
  // looking at rank 0's local memory so the code does not depend on subtle cache
  // synchronization behavior of passive-target windows.
  MPI_Win_lock(MPI_LOCK_SHARED,0,0,win_);
  MPI_Fetch_and_op(const_cast<long long*>(&zero),
                   &value,
                   MPI_LONG_LONG,
                   0,
                   0,
                   MPI_SUM,
                   win_);
  MPI_Win_flush(0,win_);
  MPI_Win_unlock(0,win_);

  if (value < 0) value = 0;
  if (totalWork_ > 0 && value > totalWork_) value = totalWork_;
  return value;
}

const char* ParallelBackendName(ParallelBackend backend) {
  switch (backend) {
  case ParallelBackend::OPENMP:  return "OPENMP";
  case ParallelBackend::THREADS: return "THREADS";
  case ParallelBackend::SERIAL:  return "SERIAL";
  }
  return "UNKNOWN";
}

ParallelBackend ResolveParallelBackend(const EarthUtil::AmpsParam& prm,
                                       const char* diagnosticContext) {
  std::string token = prm.mode3d.densityParallelBackend;
  if (token.empty()) {
    const char* env = std::getenv("AMPS_MODE3D_DENSITY_PARALLEL");
    if (env != nullptr) token = env;
  }

  token = EarthUtil::ToUpper(token);
  if (token.empty() || token == "OPENMP" || token == "OMP") {
#ifdef _OPENMP
    return ParallelBackend::OPENMP;
#else
    return ParallelBackend::SERIAL;
#endif
  }

  if (token == "THREAD" || token == "THREADS" || token == "STD_THREAD" ||
      token == "STD_THREADS" || token == "PTHREAD" || token == "PTHREADS") {
    return ParallelBackend::THREADS;
  }

  if (token == "SERIAL" || token == "NONE" || token == "OFF") {
    return ParallelBackend::SERIAL;
  }

  std::ostringstream msg;
  msg << diagnosticContext
      << ": unknown DENSITY_PARALLEL/MODE3D_DENSITY_PARALLEL backend '"
      << token << "'. Valid values: OPENMP, THREADS, SERIAL.";
  exit(__LINE__,__FILE__,msg.str().c_str());
  return ParallelBackend::SERIAL;
}

int ResolveParallelThreadCount(const EarthUtil::AmpsParam& prm,
                               ParallelBackend backend) {
  int n = prm.mode3d.densityThreads;
  if (n <= 0) n = ParsePositiveIntEnv_("AMPS_MODE3D_DENSITY_THREADS");
  if (n <= 0) n = ParsePositiveIntEnv_("OMP_NUM_THREADS");

#ifdef _OPENMP
  if (n <= 0 && backend == ParallelBackend::OPENMP) n = omp_get_max_threads();
#endif

  if (n <= 0) {
    const unsigned hw = std::thread::hardware_concurrency();
    n = (hw > 0) ? static_cast<int>(hw) : 1;
  }

  return (n > 0) ? n : 1;
}

void ApplyWideAffinityForDirectThreadsOnce(ParallelBackend backend,
                                           int workerCount,
                                           const char* diagnosticContext) {
  (void)diagnosticContext;

  if (backend != ParallelBackend::THREADS || workerCount <= 1) return;

  static bool applied = false;
  if (applied) return;
  applied = true;

  PIC::Parallel::SetWideAffinityForScheduler();
}

} // namespace Mode3D
} // namespace Earth
