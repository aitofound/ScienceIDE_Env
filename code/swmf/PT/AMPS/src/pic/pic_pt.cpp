
//$Id$

/*
 * pic_pt.cpp
 *
 *  Created on: Oct 27, 2014
 *      Author: vtenishe
 */

/*=============================================================================
 * AMPS Particle-Tracker — pic_pt.cpp
 * =============================================================================
 *
 * OVERVIEW
 * --------
 * This file implements the "particle tracker" subsystem of AMPS (Adaptive Mesh
 * Particle Simulator). When a particle is designated for tracking, every step of
 * its trajectory is recorded — position, velocity, speed, species, and a set of
 * optional derived quantities (kinetic energy, time stamp, injection-face index,
 * particle weight over local time step, dust grain charge/size). The collected
 * points are eventually written out as per-species ASCII Tecplot zone files, one
 * ZONE per trajectory, that can be loaded and visualised directly in Tecplot or
 * any compatible post-processor.
 *
 * The code runs in an MPI+OpenMP hybrid environment. Each MPI rank owns a slice
 * of the simulation mesh and therefore owns the particles that happen to reside
 * there at any given time. Particles can migrate between ranks during simulation
 * as they cross mesh-block boundaries; the trajectory data accumulated so far
 * migrates with them.
 *
 * =============================================================================
 * KEY DATA STRUCTURES (defined in pic.h, implemented/used here)
 * =============================================================================
 *
 * cParticleData  (stored inside each particle's raw data buffer, at the byte
 *   offset ParticleDataRecordOffset which is registered with the particle buffer
 *   at Init() time)
 *   - TrajectoryTrackingFlag  : bool  — true iff this particle is being tracked.
 *   - Trajectory              : cTrajectoryID — identity of the trajectory.
 *   - nSampledTrajectoryPoints: unsigned long int (64-bit) — the running total
 *       number of times RecordTrajectoryPoint has been called for this particle,
 *       i.e. the length K of the trajectory so far. This is the global count and
 *       migrates with the particle across MPI boundaries; it is the central
 *       quantity used by the thinning algorithm.
 *
 * cTrajectoryID
 *   - StartingThread: unsigned int — MPI rank on which the trajectory began.
 *   - id            : unsigned int — per-thread trajectory counter at birth.
 *   Together these two fields globally and uniquely identify every trajectory.
 *   The "global trajectory number" used in the output stage is
 *     id + TrajectoryCounterOffset[StartingThread].
 *
 * cTrajectoryPhysicalData
 *   Physical snapshot at one sampled point: x[3], v[3], Speed, spec, TimeStamp,
 *   ElectricCharge, ParticleSize, KineticEnergy, InjectionFaceNumber,
 *   ParticleWeightOverLocalTimeStepRatio. Optional fields are present in the
 *   struct unconditionally; they are populated only when the corresponding
 *   compile-time feature flag is ON.
 *
 * cTrajectoryDataRecord  (one record in the temporary data buffer)
 *   - data          : cTrajectoryPhysicalData — the physical snapshot.
 *   - Trajectory    : cTrajectoryID — which trajectory this point belongs to.
 *   - offset        : unsigned long int (64-bit) — the 0-based global index of
 *       this point along its trajectory (i.e. the value of K just BEFORE
 *       RecordTrajectoryPoint incremented it). Used at output time to map each
 *       stored record to a dense output ordinal.
 *   - ForcedEndpoint: unsigned int — set to 1 only when this record was written
 *       by FinilazeParticleRecord to capture the true trajectory endpoint that
 *       fell off the final dyadic grid; 0 in all other cases.
 *
 * cTrajectoryListRecord  (one record in the temporary list buffer)
 *   - Trajectory              : cTrajectoryID.
 *   - nSampledTrajectoryPoints: unsigned long int (64-bit) — the FINAL value of
 *       K after the trajectory ended. This is what lets the output stage know the
 *       total length of each trajectory, which is needed to compute the final
 *       thinning level.
 *
 * cTrajectoryData  (per-OpenMP-thread in-memory buffer for data records)
 *   - buffer         : cTrajectoryDataRecord[Size] — ring buffer (Size=5000).
 *   - CurrentPosition: unsigned long int — write head.
 *   - nfile          : unsigned long int — how many binary temp files have been
 *       flushed so far for this thread.
 *   flush() writes the current buffer contents to disk as a binary temp file at
 *   path ParticleTrackerTmp/amps.ParticleTracker.thread=T.out=F.TrajectoryData.pt,
 *   where T = threadOpenMP + nTotalThreadsOpenMP * MPI_rank (a globally unique
 *   integer) and F is the file counter.
 *
 * cTrajectoryList  (per-OpenMP-thread in-memory buffer for list records)
 *   Mirror of cTrajectoryData but for cTrajectoryListRecord. Flushed to
 *   ParticleTrackerTmp/amps.ParticleTracker.thread=T.out=F.TrajectoryList.pt.
 *
 * cLastTrajectoryPointCache  (anonymous-namespace, per-OpenMP-thread)
 *   - Valid     : bool.
 *   - Trajectory: cTrajectoryID — which trajectory the cached point belongs to.
 *   - offset    : unsigned long int — the 0-based index of the cached point.
 *   - data      : cTrajectoryPhysicalData — the physical snapshot.
 *   This cache always holds the most recently seen point for this thread's
 *   current particle. Its purpose is to allow FinilazeParticleRecord to
 *   force-write the true trajectory endpoint even when that point was skipped
 *   by the thinning filter (see below).
 *
 * cSelectedTrajectoryPointRecord  (anonymous-namespace)
 *   Used only by the MPI-distributed output path:
 *   - GlobalTrajectoryNumber: long int.
 *   - ordinal               : long int — dense output ordinal 0..M-1.
 *   - data                  : cTrajectoryPhysicalData.
 *
 * =============================================================================
 * SAMPLING-TIME DYADIC-GRID THINNING (TrajectorySamplingThinningMode == true)
 * =============================================================================
 *
 * MOTIVATION
 * ----------
 * A particle trajectory may have an arbitrarily large number of sampled points K
 * (proportional to the simulation time step count during which the particle is
 * alive), while the user only wants N << K output points in the final file (e.g.
 * N = 100 out of K = 1,000,000). Writing every point to disk costs O(K) I/O,
 * which can be prohibitive. The obvious fix — keep every (K/N)-th point — cannot
 * be applied during sampling because K is not known until the particle is deleted
 * or tracking stops.
 *
 * The thinning algorithm resolves this by constructing a sequence of nested grids
 * whose spacing doubles every time the trajectory outgrows the current grid. At
 * any moment, only the points that land on the current grid are written to disk.
 * The grids are always integer powers of two, so a point that was on an earlier
 * (finer) grid is automatically on every coarser grid that follows. The net effect
 * is that the total number of records ever written for one trajectory is bounded
 * by C+1 ≤ N+1, regardless of K, while the retained points are uniformly spaced.
 *
 * THE GRID
 * --------
 * Let:
 *   N = OutputTrajectoryPointNumber  (user request; set via TRAJECTORY_OUTPUT_POINTS)
 *   C = TrajectoryThinningCapacity(N)
 *     = largest power of two ≤ N   (e.g. N=100 → C=64; N=128 → C=128)
 *   K = nSampledTrajectoryPoints    (running count, includes the point being decided)
 *   L = TrajectoryThinningLevel(K, C)
 *     = smallest L ≥ 0 such that ceil(K / 2^L) ≤ C
 *   S = 2^L                         (current grid stride)
 *
 * A point at 0-based global offset 'o' (== K before the current call) is ON the
 * current grid iff  o % S == 0  (equivalently: the L lowest bits of o are zero).
 * All other points are dropped at the source — they are never written to any file,
 * so they cost nothing beyond a few arithmetic operations and the cache update.
 *
 * When K first exceeds C the level L increments by 1, doubling S. Every grid
 * point that was on the old grid (spacing S/2) that has an odd index in the old
 * numbering becomes off-grid, but since those records are already in the temp
 * files they cannot be retroactively removed. That is why the offline re-filter
 * step (output time) re-applies the FINAL grid with the FINAL L to select only
 * the records that survived all grid doublings. Points from intermediate grids
 * that were superseded are simply skipped by the ordinal function returning -1.
 *
 * INVARIANT: every point whose final ordinal (rawOffset / S_final) is in [0, G)
 * WAS committed to the temp files at sample time, because at sample time its
 * level was at most L_final (non-decreasing), so its stride was at most S_final,
 * and therefore o % S_final == 0 implies o % S_sampling == 0.
 *
 * ENDPOINT RECOVERY
 * -----------------
 * The final point of a trajectory has offset K-1. This almost never falls on the
 * final dyadic grid (it would have to satisfy (K-1) % S_final == 0, which happens
 * only when K is a multiple of S_final, an unlikely accident). The endpoint is
 * important — it records where and when the particle died — so a special mechanism
 * captures it.
 *
 * RecordTrajectoryPoint always overwrites the per-OpenMP-thread
 * LastTrajectoryPointCache with the current point's physical data, whether or not
 * that point is being committed to disk. The cache therefore always holds the very
 * last point seen by that thread for its current trajectory.
 *
 * FinilazeParticleRecord is called once, from DeleteParticle or
 * StopParticleTrajectoryTracking, exactly when the trajectory ends. It checks
 * whether (a) the cache is valid, (b) it belongs to this trajectory, (c) its
 * offset equals K-1, and (d) (K-1) % S_final != 0 (endpoint is off-grid). If all
 * four conditions hold, the cache entry is written as a new cTrajectoryDataRecord
 * with ForcedEndpoint=1. The cache is then invalidated.
 *
 * KEY PROPERTY: FinilazeParticleRecord is never called during MPI migration (only
 * at death/stop), so the cache is guaranteed to hold the true last point at the
 * moment FinilazeParticleRecord runs.
 *
 * OFFLINE RE-FILTER (output time)
 * --------------------------------
 * When CreateTrajectoryOutputFiles (or the MPI-distributed path) reads back the
 * temp files, each stored record is passed to GetThinnedTrajectoryPointOrdinal,
 * which re-applies the FINAL grid and returns either:
 *   - rawOffset / S_final      if rawOffset % S_final == 0   (on-grid point)
 *   - G                        if this is the forced endpoint and it is off-grid
 *   - -1                       otherwise   (intermediate-grid remnant; skip)
 *
 * where G = ceil(K / S_final) is the number of on-grid points. The returned
 * ordinal is the zero-based position in the output array; ordinals 0..G-1 are
 * the uniformly-spaced interior points and ordinal G (if present) is the
 * endpoint. Records with ordinal -1 are discarded without being placed in the
 * output buffer, so they leave no trace in the Tecplot file.
 *
 * THINNING OFF MODE (TrajectorySamplingThinningMode == false)
 * -----------------------------------------------------------
 * Every sampled point is committed to the temp files with no filtering. At output
 * time the exact-N selector GetSelectedTrajectoryPointOrdinal is applied:
 *   t_j = round( j * (K-1) / (M-1) ),  j = 0 … M-1,   M = min(N, K)
 * Each stored record with rawOffset == t_j returns ordinal j; all others return
 * -1. This gives exactly M uniformly-spaced points with ±1 jitter. Because K may
 * be very large, this mode can produce large temp files; use it only when exactly-N
 * output matters more than I/O cost.
 *
 * =============================================================================
 * EXECUTION FLOW
 * =============================================================================
 *
 *  ┌─────────────────────────────────────────────────────────────────────────┐
 *  │  SAMPLING PHASE  (runs continuously during the simulation)              │
 *  │                                                                         │
 *  │  Init()                                                                 │
 *  │    - Registers cParticleData in the particle buffer at byte offset      │
 *  │      ParticleDataRecordOffset.                                          │
 *  │    - Allocates per-OpenMP-thread TrajectoryDataTable, TrajectoryList-   │
 *  │      Table (ring buffers), and LastTrajectoryPointCache.                │
 *  │    - Creates/clears the ParticleTrackerTmp/ directory (rank 0 only).    │
 *  │                                                                         │
 *  │  RecordTrajectoryPoint(x, v, spec, ParticleData, nodeIn)               │
 *  │    Called by the mover at every integration step for each tracked       │
 *  │    particle. Responsibilities:                                          │
 *  │    1. Build a local cTrajectoryPhysicalData from x, v, and optional     │
 *  │       compile-time quantities (dust charge, guiding-center energy, ...) │
 *  │    2. Read offset = nSampledTrajectoryPoints (current K before this pt).│
 *  │    3. (Thinning ON) Compute L = TrajectoryThinningLevel(offset+1, C).   │
 *  │       If L>0 and (offset & (S-1)) != 0 → set CommitToFile = false.     │
 *  │    4. Always update LastTrajectoryPointCache[threadOpenMP].             │
 *  │    5. Always increment nSampledTrajectoryPoints (even if not committed).│
 *  │    6. If CommitToFile == false, return early (no disk write at all).    │
 *  │    7. If the in-memory buffer is full, flush() to disk.                 │
 *  │    8. Append the record (data, Trajectory, offset, ForcedEndpoint=0)   │
 *  │       to TrajectoryDataTable[threadOpenMP].                             │
 *  │                                                                         │
 *  │  FinilazeParticleRecord(ParticleData)                                  │
 *  │    Called once at trajectory end (DeleteParticle or StopTracking).      │
 *  │    1. (Thinning ON) If LastTrajectoryPointCache holds the true last     │
 *  │       point (offset==K-1) and it is off the final grid, force-write it  │
 *  │       with ForcedEndpoint=1. Then invalidate the cache.                 │
 *  │    2. Append a cTrajectoryListRecord (Trajectory, K) to the list buffer.│
 *  │    3. Reset TrajectoryTrackingFlag=false.                               │
 *  │                                                                         │
 *  │  OutputTrajectory(fname)                                                │
 *  │    Called periodically (typically at each output step) by the main loop.│
 *  │    1. Each rank scans its in-flight particles and writes a "temporary   │
 *  │       trajectory list" (for particles still alive): records their       │
 *  │       current K so the output stage has a complete K table.             │
 *  │    2. All threads flush their in-memory ring buffers to disk.           │
 *  │    3. Rank 0 gathers file-count and trajectory-count metadata via MPI   │
 *  │       and writes the master .TrajectoryDataSet.pt index file.           │
 *  │    4. Rank 0 calls CreateTrajectoryOutputFiles (serial output path).    │
 *  │                                                                         │
 *  └─────────────────────────────────────────────────────────────────────────┘
 *
 *  ┌─────────────────────────────────────────────────────────────────────────┐
 *  │  OUTPUT PHASE  (rank 0 only, serial path)                               │
 *  │                                                                         │
 *  │  CreateTrajectoryOutputFiles(fname, dir, TrajectoryPointBufferLength)   │
 *  │    Reads the index file, all list files (to build the K table), and    │
 *  │    then all data files (to recover and place the surviving records).    │
 *  │    Works in passes: batches of trajectories whose output points fit in  │
 *  │    a single flat TempTrajectoryBuffer[TrajectoryPointBufferLength].     │
 *  │                                                                         │
 *  │    For each stored cTrajectoryDataRecord:                               │
 *  │      ordinal = GetThinnedTrajectoryPointOrdinal(offset, ForcedEndpoint, │
 *  │                   K, N)   [thinning ON]                                 │
 *  │             or GetSelectedTrajectoryPointOrdinal(offset, K, N)          │
 *  │                                                   [thinning OFF]        │
 *  │      if ordinal < 0 → skip (off-grid remnant)                          │
 *  │      else place data at TempTrajectoryBuffer[base + ordinal]            │
 *  │                                                                         │
 *  │    After all data files are read, write the buffer to the per-species   │
 *  │    Tecplot file: one ZONE per trajectory, points in ordinal order.      │
 *  │                                                                         │
 *  └─────────────────────────────────────────────────────────────────────────┘
 *
 *  ┌─────────────────────────────────────────────────────────────────────────┐
 *  │  MPI-DISTRIBUTED OUTPUT PATH  (alternative to the serial path above)   │
 *  │                                                                         │
 *  │  ParallelPreselectTrajectoryPoints(fname, dir)   [all ranks]           │
 *  │    1. Rank 0 reads the list files to build the global K table and       │
 *  │       broadcasts it (MPI_Bcast). Every rank now knows K for every       │
 *  │       trajectory and can compute the same final grid independently.     │
 *  │    2. Each rank reads ONLY the temp data files it itself produced       │
 *  │       (thread indices [rank*nOMP, (rank+1)*nOMP)), applies the ordinal  │
 *  │       filter, and writes surviving records as cSelectedTrajectoryPoint- │
 *  │       Record entries to a compact per-rank binary file                  │
 *  │       (SelectedPoints.rank=R.pt). The file is prefixed with a magic     │
 *  │       word that encodes the format version and physical-record size.    │
 *  │    3. MPI_Barrier.                                                      │
 *  │                                                                         │
 *  │  AssembleTrajectoryOutputFromSelected(fname, dir)   [rank 0 only]      │
 *  │    1. Allocates a flat global output buffer sized by the sum of M_i     │
 *  │       over all trajectories i.                                          │
 *  │    2. Reads every rank's selected-points file, verifying the magic      │
 *  │       word, and places each record at buffer[BufOffset[traj] + ordinal].│
 *  │    3. Writes the per-species Tecplot files from the assembled buffer,   │
 *  │       exactly as the serial path does.                                  │
 *  │    4. MPI_Barrier.                                                      │
 *  │                                                                         │
 *  │  NOTE: this path is self-consistent and compilable but is NOT yet wired │
 *  │  into OutputTrajectory. It is provided for future integration and has   │
 *  │  not been exercised in a full MPI build. Wire it in and smoke-test on   │
 *  │  2+ ranks before relying on it in production.                           │
 *  └─────────────────────────────────────────────────────────────────────────┘
 *
 * =============================================================================
 * THREAD SAFETY AND MPI CORRECTNESS
 * =============================================================================
 *
 * OpenMP: every per-thread table (TrajectoryDataTable, TrajectoryListTable,
 * LastTrajectoryPointCache) is indexed by omp_get_thread_num() so threads never
 * share state during sampling. The flush() methods are called from within the
 * OpenMP parallel section and must not be invoked concurrently on the same index.
 *
 * MPI migration: when a particle crosses a mesh-block boundary, the entire
 * cParticleData record (including nSampledTrajectoryPoints) migrates with it.
 * The thinning grid is a PURE FUNCTION of (nSampledTrajectoryPoints, N); it
 * requires no communication and every rank computes the same grid for the same
 * trajectory. The global offset 'o' stored in each cTrajectoryDataRecord is
 * therefore meaningful across rank boundaries and can be used directly by the
 * output stage to assign ordinals without any rank-to-rank correction.
 *
 * =============================================================================
 * TEMP FILE LAYOUT
 * =============================================================================
 *
 * All temp files live in  <OutputDataFileDirectory>/ParticleTrackerTmp/ .
 *
 *   amps.ParticleTracker.thread=T.out=F.TrajectoryData.pt
 *     Binary: [uint64 count][count × cTrajectoryDataRecord]
 *     One or more files per global thread T, sequentially numbered F=0,1,…
 *
 *   amps.ParticleTracker.thread=T.out=F.TrajectoryList.pt
 *     Binary: [uint64 count][count × cTrajectoryListRecord]
 *     Written only at FinilazeParticleRecord; records completed trajectories.
 *
 *   amps.ParticleTracker.thread=T.TemporaryTrajectoryList.pt
 *     Binary: [uint64 count][count × cTrajectoryListRecord]
 *     Snapshot of in-flight trajectories (still-alive particles), written by
 *     OutputTrajectory. There is exactly one such file per MPI rank T.
 *
 *   <fname>.TrajectoryDataSet.pt
 *     Index file written by rank 0 at each OutputTrajectory call.
 *     Binary: [int nspec][int nthreads][int nMPIthreads]
 *             [nthreads × uint64 nListFiles]
 *             [nthreads × uint64 nDataFiles]
 *             [nthreads × uint64 nSampledTrajectories]
 *             [nspec × char[_MAX_STRING_LENGTH_PIC_] ChemSymbol]
 *
 *   amps.ParticleTracker.SelectedPoints.rank=R.pt  (MPI-distributed path only)
 *     Binary: [uint64 magic][uint64 count][count × cSelectedTrajectoryPointRecord]
 *     The magic word encodes 'SELP', format version 1, and sizeof(cTrajectory-
 *     PhysicalData) so that stale/incompatible files are detected at read time.
 *
 * =============================================================================
 * CONFIGURATION (input file keywords, parsed in pic_parser.cpp)
 * =============================================================================
 *
 *   TRAJECTORY_OUTPUT_POINTS = N
 *     (aliases: PARTICLETRACKER_OUTPUT_POINTS, PARTICLETRACKEROUTPUTPOINTS)
 *     Sets OutputTrajectoryPointNumber = N (must be ≥ 2).
 *     Default: nMaxSavedSignleTrajectoryPoints (= 1000).
 *
 *   TRAJECTORY_SAMPLING_THINNING = ON|OFF
 *     (alias: PARTICLETRACKER_SAMPLING_THINNING)
 *     Enables (ON, default) or disables (OFF) the dyadic-grid thinning.
 *     When OFF, every point is written to disk and exact-N selection is applied
 *     only at output time.
 *
 * =============================================================================
 * FUNCTION REFERENCE
 * =============================================================================
 *
 *  Public (declared in pic.h → PIC::ParticleTracker namespace):
 *   Init()                              Allocate tables, register particle data.
 *   InitParticleID(ParticleData)        Zero-initialise the cParticleData record.
 *   UpdateTrajectoryCounter()           MPI-reduce trajectory counts per species.
 *   RecordTrajectoryPoint(...)          Hot path: sample one point, maybe commit.
 *   FinilazeParticleRecord(ParticleData)Force endpoint, close list record.
 *   OutputTrajectory(fname)             Flush + gather + (rank 0) write output.
 *   TrajectoryThinningCapacity(N)       Largest power-of-two ≤ N.
 *   TrajectoryThinningLevel(count, C)   Smallest L with ceil(count/2^L) ≤ C.
 *   SetOutputTrajectoryPointNumber(n)   Set N (≥2), called from parser.
 *   ParallelPreselectTrajectoryPoints   MPI-distributed pre-selection phase.
 *   AssembleTrajectoryOutputFromSelected MPI-distributed assembly phase (rank 0).
 *
 *  Private (file-local anonymous namespace):
 *   GetThinnedTrajectoryOutputPointNumber(K, N)   M for thinning-ON mode.
 *   GetThinnedTrajectoryPointOrdinal(o, fe, K, N) Ordinal or -1, thinning ON.
 *   GetTrajectoryOutputPointNumber(K, N)           M for thinning-OFF mode.
 *   GetSelectedTrajectoryPointOrdinal(o, K, N)    Ordinal or -1, thinning OFF.
 *   SameTrajectory(a, b)                           Compare two cTrajectoryID.
 *   SelectedPointsFileName(str, fname, dir, rank)  Build selected-pts filename.
 *   BuildGlobalTrajectoryLengthTable(...)          Read all list files → K table.
 *   struct cLastTrajectoryPointCache               Per-thread endpoint cache.
 *   struct cSelectedTrajectoryPointRecord          Compact record for MPI path.
 *
 * =============================================================================
 * DESIGN Q&A
 * =============================================================================
 *
 * Q: What are cTrajectoryData and cTrajectoryList?
 *
 * Both are write-once ring-buffer managers — thin wrappers around a fixed-size
 * in-memory array that automatically spill to disk when full.
 *
 * cTrajectoryData holds cTrajectoryDataRecord entries: one record per committed
 * sampled point (physical snapshot + trajectory ID + offset + ForcedEndpoint
 * flag). RecordTrajectoryPoint appends to it on every on-grid sample. When the
 * buffer reaches Size entries, flush() writes the whole buffer as a binary chunk
 * to a new numbered temp file (amps.ParticleTracker.thread=T.out=F.TrajectoryData.pt),
 * increments the file counter, and resets the write head to zero. One instance
 * exists per OpenMP thread in TrajectoryDataTable[]; threads write exclusively to
 * their own slot with no locking.
 *
 * cTrajectoryList holds cTrajectoryListRecord entries: one record per completed
 * trajectory (ID + final total point count K). FinilazeParticleRecord appends to
 * it once at trajectory end. It uses the same flush-and-number scheme, producing
 * .TrajectoryList.pt files. The output stage reads these list files first to build
 * the global K table, which is needed to compute the final thinning level before
 * any data records can be assigned output ordinals.
 *
 * ----------------------------------------------------------------------------
 *
 * Q: With both ring buffers capped at Size=5000, what are the limits on
 *    (a) total trajectory points, (b) total trajectories, (c) concurrent
 *    trajectories?
 *
 * (a) Total trajectory points: unlimited.  The ring flushes to numbered files on
 *     disk as many times as needed; the file counter nfile is uint64.  Total
 *     points are bounded only by disk space and the 64-bit offset counter.
 *
 * (b) Total trajectories (across the whole run): unlimited, for the same reason.
 *     The list buffer flushes identically.
 *
 * (c) Concurrent trajectories (simultaneously in-flight): unlimited from a
 *     storage standpoint — points from many live trajectories interleave freely
 *     in the data buffer, each record carrying its own cTrajectoryID.  However,
 *     there is a functional limit on endpoint correctness: LastTrajectoryPointCache
 *     has exactly ONE slot per OpenMP thread.  RecordTrajectoryPoint overwrites it
 *     on every call regardless of which trajectory the point belongs to, so only
 *     the trajectory most recently touched by its thread can have its forced
 *     endpoint correctly captured at finalization.  For a simulation with T OpenMP
 *     threads, at most T trajectories simultaneously have reliable endpoint
 *     recovery; all others silently lose their final point if it falls off the
 *     final dyadic grid.
 *
 * ----------------------------------------------------------------------------
 *
 * Q: With thinning ON, how many points of a trajectory are actually written to
 *    the temp files?
 *
 * Three counts are distinct:
 *
 * 1. Calls to RecordTrajectoryPoint: K (every integration step).
 *
 * 2. Records committed to the temp files: approximately C + (C/2)·log₂(K/C)
 *    for K > C, which grows logarithmically with K, not linearly.  The reason
 *    is that sampling proceeds in phases keyed on the running level L at the
 *    time of each call (not the final L).  Phase 0 (offsets 0…C-1, L=0) commits
 *    all C points.  Every subsequent phase covers twice as many offsets but with
 *    twice the stride, so each contributes exactly C/2 committed records.  After
 *    p = ⌊log₂(K/C)⌋ phases the total committed is C·(1 + p/2).  Example:
 *    N=100 → C=64; K=10^6 → ~480 records on disk instead of 10^6.
 *
 * 3. Points surviving the offline re-filter to the final output: C/2 < M ≤ C+1,
 *    independent of K.  The output stage re-applies the FINAL grid (stride
 *    2^L_final) and discards intermediate-phase records that are no longer on it.
 *
 *    The committed count C·(1 + ½ log₂(K/C)) grows so slowly that for any
 *    practical K (and C=64) it stays well below 5000, meaning the ring buffer
 *    virtually never needs to flush mid-trajectory — only at OutputTrajectory
 *    call boundaries.
 *
 * ----------------------------------------------------------------------------
 *
 * Q: What is thinning for, how is it controlled, and what happens in each mode?
 *
 * PURPOSE: a particle may live for millions of steps, producing millions of
 * sampled points.  Only ~N are useful in the output file.  Without thinning
 * all K points are written to disk (O(K) I/O) and discarded at output time,
 * which is wasteful.  Thinning filters at sample time so the temp files stay
 * small regardless of K.
 *
 * CONTROL (input file #MAIN block):
 *   TRAJECTORY_SAMPLING_THINNING = ON | OFF   (default ON)
 *   TRAJECTORY_OUTPUT_POINTS     = N          (default 1000; must be >= 2)
 * Aliases: PARTICLETRACKER_SAMPLING_THINNING, PARTICLETRACKER_OUTPUT_POINTS,
 *          PARTICLETRACKEROUTPUTPOINTS.
 * Programmatic: PIC::ParticleTracker::TrajectorySamplingThinningMode = true/false.
 *
 * THINNING OFF: every sampled point is committed to the temp files (O(K) I/O).
 * At output time the exact-N selector picks M = min(N,K) points at positions
 * round(j·(K−1)/(M−1)) for j=0…M−1, giving exactly N evenly-spaced points
 * with at most ±1 spacing jitter.  Use this mode when exactly-N output matters
 * more than I/O cost.
 *
 * THINNING ON (default): only on-grid points are committed during sampling
 * (O(N log(K/N)) I/O).  The dyadic grid starts at stride 1 and doubles every
 * time the trajectory outgrows the current capacity C.  At output time the final
 * grid is re-applied; the true trajectory endpoint is recovered via the per-
 * thread LastTrajectoryPointCache and written with ForcedEndpoint=1 if it fell
 * off the grid.  Output spacing is exactly uniform at 2^L_final throughout the
 * interior (one shorter stub to the endpoint).  Output count M satisfies
 * C/2 < M ≤ C+1 ≈ N regardless of K.
 *
 *   Mode        Temp-file records   Output points   I/O cost
 *   OFF         K                   min(N,K)        O(K)
 *   ON          ~N·log₂(K/N)        C/2 < M ≤ C+1  O(N log(K/N))
 *
 * =============================================================================
 */

//contains routines that are used when AMPS is used in a 'particle tracker' mode

#include "pic.h"

// Byte offset of the cParticleData record inside each particle's raw data buffer.
// Assigned by PIC::ParticleBuffer::RequestDataStorage() during Init() and used in
// every function that needs to reach a particle's trajectory tracking state.
// The value -1 means Init() has not been called yet.
long int PIC::ParticleTracker::ParticleDataRecordOffset=-1;

// Number of cTrajectoryDataRecord entries that fit in one thread's in-memory ring
// buffer before it is flushed to a binary temp file on disk.  Keeping this small
// (≤5000) limits peak memory use per thread while still amortising file-open
// overhead over many records; the flush() path handles arbitrarily long runs by
// writing sequentially numbered output files (out=0, out=1, …).
unsigned long int PIC::ParticleTracker::cTrajectoryData::Size=5000;

// Same capacity limit for the trajectory-list ring buffer.  List records are much
// smaller than data records (just an ID + a point count), so 5000 entries covers
// thousands of completed trajectories before a flush is needed.
unsigned long int PIC::ParticleTracker::cTrajectoryList::Size=5000;

// Maximum number of trajectories to sample per species across all ranks.  When the
// compile-time flag _PIC_PARTICLE_TRACKER__STOP_RECORDING_..._MODE_ is ON, sampling
// for species s is halted once totalSampledTrajectoryNumber[s] reaches this value.
// -1 means "no limit enforced" (the flag overrides it anyway when it is OFF).
int PIC::ParticleTracker::maxSampledTrajectoryNumber=-1;

// Per-species, per-OpenMP-thread trajectory count: threadSampledTrajectoryNumber[s][t]
// is the number of trajectories that OpenMP thread t on this MPI rank has started for
// species s.  Layout is [nTotalSpecies][nTotalThreadsOpenMP], allocated as a single
// contiguous block in Init() so the inner dimension can be walked cheaply.
int **PIC::ParticleTracker::threadSampledTrajectoryNumber=NULL;

// Global (all-rank, all-thread) trajectory count per species, populated by
// UpdateTrajectoryCounter() via MPI_Allreduce.  Used to enforce maxSampledTrajectoryNumber
// and to report summary statistics at output time.
int *PIC::ParticleTracker::totalSampledTrajectoryNumber=NULL;

// Per-OpenMP-thread counter: total number of trajectories that have been opened
// (TrajectoryTrackingFlag set to true) on this thread since Init().  Contributes to
// cTrajectoryID::id — each new trajectory on thread t gets the current value of
// SampledTrajectoryCounter[t] as its local identity, which together with the MPI rank
// forms a globally unique ID.
unsigned long int *PIC::ParticleTracker::SampledTrajectoryCounter=NULL;

// Per-OpenMP-thread counter: total number of cTrajectoryDataRecord entries written to
// the temp data files (including the forced-endpoint records written at finalize).
// Gathered by OutputTrajectory via MPI_Reduce to report the global point count.
unsigned long int *PIC::ParticleTracker::SampledTrajectoryPointCounter=NULL;

// Legacy upper bound on points kept per trajectory in the output file, used as the
// default value for OutputTrajectoryPointNumber.  The thinning algorithm supersedes
// the old Step-based decimation but this constant remains as a convenient default N.
int PIC::ParticleTracker::nMaxSavedSignleTrajectoryPoints=1000;

// Per-species flag: AllowRecordingParticleTrajectoryPoints[s] is false once the
// global trajectory count for species s has reached maxSampledTrajectoryNumber and
// the compile-time stop-at-max mode is ON.  RecordTrajectoryPoint checks this flag
// at entry and returns immediately when it is false, so no further points are
// written for that species.
bool PIC::ParticleTracker::AllowRecordingParticleTrajectoryPoints[PIC::nTotalSpecies];

// Array of per-OpenMP-thread in-memory ring buffers for cTrajectoryDataRecord.
// Indexed by omp_get_thread_num(); each thread writes exclusively to its own slot,
// providing lock-free sampling.  Allocated in Init(); flushed to disk by flush()
// when the buffer is full or at each OutputTrajectory call.
PIC::ParticleTracker::cTrajectoryData *PIC::ParticleTracker::TrajectoryDataTable=NULL;

// Array of per-OpenMP-thread in-memory ring buffers for cTrajectoryListRecord.
// A list record is appended once per trajectory at finalization (FinilazeParticle-
// Record) and records the trajectory's final total point count K.  The output stage
// reads these to build the global K table before placing data records into the
// output buffer.
PIC::ParticleTracker::cTrajectoryList *PIC::ParticleTracker::TrajectoryListTable=NULL;

// ---- sampling-time dyadic-grid reservoir thinning ---------------------------

// Master switch for the thinning algorithm.  true (default): only points on the
// current dyadic grid are written during sampling, bounding I/O to O(N) regardless
// of trajectory length K.  false: every sampled point is written; exact-N selection
// is applied offline at output time (O(K) I/O, exactly N output points).
// Controlled by the input keyword TRAJECTORY_SAMPLING_THINNING = ON|OFF.
bool PIC::ParticleTracker::TrajectorySamplingThinningMode=true;

// Requested number of output points per trajectory (N).  The thinning capacity
// C = TrajectoryThinningCapacity(N) (largest power-of-two ≤ N) is derived from
// this value; the actual output count M satisfies C/2 < M ≤ C+1 ≤ N+1.
// Set via the input keyword TRAJECTORY_OUTPUT_POINTS = N (must be ≥ 2).
// Default matches nMaxSavedSignleTrajectoryPoints (1000).
int  PIC::ParticleTracker::OutputTrajectoryPointNumber=PIC::ParticleTracker::nMaxSavedSignleTrajectoryPoints;

//set the requested number of output points per trajectory (N); N must be >= 2
void PIC::ParticleTracker::SetOutputTrajectoryPointNumber(int nRequestedOutputPoints) {
  if (nRequestedOutputPoints<2) exit(__LINE__,__FILE__,"Error: the requested number of trajectory output points must be >= 2");
  OutputTrajectoryPointNumber=nRequestedOutputPoints;
}

//the largest power of two not exceeding N (the reservoir capacity C)
int PIC::ParticleTracker::TrajectoryThinningCapacity(int nRequestedOutputPoints) {
  if (nRequestedOutputPoints<1) return 1;
  int C=1;
  while ((C<<1)<=nRequestedOutputPoints) C<<=1;
  return C;
}

//the smallest L such that ceil(count/2^L) <= Capacity (the compression level)
int PIC::ParticleTracker::TrajectoryThinningLevel(unsigned long int count,int Capacity) {
  if (count<=1) return 0;
  if (Capacity<1) Capacity=1;
  int L=0;
  while ((((count-1)>>L)+1)>(unsigned long int)Capacity) L++;
  return L;
}

//----------------------------------------------------------------------------
// file-local helpers used by the thinning sampling + offline reassembly path
//----------------------------------------------------------------------------
namespace {
  using PIC::ParticleTracker::cTrajectoryID;
  using PIC::ParticleTracker::cTrajectoryPhysicalData;

  //per-OpenMP-thread "last point" cache, used to recover the true endpoint of a
  //trajectory that fell off the final dyadic grid (force-written at finalize)
  struct cLastTrajectoryPointCache {
    bool Valid;
    cTrajectoryID Trajectory;
    unsigned long int offset;
    cTrajectoryPhysicalData data;

    cLastTrajectoryPointCache() : Valid(false), offset(0) {}
  };

  cLastTrajectoryPointCache *LastTrajectoryPointCache=NULL; //allocated per OpenMP thread in Init()

  inline bool SameTrajectory(const cTrajectoryID& a,const cTrajectoryID& b) {
    return (a.StartingThread==b.StartingThread)&&(a.id==b.id);
  }

  //==== thinning ON offline helpers (re-apply the final dyadic grid) ====

  //number of output points after thinning: grid count + (forced endpoint if off-grid)
  unsigned long int GetThinnedTrajectoryOutputPointNumber(unsigned long int K,int N) {
    if (K==0) return 0;
    int C=PIC::ParticleTracker::TrajectoryThinningCapacity(N);
    int L=PIC::ParticleTracker::TrajectoryThinningLevel(K,C);
    unsigned long int S=((unsigned long int)1)<<L;
    unsigned long int G=((K-1)/S)+1;
    bool endpointOnGrid=(((K-1)%S)==0);
    return G+(endpointOnGrid?0:1);
  }

  //dense output ordinal for a stored record, or -1 if it is not on the final grid
  //(and is not the forced endpoint)
  long int GetThinnedTrajectoryPointOrdinal(unsigned long int rawOffset,unsigned int forcedEndpoint,unsigned long int K,int N) {
    if (K==0) return -1;
    int C=PIC::ParticleTracker::TrajectoryThinningCapacity(N);
    int L=PIC::ParticleTracker::TrajectoryThinningLevel(K,C);
    unsigned long int S=((unsigned long int)1)<<L;
    unsigned long int G=((K-1)/S)+1;
    bool endpointOnGrid=(((K-1)%S)==0);

    if ((rawOffset&(S-1))==0) return (long int)(rawOffset/S);
    else if (rawOffset==K-1 && forcedEndpoint && !endpointOnGrid) return (long int)G;
    return -1;
  }

  //==== thinning OFF offline helpers (exact-N selection round(j*(K-1)/(M-1))) ====

  unsigned long int GetTrajectoryOutputPointNumber(unsigned long int K,int N) {
    if ((unsigned long int)N<K) return (unsigned long int)N;
    return K;
  }

  long int GetSelectedTrajectoryPointOrdinal(unsigned long int rawOffset,unsigned long int K,int N) {
    if (K==0) return -1;
    unsigned long int M=GetTrajectoryOutputPointNumber(K,N);
    if (M<=1) return (rawOffset==0)?0:-1;
    if (M==K) return (long int)rawOffset;

    long double scale=(long double)(K-1)/(long double)(M-1);
    long int guess=(long int)llroundl((long double)rawOffset/scale);

    for (long int j=guess-1;j<=guess+1;j++) {
      if (j<0||(unsigned long int)j>=M) continue;
      unsigned long int t=(unsigned long int)llroundl((long double)j*scale);
      if (t==rawOffset) return j;
    }
    return -1;
  }

  //==== compact "selected-points" file format used by the MPI-distributed output path ====
  //Each rank pre-selects the surviving points from ONLY its own temp files and writes them
  //to a small per-rank file. Rank 0 then assembles the ASCII Tecplot from these files,
  //reassembling split trajectories by (global trajectory id, dense output ordinal).
  //
  //The leading magic word encodes the physical-record size so stale/incompatible files
  //(e.g. produced by a build with a different cTrajectoryPhysicalData layout) are rejected.
  const unsigned long int SelectedPointsFileMagic =
      0x53454C50UL                                        //'SELP'
      | (((unsigned long int)1)<<32)                      //format version 1
      | (((unsigned long int)sizeof(cTrajectoryPhysicalData))<<40); //physical record size

  struct cSelectedTrajectoryPointRecord {
    long int GlobalTrajectoryNumber; //global trajectory id (== id + offset of the starting thread)
    long int ordinal;                //dense output ordinal 0..M-1 for this trajectory
    cTrajectoryPhysicalData data;
  };

  //compose the per-rank selected-points file name
  inline void SelectedPointsFileName(char *str,const char *fname,const char *Dir,int rank) {
    sprintf(str,"%s/ParticleTrackerTmp/amps.ParticleTracker.SelectedPoints.rank=%i.pt",Dir,rank);
    (void)fname;
  }
}


//init the particle tracker
void PIC::ParticleTracker::Init() {
  //reserve memory in the particle data
  PIC::ParticleBuffer::RequestDataStorage(ParticleDataRecordOffset,sizeof(cParticleData));

  //init TrajectoryListTable and TrajectoryDataTable
  TrajectoryListTable=new cTrajectoryList[PIC::nTotalThreadsOpenMP];
  TrajectoryDataTable=new cTrajectoryData[PIC::nTotalThreadsOpenMP];

  //init the data buffers
  for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) {
    TrajectoryDataTable[thread].buffer=new cTrajectoryDataRecord[cTrajectoryData::Size];
    TrajectoryListTable[thread].buffer=new cTrajectoryListRecord[cTrajectoryList::Size];
  }

  //init the trajectory counter
  threadSampledTrajectoryNumber=new int* [PIC::nTotalSpecies];
  totalSampledTrajectoryNumber=new int [PIC::nTotalSpecies];
  threadSampledTrajectoryNumber[0]=new int [PIC::nTotalSpecies*PIC::nTotalThreadsOpenMP];
  SampledTrajectoryCounter=new unsigned long int [PIC::nTotalThreadsOpenMP];
  SampledTrajectoryPointCounter=new unsigned long int [PIC::nTotalThreadsOpenMP];

  for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) SampledTrajectoryCounter[thread]=0,SampledTrajectoryPointCounter[thread]=0;

  //allocate the per-OpenMP-thread "last point" cache used by the thinning endpoint recovery
  LastTrajectoryPointCache=new cLastTrajectoryPointCache[PIC::nTotalThreadsOpenMP];
  for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) LastTrajectoryPointCache[thread].Valid=false;

  for (int s=1;s<PIC::nTotalSpecies;s++) {
    threadSampledTrajectoryNumber[s]=threadSampledTrajectoryNumber[s-1]+PIC::nTotalThreadsOpenMP;
  }

  //init the trajectory counter
  for (int s=0;s<PIC::nTotalSpecies;s++) {
    AllowRecordingParticleTrajectoryPoints[s]=true;
    totalSampledTrajectoryNumber[s]=0;

    for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) threadSampledTrajectoryNumber[s][thread]=0;
  }

  //remove old and create new directory for temporary files
  if (PIC::ThisThread==0) {
    char cmd[_MAX_STRING_LENGTH_PIC_];
    sprintf(cmd,"rm -rf %s/ParticleTrackerTmp",PIC::OutputDataFileDirectory);
    if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed"); 

    sprintf(cmd,"mkdir -p %s/ParticleTrackerTmp",PIC::OutputDataFileDirectory);
    if (system(cmd)==-1) exit(__LINE__,__FILE__,"Error: system failed"); 
  }
}

//update the total number of samples trajectories
void PIC::ParticleTracker::UpdateTrajectoryCounter() {

  //calculate the total number of the trajectories initiated by this MPI thread (summ all OpenMP threads)
  int thread,s,tmpTrajectoryCounter[PIC::nTotalSpecies];

  for (s=0;s<PIC::nTotalSpecies;s++) {
    tmpTrajectoryCounter[s]=0;

    for (thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) tmpTrajectoryCounter[s]+=threadSampledTrajectoryNumber[s][thread];
  }

  //collect the statictic data from all MPI threads
  MPI_Allreduce(tmpTrajectoryCounter,totalSampledTrajectoryNumber,PIC::nTotalSpecies,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

  if (_PIC_PARTICLE_TRACKER__STOP_RECORDING_TRAJECTORY_POINTS_WHEN_TRAJECTORY_NUMBER_REACHES_MAXIMUM_VALUE__MODE_==_PIC_MODE_ON_) {
    for (int spec=0;spec<PIC::nTotalSpecies;spec++) {
      if (maxSampledTrajectoryNumber<totalSampledTrajectoryNumber[spec]) AllowRecordingParticleTrajectoryPoints[spec]=false;
    }
  }
}

//init the particle trajecotry record
void PIC::ParticleTracker::InitParticleID(void *ParticleData) {
  cParticleData *DataRecord=(cParticleData*)(ParticleDataRecordOffset+((PIC::ParticleBuffer::byte*)ParticleData));

  DataRecord->TrajectoryTrackingFlag=false;
  DataRecord->nSampledTrajectoryPoints=0;

  DataRecord->Trajectory.StartingThread=0;
  DataRecord->Trajectory.id=0;
}

void PIC::ParticleTracker::cTrajectoryData::flush() {
  FILE *fout;
  char fname[_MAX_STRING_LENGTH_PIC_];

  if (CurrentPosition!=0) {
    int threadOpenMP=0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    threadOpenMP=omp_get_thread_num();
#endif

    sprintf(fname,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.out=%ld.TrajectoryData.pt",PIC::OutputDataFileDirectory,threadOpenMP+PIC::nTotalThreadsOpenMP*PIC::ThisThread,nfile);
    fout=fopen(fname,"w");

    if (fout==NULL) {
      char message[_MAX_STRING_LENGTH_PIC_];
      sprintf(message,"Error: cannot open file %s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.out=%ld.TrajectoryData.pt for writting of the temporary trajectory data",PIC::OutputDataFileDirectory,threadOpenMP+PIC::nTotalThreadsOpenMP*PIC::ThisThread,nfile);

      exit(__LINE__,__FILE__,message);
    }

    fwrite(&CurrentPosition,sizeof(unsigned long int),1,fout);
    fwrite(buffer,sizeof(cTrajectoryDataRecord),CurrentPosition,fout);
    fclose(fout);

    CurrentPosition=0;
    ++nfile;
  }
}

void PIC::ParticleTracker::cTrajectoryList::flush() {
  FILE *fout;
  char fname[_MAX_STRING_LENGTH_PIC_];

  if (CurrentPosition!=0) {
    int threadOpenMP=0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    threadOpenMP=omp_get_thread_num();
#endif

    sprintf(fname,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.out=%ld.TrajectoryList.pt",PIC::OutputDataFileDirectory,threadOpenMP+PIC::nTotalThreadsOpenMP*PIC::ThisThread,nfile);
    fout=fopen(fname,"w");

    fwrite(&CurrentPosition,sizeof(unsigned long int),1,fout);
    fwrite(buffer,sizeof(cTrajectoryListRecord),CurrentPosition,fout);
    fclose(fout);

    CurrentPosition=0;
    ++nfile;
  }
}

void PIC::ParticleTracker::RecordTrajectoryPoint(double *x,double *v,int spec,void *ParticleData,void *nodeIn) {
  cParticleData *ParticleTrajectoryRecord;

  //OpenMP thread
  int threadOpenMP=0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  threadOpenMP=omp_get_thread_num();
#endif

  //the particle trajectory will be recorded if allowed
  if (_PIC_PARTICLE_TRACKER__STOP_RECORDING_TRAJECTORY_POINTS_WHEN_TRAJECTORY_NUMBER_REACHES_MAXIMUM_VALUE__MODE_==_PIC_MODE_ON_) {
    if (AllowRecordingParticleTrajectoryPoints[spec]==false) return;
  }

  //pointer to the particle trajectory data
  ParticleTrajectoryRecord=(cParticleData*)(ParticleDataRecordOffset+((PIC::ParticleBuffer::byte*)ParticleData));

  //the trajectory is traced only if the particle trajecotry tracking flag is set 'true'
  if (ParticleTrajectoryRecord->TrajectoryTrackingFlag==false) return;

  //compute the physical data into a LOCAL record (so it can be cached and/or committed)
  cTrajectoryPhysicalData data;

  //save physical data
  memcpy(data.x,x,3*sizeof(double));
  memcpy(data.v,v,3*sizeof(double));
  data.Speed=sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  data.spec=spec;

#if _PIC_PARTICLE_TRACKER__PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_MODE_ == _PIC_MODE_ON_
  //convert pinter to the block
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *)nodeIn;

  if (node!=NULL) {
    if (node->block!=NULL) {
      data.ParticleWeightOverLocalTimeStepRatio=node->block->GetLocalParticleWeight(spec)/node->block->GetLocalTimeStep(spec)*
          PIC::ParticleBuffer::GetIndividualStatWeightCorrection((PIC::ParticleBuffer::byte *)ParticleData);
    }
    else data.ParticleWeightOverLocalTimeStepRatio=1.0;
  }
  else data.ParticleWeightOverLocalTimeStepRatio=1.0;
#endif  //_PIC_PARTICLE_TRACKER__PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_MODE_ == _PIC_MODE_ON_

  //save the time stamp of the trajectory point
if (_PIC_PARTICLE_TRACKER__TRAJECTORY_TIME_STAMP_MODE_ == _PIC_MODE_ON_) {
  data.TimeStamp=PIC::SimulationTime::Get();
}

  //save the electric charge carried by the particle
#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
  double ParticleElectricCharge=0.0,ParticleSize=0.0;

  if ((spec>=_DUST_SPEC_) && (spec<_DUST_SPEC_+ElectricallyChargedDust::GrainVelocityGroup::nGroups)) {
    if (_PIC_MODEL__DUST__ELECTRIC_CHARGE_MODE_ == _PIC_MODEL__DUST__ELECTRIC_CHARGE_MODE__ON_) { 
      ParticleElectricCharge=ElectricallyChargedDust::GetGrainCharge((PIC::ParticleBuffer::byte*)ParticleData);
    }

    ParticleSize=ElectricallyChargedDust::GetGrainRadius((PIC::ParticleBuffer::byte*)ParticleData);
  }
  else {
    ParticleElectricCharge=PIC::MolecularData::ElectricChargeTable[spec];
    ParticleSize=0.0;
  }

  data.ElectricCharge=ParticleElectricCharge;
  data.ParticleSize=ParticleSize;
#endif //_PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_

  //save total kinetic energy
if (_PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_) {
  double KinEnergy=0.0;
  double m0=PIC::MolecularData::GetMass(spec); 
  // contribution of guiding center motion

  if (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_ == _PIC_MODE_ON_) {
    exit(__LINE__,__FILE__,"ERROR:not implemented");
  } else {
    KinEnergy+=0.5*m0*(v[0]*v[0]+ v[1]*v[1]+ v[2]*v[2]);
  }


  // contribtion of gyrations
  //magnetic moment
  double mu= PIC::ParticleBuffer::GetMagneticMoment((PIC::ParticleBuffer::byte*)ParticleData);
  // get the mag field magnitude at particle's location
  double AbsB=0, B[3]={0};

  PIC::CPLR::InitInterpolationStencil(x);
  PIC::CPLR::GetBackgroundMagneticField(B);
  AbsB = pow(B[0]*B[0]+B[1]*B[1]+B[2]*B[2],0.5)+1E-15;

  if (_PIC_PARTICLE_MOVER__RELATIVITY_MODE_ == _PIC_MODE_ON_) {
    exit(__LINE__,__FILE__,"ERROR:not implemented");
  } else {
    KinEnergy+= AbsB*mu;
  }

  //record the energy value
  data.KineticEnergy=KinEnergy;
} //_PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_
else {
  double m0=PIC::MolecularData::GetMass(spec);
  data.KineticEnergy=Relativistic::Speed2E(data.Speed,m0);
} 



  //save the number of the face where the particle was created
if (_PIC_PARTICLE_TRACKER__INJECTION_FACE_MODE_ ==  _PIC_MODE_ON_) {
  data.InjectionFaceNumber=PIC::ParticleBuffer::GetInjectionFaceNumber((PIC::ParticleBuffer::byte*)ParticleData);
}

  //the global 0-based offset of this point along the trajectory (== running count K before this point)
  unsigned long int offset=ParticleTrajectoryRecord->nSampledTrajectoryPoints;

  //decide whether this point lies on the current dyadic grid and must be committed to the temp files
  bool CommitToFile=true;

  if (TrajectorySamplingThinningMode==true) {
    int C=TrajectoryThinningCapacity(OutputTrajectoryPointNumber);
    int L=TrajectoryThinningLevel(offset+1,C); //running count includes this point

    if ((L>0)&&((offset&((((unsigned long int)1)<<L)-1))!=0)) CommitToFile=false;
  }

  //always update the per-OpenMP-thread "last point" cache (used to recover the true endpoint)
  LastTrajectoryPointCache[threadOpenMP].Valid=true;
  LastTrajectoryPointCache[threadOpenMP].Trajectory=ParticleTrajectoryRecord->Trajectory;
  LastTrajectoryPointCache[threadOpenMP].offset=offset;
  LastTrajectoryPointCache[threadOpenMP].data=data;

  //always increment the true global trajectory point count K (migrates with the particle)
  ParticleTrajectoryRecord->nSampledTrajectoryPoints++;
  if (ParticleTrajectoryRecord->nSampledTrajectoryPoints==0) exit(__LINE__,__FILE__,"Error: ParticleTrajectoryRecord->nSampledTrajectoryPoints is zero");

  //off-grid points are skipped (this is the I/O saving)
  if (CommitToFile==false) return;

  //save the data buffer if full
  if (TrajectoryDataTable[threadOpenMP].CurrentPosition==cTrajectoryData::Size) TrajectoryDataTable[threadOpenMP].flush();

  //commit the local record to the temp buffer
  cTrajectoryDataRecord *TrajectoryRecord=TrajectoryDataTable[threadOpenMP].buffer+TrajectoryDataTable[threadOpenMP].CurrentPosition;

  TrajectoryRecord->data=data;
  TrajectoryRecord->Trajectory=ParticleTrajectoryRecord->Trajectory;
  TrajectoryRecord->offset=offset;
  TrajectoryRecord->ForcedEndpoint=0;

  //update the trajectory data buffer pointer
  TrajectoryDataTable[threadOpenMP].CurrentPosition++;

  //increment the sample point counter
  SampledTrajectoryPointCounter[threadOpenMP]++;
}

void PIC::ParticleTracker::FinilazeParticleRecord(void *ParticleData) {
  cParticleData *ParticleTrajectoryRecord;

  //OpenMP thread
  int threadOpenMP=0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  threadOpenMP=omp_get_thread_num();
#endif

  //the trajectory is traced only if the particle trajecotry tracking flag is set 'true'
  ParticleTrajectoryRecord=(cParticleData*)(ParticleDataRecordOffset+((PIC::ParticleBuffer::byte*)ParticleData));
  if (ParticleTrajectoryRecord->TrajectoryTrackingFlag==false) return;

  //if thinning is on, the true endpoint (offset K-1) is usually off the final dyadic grid and was
  //therefore skipped at sample time -> force-write it now from the per-thread "last point" cache.
  //Finalize is called only from DeleteParticle and StopParticleTrajectoryTracking (NOT during MPI
  //migration), so cache.offset==K-1 reliably identifies the true trajectory end.
  if (TrajectorySamplingThinningMode==true) {
    unsigned long int K=ParticleTrajectoryRecord->nSampledTrajectoryPoints;

    if ((K>0)&&(LastTrajectoryPointCache[threadOpenMP].Valid==true)&&
        (SameTrajectory(LastTrajectoryPointCache[threadOpenMP].Trajectory,ParticleTrajectoryRecord->Trajectory)==true)&&
        (LastTrajectoryPointCache[threadOpenMP].offset==K-1)) {

      int C=TrajectoryThinningCapacity(OutputTrajectoryPointNumber);
      int L=TrajectoryThinningLevel(K,C);
      unsigned long int S=((unsigned long int)1)<<L;

      //force-write only if the endpoint is NOT already on the final grid
      if (((K-1)&(S-1))!=0) {
        if (TrajectoryDataTable[threadOpenMP].CurrentPosition==cTrajectoryData::Size) TrajectoryDataTable[threadOpenMP].flush();

        cTrajectoryDataRecord *EndpointRecord=TrajectoryDataTable[threadOpenMP].buffer+TrajectoryDataTable[threadOpenMP].CurrentPosition;

        EndpointRecord->data=LastTrajectoryPointCache[threadOpenMP].data;
        EndpointRecord->Trajectory=LastTrajectoryPointCache[threadOpenMP].Trajectory;
        EndpointRecord->offset=LastTrajectoryPointCache[threadOpenMP].offset;
        EndpointRecord->ForcedEndpoint=1;

        TrajectoryDataTable[threadOpenMP].CurrentPosition++;
        SampledTrajectoryPointCounter[threadOpenMP]++;
      }
    }

    //invalidate the cache so a later (different) trajectory cannot be mistaken for this one
    LastTrajectoryPointCache[threadOpenMP].Valid=false;
  }

  //save the data buffer if full
  if (TrajectoryListTable[threadOpenMP].CurrentPosition==cTrajectoryList::Size) TrajectoryListTable[threadOpenMP].flush();
  TrajectoryListTable[threadOpenMP].buffer[TrajectoryListTable[threadOpenMP].CurrentPosition].Trajectory=ParticleTrajectoryRecord->Trajectory;
  TrajectoryListTable[threadOpenMP].buffer[TrajectoryListTable[threadOpenMP].CurrentPosition].nSampledTrajectoryPoints=ParticleTrajectoryRecord->nSampledTrajectoryPoints;

  //update the trajectory data buffer pointer
  ++TrajectoryListTable[threadOpenMP].CurrentPosition;

  //reset the tracking flag
  ParticleTrajectoryRecord->TrajectoryTrackingFlag=false;
}

//===================================================================================================
//output sampled particle trajectories
void PIC::ParticleTracker::OutputTrajectory(const char *fname) {
  int thread;
  char str[_MAX_STRING_LENGTH_PIC_];

  //save a temporary file that contains the trajectory information of the particles that are currently in the system
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_],ptr;
  long int i,j,k;
  PIC::ParticleBuffer::byte *ParticleData;
  cParticleData ParticleTrajectoryRecord;
  FILE *fTemporatyTrajectoryList;
  unsigned long int length=0;

  sprintf(str,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.TemporaryTrajectoryList.pt",PIC::OutputDataFileDirectory,PIC::ThisThread);
  fTemporatyTrajectoryList=fopen(str,"w");

  //calculate the number of the particles that will be placed into the list
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if (node->block!=NULL) {
    memcpy(FirstCellParticleTable,node->block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

    for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (j=0;j<_BLOCK_CELLS_Y_;j++)  {
        for (i=0;i<_BLOCK_CELLS_X_;i++) {
          ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

          while (ptr!=-1) {
            ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
            memcpy(&ParticleTrajectoryRecord,ParticleDataRecordOffset+ParticleData,sizeof(cParticleData));

            if (ParticleTrajectoryRecord.TrajectoryTrackingFlag==true) {
              ++length;
            }

            ptr=PIC::ParticleBuffer::GetNext(ParticleData);
          }

        }
      }
    }
  }

  fwrite(&length,sizeof(unsigned long int),1,fTemporatyTrajectoryList);

  //save the list
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) if (node->block!=NULL) {
    memcpy(FirstCellParticleTable,node->block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

    for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (j=0;j<_BLOCK_CELLS_Y_;j++)  {
        for (i=0;i<_BLOCK_CELLS_X_;i++) {
          ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

          while (ptr!=-1) {
            ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
            memcpy(&ParticleTrajectoryRecord,ParticleDataRecordOffset+ParticleData,sizeof(cParticleData));

            if (ParticleTrajectoryRecord.TrajectoryTrackingFlag==true) {
              cTrajectoryListRecord record;

              record.Trajectory=ParticleTrajectoryRecord.Trajectory;
              record.nSampledTrajectoryPoints=ParticleTrajectoryRecord.nSampledTrajectoryPoints;
              if (record.nSampledTrajectoryPoints<=0) exit(__LINE__,__FILE__,"Error: the number of sampled points is out of range");

              fwrite(&record,sizeof(cTrajectoryListRecord),1,fTemporatyTrajectoryList);
            }

            ptr=PIC::ParticleBuffer::GetNext(ParticleData);
          }

        }
      }
    }
  }

  fclose(fTemporatyTrajectoryList);

  //flush trajectory buffer (has to be done in a patallel section)
#if _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
  TrajectoryDataTable[0].flush();
  TrajectoryListTable[0].flush();
#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
#pragma omp parallel
  {
    int threadOpenMP=omp_get_thread_num();

    TrajectoryDataTable[threadOpenMP].flush();
    TrajectoryListTable[threadOpenMP].flush();
  }
#else
#error Unknown option
#endif //_COMPILATION_MODE_

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

  //get the number of the trajectory list and data files, and the total number of the sampled trajectories
  unsigned long int nTrajectoryListFiles[PIC::nTotalThreads*PIC::nTotalThreadsOpenMP],nTrajectoryDataFiles[PIC::nTotalThreads*PIC::nTotalThreadsOpenMP],nTotalSampledTrajectories[PIC::nTotalThreads*PIC::nTotalThreadsOpenMP];
  unsigned long int tmpTrajectoryListFiles[PIC::nTotalThreadsOpenMP],tmpTrajectoryDataFiles[PIC::nTotalThreadsOpenMP];

  for (thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) {
    tmpTrajectoryListFiles[thread]=TrajectoryListTable[thread].nfile;
    tmpTrajectoryDataFiles[thread]=TrajectoryDataTable[thread].nfile;
  }

  MPI_Gather(tmpTrajectoryListFiles,PIC::nTotalThreadsOpenMP,MPI_UNSIGNED_LONG,nTrajectoryListFiles,PIC::nTotalThreadsOpenMP,MPI_UNSIGNED_LONG,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Gather(tmpTrajectoryDataFiles,PIC::nTotalThreadsOpenMP,MPI_UNSIGNED_LONG,nTrajectoryDataFiles,PIC::nTotalThreadsOpenMP,MPI_UNSIGNED_LONG,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Gather(SampledTrajectoryCounter,PIC::nTotalThreadsOpenMP,MPI_UNSIGNED_LONG,nTotalSampledTrajectories,PIC::nTotalThreadsOpenMP,MPI_UNSIGNED_LONG,0,MPI_GLOBAL_COMMUNICATOR);

  //get the total number of the sampled trajectory points
  unsigned long int threadTrajectoryPointCounter=0,totalTrajectoryPointCounter=0;

  for (thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) threadTrajectoryPointCounter+=SampledTrajectoryPointCounter[thread];

  MPI_Reduce(&threadTrajectoryPointCounter,&totalTrajectoryPointCounter,1,MPI_UNSIGNED_LONG,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);

  //save data needed for unpacking of the trajecotry files in postprocessing
  if (PIC::ThisThread==0) {
    FILE *fTrajectoryDataSet;

    sprintf(str,"%s.TrajectoryDataSet.pt",fname);
    fTrajectoryDataSet=fopen(str,"w");

    int nspec=PIC::nTotalSpecies;
    int nthreads=PIC::nTotalThreads*PIC::nTotalThreadsOpenMP;  //the total number of threads (OpenMP*MPI) used in the simulation
    int nMPIthreads=PIC::nTotalThreads; //the number of the MPI threads used in the simulation

    fwrite(&nspec,sizeof(int),1,fTrajectoryDataSet);
    fwrite(&nthreads,sizeof(int),1,fTrajectoryDataSet);
    fwrite(&nMPIthreads,sizeof(int),1,fTrajectoryDataSet);

    fwrite(nTrajectoryListFiles,sizeof(unsigned long int),PIC::nTotalThreads*PIC::nTotalThreadsOpenMP,fTrajectoryDataSet);
    fwrite(nTrajectoryDataFiles,sizeof(unsigned long int),PIC::nTotalThreads*PIC::nTotalThreadsOpenMP,fTrajectoryDataSet);
    fwrite(nTotalSampledTrajectories,sizeof(unsigned long int),PIC::nTotalThreads*PIC::nTotalThreadsOpenMP,fTrajectoryDataSet);

    //save the species chemical symbols
    for (int spec=0;spec<nTotalSpecies;spec++) {
      char ChemSymbol[_MAX_STRING_LENGTH_PIC_];
      PIC::MolecularData::GetChemSymbol(ChemSymbol,spec);

      fwrite(ChemSymbol,sizeof(char),_MAX_STRING_LENGTH_PIC_,fTrajectoryDataSet);
    }

    fclose(fTrajectoryDataSet);

    //print the total number of saved trajectories and saved trajectory points
    int nTotalSavedTrajectories=0;

    for (int thread=0;thread<PIC::nTotalThreads*PIC::nTotalThreadsOpenMP;thread++) nTotalSavedTrajectories+=nTotalSampledTrajectories[thread];
    printf("$PREFIX: The total number of sampled trajectories: %i\n",nTotalSavedTrajectories);
    printf("$PREFIX: The total number of sampled trajectory points: %lu\n",totalTrajectoryPointCounter);
  }


  //create the trajectory files
  if (_PIC_PARTICLE_TRACKER__RUNTIME_OUTPUT_==_PIC_MODE_ON_) {
    int TrajectoryPointBufferLength=10000000;

    if (PIC::ThisThread==0) {
      CreateTrajectoryOutputFiles(fname,PIC::OutputDataFileDirectory,TrajectoryPointBufferLength);
    }
  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
}

//===================================================================================================
//MPI-distributed trajectory output (companion path to the serial CreateTrajectoryOutputFiles).
//
//Phase 1 (all ranks): rank 0 builds the global per-trajectory length table K from the small
//trajectory-list files and broadcasts it; each rank then pre-selects the surviving points from
//ONLY its own temp data files (no read amplification) and writes a compact selected-points file.
//Phase 2 (rank 0): assemble the ASCII Tecplot from the per-rank selected-points files,
//reassembling trajectories that were split across ranks by (global trajectory id, ordinal).
//
//Both phases branch on TrajectorySamplingThinningMode: ON uses the dyadic-grid offline rule
//(GetThinnedTrajectoryPointOrdinal / GetThinnedTrajectoryOutputPointNumber), OFF uses the
//exact-N offline rule (GetSelectedTrajectoryPointOrdinal / GetTrajectoryOutputPointNumber).
//
//NOTE: this companion path was added against the pristine baseline (which had no parallel
//output functions) and is kept self-consistent and compilable; it has NOT been exercised in a
//full AMPS+MPI build. The serial CreateTrajectoryOutputFiles remains the default output path
//invoked by OutputTrajectory. Do a 2+ rank smoke test before relying on this path.

//read the global per-trajectory length table K (and the starting-thread offsets) from the
//trajectory-list files; mirrors the bookkeeping in CreateTrajectoryOutputFiles.
namespace {
  void BuildGlobalTrajectoryLengthTable(const char *fname,const char *Dir,
      long int &nTotalTracedTrajectories,unsigned long int *&K,long int *&TrajectoryCounterOffset,
      int &nTotalThreads,int &nMPIthread,int &nTotalSpeciesOut) {

    char str[_MAX_STRING_LENGTH_PIC_];
    FILE *fTrajectoryDataSet=NULL;

    sprintf(str,"%s.TrajectoryDataSet.pt",fname);
    fTrajectoryDataSet=fopen(str,"r");
    if (fTrajectoryDataSet==NULL) exit(__LINE__,__FILE__,"Error: cannot open the trajectory setting file");

    int nTotalSpecies;
    if (fread(&nTotalSpecies,sizeof(int),1,fTrajectoryDataSet)!=1) exit(__LINE__,__FILE__,"Error: fread has failed");
    if (fread(&nTotalThreads,sizeof(int),1,fTrajectoryDataSet)!=1) exit(__LINE__,__FILE__,"Error: fread has failed");
    if (fread(&nMPIthread,sizeof(int),1,fTrajectoryDataSet)!=1) exit(__LINE__,__FILE__,"Error: fread has failed");
    nTotalSpeciesOut=nTotalSpecies;

    unsigned long int *nTrajectoryListFiles=new unsigned long int[nTotalThreads];
    unsigned long int *nTrajectoryDataFiles=new unsigned long int[nTotalThreads];
    unsigned long int *nTotalSampledTrajectories=new unsigned long int[nTotalThreads];

    if (fread(nTrajectoryListFiles,sizeof(unsigned long int),nTotalThreads,fTrajectoryDataSet)!=(size_t)nTotalThreads) exit(__LINE__,__FILE__,"Error: file reading error");
    if (fread(nTrajectoryDataFiles,sizeof(unsigned long int),nTotalThreads,fTrajectoryDataSet)!=(size_t)nTotalThreads) exit(__LINE__,__FILE__,"Error: file reading error");
    if (fread(nTotalSampledTrajectories,sizeof(unsigned long int),nTotalThreads,fTrajectoryDataSet)!=(size_t)nTotalThreads) exit(__LINE__,__FILE__,"Error: file reading error");
    fclose(fTrajectoryDataSet);

    nTotalTracedTrajectories=0;
    TrajectoryCounterOffset=new long int[nTotalThreads];
    for (int thread=0;thread<nTotalThreads;thread++) {
      TrajectoryCounterOffset[thread]=nTotalTracedTrajectories;
      nTotalTracedTrajectories+=nTotalSampledTrajectories[thread];
    }

    K=new unsigned long int[(nTotalTracedTrajectories>0)?nTotalTracedTrajectories:1];
    for (long int i=0;i<nTotalTracedTrajectories;i++) K[i]=0;

    //read all trajectory-list files (npass 0: in-flight temporary lists; npass 1: completed lists)
    for (int npass=0;npass<2;npass++) for (int thread=0;thread<((npass==0)?nMPIthread:nTotalThreads);thread++)
      for (unsigned long int nfile=0;nfile<((npass==0)?1UL:nTrajectoryListFiles[thread]);nfile++) {

      if (npass==0) sprintf(str,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.TemporaryTrajectoryList.pt",Dir,thread);
      else          sprintf(str,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.out=%lu.TrajectoryList.pt",Dir,thread,nfile);

      FILE *fTrajectoryList=fopen(str,"r");
      if (fTrajectoryList==NULL) exit(__LINE__,__FILE__,"Error: cannot open file");

      unsigned long int length;
      if (fread(&length,sizeof(unsigned long int),1,fTrajectoryList)!=1) exit(__LINE__,__FILE__,"Error: fread has failed");

      for (unsigned long int i=0;i<length;i++) {
        PIC::ParticleTracker::cTrajectoryListRecord Record;
        if (fread(&Record,sizeof(PIC::ParticleTracker::cTrajectoryListRecord),1,fTrajectoryList)!=1) exit(__LINE__,__FILE__,"Error: fread has failed");

        long int el=Record.Trajectory.id+TrajectoryCounterOffset[Record.Trajectory.StartingThread];
        if ((el<0)||(el>=nTotalTracedTrajectories)) exit(__LINE__,__FILE__,"Error: out of range");
        K[el]=Record.nSampledTrajectoryPoints;
      }

      fclose(fTrajectoryList);
    }

    delete [] nTrajectoryListFiles;
    delete [] nTrajectoryDataFiles;
    delete [] nTotalSampledTrajectories;
  }
}

void PIC::ParticleTracker::ParallelPreselectTrajectoryPoints(const char *fname,const char *OutputDataFileDirectory) {
  long int nTotalTracedTrajectories=0;
  unsigned long int *K=NULL;
  long int *TrajectoryCounterOffset=NULL;
  int nTotalThreads=0,nMPIthread=0,nTotalSpecies=0;

  //rank 0 builds the global per-trajectory length table and broadcasts the pieces every rank
  //needs to compute the SAME global grid (so no further communication is required).
  if (PIC::ThisThread==0) {
    BuildGlobalTrajectoryLengthTable(fname,OutputDataFileDirectory,nTotalTracedTrajectories,K,TrajectoryCounterOffset,nTotalThreads,nMPIthread,nTotalSpecies);
  }

  long int header[3];
  if (PIC::ThisThread==0) { header[0]=nTotalTracedTrajectories; header[1]=nTotalThreads; header[2]=nMPIthread; }
  MPI_Bcast(header,3,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);
  nTotalTracedTrajectories=header[0]; nTotalThreads=(int)header[1]; nMPIthread=(int)header[2];

  if (PIC::ThisThread!=0) {
    K=new unsigned long int[(nTotalTracedTrajectories>0)?nTotalTracedTrajectories:1];
    TrajectoryCounterOffset=new long int[nTotalThreads];
  }

  MPI_Bcast(K,(int)((nTotalTracedTrajectories>0)?nTotalTracedTrajectories:1),MPI_UNSIGNED_LONG,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Bcast(TrajectoryCounterOffset,nTotalThreads,MPI_LONG,0,MPI_GLOBAL_COMMUNICATOR);

  //this rank owns global thread indices [ThisThread*nThreadsOpenMP, (ThisThread+1)*nThreadsOpenMP)
  //(see the temp-file naming in cTrajectoryData::flush()).
  int nThreadsOpenMP=PIC::nTotalThreadsOpenMP;
  int firstGlobalThread=PIC::ThisThread*nThreadsOpenMP;
  int lastGlobalThread =firstGlobalThread+nThreadsOpenMP; //exclusive

  //open this rank's selected-points file and write the header
  char str[_MAX_STRING_LENGTH_PIC_];
  SelectedPointsFileName(str,fname,OutputDataFileDirectory,PIC::ThisThread);
  FILE *fSelected=fopen(str,"w");
  if (fSelected==NULL) exit(__LINE__,__FILE__,"Error: cannot open the selected-points file for writing");

  unsigned long int magic=SelectedPointsFileMagic;
  unsigned long int nSelected=0;
  fwrite(&magic,sizeof(unsigned long int),1,fSelected);
  long int nSelectedPos=ftell(fSelected);
  fwrite(&nSelected,sizeof(unsigned long int),1,fSelected); //placeholder, patched at the end

  //we need the number of data files per owned global thread; reread the dataset file (cheap).
  //(rank 0 already has it, but every rank reads it independently here for simplicity).
  unsigned long int *nTrajectoryDataFiles=new unsigned long int[nTotalThreads];
  {
    sprintf(str,"%s.TrajectoryDataSet.pt",fname);
    FILE *fDS=fopen(str,"r");
    if (fDS==NULL) exit(__LINE__,__FILE__,"Error: cannot open the trajectory setting file");
    int dummy;
    if (fread(&dummy,sizeof(int),1,fDS)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); //nspec
    if (fread(&dummy,sizeof(int),1,fDS)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); //nthreads
    if (fread(&dummy,sizeof(int),1,fDS)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); //nMPIthreads
    unsigned long int *skip=new unsigned long int[nTotalThreads];
    if (fread(skip,sizeof(unsigned long int),nTotalThreads,fDS)!=(size_t)nTotalThreads) exit(__LINE__,__FILE__,"Error: fread failed"); //list files
    if (fread(nTrajectoryDataFiles,sizeof(unsigned long int),nTotalThreads,fDS)!=(size_t)nTotalThreads) exit(__LINE__,__FILE__,"Error: fread failed"); //data files
    delete [] skip;
    fclose(fDS);
  }

  cTrajectoryDataRecord TrajectoryRecord;
  int TrajectoryRecordLength=sizeof(cTrajectoryDataRecord);

  for (int thread=firstGlobalThread;thread<lastGlobalThread;thread++) {
    if (thread>=nTotalThreads) break;

    for (unsigned long int nfile=0;nfile<nTrajectoryDataFiles[thread];nfile++) {
      sprintf(str,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.out=%lu.TrajectoryData.pt",OutputDataFileDirectory,thread,nfile);

      FILE *fData=fopen(str,"r");
      if (fData==NULL) exit(__LINE__,__FILE__,"Error: cannot open file");

      unsigned long int length;
      if (fread(&length,sizeof(unsigned long int),1,fData)!=1) exit(__LINE__,__FILE__,"Error: file reading error");

      for (unsigned long int i=0;i<length;i++) {
        if (fread(&TrajectoryRecord,TrajectoryRecordLength,1,fData)!=1) exit(__LINE__,__FILE__,"Error: file reading error");

        long int GlobalTrajectoryNumber=TrajectoryRecord.Trajectory.id+TrajectoryCounterOffset[TrajectoryRecord.Trajectory.StartingThread];
        if ((GlobalTrajectoryNumber<0)||(GlobalTrajectoryNumber>=nTotalTracedTrajectories)) exit(__LINE__,__FILE__,"Error: out of range");

        unsigned long int Kthis=K[GlobalTrajectoryNumber];

        long int ordinal = (TrajectorySamplingThinningMode==true) ?
          GetThinnedTrajectoryPointOrdinal(TrajectoryRecord.offset,TrajectoryRecord.ForcedEndpoint,Kthis,OutputTrajectoryPointNumber) :
          GetSelectedTrajectoryPointOrdinal(TrajectoryRecord.offset,Kthis,OutputTrajectoryPointNumber);

        if (ordinal<0) continue;

        cSelectedTrajectoryPointRecord out;
        out.GlobalTrajectoryNumber=GlobalTrajectoryNumber;
        out.ordinal=ordinal;
        out.data=TrajectoryRecord.data;

        fwrite(&out,sizeof(cSelectedTrajectoryPointRecord),1,fSelected);
        nSelected++;
      }

      fclose(fData);
    }
  }

  //patch the record count into the header
  fseek(fSelected,nSelectedPos,SEEK_SET);
  fwrite(&nSelected,sizeof(unsigned long int),1,fSelected);
  fclose(fSelected);

  delete [] nTrajectoryDataFiles;
  delete [] K;
  delete [] TrajectoryCounterOffset;

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
}

void PIC::ParticleTracker::AssembleTrajectoryOutputFromSelected(const char *fname,const char *OutputDataFileDirectory) {
  //only rank 0 assembles the final ASCII Tecplot files
  if (PIC::ThisThread!=0) { MPI_Barrier(MPI_GLOBAL_COMMUNICATOR); return; }

  long int nTotalTracedTrajectories=0;
  unsigned long int *K=NULL;
  long int *TrajectoryCounterOffset=NULL;
  int nTotalThreads=0,nMPIthread=0,nTotalSpecies=0;

  BuildGlobalTrajectoryLengthTable(fname,OutputDataFileDirectory,nTotalTracedTrajectories,K,TrajectoryCounterOffset,nTotalThreads,nMPIthread,nTotalSpecies);

  //per-trajectory output-point count M and a flat placement offset into a global buffer
  auto OutputPointNumber=[&](unsigned long int Kk)->long int {
    unsigned long int M=(TrajectorySamplingThinningMode==true) ?
        GetThinnedTrajectoryOutputPointNumber(Kk,OutputTrajectoryPointNumber) :
        GetTrajectoryOutputPointNumber(Kk,OutputTrajectoryPointNumber);
    return (long int)M;
  };

  long int *Mtable=new long int[(nTotalTracedTrajectories>0)?nTotalTracedTrajectories:1];
  long int *BufOffset=new long int[(nTotalTracedTrajectories>0)?nTotalTracedTrajectories:1];
  long int TotalPoints=0;

  for (long int tr=0;tr<nTotalTracedTrajectories;tr++) {
    Mtable[tr]=OutputPointNumber(K[tr]);
    BufOffset[tr]=TotalPoints;
    TotalPoints+=Mtable[tr];
  }

  cTrajectoryPhysicalData *Buffer=new cTrajectoryPhysicalData[(TotalPoints>0)?TotalPoints:1];
  char *Filled=new char[(TotalPoints>0)?TotalPoints:1];
  for (long int i=0;i<TotalPoints;i++) Filled[i]=0;

  //read every rank's selected-points file and place each record by (trajectory, ordinal)
  char str[_MAX_STRING_LENGTH_PIC_];

  for (int rank=0;rank<nMPIthread;rank++) {
    SelectedPointsFileName(str,fname,OutputDataFileDirectory,rank);
    FILE *fSelected=fopen(str,"r");
    if (fSelected==NULL) continue; //a rank may have produced no points

    unsigned long int magic=0,nSelected=0;
    if (fread(&magic,sizeof(unsigned long int),1,fSelected)!=1) { fclose(fSelected); continue; }
    if (magic!=SelectedPointsFileMagic) exit(__LINE__,__FILE__,"Error: incompatible selected-points file (magic/record-size mismatch)");
    if (fread(&nSelected,sizeof(unsigned long int),1,fSelected)!=1) { fclose(fSelected); continue; }

    for (unsigned long int i=0;i<nSelected;i++) {
      cSelectedTrajectoryPointRecord rec;
      if (fread(&rec,sizeof(cSelectedTrajectoryPointRecord),1,fSelected)!=1) exit(__LINE__,__FILE__,"Error: file reading error");

      if ((rec.GlobalTrajectoryNumber<0)||(rec.GlobalTrajectoryNumber>=nTotalTracedTrajectories)) exit(__LINE__,__FILE__,"Error: out of range");
      if ((rec.ordinal<0)||(rec.ordinal>=Mtable[rec.GlobalTrajectoryNumber])) exit(__LINE__,__FILE__,"Error: ordinal out of range");

      long int el=BufOffset[rec.GlobalTrajectoryNumber]+rec.ordinal;
      Buffer[el]=rec.data;
      Filled[el]=1;
    }

    fclose(fSelected);
  }

  //open the per-species Tecplot output files (same header as CreateTrajectoryOutputFiles)
  FILE *fTrajectoryDataSet=NULL;
  sprintf(str,"%s.TrajectoryDataSet.pt",fname);
  fTrajectoryDataSet=fopen(str,"r");
  if (fTrajectoryDataSet==NULL) exit(__LINE__,__FILE__,"Error: cannot open the trajectory setting file");
  { int d; for (int q=0;q<3;q++) if (fread(&d,sizeof(int),1,fTrajectoryDataSet)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); }
  { unsigned long int *skip=new unsigned long int[nTotalThreads];
    for (int q=0;q<3;q++) if (fread(skip,sizeof(unsigned long int),nTotalThreads,fTrajectoryDataSet)!=(size_t)nTotalThreads) exit(__LINE__,__FILE__,"Error: fread failed");
    delete [] skip; }

  FILE **fout=new FILE*[nTotalSpecies];
  int *TrajectoryCounter=new int[nTotalSpecies];

  for (int spec=0;spec<nTotalSpecies;spec++) {
    char ChemSymbol[_MAX_STRING_LENGTH_PIC_];
    TrajectoryCounter[spec]=0;
    if (fread(ChemSymbol,sizeof(char),_MAX_STRING_LENGTH_PIC_,fTrajectoryDataSet)!=_MAX_STRING_LENGTH_PIC_) exit(__LINE__,__FILE__,"Error: fread has failed");

    sprintf(str,"%s.s=%i.%s.dat",fname,spec,ChemSymbol);
    fout[spec]=fopen(str,"w");
    fprintf(fout[spec],"VARIABLES=\"x\", \"y\", \"z\", \"spec\", \"Speed\", \"vx\", \"vy\", \"vz\", \"Kinetic Energy [eV]\"");

    if (_PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_) fprintf(fout[spec],", \"Electric Charge\", \"Particle Size\"");
    if (_PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_) fprintf(fout[spec],", \"Total kinetic energy [J]\"");
    if (_PIC_PARTICLE_TRACKER__TRAJECTORY_TIME_STAMP_MODE_ == _PIC_MODE_ON_) fprintf(fout[spec],", \"Time Stamp\"");
    if (_PIC_PARTICLE_TRACKER__INJECTION_FACE_MODE_ ==  _PIC_MODE_ON_) fprintf(fout[spec],", \"Injection Face Number\"");
    if (_PIC_PARTICLE_TRACKER__PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_MODE_ == _PIC_MODE_ON_) fprintf(fout[spec],", \"Particle Weight Over Time Step Ratio\"");
    fprintf(fout[spec],"\n");
  }
  fclose(fTrajectoryDataSet);

  //write one ZONE per trajectory, in dense ordinal order
  for (long int tr=0;tr<nTotalTracedTrajectories;tr++) {
    long int M=Mtable[tr];
    if (M<=0) continue;

    int spec=Buffer[BufOffset[tr]].spec;
    FILE *trOut=fout[spec];

    fprintf(trOut,"ZONE T=\"Trajectory=%i\" F=POINT\n",TrajectoryCounter[spec]);
    ++TrajectoryCounter[spec];

    for (long int j=0;j<M;j++) {
      long int el=BufOffset[tr]+j;
      if (Filled[el]==0) continue; //missing point (should not happen for a complete run)

      cTrajectoryPhysicalData *TrajectoryData=Buffer+el;

      fprintf(trOut,"%e  %e  %e  %i  %e   %e  %e  %e",TrajectoryData->x[0],TrajectoryData->x[1],TrajectoryData->x[2],
          TrajectoryData->spec,TrajectoryData->Speed,TrajectoryData->v[0],TrajectoryData->v[1],TrajectoryData->v[2]);

      double KineticEnergy;
      if (Relativistic::GetGamma(TrajectoryData->v)<0.5) KineticEnergy=PIC::MolecularData::GetMass(TrajectoryData->spec)*Vector3D::DotProduct(TrajectoryData->v,TrajectoryData->v)/2.0;
      else KineticEnergy=Relativistic::Vel2E(TrajectoryData->v,PIC::MolecularData::GetMass(TrajectoryData->spec));
      fprintf(trOut," %e",KineticEnergy/ElectronCharge);

#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_
      fprintf(trOut," %e  %e ",TrajectoryData->ElectricCharge,TrajectoryData->ParticleSize);
#endif
#if _PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_
      fprintf(trOut," %e ",TrajectoryData->KineticEnergy);
#endif
#if _PIC_PARTICLE_TRACKER__TRAJECTORY_TIME_STAMP_MODE_ == _PIC_MODE_ON_
      fprintf(trOut," %e ",TrajectoryData->TimeStamp);
#endif
#if _PIC_PARTICLE_TRACKER__INJECTION_FACE_MODE_ ==  _PIC_MODE_ON_
      fprintf(trOut," %i ",TrajectoryData->InjectionFaceNumber);
#endif
#if _PIC_PARTICLE_TRACKER__PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_MODE_ == _PIC_MODE_ON_
      fprintf(trOut," %e ",TrajectoryData->ParticleWeightOverLocalTimeStepRatio);
#endif
      fprintf(trOut,"\n");
    }
  }

  for (int spec=0;spec<nTotalSpecies;spec++) fclose(fout[spec]);

  delete [] fout;
  delete [] TrajectoryCounter;
  delete [] Buffer;
  delete [] Filled;
  delete [] Mtable;
  delete [] BufOffset;
  delete [] K;
  delete [] TrajectoryCounterOffset;

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
}

//===================================================================================================
//output trajectory files
void PIC::ParticleTracker::CreateTrajectoryOutputFiles(const char *fname,const char *OutputDataFileDirectory,int TrajectoryPointBufferLength) {
  int thread;
  char str[_MAX_STRING_LENGTH_PIC_];
  int nTotalThreads,nTotalSpecies,nMPIthread;
  unsigned long int *nTrajectoryListFiles,*nTrajectoryDataFiles,*nTotalSampledTrajectories;

  //read the trajectory set file
  FILE *fTrajectoryDataSet=NULL;

  sprintf(str,"%s.TrajectoryDataSet.pt",fname);
  fTrajectoryDataSet=fopen(str,"r");
  if (fTrajectoryDataSet==NULL) exit(__LINE__,__FILE__,"Error: cannot open the trajectory swtting file");

  if (fread(&nTotalSpecies,sizeof(int),1,fTrajectoryDataSet)!=1) exit(__LINE__,__FILE__,"Error: fread has failed"); 
  if (fread(&nTotalThreads,sizeof(int),1,fTrajectoryDataSet)!=1) exit(__LINE__,__FILE__,"Error: fread has failed");   //the total number of threads (OpenMP*MPI) used in the simulation
  if (fread(&nMPIthread,sizeof(int),1,fTrajectoryDataSet)!=1) exit(__LINE__,__FILE__,"Error: fread has failed");     //the number of the MPI threads used in the simulation


  nTrajectoryListFiles=new unsigned long int[nTotalThreads];
  nTrajectoryDataFiles=new unsigned long int[nTotalThreads];
  nTotalSampledTrajectories=new unsigned long int[nTotalThreads];

  if (fread(nTrajectoryListFiles,sizeof(unsigned long int),nTotalThreads,fTrajectoryDataSet)!=nTotalThreads) exit(__LINE__,__FILE__,"Error: file reading error");
  if (fread(nTrajectoryDataFiles,sizeof(unsigned long int),nTotalThreads,fTrajectoryDataSet)!=nTotalThreads) exit(__LINE__,__FILE__,"Error: file reading error");
  if (fread(nTotalSampledTrajectories,sizeof(unsigned long int),nTotalThreads,fTrajectoryDataSet)!=nTotalThreads) exit(__LINE__,__FILE__,"Error: file reading error");



  //open sampled trajectory output files
  FILE *fout[nTotalSpecies];
  unsigned int spec, TrajectoryCounter[nTotalSpecies];

  for (spec=0;spec<nTotalSpecies;spec++) {
    char ChemSymbol[_MAX_STRING_LENGTH_PIC_];

    TrajectoryCounter[spec]=0;
    if (fread(ChemSymbol,sizeof(char),_MAX_STRING_LENGTH_PIC_,fTrajectoryDataSet)!=_MAX_STRING_LENGTH_PIC_) exit(__LINE__,__FILE__,"Error: fread has failed"); 

    sprintf(str,"%s.s=%i.%s.dat",fname,spec,ChemSymbol);

    fout[spec]=fopen(str,"w");
    fprintf(fout[spec],"VARIABLES=\"x\", \"y\", \"z\", \"spec\", \"Speed\", \"vx\", \"vy\", \"vz\", \"Kinetic Energy [eV]\"");

    if (_PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_) {
      fprintf(fout[spec],", \"Electric Charge\", \"Particle Size\"");
    }

    if (_PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_) {
      fprintf(fout[spec],", \"Total kinetic energy [J]\"");
    }

    if (_PIC_PARTICLE_TRACKER__TRAJECTORY_TIME_STAMP_MODE_ == _PIC_MODE_ON_) {
      fprintf(fout[spec],", \"Time Stamp\"");
    }

    if (_PIC_PARTICLE_TRACKER__INJECTION_FACE_MODE_ ==  _PIC_MODE_ON_) {
      fprintf(fout[spec],", \"Injection Face Number\"");
    }

    if (_PIC_PARTICLE_TRACKER__PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_MODE_ == _PIC_MODE_ON_) {
      fprintf(fout[spec],", \"Particle Weight Over Time Step Ratio\"");
    }

    fprintf(fout[spec],"\n");
  }

  //Create the array of sampled trajectory points for all trajectories (used also as a flag to determine trajectories that was already processed)
  long int nTotalTracedTrajectories=0;
  long int TrajectoryCounterOffset[nTotalThreads]; //the "global" trajectory number is TrajectoryCounterOffset[TrajectoryStartingThread]+ trajectory counting number at partuculat processor

  for (thread=0;thread<nTotalThreads;thread++) {
    TrajectoryCounterOffset[thread]=nTotalTracedTrajectories;
    nTotalTracedTrajectories+=nTotalSampledTrajectories[thread];
  }

  //2. Read the table of the sampled trajectory numbers
  int nfile;
  FILE *fTrajectoryList;
  //  PIC::ParticleTracker::cTrajectoryID Trajectory;
  unsigned long int i,length,nReadTrajectoryNumber=0;

  unsigned long int *nSampledTrajectoryPoints=new unsigned long int [nTotalTracedTrajectories];
  int *SampledTrajectoryDataOffset=new int [nTotalTracedTrajectories];
  int *nReadSampledTrajectoryPoints=new int [nTotalTracedTrajectories];
  int npass;

  for (npass=0;npass<2;npass++) for (thread=0;thread<((npass==0) ? nMPIthread : nTotalThreads);thread++) for (nfile=0;nfile<((npass==0) ? 1 : nTrajectoryListFiles[thread]);nfile++) {
    //scroll through all trajectory lists inclusing the temporary lists that contain the trajectory information regarding particles that are still in the simulation

    if (npass==0) {
      sprintf(str,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.TemporaryTrajectoryList.pt",PIC::OutputDataFileDirectory,thread);
    }
    else {
      sprintf(str,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.out=%i.TrajectoryList.pt",PIC::OutputDataFileDirectory,thread,nfile);
    }

    if ((fTrajectoryList=fopen(str,"r"))==NULL) exit(__LINE__,__FILE__,"Error: cannot open file");
    if (fread(&length,sizeof(unsigned long int),1,fTrajectoryList)!=1) exit(__LINE__,__FILE__,"Error: fread has failed"); 

    for (i=0;i<length;i++) {
      cTrajectoryListRecord Record;
      int el;

      if (fread(&Record,sizeof(PIC::ParticleTracker::cTrajectoryListRecord),1,fTrajectoryList)!=1) exit(__LINE__,__FILE__,"Error: fread has failed"); 

      el=Record.Trajectory.id+TrajectoryCounterOffset[Record.Trajectory.StartingThread];
      if ((el<0)||(el>=nTotalTracedTrajectories)) exit(__LINE__,__FILE__,"Error: out of range");

      nSampledTrajectoryPoints[el]=Record.nSampledTrajectoryPoints;
      nReadTrajectoryNumber++;
    }

    fclose(fTrajectoryList);
  }

  if (nReadTrajectoryNumber!=nTotalTracedTrajectories) {
    exit(__LINE__,__FILE__,"Error: the number of the read trajectories is different from the total number of the sampled trajectories");
  }

  //2. Read the particle trajectories
  nReadTrajectoryNumber=0;

  cTrajectoryPhysicalData *TempTrajectoryBuffer=new cTrajectoryPhysicalData[TrajectoryPointBufferLength];
  int UsedTrajectoryPointBuffer,StartTrajectoryNumber;

  //number of output points for a trajectory of length K under the active mode
  //(thinning ON: dyadic-grid count + forced endpoint; OFF: exact-N selection)
  auto OutputPointNumber=[&](unsigned long int K)->int {
    unsigned long int M=(TrajectorySamplingThinningMode==true) ?
        GetThinnedTrajectoryOutputPointNumber(K,OutputTrajectoryPointNumber) :
        GetTrajectoryOutputPointNumber(K,OutputTrajectoryPointNumber);
    return (int)M;
  };

  while (nReadTrajectoryNumber!=nTotalTracedTrajectories) {
    UsedTrajectoryPointBuffer=0;
    StartTrajectoryNumber=nReadTrajectoryNumber;

    //reset the offset array
    for (i=0;i<nTotalTracedTrajectories;i++) SampledTrajectoryDataOffset[i]=-1,nReadSampledTrajectoryPoints[i]=0;

    while (UsedTrajectoryPointBuffer+OutputPointNumber(nSampledTrajectoryPoints[nReadTrajectoryNumber])<=TrajectoryPointBufferLength) {
      SampledTrajectoryDataOffset[nReadTrajectoryNumber]=UsedTrajectoryPointBuffer;
      UsedTrajectoryPointBuffer+=OutputPointNumber(nSampledTrajectoryPoints[nReadTrajectoryNumber]);
      nReadTrajectoryNumber++;

      if (nReadTrajectoryNumber==nTotalTracedTrajectories) break;
    }

    //scroll throught all trajectory files and locate all points that corresponds to this trajectories
    FILE *fTrajectoryData;
    cTrajectoryDataRecord TrajectoryRecord;
    int GlobalTrajectoryNumber,ReadTrajectoryPoints=0;
    int TrajectoryRecordLength=sizeof(cTrajectoryDataRecord);

    for (thread=0;thread<nTotalThreads;thread++) {
      for (nfile=0;nfile<nTrajectoryDataFiles[thread];nfile++) {
        sprintf(str,"%s/ParticleTrackerTmp/amps.ParticleTracker.thread=%i.out=%i.TrajectoryData.pt",PIC::OutputDataFileDirectory,thread,nfile);

        fTrajectoryData=NULL;
        fTrajectoryData=fopen(str,"r");
        if (fTrajectoryData==NULL) exit(__LINE__,__FILE__,"Error: cannot open file");

        if (fread(&length,sizeof(unsigned long int),1,fTrajectoryData)!=1) exit(__LINE__,__FILE__,"Error: file reading error");

        for (i=0;i<length;i++) {
          if (fread(&TrajectoryRecord,TrajectoryRecordLength,1,fTrajectoryData)!=1) exit(__LINE__,__FILE__,"Error: file reading error");
          GlobalTrajectoryNumber=TrajectoryRecord.Trajectory.id+TrajectoryCounterOffset[TrajectoryRecord.Trajectory.StartingThread];

          if (GlobalTrajectoryNumber>=nTotalTracedTrajectories) exit(__LINE__,__FILE__,"Error: out of range");

          if (SampledTrajectoryDataOffset[GlobalTrajectoryNumber]!=-1) {
            unsigned long int Kthis=nSampledTrajectoryPoints[GlobalTrajectoryNumber];

            //map the raw sampling offset to a dense output ordinal 0..M-1 using the
            //thinning-aware selector (mode ON) or the exact-N offline rule (mode OFF).
            //ordinal<0 marks an off-grid sample that must be discarded.
            long int ordinal = (TrajectorySamplingThinningMode==true) ?
              GetThinnedTrajectoryPointOrdinal(TrajectoryRecord.offset,TrajectoryRecord.ForcedEndpoint,Kthis,OutputTrajectoryPointNumber) :
              GetSelectedTrajectoryPointOrdinal(TrajectoryRecord.offset,Kthis,OutputTrajectoryPointNumber);

            if (ordinal<0) continue;

            int el=SampledTrajectoryDataOffset[GlobalTrajectoryNumber]+(int)ordinal;
            if ((el<0)||(el>=TrajectoryPointBufferLength)||(ordinal>=(long int)OutputPointNumber(Kthis))) exit(__LINE__,__FILE__,"Error: out of range");

            TempTrajectoryBuffer[el]=TrajectoryRecord.data;
            ++nReadSampledTrajectoryPoints[GlobalTrajectoryNumber];
            ++ReadTrajectoryPoints;

            if (nReadSampledTrajectoryPoints[GlobalTrajectoryNumber]>nSampledTrajectoryPoints[GlobalTrajectoryNumber]) {
              exit(__LINE__,__FILE__,"Error: the number of the read trajectory number is out of range");
            }
          }
        }

        fclose(fTrajectoryData);

        //if all points are found stop reading the trajectory files
        if (ReadTrajectoryPoints==UsedTrajectoryPointBuffer) {
          break;
        }
      }
    }

    //save the found trajectories
    long int tr,offset;
    int StartTrajectorySpec;
    cTrajectoryPhysicalData *TrajectoryData;
    FILE *trOut;

    for (tr=StartTrajectoryNumber;tr<nReadTrajectoryNumber;tr++) {
      offset=SampledTrajectoryDataOffset[tr];
      StartTrajectorySpec=-1;

      for (i=0;i<nReadSampledTrajectoryPoints[tr];i++) {
        TrajectoryData=TempTrajectoryBuffer+offset+i;
        
        switch(_PIC_PARTICLE_TRACKER__TRAJECTORY_OUTPUT_MODE_) {
        case _PIC_PARTICLE_TRACKER__TRAJECTORY_OUTPUT_MODE__ENTIRE_TRAJECTORY_:
          if (StartTrajectorySpec==-1) {
            trOut=fout[TrajectoryData->spec];

            //print the header of the new trajectory
            fprintf(trOut,"ZONE T=\"Trajectory=%i\" F=POINT\n",TrajectoryCounter[TrajectoryData->spec]);
            ++TrajectoryCounter[TrajectoryData->spec];
            StartTrajectorySpec=TrajectoryData->spec;
          }
          break;
        case _PIC_PARTICLE_TRACKER__TRAJECTORY_OUTPUT_MODE__SPECIES_TYPE_SEGMENTS_:
          if ((StartTrajectorySpec==-1)||(StartTrajectorySpec!=TrajectoryData->spec)) {
            trOut=fout[TrajectoryData->spec];

            //print the header of the new trajectory
            fprintf(trOut,"ZONE T=\"Trajectory=%i\" F=POINT\n",TrajectoryCounter[TrajectoryData->spec]);
            ++TrajectoryCounter[TrajectoryData->spec];
            StartTrajectorySpec=TrajectoryData->spec;
          }
          break;
        default:
          exit(__LINE__,__FILE__,"Error: unknown option");
          break;
        }

        fprintf(trOut,"%e  %e  %e  %i  %e   %e  %e  %e",TrajectoryData->x[0],TrajectoryData->x[1],TrajectoryData->x[2],
            TrajectoryData->spec,TrajectoryData->Speed,TrajectoryData->v[0],TrajectoryData->v[1],TrajectoryData->v[2]);

        double KineticEnergy;

        if (Relativistic::GetGamma(TrajectoryData->v)<0.5) {
          KineticEnergy=PIC::MolecularData::GetMass(TrajectoryData->spec)*Vector3D::DotProduct(TrajectoryData->v,TrajectoryData->v)/2.0;
        }
        else {
          KineticEnergy=Relativistic::Vel2E(TrajectoryData->v,PIC::MolecularData::GetMass(TrajectoryData->spec)); 
        }

        fprintf(trOut," %e",KineticEnergy/ElectronCharge);

#if _PIC_MODEL__DUST__MODE_ == _PIC_MODEL__DUST__MODE__ON_ 
        fprintf(trOut," %e  %e ",TrajectoryData->ElectricCharge,TrajectoryData->ParticleSize);
#endif

#if _PIC_MOVER_INTEGRATOR_MODE_ == _PIC_MOVER_INTEGRATOR_MODE__GUIDING_CENTER_  
        fprintf(trOut," %e ",TrajectoryData->KineticEnergy);
#endif 

#if _PIC_PARTICLE_TRACKER__TRAJECTORY_TIME_STAMP_MODE_ == _PIC_MODE_ON_  
        fprintf(trOut," %e ",TrajectoryData->TimeStamp);
#endif 

#if _PIC_PARTICLE_TRACKER__INJECTION_FACE_MODE_ ==  _PIC_MODE_ON_  
        fprintf(trOut," %i ",TrajectoryData->InjectionFaceNumber);
#endif

#if _PIC_PARTICLE_TRACKER__PARTICLE_WEIGHT_OVER_LOCAL_TIME_STEP_MODE_ == _PIC_MODE_ON_  
        fprintf(trOut," %e ",TrajectoryData->ParticleWeightOverLocalTimeStepRatio);
#endif 

        fprintf(trOut,"\n");
      }
    }

  }

  //close all open files and deallocate the temporatry data buffers
  for (spec=0;spec<nTotalSpecies;spec++) {
    fclose(fout[spec]);
    fout[spec]=NULL;
  }

  delete [] TempTrajectoryBuffer;
  delete [] nSampledTrajectoryPoints;
  delete [] SampledTrajectoryDataOffset;
  delete [] nReadSampledTrajectoryPoints;

  delete [] nTrajectoryListFiles;
  delete [] nTrajectoryDataFiles;
  delete [] nTotalSampledTrajectories;

  fclose(fTrajectoryDataSet);
}



//set up the default tracking flag to all particles
void PIC::ParticleTracker::SetDefaultParticleTrackingFlag(void* StartNodeVoid) {
  int i,j,k;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode,*StartNode;
  cParticleData *DataRecord;
  long int ptr;

  StartNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*) StartNodeVoid;
  if (StartNode==NULL) StartNode=PIC::Mesh::mesh->rootTree;

  //serch the tree
  for (i=0;i<(1<<DIM);i++) if ((downNode=StartNode->downNode[i])!=NULL) SetDefaultParticleTrackingFlag(downNode);

  //reset the particles
  if (StartNode->block!=NULL) {
    long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];

    memcpy(FirstCellParticleTable,StartNode->block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

    for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++)  for (i=0;i<_BLOCK_CELLS_X_;i++)  {
      ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

      while (ptr!=-1) {
        DataRecord=(cParticleData*)(ParticleDataRecordOffset+PIC::ParticleBuffer::GetParticleDataPointer(ptr));
        DataRecord->TrajectoryTrackingFlag=false;

        ptr=PIC::ParticleBuffer::GetNext(ptr);
      }
    }
  }
}

//set the particle tracking flag "on"
void PIC::ParticleTracker::StartParticleTrajectoryTracking(void *ParticleData) {
  cParticleData *DataRecord=(cParticleData*)(ParticleDataRecordOffset+((PIC::ParticleBuffer::byte*)ParticleData));

  DataRecord->TrajectoryTrackingFlag=true;
}

void PIC::ParticleTracker::StopParticleTrajectoryTracking(void *ParticleData) {
  cParticleData *DataRecord=(cParticleData*)(ParticleDataRecordOffset+((PIC::ParticleBuffer::byte*)ParticleData));

  if (DataRecord->TrajectoryTrackingFlag==true) FinilazeParticleRecord(ParticleData);
  DataRecord->TrajectoryTrackingFlag=false;
}

//the default particle trajectory tracking condition
bool PIC::ParticleTracker::TrajectoryTrackingCondition_default(double *x,double *v,int spec,void *ParticleData) {

  //start particle trackeing only after the data output number has reached that requesterd by the user
  if (PIC::DataOutputFileNumber<_PIC_PARTICLE_TRACKER__BEGIN_TRACKING_FILE_OUTPUT_NUMBER_) return false;

  //get the OpenMP thread number
  int threadOpenMP=0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  threadOpenMP=omp_get_thread_num();
#endif

  if ((maxSampledTrajectoryNumber>totalSampledTrajectoryNumber[spec])&&(maxSampledTrajectoryNumber>threadSampledTrajectoryNumber[spec][threadOpenMP])) {
    return true;
  }

  return false;
}

//apply the particle tracking condition to a particle
void PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(double *x,double *v,int spec,void *ParticleData,void *nodeIn) {
  bool flag=false;
  cParticleData *DataRecord=(cParticleData*)(ParticleDataRecordOffset+(PIC::ParticleBuffer::byte*)ParticleData);

  if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
    //check the tracking condition ONLY for partilces that was not tracked previously
    if (DataRecord->TrajectoryTrackingFlag==true) return;
  }


  if (PIC::DataOutputFileNumber>=_PIC_PARTICLE_TRACKER__BEGIN_TRACKING_FILE_OUTPUT_NUMBER_) {
    flag=_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_(x,v,spec,ParticleData);
  }

  if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
    DataRecord->TrajectoryTrackingFlag=flag;

    if (flag==true) {
      int threadOpenMP=0;

#if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
      threadOpenMP=omp_get_thread_num();
#endif

      DataRecord->nSampledTrajectoryPoints=0;
      DataRecord->Trajectory.StartingThread=threadOpenMP+PIC::nTotalThreadsOpenMP*PIC::ThisThread;
      DataRecord->Trajectory.id=SampledTrajectoryCounter[threadOpenMP];

      RecordTrajectoryPoint(x,v,spec,ParticleData,nodeIn);

      //increment the trajectory counter
      ++threadSampledTrajectoryNumber[spec][threadOpenMP];
      ++SampledTrajectoryCounter[threadOpenMP];
    }
  }
}

//apply the particle tracking condition to all particles in the simulation
void PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(void* StartNodeVoid) {
  int i,j,k;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *downNode,*StartNode;
  long int ptr;

  double x[3],v[3];
  int spec;
  PIC::ParticleBuffer::byte* ParticleData;

  StartNode=(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*) StartNodeVoid;
  if (StartNode==NULL) StartNode=PIC::Mesh::mesh->rootTree;

  //serch the tree
  for (i=0;i<(1<<DIM);i++) if ((downNode=StartNode->downNode[i])!=NULL) ApplyTrajectoryTrackingCondition(downNode);

  //reset the particles
  if (StartNode->block!=NULL) {
    long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];

    memcpy(FirstCellParticleTable,StartNode->block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

    for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++)  for (i=0;i<_BLOCK_CELLS_X_;i++)  {
      ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

      while (ptr!=-1) {
        ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
        spec=PIC::ParticleBuffer::GetI(ParticleData);
        PIC::ParticleBuffer::GetX(x,ParticleData);
        PIC::ParticleBuffer::GetV(v,ParticleData);

        ApplyTrajectoryTrackingCondition(x,v,spec,ParticleData,StartNode);

        ptr=PIC::ParticleBuffer::GetNext(ptr);
      }
    }
  }
}










