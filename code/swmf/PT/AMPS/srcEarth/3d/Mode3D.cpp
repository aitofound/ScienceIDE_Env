#include "Mode3D.h"
#include "ElectricField.h"
#include "CutoffRigidityMode3D.h"
#include "DensityMode3D.h"
#include "GlobalMagneticField.h"
#include "Mode3DParallel.h"
#include "../gridless/DipoleInterface.h"

#include <cstdio>
#include <cmath>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <map>
#include <pthread.h>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>

#ifndef _NO_SPICE_CALLS_
#include "SpiceUsr.h"
#endif

#include "pic.h"

#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
#include "../../interface/T96Interface.h"
#include "../../interface/T05Interface.h"
#include "../../interface/TA16Interface.h"
#include "GeopackInterface.h"
#endif

void amps_init_mesh();
void amps_init();

// Sphere surface-mesh resolution parameters and the per-surface-element
// resolution function are defined in main_lib.cpp and shared with Mode3D.
extern int nZenithElements;
extern int nAzimuthalElements;
double localSphericalSurfaceResolution(double *x);

namespace Earth {
namespace Mode3D {

bool ParsedDomainActive=false;
double ParsedDomainMin[3]={0.0,0.0,0.0};
double ParsedDomainMax[3]={0.0,0.0,0.0};

// User-defined Mode3D AMR mesh-resolution profile.  These globals are read by
// main_lib.cpp::localResolution() while AMPS builds the tree.  The inactive default
// preserves the historical hard-coded resolution function exactly.
bool MeshResolutionProfileActive=false;
double MeshResolutionEarth_m=0.0;
double MeshResolutionBoundary_m=0.0;
double MeshResolutionOuterRadius_Re=0.0;
double MeshResolutionExponent=1.0;
int MeshResolutionCoarseningCode=0; // 0=LINEAR, 1=LOG/EXPONENTIAL, 2=POWER, 3=CONSTANT

namespace {

// Standalone Mode3D mesh coordinates, trajectory positions, and analytic field
// evaluation are all expressed in GSM.  Empirical-field wrappers must therefore
// be initialized in GSM as well; using Exosphere::SO_FRAME here is incorrect
// because its global default is GSE and rotates the populated field relative to
// the Mode3D mesh.
constexpr const char* kStandaloneMode3DFieldFrame = "GSM";

//--------------------------------------------------------------------------------------
// Mode3D calculation-target helpers
//--------------------------------------------------------------------------------------
//
// The parser stores CALC_TARGET as one string for backward compatibility.  For Mode3D we
// intentionally accept both historical single-product tokens and compact combined tokens
// so one input file can request:
//   CALC_TARGET  CUTOFF_RIGIDITY                 -> cutoff only
//   CALC_TARGET  DENSITY_SPECTRUM                -> density + flux only
//   CALC_TARGET  CUTOFF_RIGIDITY+DENSITY_SPECTRUM -> both products from one field snapshot
//
// We use substring tests rather than a rigid enum so separators such as '+', ',', or
// whitespace (if preserved by an input reader) all work.  The solver still fails below if
// neither recognized product is requested.
static bool Mode3DTargetRequestsCutoff(const EarthUtil::AmpsParam& prm) {
  const std::string t = EarthUtil::ToUpper(prm.calc.target);
  return t.find("CUTOFF") != std::string::npos || t=="ALL" || t=="BOTH";
}

static bool Mode3DTargetRequestsDensityFlux(const EarthUtil::AmpsParam& prm) {
  const std::string t = EarthUtil::ToUpper(prm.calc.target);
  return t.find("DENSITY") != std::string::npos || t.find("FLUX") != std::string::npos ||
         t=="ALL" || t=="BOTH";
}


void ApplyParsedDomain(const EarthUtil::AmpsParam& prm) {
  ParsedDomainActive=true;
  ParsedDomainMin[0]=prm.domain.xMin*1000.0;
  ParsedDomainMin[1]=prm.domain.yMin*1000.0;
  ParsedDomainMin[2]=prm.domain.zMin*1000.0;
  ParsedDomainMax[0]=prm.domain.xMax*1000.0;
  ParsedDomainMax[1]=prm.domain.yMax*1000.0;
  ParsedDomainMax[2]=prm.domain.zMax*1000.0;
}

static double MaxAbsDomainFaceRadiusRe(const EarthUtil::AmpsParam& prm) {
  const double rEarth_km = _EARTH__RADIUS_ / 1000.0;
  double rmax_km = 0.0;
  rmax_km = std::max(rmax_km, std::fabs(prm.domain.xMin));
  rmax_km = std::max(rmax_km, std::fabs(prm.domain.xMax));
  rmax_km = std::max(rmax_km, std::fabs(prm.domain.yMin));
  rmax_km = std::max(rmax_km, std::fabs(prm.domain.yMax));
  rmax_km = std::max(rmax_km, std::fabs(prm.domain.zMin));
  rmax_km = std::max(rmax_km, std::fabs(prm.domain.zMax));
  return (rEarth_km > 0.0) ? (rmax_km / rEarth_km) : 0.0;
}

void ConfigureBackgroundFieldModel(const EarthUtil::AmpsParam& prm) {
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // In the live AMPS--SWMF coupling mode the background magnetic field is not
  // selected from the standalone Tsyganenko/TA16 wrappers.  SWMF supplies the
  // MHD state to AMPS through PIC::CPLR, and the field that should be used by
  // particle movers / diagnostics is exposed by the standard AMPS coupler
  // accessors:
  //
  //   PIC::CPLR::InitInterpolationStencil(...)
  //   PIC::CPLR::GetBackgroundMagneticField(...)
  //
  // Therefore this setup routine must be a no-op in SWMF builds: do not set
  // Earth::T96/Earth::T05 active flags and do not call ::T96::Init(),
  // ::T05::Init(), or ::TA16::Init().  Those model interfaces may not even be
  // linked in the coupled executable.
  (void)prm;
#else
  Earth::T96::active_flag=false;
  Earth::T05::active_flag=false;
  Earth::BackgroundMagneticFieldModelType=Earth::_undef;

  const std::string model=EarthUtil::ToUpper(prm.field.model);
  if (model=="DIPOLE") {
    // Configure the shared analytic-dipole parameters once, before any temporary
    // field-initialization pthreads are launched.  Evaluation then reads this
    // frozen state only; it must not call the setters concurrently from workers.
    Earth::GridlessMode::Dipole::SetMomentScale(prm.field.dipoleMoment_Me);
    Earth::GridlessMode::Dipole::SetTiltDeg(prm.field.dipoleTilt_deg);
  }
  else if (model=="IGRF") {
    // Pure internal-field initialization used by the gridded C6 validation.
    //
    // Initialize the selected IGRF coefficients and GSM transformation context
    // once before InitMeshFields() traverses the AMR tree.  The reentrant IGRF
    // evaluator then reads that frozen context concurrently from the temporary
    // field-initialization pthreads.
    Geopack::Init(prm.field.epoch.c_str(),kStandaloneMode3DFieldFrame);
  }
  else if (model=="T96") {
    Earth::BackgroundMagneticFieldModelType=Earth::_t96;
    Earth::T96::active_flag=true;
    Earth::T96::solar_wind_pressure=prm.field.pdyn_nPa*_NANO_;
    Earth::T96::dst=prm.field.dst_nT*_NANO_;
    Earth::T96::by=prm.field.imfBy_nT*_NANO_;
    Earth::T96::bz=prm.field.imfBz_nT*_NANO_;
    ::T96::SetSolarWindPressure(Earth::T96::solar_wind_pressure);
    ::T96::SetDST(Earth::T96::dst);
    ::T96::SetBYIMF(Earth::T96::by);
    ::T96::SetBZIMF(Earth::T96::bz);
    ::T96::Init(prm.field.epoch.c_str(),kStandaloneMode3DFieldFrame);
  }
  else if (model=="T05") {
    Earth::BackgroundMagneticFieldModelType=Earth::_t05;
    Earth::T05::active_flag=true;
    Earth::T05::solar_wind_pressure=prm.field.pdyn_nPa*_NANO_;
    Earth::T05::dst=prm.field.dst_nT*_NANO_;
    Earth::T05::by=prm.field.imfBy_nT*_NANO_;
    Earth::T05::bz=prm.field.imfBz_nT*_NANO_;
    for (int i=0;i<6;i++) Earth::T05::W[i]=prm.field.w[i];
    ::T05::SetSolarWindPressure(Earth::T05::solar_wind_pressure);
    ::T05::SetDST(Earth::T05::dst);
    ::T05::SetBXIMF(prm.field.imfBx_nT*_NANO_);
    ::T05::SetBYIMF(Earth::T05::by);
    ::T05::SetBZIMF(Earth::T05::bz);
    ::T05::SetW(Earth::T05::W[0],Earth::T05::W[1],Earth::T05::W[2],Earth::T05::W[3],Earth::T05::W[4],Earth::T05::W[5]);
    ::T05::Init(prm.field.epoch.c_str(),kStandaloneMode3DFieldFrame);
  }
  else if (model=="TA16") {
    // TA16 does not use BackgroundMagneticFieldModelType — it is driven
    // entirely through _PIC_COUPLER_MODE__TA16_ compile-time guards,
    // consistent with TA15 and T01.
    if (!prm.field.ta16CoeffFile.empty())
      ::TA16::SetCoeffFileName(prm.field.ta16CoeffFile);
    // TA16 PARMOD: [PDYN, SymHc, XIND, BYIMF, W1..W6]
    // SetSolarWindPressure/SetSymHc accept SI values (Pa / T); the _NANO_
    // factor converts from nPa / nT to SI, matching the T05 convention.
    ::TA16::SetSolarWindPressure(prm.field.pdyn_nPa*_NANO_);
    ::TA16::SetSymHc(prm.field.dst_nT*_NANO_);
    ::TA16::SetXIND(prm.field.xind);
    ::TA16::SetBYIMF(prm.field.imfBy_nT*_NANO_);
    ::TA16::Init(prm.field.epoch.c_str(),kStandaloneMode3DFieldFrame);
  }

  if (PIC::ThisThread==0 &&
      (model=="DIPOLE" || model=="IGRF" || model=="T96" || model=="T05" || model=="TA16")) {
    std::cout << "[Mode3D] Mesh coordinate frame: "
              << kStandaloneMode3DFieldFrame << "\n";
    std::cout << "[Mode3D] " << model << " interface frame: "
              << kStandaloneMode3DFieldFrame
              << " (matches mesh coordinates)\n";
  }
#endif
}

// Traverse the full AMR tree and write B and E into every cell's data buffer,
// following the same ghost-cell-inclusive iteration pattern used by
// Earth::InitMagneticField in Earth.cpp.  Unlike that function, this version:
//   - uses EvaluateBackgroundMagneticFieldSI / EvaluateElectricFieldSI so all
//     standalone models (IGRF, T96, T05, TA16, DIPOLE) are handled uniformly, and
//   - initialises E from the configured electric-field model instead of
//     unconditionally writing zero.
//
// When prm.mode3d.parallelFieldInitialization is true, each MPI rank builds a
// temporary POSIX-thread team.  The temporary pthread count is the same value
// resolved from MODE3D_THREADS/-mode3d-threads for the calculation.  The calling
// MPI-rank thread also participates, so N means N temporary workers plus one
// caller.  The flat owner-cell range is divided statically by index because every
// background-field evaluation has approximately the same cost.
using FieldInitNode = cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>;

struct FieldInitSharedWork {
  const EarthUtil::AmpsParam* prm{nullptr};
  const std::vector<FieldInitNode*>* ownerBlocks{nullptr};

  int iMin{0},iCount{0};
  int jMin{0},jCount{0};
  int kMin{0},kCount{0};
  int cellsPerBlock{0};
  int participantCount{1};

  // Completion counters are rank-local and are updated by every temporary
  // pthread as well as by the original MPI-rank thread.  They are deliberately
  // std::atomic rather than MPI objects: worker pthreads must NEVER call MPI
  // because AMPS does not require MPI_THREAD_MULTIPLE for this Mode3D path.
  // The original rank/main thread periodically reads completedLocal and publishes
  // only the newly completed delta through DynamicMpiProgressCounter.
  std::atomic<long long> completedLocal;
  std::atomic<int> workersFinished;

  // Creation gate: workers are not allowed to start until every requested
  // pthread has been created successfully.  On a partial creation failure the
  // already-created workers are released with abort=true and joined cleanly.
  pthread_mutex_t gateMutex;
  pthread_cond_t gateCond;
  bool gateReleased{false};
  bool abort{false};

  FieldInitSharedWork() : completedLocal(0),workersFinished(0) {}
};

struct FieldInitWorkerArg {
  FieldInitSharedWork* shared{nullptr};
  int participantId{0};
};

//======================================================================================
// Background-field initialization progress reporting
//======================================================================================
// Keep the visual grammar intentionally identical to the cutoff progress bar:
//
//   [Mode3D field INITIALIZATION] [rank 0/global over N MPI ranks]
//   [########----------------------------] 22.2%  (Cell 1234/5555) ETA 00:03:17
//
// In particular, both bars use a 36-character body, '#' for completed work, and
// '-' for remaining work.  This is useful in long C19 runs because the user can
// identify initialization and trajectory progress at a glance without learning
// two different bar conventions.
//
// The cutoff solver currently throttles normal progress output at one line per
// second.  Background-field initialization is less variable and can involve a
// very large number of cell slots, so its normal print interval is deliberately
// TWO seconds (a factor-of-two reduction in output frequency requested for this
// stage).  Initial and final lines are still always printed.
//
// Every emitted line is explicitly flushed.  C19 is commonly launched under
// mpirun with stdout redirected through a batch scheduler or tee; relying on
// newline buffering alone can otherwise make the progress display arrive in
// large delayed bursts instead of in real time.
//======================================================================================
constexpr double kFieldInitProgressPrintIntervalSeconds = 2.0;
constexpr double kFieldInitProgressPublishIntervalSeconds = 0.25;
constexpr int kFieldInitProgressBarWidth = 36;

static std::string FormatFieldInitHms(double seconds) {
  if (seconds < 0.0) return std::string("--:--:--");
  long long is=static_cast<long long>(std::llround(seconds));
  const long long hh=is/3600; is-=hh*3600;
  const long long mm=is/60;   is-=mm*60;
  const long long ss=is;
  char buf[64];
  std::snprintf(buf,sizeof(buf),"%02lld:%02lld:%02lld",hh,mm,ss);
  return std::string(buf);
}

class FieldInitProgressReporter {
public:
  explicit FieldInitProgressReporter(long long localTotal)
      : localTotal_(std::max(0LL,localTotal)) {
    MPI_Comm_rank(MPI_GLOBAL_COMMUNICATOR,&rank_);
    MPI_Comm_size(MPI_GLOBAL_COMMUNICATOR,&mpiSize_);

    // totalGlobal_ is the exact number of owner-cell SLOTS traversed over all
    // MPI ranks, including the normal ghost-cell-inclusive block range.  Some
    // slots can have a null center-node pointer and therefore require no field
    // write; they still count as completed work because the initialization loop
    // must visit and classify each slot.
    MPI_Allreduce(&localTotal_,&totalGlobal_,1,MPI_LONG_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

    progressCounter_=new DynamicMpiProgressCounter(
        MPI_GLOBAL_COMMUNICATOR,totalGlobal_,"Mode3D background-field initialization progress");

    startTime_=MPI_Wtime();
    lastPublishTime_=startTime_;
    lastPrintTime_=-1.0;

    // Match the cutoff progress display by printing an explicit 0% line before
    // expensive field evaluation starts.  The force flag bypasses the normal
    // two-second throttle.
    Publish(0,true);
  }

  ~FieldInitProgressReporter() {
    delete progressCounter_;
    progressCounter_=nullptr;
  }

  void Publish(long long localCompleted,bool force) {
    if (progressCounter_==nullptr) return;

    const double now=MPI_Wtime();

    // Do not perform an MPI RMA accumulate for every cell.  Worker pthreads can
    // finish many cells between calls; the main thread publishes their aggregate
    // rank-local delta at most four times per second.  This keeps progress
    // overhead negligible while still feeding the two-second user-facing bar
    // with fresh global counts.
    if (!force && now-lastPublishTime_<kFieldInitProgressPublishIntervalSeconds) return;
    lastPublishTime_=now;

    if (localCompleted<0) localCompleted=0;
    if (localCompleted>localTotal_) localCompleted=localTotal_;

    const long long delta=localCompleted-publishedLocal_;
    if (delta>0) {
      progressCounter_->Add(delta);
      publishedLocal_=localCompleted;
    }

    if (rank_!=0) return;

    const long long globalCompleted=progressCounter_->Get();
    MaybePrintGlobal(globalCompleted,now,force);
  }

  // Publish the final rank-local count and keep the rank/main thread alive until
  // every MPI rank has published all of its cells.  This polling phase matters
  // when rank 0 happens to finish early: it lets rank 0 continue printing global
  // progress instead of blocking in MPI_Win_free while another rank is still
  // evaluating its last expensive field cells.
  void Finish(long long localCompleted) {
    Publish(localCompleted,true);

    while (true) {
      const long long globalCompleted=progressCounter_->Get();
      const double now=MPI_Wtime();
      if (rank_==0) MaybePrintGlobal(globalCompleted,now,false);
      if (globalCompleted>=totalGlobal_) break;

      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Always emit and flush an exact 100% line, even if the previous throttled
    // update happened only milliseconds earlier.
    if (rank_==0) MaybePrintGlobal(totalGlobal_,MPI_Wtime(),true);
  }

  long long TotalGlobal() const { return totalGlobal_; }

private:
  void MaybePrintGlobal(long long done,double now,bool force) {
    if (rank_!=0) return;

    if (!force) {
      if (lastPrintTime_>=0.0 &&
          now-lastPrintTime_<kFieldInitProgressPrintIntervalSeconds) return;
    }
    lastPrintTime_=now;

    if (done<0) done=0;
    if (done>totalGlobal_) done=totalGlobal_;

    const double frac=(totalGlobal_>0)
        ? static_cast<double>(done)/static_cast<double>(totalGlobal_) : 1.0;
    const double elapsed=now-startTime_;
    const double rate=(elapsed>0.0) ? static_cast<double>(done)/elapsed : 0.0;
    double eta=-1.0;
    if (rate>0.0 && totalGlobal_>done)
      eta=static_cast<double>(totalGlobal_-done)/rate;

    int filled=static_cast<int>(std::floor(frac*kFieldInitProgressBarWidth+0.5));
    if (filled<0) filled=0;
    if (filled>kFieldInitProgressBarWidth) filled=kFieldInitProgressBarWidth;

    std::ostringstream line;
    line << "[Mode3D field INITIALIZATION] ";
    line << "[rank 0/global over " << mpiSize_ << " MPI ranks] ";
    line << "[";
    for (int i=0;i<kFieldInitProgressBarWidth;++i)
      line << (i<filled ? "#" : "-");
    line << "] ";

    line.setf(std::ios::fixed);
    line.precision(1);
    line << (100.0*frac) << "%  ";
    line << "(Cell " << done << "/" << totalGlobal_ << ")  ETA "
         << FormatFieldInitHms(eta) << "\n";

    std::cout << line.str();
    std::cout.flush();
  }

  int rank_{0};
  int mpiSize_{1};
  long long localTotal_{0};
  long long totalGlobal_{0};
  long long publishedLocal_{0};
  double startTime_{0.0};
  double lastPublishTime_{0.0};
  double lastPrintTime_{-1.0};
  DynamicMpiProgressCounter* progressCounter_{nullptr};
};

void CollectOwnerFieldInitBlocks(FieldInitNode* node,
                                 std::vector<FieldInitNode*>& ownerBlocks) {
  if (node==nullptr) return;

  if (node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    if (node->block!=NULL && node->Thread==PIC::ThisThread) {
      ownerBlocks.push_back(node);
    }
    return;
  }

  for (int i=0;i<(1<<DIM);++i) {
    if (node->downNode[i]!=NULL) {
      CollectOwnerFieldInitBlocks(node->downNode[i],ownerBlocks);
    }
  }
}

void InitializeOneMeshFieldCell(const EarthUtil::AmpsParam& prm,
                                FieldInitNode* node,
                                int i,int j,int k) {
  const int nd=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
  PIC::Mesh::cDataCenterNode* CenterNode=node->block->GetCenterNode(nd);
  if (CenterNode==NULL) return;

  char* offset=CenterNode->GetAssociatedDataBufferPointer()
              +PIC::CPLR::DATAFILE::CenterNodeAssociatedDataOffsetBegin
              +PIC::CPLR::DATAFILE::MULTIFILE::CurrDataFileOffset;

  double xCell[3];
  xCell[0]=node->xmin[0]+(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_*(0.5+i);
  xCell[1]=node->xmin[1]+(node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_*(0.5+j);
  xCell[2]=node->xmin[2]+(node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_*(0.5+k);

  double B[3],E[3];
  EvaluateBackgroundMagneticFieldSI(B,xCell,prm);
  EvaluateElectricFieldSI(E,xCell,prm);

  for (int idim=0;idim<3;idim++) {
    if (PIC::CPLR::DATAFILE::Offset::MagneticField.active==true) {
      *((double*)(offset+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+idim*sizeof(double)))=B[idim];
    }
    if (PIC::CPLR::DATAFILE::Offset::ElectricField.active==true) {
      *((double*)(offset+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+idim*sizeof(double)))=E[idim];
    }
  }
}

static void ConfigureFieldInitSharedWork(FieldInitSharedWork& shared,
                                         const EarthUtil::AmpsParam& prm,
                                         const std::vector<FieldInitNode*>& ownerBlocks,
                                         int participantCount) {
  shared.prm=&prm;
  shared.ownerBlocks=&ownerBlocks;
  shared.iMin=-_GHOST_CELLS_X_;
  shared.jMin=-_GHOST_CELLS_Y_;
  shared.kMin=-_GHOST_CELLS_Z_;
  shared.iCount=_BLOCK_CELLS_X_+2*_GHOST_CELLS_X_;
  shared.jCount=_BLOCK_CELLS_Y_+2*_GHOST_CELLS_Y_;
  shared.kCount=_BLOCK_CELLS_Z_+2*_GHOST_CELLS_Z_;
  shared.cellsPerBlock=shared.iCount*shared.jCount*shared.kCount;
  shared.participantCount=std::max(1,participantCount);
  shared.completedLocal.store(0,std::memory_order_relaxed);
  shared.workersFinished.store(0,std::memory_order_relaxed);
}

void ProcessFieldInitParticipant(FieldInitSharedWork& shared,
                                 int participantId,
                                 FieldInitProgressReporter* reporter=nullptr) {
  const std::size_t totalWork=
      shared.ownerBlocks->size()*static_cast<std::size_t>(shared.cellsPerBlock);

  // Static equal partition.  This formulation distributes a possible remainder
  // by at most one cell and gives the original MPI-rank thread exactly the same
  // kind of share as every temporary pthread worker.
  const std::size_t begin=
      totalWork*static_cast<std::size_t>(participantId)
      /static_cast<std::size_t>(shared.participantCount);
  const std::size_t end=
      totalWork*static_cast<std::size_t>(participantId+1)
      /static_cast<std::size_t>(shared.participantCount);

  for (std::size_t flat=begin;flat<end;++flat) {
    const std::size_t iBlock=flat/static_cast<std::size_t>(shared.cellsPerBlock);
    int local=static_cast<int>(flat%static_cast<std::size_t>(shared.cellsPerBlock));

    const int i=shared.iMin+local/(shared.jCount*shared.kCount);
    local%=shared.jCount*shared.kCount;
    const int j=shared.jMin+local/shared.kCount;
    const int k=shared.kMin+local%shared.kCount;

    InitializeOneMeshFieldCell(*shared.prm,(*shared.ownerBlocks)[iBlock],i,j,k);

    // Count the slot only after its field evaluation/write attempt is complete.
    // This makes the global progress bar a COMPLETION metric, matching the cutoff
    // bar, rather than an assignment counter that can run ahead of expensive work.
    const long long localDone=
        shared.completedLocal.fetch_add(1,std::memory_order_relaxed)+1;

    // Only participant 0 is the original MPI rank/main thread and therefore the
    // only participant allowed to touch the MPI RMA progress object.  Temporary
    // pthreads simply increment completedLocal.  Checking every 64 caller cells
    // avoids unnecessary clock/MPI overhead; Publish() applies an additional
    // quarter-second RMA throttle and a two-second print throttle.
    if (reporter!=nullptr && participantId==0 &&
        (((flat-begin)&63U)==0U || flat+1==end)) {
      reporter->Publish(localDone,false);
    }
  }
}

void* FieldInitPthreadEntry(void* rawArg) {
  FieldInitWorkerArg* arg=static_cast<FieldInitWorkerArg*>(rawArg);
  FieldInitSharedWork& shared=*arg->shared;

  pthread_mutex_lock(&shared.gateMutex);
  while (!shared.gateReleased) pthread_cond_wait(&shared.gateCond,&shared.gateMutex);
  const bool abort=shared.abort;
  pthread_mutex_unlock(&shared.gateMutex);

  if (!abort) ProcessFieldInitParticipant(shared,arg->participantId,nullptr);

  // workersFinished is used only for nonblocking progress polling by the original
  // MPI-rank thread.  pthread_join below remains the authoritative lifetime/join
  // operation and still reports any pthread error.
  shared.workersFinished.fetch_add(1,std::memory_order_release);
  return nullptr;
}

void InitMeshFieldsSerial(const EarthUtil::AmpsParam& prm,FieldInitNode* startNode) {
  // Use the same flat owner-block traversal as the threaded path.  This preserves
  // the historical ghost-cell-inclusive work exactly while allowing serial and
  // pthread initialization to share one global MPI progress implementation.
  std::vector<FieldInitNode*> ownerBlocks;
  CollectOwnerFieldInitBlocks(startNode,ownerBlocks);

  FieldInitSharedWork shared;
  ConfigureFieldInitSharedWork(shared,prm,ownerBlocks,1);

  const long long localTotal=static_cast<long long>(ownerBlocks.size())
      *static_cast<long long>(shared.cellsPerBlock);
  FieldInitProgressReporter reporter(localTotal);

  ProcessFieldInitParticipant(shared,0,&reporter);
  reporter.Finish(shared.completedLocal.load(std::memory_order_relaxed));
}

void InitMeshFieldsPthreads(const EarthUtil::AmpsParam& prm,FieldInitNode* startNode) {
  std::vector<FieldInitNode*> ownerBlocks;
  CollectOwnerFieldInitBlocks(startNode,ownerBlocks);

  const int temporaryWorkerCount=
      ResolveParallelThreadCount(prm,ParallelBackend::THREADS);

  // Even a rank with no owner blocks must participate in the collective progress
  // window.  Fall back to the serial implementation only when no temporary worker
  // team was requested; an empty local block list is valid in a distributed mesh.
  if (temporaryWorkerCount<=0) {
    InitMeshFieldsSerial(prm,startNode);
    return;
  }

  const int participantCount=temporaryWorkerCount+1;

  ApplyWideAffinityForDirectThreadsOnce(
      ParallelBackend::THREADS,participantCount,
      "Mode3D background-field initialization");

  FieldInitSharedWork shared;
  ConfigureFieldInitSharedWork(shared,prm,ownerBlocks,participantCount);

  const long long localTotal=static_cast<long long>(ownerBlocks.size())
      *static_cast<long long>(shared.cellsPerBlock);
  FieldInitProgressReporter reporter(localTotal);

  int rc=pthread_mutex_init(&shared.gateMutex,nullptr);
  if (rc!=0) {
    std::ostringstream msg;
    msg << "[Mode3D] pthread_mutex_init failed during background-field initialization: "
        << std::strerror(rc) << " (rc=" << rc << ")";
    exit(__LINE__,__FILE__,msg.str().c_str());
  }

  rc=pthread_cond_init(&shared.gateCond,nullptr);
  if (rc!=0) {
    pthread_mutex_destroy(&shared.gateMutex);
    std::ostringstream msg;
    msg << "[Mode3D] pthread_cond_init failed during background-field initialization: "
        << std::strerror(rc) << " (rc=" << rc << ")";
    exit(__LINE__,__FILE__,msg.str().c_str());
  }

  std::vector<pthread_t> threads(static_cast<std::size_t>(temporaryWorkerCount));
  std::vector<FieldInitWorkerArg> args(static_cast<std::size_t>(temporaryWorkerCount));
  int nCreated=0;

  for (int iWorker=0;iWorker<temporaryWorkerCount;++iWorker) {
    args[static_cast<std::size_t>(iWorker)].shared=&shared;
    args[static_cast<std::size_t>(iWorker)].participantId=iWorker+1;

    rc=pthread_create(&threads[static_cast<std::size_t>(iWorker)],nullptr,
                      FieldInitPthreadEntry,&args[static_cast<std::size_t>(iWorker)]);
    if (rc!=0) {
      pthread_mutex_lock(&shared.gateMutex);
      shared.abort=true;
      shared.gateReleased=true;
      pthread_cond_broadcast(&shared.gateCond);
      pthread_mutex_unlock(&shared.gateMutex);

      for (int i=0;i<nCreated;++i) {
        pthread_join(threads[static_cast<std::size_t>(i)],nullptr);
      }
      pthread_cond_destroy(&shared.gateCond);
      pthread_mutex_destroy(&shared.gateMutex);

      std::ostringstream msg;
      msg << "[Mode3D] pthread_create failed after creating " << nCreated
          << " of " << temporaryWorkerCount
          << " temporary background-field workers: " << std::strerror(rc)
          << " (rc=" << rc << ")";
      exit(__LINE__,__FILE__,msg.str().c_str());
    }
    ++nCreated;
  }

  if (PIC::ThisThread==0) {
    std::cout << "[Mode3D] Parallel background-field initialization: POSIX threads; "
              << temporaryWorkerCount << " temporary workers + caller ("
              << participantCount << " equal shares per MPI rank); "
              << reporter.TotalGlobal() << " owner-cell slots globally.\n";
    std::cout.flush();
  }

  // Release all temporary workers together, then let the original MPI-rank
  // thread process participant 0's equal share while the N temporary workers
  // process shares 1..N.
  pthread_mutex_lock(&shared.gateMutex);
  shared.gateReleased=true;
  pthread_cond_broadcast(&shared.gateCond);
  pthread_mutex_unlock(&shared.gateMutex);

  ProcessFieldInitParticipant(shared,0,&reporter);

  // Do not immediately block in pthread_join.  If temporary workers have a
  // slightly more expensive static share, keep the rank/main thread polling the
  // atomic completion counter so their work continues to appear in the live
  // global bar.  MPI remains confined to this original rank thread.
  while (shared.workersFinished.load(std::memory_order_acquire)<temporaryWorkerCount) {
    reporter.Publish(shared.completedLocal.load(std::memory_order_relaxed),false);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  int firstJoinError=0;
  for (int iWorker=0;iWorker<temporaryWorkerCount;++iWorker) {
    rc=pthread_join(threads[static_cast<std::size_t>(iWorker)],nullptr);
    if (rc!=0 && firstJoinError==0) firstJoinError=rc;
  }

  pthread_cond_destroy(&shared.gateCond);
  pthread_mutex_destroy(&shared.gateMutex);

  if (firstJoinError!=0) {
    std::ostringstream msg;
    msg << "[Mode3D] pthread_join failed during background-field initialization: "
        << std::strerror(firstJoinError) << " (rc=" << firstJoinError << ")";
    exit(__LINE__,__FILE__,msg.str().c_str());
  }

  reporter.Finish(shared.completedLocal.load(std::memory_order_relaxed));
}

void InitMeshFields(const EarthUtil::AmpsParam& prm,FieldInitNode* startNode) {
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // In SWMF-coupled builds the cell-centered B and E values are owned by the
  // live coupler data structures, not by the DATAFILE/MULTIFILE buffers that
  // this standalone initializer fills.  Leave the mesh field buffers untouched.
  (void)prm;
  (void)startNode;
  return;
#else
  if (prm.mode3d.parallelFieldInitialization) {
    InitMeshFieldsPthreads(prm,startNode);
  }
  else {
    InitMeshFieldsSerial(prm,startNode);
  }
#endif
}

void WriteTecplotMesh(const EarthUtil::AmpsParam& prm,const char* fnameBase) {
  // In MPI runs each rank writes its own Tecplot file. This avoids the need for
  // manual gather logic in this patch while still preserving all initialized
  // cell-center data from the distributed AMPS mesh.
  char fname[256];
  if (PIC::nTotalThreads>1) std::sprintf(fname,"%s.thread=%04d.dat",fnameBase,PIC::ThisThread);
  else std::sprintf(fname,"%s",fnameBase);

  FILE* fout=std::fopen(fname,"w");
  if (fout==nullptr) return;

  std::fprintf(fout,
    "TITLE=\"AMPS 3D Mesh Field Initialization\"\n"
    "VARIABLES=\"x_Re\",\"y_Re\",\"z_Re\",\"CellSize_Re\",\"Bx_nT\",\"By_nT\",\"Bz_nT\",\"Bmag_nT\",\"Ex_mVm\",\"Ey_mVm\",\"Ez_mVm\",\"Emag_mVm\",\"PhiE_kV\"\n"
    "ZONE T=\"AMPS_CELL_CENTERS\", F=POINT\n");

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread]; node!=NULL; node=node->nextNodeThisThread) {
    if (node->lastBranchFlag()!=_BOTTOM_BRANCH_TREE_ || node->block==NULL) continue;

    for (int i=0;i<_BLOCK_CELLS_X_;i++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++) {
        for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
          const int nd=PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k);
          PIC::Mesh::cDataCenterNode* center=node->block->GetCenterNode(nd);
          if (center==NULL) continue;

          double xCell[3];
          xCell[0]=node->xmin[0]+(node->xmax[0]-node->xmin[0])/_BLOCK_CELLS_X_*(0.5+i);
          xCell[1]=node->xmin[1]+(node->xmax[1]-node->xmin[1])/_BLOCK_CELLS_Y_*(0.5+j);
          xCell[2]=node->xmin[2]+(node->xmax[2]-node->xmin[2])/_BLOCK_CELLS_Z_*(0.5+k);

          double B[3],E[3];
          EvaluateBackgroundMagneticFieldSI(B,xCell,prm);
          EvaluateElectricFieldSI(E,xCell,prm);

          const double Bmag=std::sqrt(B[0]*B[0]+B[1]*B[1]+B[2]*B[2]);
          const double Emag=std::sqrt(E[0]*E[0]+E[1]*E[1]+E[2]*E[2]);
          const double phiV=EvaluateElectricPotential_V(xCell,prm);

          std::fprintf(fout,
            "%e %e %e %e %e %e %e %e %e %e %e %e %e\n",
            xCell[0]/_EARTH__RADIUS_,xCell[1]/_EARTH__RADIUS_,xCell[2]/_EARTH__RADIUS_,
            node->GetCharacteristicCellSize()/_EARTH__RADIUS_,
            B[0]/_NANO_,B[1]/_NANO_,B[2]/_NANO_,Bmag/_NANO_,
            E[0]/1.0e-3,E[1]/1.0e-3,E[2]/1.0e-3,Emag/1.0e-3,
            phiV/1000.0);
        }
      }
    }
  }

  std::fclose(fout);
}

// ---------------------------------------------------------------------------
// InitSphere — explicitly initialises and configures the inner Earth boundary
// sphere following the same pattern as the SphereInsideDomain block in
// main_lib.cpp::amps_init_mesh().
//
// amps_init_mesh() registers the sphere and sets Earth::Planet; we retrieve
// that handle here and re-apply every property so that the sphere setup is
// self-contained and auditable in the Mode3D flow.
//
// One important difference from main_lib.cpp: the cutoff-rigidity output
// callbacks are wired unconditionally.  In main_lib.cpp they are only set
// when (RigidityCalculationMode == Earth::_sphere &&
//       CutoffRigidity::SampleRigidityMode == true).
// Mode3D::Run() always operates in CutoffRigidityMode, so both conditions
// are implicitly satisfied and the guard can be dropped.
// ---------------------------------------------------------------------------
void InitSphere() {
  // amps_init_mesh() has already registered the sphere and assigned
  // Earth::Planet.  Retrieve the pointer and bail if it is somehow null.
  cInternalSphericalData* Sphere =
      static_cast<cInternalSphericalData*>(Earth::Planet);
  if (Sphere == nullptr) return;

  // ---- Geometry -----------------------------------------------------------
  // Sphere centred at the origin, radius = Earth radius (matches main_lib.cpp
  // where sx0={0,0,0} and rSphere=_EARTH__RADIUS_).
  double sx0[3] = {0.0, 0.0, 0.0};
  Sphere->SetSphereGeometricalParameters(sx0, _EARTH__RADIUS_);
  Sphere->Radius = _RADIUS_(_EARTH_);

  // ---- Surface mesh -------------------------------------------------------
  // Surface-mesh discretisation is shared with main_lib.cpp via the externs
  // declared at the top of this file.
  cInternalSphericalData::SetGeneralSurfaceMeshParameters(
      nZenithElements, nAzimuthalElements);

  // ---- Callbacks ----------------------------------------------------------
  // Particle–sphere interaction and injection (same values as main_lib.cpp).
  Sphere->ParticleSphereInteraction  = Earth::BC::ParticleSphereInteraction;
  Sphere->InjectionRate              = Exosphere::SourceProcesses::totalProductionRate;
  Sphere->InjectionBoundaryCondition = Exosphere::SourceProcesses::InjectionBoundaryModel;

  // Per-surface-element resolution function and face offset.
  Sphere->localResolution = localSphericalSurfaceResolution;
  Sphere->faceat          = 0;

  // ---- Diagnostic surface files -------------------------------------------
  // Re-emit the surface-mesh and initial surface-data files so that the output
  // on disk reflects the final Mode3D sphere configuration (geometry and
  // callbacks set above), not just the partial state left by amps_init_mesh().
  // The filenames match those written by main_lib.cpp::amps_init_mesh() so
  // that downstream post-processing scripts find the expected files.
  Sphere->PrintSurfaceMesh("Sphere.dat");
  Sphere->PrintSurfaceData("SpheraData.dat", 0);

  // ---- Cutoff-rigidity output callbacks -----------------------------------
  // In Mode3D, Run() always sets Earth::ModelMode = CutoffRigidityMode, so
  // the cutoff-rigidity output callbacks must always be connected.  Wire them
  // unconditionally here instead of repeating the conditional from
  // main_lib.cpp (which only fires when RigidityCalculationMode == _sphere &&
  // SampleRigidityMode == true).
  Earth::CutoffRigidity::AllocateCutoffRigidityTable();
  Sphere->PrintDataStateVector =
      Earth::CutoffRigidity::OutputDataFile::PrintDataStateVector;
  Sphere->PrintVariableList =
      Earth::CutoffRigidity::OutputDataFile::PrintVariableList;
}


// ---------------------------------------------------------------------------
// Mode3D time-snapshot helpers
// ---------------------------------------------------------------------------
// The AMPS_PARAM parser already knows how to load a Tsyganenko driver table from
// either
//
//   #TEMPORAL
//   TS_INPUT_MODE  FILE
//   TS_INPUT_FILE  ...
//
// or the more compact
//
//   #BACKGROUND_FIELD
//   DRIVER_FILE    ...
//
// and stores that table in prm.temporal.driverTable.  The helpers below are the
// runtime counterpart for the standalone mesh-backed 3-D cutoff path: they build
// the list of magnetic-field snapshots, interpolate the driver table at each
// snapshot epoch, update prm.field, and create a unique output suffix so that a
// multi-snapshot run does not overwrite its own Tecplot files.
// ---------------------------------------------------------------------------

bool Mode3DTimeSeriesRequested(const EarthUtil::AmpsParam& prm) {
  return EarthUtil::ToUpper(prm.temporal.mode) == "TIME_SERIES";
}

bool Mode3DSnapshotListRequested(const EarthUtil::AmpsParam& prm) {
  return EarthUtil::ToUpper(prm.temporal.mode) == "SNAPSHOT_LIST";
}

std::string Mode3DSanitizeSuffixToken(const std::string& in) {
  // Output suffixes become part of filenames such as
  //   cutoff_3d_shells_snapshot_000003_2024_05_10T12_15_00.dat
  // Keep only portable filename characters.  This intentionally converts ':',
  // '-', '.', whitespace, and any SPICE-specific decorations to underscores.
  std::string out;
  out.reserve(in.size());
  for (std::size_t i=0;i<in.size();++i) {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    if (std::isalnum(c)) out.push_back(static_cast<char>(c));
    else if (in[i]=='T' || in[i]=='Z') out.push_back(in[i]);
    else out.push_back('_');
  }

  // Avoid extremely long filenames if a user supplies a verbose time string.
  if (out.size()>48) out.resize(48);
  return out;
}

std::string Mode3DSnapshotSuffix(int iSnapshot,const std::string& epochUTC) {
  std::ostringstream os;
  os << "_snapshot_" << std::setw(6) << std::setfill('0') << iSnapshot
     << "_" << Mode3DSanitizeSuffixToken(epochUTC);
  return os.str();
}

#ifndef _NO_SPICE_CALLS_
double Mode3DEpochToEtOrExit(const std::string& epochUTC,const char* context) {
  SpiceDouble et = 0.0;
  try {
    str2et_c(epochUTC.c_str(),&et);
  }
  catch (...) {
    std::ostringstream msg;
    msg << "[Mode3D] Cannot convert UTC epoch '" << epochUTC
        << "' to SPICE ET while " << context << ".";
    exit(__LINE__,__FILE__,msg.str().c_str());
  }
  return static_cast<double>(et);
}

std::string Mode3DEtToUtc(double et) {
  // ISOC gives a compact sortable UTC label: YYYY-MM-DDTHH:MM:SS.sss.
  // Three decimals are enough for field-update cadences specified in minutes.
  char utc[96];
  et2utc_c(static_cast<SpiceDouble>(et),"ISOC",3,static_cast<SpiceInt>(sizeof(utc)),utc);
  return std::string(utc);
}
#endif

std::vector<std::string> Mode3DBuildSnapshotEpochs(const EarthUtil::AmpsParam& prm) {
  std::vector<std::string> epochs;

  // Explicit snapshot lists are the batching primitive used by C19. Unlike the
  // regular TIME_SERIES cadence, the list can contain irregular mission observation
  // times (for example 17:30 and 05:55). The list is sorted chronologically and
  // duplicate epochs are collapsed so one expensive mesh-field fill is shared by all
  // trajectory locations observed at that time.
  if (Mode3DSnapshotListRequested(prm)) {
    const std::string fileName=EarthUtil::Trim(prm.temporal.snapshotListFile);
    if (fileName.empty()) {
      exit(__LINE__,__FILE__,
           "[Mode3D] TEMPORAL_MODE=SNAPSHOT_LIST requires SNAPSHOT_LIST_FILE in #TEMPORAL.");
    }

    std::ifstream in(fileName.c_str());
    if (!in.is_open()) {
      const std::string msg="[Mode3D] Cannot open SNAPSHOT_LIST_FILE: "+fileName;
      exit(__LINE__,__FILE__,msg.c_str());
    }

#ifdef _NO_SPICE_CALLS_
    exit(__LINE__,__FILE__,
         "[Mode3D] TEMPORAL_MODE=SNAPSHOT_LIST requires SPICE to validate and order UTC epochs.");
#else
    std::vector<std::pair<double,std::string> > records;
    std::string line;
    int lineNo=0;
    while (std::getline(in,line)) {
      ++lineNo;
      const std::size_t h=line.find('#');
      const std::size_t b=line.find('!');
      std::size_t cut=std::string::npos;
      if (h!=std::string::npos) cut=h;
      if (b!=std::string::npos) cut=(cut==std::string::npos ? b : std::min(cut,b));
      if (cut!=std::string::npos) line=line.substr(0,cut);
      line=EarthUtil::Trim(line);
      if (line.empty()) continue;

      std::istringstream row(line);
      std::string epoch,extra;
      row >> epoch;
      if (epoch.empty()) continue;
      if (row >> extra) {
        std::ostringstream msg;
        msg << "[Mode3D] SNAPSHOT_LIST_FILE " << fileName << " line " << lineNo
            << " has an unexpected extra token '" << extra
            << "'. Expected one ISO-8601 UTC epoch per line.";
        exit(__LINE__,__FILE__,msg.str().c_str());
      }
      records.push_back(std::make_pair(
          Mode3DEpochToEtOrExit(epoch,"reading SNAPSHOT_LIST_FILE"),epoch));
    }
    if (records.empty()) {
      const std::string msg="[Mode3D] SNAPSHOT_LIST_FILE contains no UTC epochs: "+fileName;
      exit(__LINE__,__FILE__,msg.c_str());
    }

    std::sort(records.begin(),records.end(),
              [](const std::pair<double,std::string>& a,
                 const std::pair<double,std::string>& b) { return a.first<b.first; });
    const double duplicateTolerance_s=1.0e-6;
    double previousEt=0.0;
    bool havePrevious=false;
    for (const auto& record : records) {
      if (!havePrevious || std::fabs(record.first-previousEt)>duplicateTolerance_s) {
        epochs.push_back(record.second);
        previousEt=record.first;
        havePrevious=true;
      }
    }
    return epochs;
#endif
  }

  // Legacy/snapshot behavior: one field realization at #BACKGROUND_FIELD/EPOCH.
  // A driver table may still be present in this mode; it is sampled once at this
  // epoch by Mode3DBuildSnapshotParam() below.
  if (!Mode3DTimeSeriesRequested(prm)) {
    epochs.push_back(prm.field.epoch);
    return epochs;
  }

  // TIME_SERIES is meaningful only when an external driver table is loaded.  The
  // parser loads that table from #TEMPORAL/TS_INPUT_FILE or #BACKGROUND_FIELD/DRIVER_FILE.
  if (prm.temporal.driverTable.empty()) {
    exit(__LINE__,__FILE__,
         "[Mode3D] TEMPORAL_MODE=TIME_SERIES requires a Tsyganenko driver table: "
         "set #TEMPORAL TS_INPUT_MODE=FILE and TS_INPUT_FILE, or set "
         "#BACKGROUND_FIELD DRIVER_FILE.");
  }

  if (prm.temporal.eventStart.empty() || prm.temporal.eventEnd.empty()) {
    exit(__LINE__,__FILE__,
         "[Mode3D] TEMPORAL_MODE=TIME_SERIES requires EVENT_START and EVENT_END "
         "in the #TEMPORAL section.");
  }

  if (!(prm.temporal.fieldUpdateDt_min>0.0)) {
    exit(__LINE__,__FILE__,
         "[Mode3D] FIELD_UPDATE_DT must be positive for TEMPORAL_MODE=TIME_SERIES.");
  }

#ifdef _NO_SPICE_CALLS_
  exit(__LINE__,__FILE__,
       "[Mode3D] TEMPORAL_MODE=TIME_SERIES requires SPICE to convert EVENT_START/END "
       "and driver-table timestamps. Rebuild without _NO_SPICE_CALLS_.");
#else
  const double etStart = Mode3DEpochToEtOrExit(prm.temporal.eventStart,"building the Mode3D snapshot list");
  const double etEnd   = Mode3DEpochToEtOrExit(prm.temporal.eventEnd,  "building the Mode3D snapshot list");
  const double dt      = prm.temporal.fieldUpdateDt_min * 60.0;

  if (etEnd < etStart) {
    exit(__LINE__,__FILE__,
         "[Mode3D] EVENT_END must not be earlier than EVENT_START for TIME_SERIES.");
  }

  // Include both endpoints when they lie on the requested cadence.  The small
  // tolerance protects against roundoff in floating-point ET arithmetic.
  const double tol = std::max(1.0e-6,1.0e-9*dt);
  for (double et=etStart; et<=etEnd+tol; et+=dt) {
    epochs.push_back(Mode3DEtToUtc(std::min(et,etEnd)));
  }

  if (epochs.empty()) epochs.push_back(Mode3DEtToUtc(etStart));
  return epochs;
#endif
}

// Validate and construct the output-domain view for one explicit snapshot.
//
// Why this helper is required
// ---------------------------
// A normal trajectory stores many timestamped samples. The historical Mode3D
// TIME_SERIES loop evaluates the complete output domain at every field snapshot,
// which is useful for regular field-evolution studies. C19 instead describes a
// collection of independent observations: each location must be evaluated only with
// the field snapshot at its own timestamp. Reusing TIME_SERIES without filtering
// would create an incorrect N_snapshot x N_location Cartesian product.
//
// For TRAJECTORY, the returned AmpsParam contains only samples matching the
// current epoch. Its flattened point list is rebuilt, and LOCATION-qualified
// apertures are remapped to local indices. For SHELLS, the spatial domain is
// deliberately unchanged because every shell is sampled at every listed epoch.
EarthUtil::AmpsParam Mode3DBuildSnapshotWorkParam(
    const EarthUtil::AmpsParam& snap,const std::string& epochUTC) {
  if (!Mode3DSnapshotListRequested(snap)) return snap;

  const std::string outputMode=
      EarthUtil::ToUpper(EarthUtil::Trim(snap.output.mode));

  // SHELLS describes a static spatial sampling domain.  Unlike a timestamped
  // trajectory, there is no per-epoch subset to select and no LOCATION-qualified
  // aperture index to remap: every shell is intentionally evaluated at every
  // epoch in SNAPSHOT_LIST_FILE.  Returning the snapshot unchanged enables true
  // temporal batching while preserving the existing lifecycle below: the AMR
  // topology is allocated once outside the snapshot loop, whereas B/E values and
  // Tsyganenko/Geopack state are rebuilt for every epoch.
  if (outputMode=="SHELLS") return snap;

  if (outputMode!="TRAJECTORY" ||
      snap.output.trajectories.size()!=1) {
    exit(__LINE__,__FILE__,
         "[Mode3D] SNAPSHOT_LIST requires OUTPUT_MODE=SHELLS or "
         "OUTPUT_MODE=TRAJECTORY with one trajectory file.");
  }

#ifdef _NO_SPICE_CALLS_
  exit(__LINE__,__FILE__,
       "[Mode3D] SNAPSHOT_LIST trajectory filtering requires SPICE UTC conversion.");
  return snap;
#else
  const double epochEt=Mode3DEpochToEtOrExit(epochUTC,"selecting snapshot trajectory locations");
  const double matchTolerance_s=1.0e-3;
  const EarthUtil::SpacecraftTrajectory& source=snap.output.trajectories[0];

  EarthUtil::SpacecraftTrajectory selected;
  selected.name=source.name;
  selected.sourceFrame=source.sourceFrame;
  std::map<int,int> globalToLocal;

  for (std::size_t global=0;global<source.samples.size();++global) {
    const double sampleEt=Mode3DEpochToEtOrExit(
        source.samples[global].timeUTC,"matching a trajectory sample to SNAPSHOT_LIST");
    if (std::fabs(sampleEt-epochEt)<=matchTolerance_s) {
      const int local=static_cast<int>(selected.samples.size());
      selected.samples.push_back(source.samples[global]);
      globalToLocal[static_cast<int>(global)]=local;
    }
  }

  if (selected.samples.empty()) {
    const std::string msg="[Mode3D] SNAPSHOT_LIST epoch "+epochUTC+
        " has no matching trajectory location. Every listed snapshot must own at least one case.";
    exit(__LINE__,__FILE__,msg.c_str());
  }

  EarthUtil::AmpsParam work=snap;
  work.output.trajectories.clear();
  work.output.trajectories.push_back(selected);
  work.output.RebuildFlattenedPointsFromTrajectories();

  std::vector<EarthUtil::DirectionalAperture> selectedApertures;
  selectedApertures.reserve(snap.cutoff.dirMapApertures.size());
  for (const auto& aperture : snap.cutoff.dirMapApertures) {
    if (aperture.locationIndex<0) {
      selectedApertures.push_back(aperture);
      continue;
    }
    if (aperture.locationIndex>=static_cast<int>(source.samples.size())) {
      std::ostringstream msg;
      msg << "[Mode3D] aperture '" << aperture.name << "' selects LOCATION="
          << aperture.locationIndex << " but the trajectory has only "
          << source.samples.size() << " samples.";
      exit(__LINE__,__FILE__,msg.str().c_str());
    }
    const auto found=globalToLocal.find(aperture.locationIndex);
    if (found!=globalToLocal.end()) {
      EarthUtil::DirectionalAperture remapped=aperture;
      remapped.locationIndex=found->second;
      selectedApertures.push_back(remapped);
    }
  }
  work.cutoff.dirMapApertures.swap(selectedApertures);

  if (EarthUtil::ToUpper(work.cutoff.dirMapCoverage)=="VECTOR_APERTURES" &&
      work.cutoff.dirMapApertures.empty()) {
    const std::string msg="[Mode3D] snapshot "+epochUTC+
        " has no active directional apertures after LOCATION filtering.";
    exit(__LINE__,__FILE__,msg.c_str());
  }
  return work;
#endif
}

void Mode3DValidateSnapshotListCoverage(
    const EarthUtil::AmpsParam& prm,const std::vector<std::string>& epochs) {
  if (!Mode3DSnapshotListRequested(prm)) return;

  const std::string outputMode=
      EarthUtil::ToUpper(EarthUtil::Trim(prm.output.mode));

  // Coverage validation below proves that each timestamped trajectory sample
  // belongs to exactly one explicit epoch.  SHELLS has no timestamped samples:
  // its complete fixed geometry is evaluated at every epoch, so this trajectory-
  // specific validation is both inapplicable and unnecessary.
  if (outputMode=="SHELLS") return;

  if (outputMode!="TRAJECTORY" ||
      prm.output.trajectories.size()!=1) {
    exit(__LINE__,__FILE__,
         "[Mode3D] SNAPSHOT_LIST validation requires SHELLS or one parsed TRAJECTORY.");
  }
#ifdef _NO_SPICE_CALLS_
  exit(__LINE__,__FILE__,
       "[Mode3D] SNAPSHOT_LIST coverage validation requires SPICE UTC conversion.");
#else
  const double tolerance_s=1.0e-3;
  std::vector<double> epochEt;
  epochEt.reserve(epochs.size());
  for (const std::string& epoch : epochs)
    epochEt.push_back(Mode3DEpochToEtOrExit(epoch,"validating SNAPSHOT_LIST coverage"));

  std::vector<int> locationsPerEpoch(epochs.size(),0);
  const auto& samples=prm.output.trajectories[0].samples;
  for (std::size_t location=0;location<samples.size();++location) {
    const double sampleEt=Mode3DEpochToEtOrExit(
        samples[location].timeUTC,"validating a batched trajectory timestamp");
    int nMatches=0;
    int matchedEpoch=-1;
    for (std::size_t i=0;i<epochEt.size();++i) {
      if (std::fabs(sampleEt-epochEt[i])<=tolerance_s) {
        ++nMatches;
        matchedEpoch=static_cast<int>(i);
      }
    }
    if (nMatches!=1) {
      std::ostringstream msg;
      msg << "[Mode3D] trajectory location " << location << " at "
          << samples[location].timeUTC << " matches " << nMatches
          << " SNAPSHOT_LIST epochs; every location must match exactly one epoch.";
      exit(__LINE__,__FILE__,msg.str().c_str());
    }
    locationsPerEpoch[static_cast<std::size_t>(matchedEpoch)]++;
  }
  for (std::size_t i=0;i<epochs.size();++i) {
    if (locationsPerEpoch[i]==0) {
      const std::string msg="[Mode3D] SNAPSHOT_LIST epoch "+epochs[i]+
          " has no matching trajectory location.";
      exit(__LINE__,__FILE__,msg.c_str());
    }
  }
#endif
}

EarthUtil::AmpsParam Mode3DBuildSnapshotParam(const EarthUtil::AmpsParam& base,
                                              const std::string& epochUTC) {
  EarthUtil::AmpsParam snap = base;
  snap.field.epoch = epochUTC;

  // If a driver table is loaded, interpolate it at this snapshot epoch and copy
  // the resulting Pdyn/Dst/IMF/W/G/BZ/XIND values into snap.field.  Downstream
  // code then treats this snapshot exactly like a normal one-time input-file
  // configuration; no field evaluator needs to know whether the parameters came
  // from a static block or from a time table.
  if (!snap.temporal.driverTable.empty()) {
#ifdef _NO_SPICE_CALLS_
    exit(__LINE__,__FILE__,
         "[Mode3D] A Tsyganenko driver table was loaded, but this build has "
         "_NO_SPICE_CALLS_ defined. Cannot convert snapshot UTC to ET.");
#else
    const double et = Mode3DEpochToEtOrExit(epochUTC,"sampling the Mode3D Tsyganenko driver table");
    const EarthUtil::TsDriverRecord rec = snap.temporal.driverTable.Lookup(et);
    EarthUtil::TsDriverTable::ApplyToField(rec,snap.field);
#endif
  }

  return snap;
}

void Mode3DPrepareMagneticFieldSnapshot(const EarthUtil::AmpsParam& snap,
                                         const std::string& suffix,
                                         bool verbose) {
  // Reinitialize the selected standalone field backend for this snapshot.  For
  // Tsyganenko models this updates the model parameters and Geopack epoch before
  // any cell-centered field values are evaluated.  For DIPOLE this is cheap and
  // simply resets the internal Mode3D model flags.
  ConfigureBackgroundFieldModel(snap);

  // Fill DATAFILE B/E buffers only on owner-rank blocks of the normal distributed
  // AMPS mesh.  The compact global arrays assembled below copy authoritative interior
  // cells from those owners and never allocate nonlocal blocks.
  InitMeshFields(snap,PIC::Mesh::mesh->rootTree);

  // Optional diagnostic output of the initialized cell-centered field.  Append the
  // same snapshot suffix used by cutoff outputs so multiple snapshots do not
  // overwrite each other.
  if (snap.mode3d.outputInitializedFile) {
    const std::string fname = "amps_3d_initialized" + suffix + ".data.dat";
    PIC::Mesh::mesh->outputMeshDataTECPLOT(fname.c_str(),0);
  }

#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
  // Assemble compact global B and E arrays for this snapshot.  The AMR tree is
  // already global; node->Temp_ID provides the block index and row-stencil
  // interpolation resolves cells without requiring node->block on this rank.
  if (!PIC::CPLR::DATAFILE::Offset::MagneticField.active) {
    exit(__LINE__,__FILE__,
         "[Mode3D] DATAFILE magnetic-field offset is inactive before standalone "
         "3-D cutoff materialization.");
  }

  const long int electricFieldOffset =
      PIC::CPLR::DATAFILE::Offset::ElectricField.active ?
      Earth::Mode3D::GlobalMagneticField::DataFileElectricFieldDataOffset() : -1;

  Earth::Mode3D::GlobalMagneticField::AssembleCellCenteredFieldsForCutoff(
      "Mode3D",
      Earth::Mode3D::GlobalMagneticField::DataFileMagneticFieldDataOffset(),
      electricFieldOffset,
      -1,
      verbose);
#else
  (void)verbose;
#endif
}

} // namespace

void ConfigureMeshResolutionProfile(const EarthUtil::AmpsParam& prm) {
  MeshResolutionProfileActive = prm.mode3d.meshResolutionProfileActive;

  if (!MeshResolutionProfileActive) {
    MeshResolutionEarth_m=0.0;
    MeshResolutionBoundary_m=0.0;
    MeshResolutionOuterRadius_Re=0.0;
    MeshResolutionExponent=1.0;
    MeshResolutionCoarseningCode=0;
    return;
  }

  MeshResolutionEarth_m = prm.mode3d.meshResolutionEarth_km * 1000.0;
  MeshResolutionBoundary_m = prm.mode3d.meshResolutionBoundary_km * 1000.0;
  MeshResolutionExponent = prm.mode3d.meshResolutionExponent;

  if (prm.mode3d.meshResolutionOuterRadius_km > 0.0) {
    MeshResolutionOuterRadius_Re = (prm.mode3d.meshResolutionOuterRadius_km * 1000.0) / _EARTH__RADIUS_;
  }
  else {
    MeshResolutionOuterRadius_Re = MaxAbsDomainFaceRadiusRe(prm);
  }
  if (!(MeshResolutionOuterRadius_Re > 1.0)) MeshResolutionOuterRadius_Re = 1.01;

  const std::string c = EarthUtil::ToUpper(prm.mode3d.meshResolutionCoarsening);
  if (c=="LOG" || c=="EXP" || c=="EXPONENTIAL" || c=="GEOMETRIC" || c=="LOGARITHMIC")
    MeshResolutionCoarseningCode=1;
  else if (c=="POWER" || c=="POW" || c=="EXPONENT" || c=="POLYNOMIAL")
    MeshResolutionCoarseningCode=2;
  else if (c=="CONSTANT" || c=="CONST" || c=="UNIFORM")
    MeshResolutionCoarseningCode=3;
  else
    MeshResolutionCoarseningCode=0;

  if (PIC::ThisThread == 0) {
    std::cout << "[Mode3D mesh] User-defined AMR resolution profile: "
              << "res_earth=" << MeshResolutionEarth_m/_EARTH__RADIUS_ << " Re, "
              << "res_boundary=" << MeshResolutionBoundary_m/_EARTH__RADIUS_ << " Re, "
              << "r_boundary=" << MeshResolutionOuterRadius_Re << " Re, "
              << "coarsening=" << c << ", exponent=" << MeshResolutionExponent
              << std::endl;
  }
}

double ConfiguredMeshResolutionSI(const double *x) {
  if (!MeshResolutionProfileActive) return -1.0;

  double r2=0.0;
  for (int idim=0; idim<DIM; ++idim) r2 += x[idim]*x[idim];
  const double r = std::sqrt(r2);
  const double r_Re = r / _EARTH__RADIUS_;

  // Preserve the legacy AMR behavior inside the loss sphere.  The Mode3D mesh
  // exists in a Cartesian box that includes the inactive Earth interior, but
  // particle tracing stops at the inner boundary and does not need a finely
  // resolved interior volume.  Applying the user requested near-Earth surface
  // resolution throughout r < Re can create a very large number of unnecessary
  // AMR blocks and may make validation cases such as C5 fail before they ever
  // test the mesh-interpolated field.  The old localResolution() path used a
  // coarse cell size for r < 0.98 Re; keep that protection when the optional
  // user-defined radial profile is active.
  if (r < 0.98*_EARTH__RADIUS_) return _EARTH__RADIUS_;

  double t = 0.0;
  if (MeshResolutionOuterRadius_Re > 1.0) {
    t = (r_Re - 1.0) / (MeshResolutionOuterRadius_Re - 1.0);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
  }

  const double a = MeshResolutionEarth_m;
  const double b = MeshResolutionBoundary_m;

  if (!(a > 0.0) || !(b > 0.0)) return -1.0;

  switch (MeshResolutionCoarseningCode) {
    case 1: { // LOG/EXPONENTIAL/GEOMETRIC interpolation in resolution.
      return std::exp(std::log(a) + t*(std::log(b)-std::log(a)));
    }
    case 2: { // POWER profile in normalized altitude.
      const double tt = std::pow(t,MeshResolutionExponent);
      return a + (b-a)*tt;
    }
    case 3: { // CONSTANT/UNIFORM resolution.
      return a;
    }
    case 0:
    default: { // LINEAR profile.
      return a + (b-a)*t;
    }
  }
}

int Run(const EarthUtil::AmpsParam& prm) {
  //============================================================================
  // Mode3D::Run — entry point for the standalone 3-D cutoff-rigidity workflow.
  //
  // Initialization sequence
  // -----------------------
  // 1. Mark execution mode and configure the physical domain bounds from prm.
  //    main_lib.cpp::amps_init_mesh() reads ParsedDomainMin/Max while building the
  //    AMR tree, and RunCutoffRigidity() later uses the same values for the outer
  //    escape box.  This keeps mesh geometry and trajectory classification aligned.
  //
  // 2. Use the normal AMPS MPI initialization, not independentDomainMode.
  //    Earlier standalone cutoff code called
  //
  //       PIC::InitMPI(/*independentDomainMode=*/true)
  //
  //    so that every MPI process built a private complete copy of the AMR mesh.
  //    That is no longer needed.  The current path initializes AMPS in the standard
  //    distributed-domain mode used elsewhere in the model.
  //
  // 3. Fill the selected standalone B/E field into the owner-rank DATAFILE cell
  //    buffers.  At this point only the normal distributed set of blocks is
  //    allocated on each rank.
  //
  // 4. Assemble compact global read-only B/E arrays for cutoff tracing:
  //
  //       reset Temp_ID throughout the global AMR tree,
  //       assign deterministic dense Temp_ID values to used leaves,
  //       pack owner-rank interior-cell B/E values,
  //       MPI_Allreduce the compact arrays and one presence entry per cell.
  //
  //    No nonlocal AMR blocks or ghost-cell state vectors are allocated.  During
  //    tracing, the field evaluator builds a decomposition-independent row stencil
  //    and maps each (node,i,j,k) entry to Temp_ID*(Nx*Ny*Nz)+localCellIndex.
  //
  // 5. RunCutoffRigidity performs MPI × OpenMP work distribution:
  //    - static point distribution across MPI ranks;
  //    - OpenMP parallelism over points/directions inside each rank;
  //    - rank 0 gathers scalar Rc/Emin products and writes Tecplot output.
  //============================================================================

  Earth::ModelMode = Earth::CutoffRigidityMode;
  ApplyParsedDomain(prm);

  // ---- Standard distributed-domain initialization --------------------------
  //
  // Do not request independentDomainMode here.  Owner blocks remain distributed.
  // The compact global-field assembly copies only B/E values from owner interior
  // cells; it never changes the AMPS block allocation.
  // -------------------------------------------------------------------------
  PIC::InitMPI();
  ConfigureMeshResolutionProfile(prm);
  Exosphere::Init_SPICE();

  amps_init_mesh();   // build the replicated AMR tree topology and distributed blocks
  amps_init();        // data-offset registration and cutoff-mode particle settings

  //----------------------------------------------------------------------------
  // Sphere and static mesh geometry
  //----------------------------------------------------------------------------
  InitSphere();

  // Build the list of magnetic-field snapshots for this run.
  //
  //   SNAPSHOT mode (default): one entry, prm.field.epoch.
  //   TIME_SERIES mode       : EVENT_START..EVENT_END in steps of FIELD_UPDATE_DT.
  //   SNAPSHOT_LIST mode     : explicit irregular epochs from SNAPSHOT_LIST_FILE.
  //
  // The parser has already loaded the Tsyganenko driver file, if requested, into
  // prm.temporal.driverTable.  For every epoch below we interpolate that table into
  // a temporary AmpsParam copy and then reuse the normal single-snapshot field
  // initialization + compact global-field assembly + cutoff solver.
  const std::vector<std::string> snapshotEpochs = Mode3DBuildSnapshotEpochs(prm);
  Mode3DValidateSnapshotListCoverage(prm,snapshotEpochs);

  if (PIC::ThisThread == 0) {
    std::cout << "[Mode3D] Magnetic-field snapshot count: "
              << snapshotEpochs.size();
    if (Mode3DTimeSeriesRequested(prm)) {
      std::cout << " (TIME_SERIES, FIELD_UPDATE_DT="
                << prm.temporal.fieldUpdateDt_min << " min)";
    }
    else if (Mode3DSnapshotListRequested(prm)) {
      std::cout << " (SNAPSHOT_LIST='" << prm.temporal.snapshotListFile << "')";
    }
    std::cout << "\n";
    if (Mode3DSnapshotListRequested(prm) &&
        EarthUtil::ToUpper(EarthUtil::Trim(prm.output.mode))=="SHELLS") {
      // Make the optimization visible in batch logs.  Users should expect one
      // mesh allocation followed by one field refill and cutoff solve per
      // epoch; the latter messages are not evidence that the mesh was rebuilt.
      std::cout << "[Mode3D] SNAPSHOT_LIST SHELLS mesh reuse: one AMR topology; "
                << snapshotEpochs.size()
                << " epoch-specific field initialization(s) and suffixed product(s).\n";
    }
    std::cout.flush();
  }

  int finalStatus = 0;

  for (std::size_t iSnapshot=0; iSnapshot<snapshotEpochs.size(); ++iSnapshot) {
    // Convert the base run parameters into this snapshot's effective magnetic
    // configuration.  If a driver table is present, this is where Pdyn/Dst/IMF/W
    // are interpolated at snapshotEpochs[iSnapshot].
    EarthUtil::AmpsParam snap = Mode3DBuildSnapshotParam(prm,snapshotEpochs[iSnapshot]);

    // SNAPSHOT_LIST is an explicit batching mode. Timestamped TRAJECTORY domains
    // select and remap only observations owned by this epoch; static SHELLS domains
    // are returned unchanged so every requested shell is sampled at every epoch.
    // Other temporal modes preserve their historical all-locations behavior.
    snap = Mode3DBuildSnapshotWorkParam(snap,snapshotEpochs[iSnapshot]);

    const std::string suffix = (snapshotEpochs.size()>1 || Mode3DTimeSeriesRequested(prm) ||
                                Mode3DSnapshotListRequested(prm))
        ? Mode3DSnapshotSuffix(static_cast<int>(iSnapshot),snap.field.epoch)
        : std::string("");

    // Tell the cutoff writer to append the snapshot suffix before ".dat".  This
    // prevents outputs from later snapshots from overwriting earlier snapshots.
    SetCutoffOutputFileSuffix(suffix);
    SetDensityOutputFileSuffix(suffix);

    if (PIC::ThisThread == 0) {
      std::cout << "[Mode3D] Preparing magnetic-field snapshot "
                << (iSnapshot+1) << "/" << snapshotEpochs.size()
                << ": epoch=" << snap.field.epoch;
      if (!suffix.empty()) std::cout << ", suffix=" << suffix;
      std::cout << "\n";
      std::cout.flush();
    }

    // Reuse the existing single-snapshot data-management path for every time.
    // This call reinitializes the selected Tsyganenko/dipole field, writes it into
    // the mesh DATAFILE buffers, and assembles compact global read-only B/E arrays on
    // every MPI process for the cutoff tracer.
    Mode3DPrepareMagneticFieldSnapshot(snap,suffix,/*verbose=*/true);

    //------------------------------------------------------------------------
    // Requested physics products for this snapshot
    //------------------------------------------------------------------------
    // Both products below use the same prepared mesh-field snapshot.  Running them
    // consecutively here guarantees that, when the input requests
    // CUTOFF_RIGIDITY+DENSITY_SPECTRUM, cutoff, directional maps, density, spectra,
    // and flux are all derived from the identical magnetic-field state and carry the
    // same snapshot suffix in their file names.
    //------------------------------------------------------------------------
    const bool doCutoff      = Mode3DTargetRequestsCutoff(snap);
    const bool doDensityFlux = Mode3DTargetRequestsDensityFlux(snap);

    if (!doCutoff && !doDensityFlux) {
      std::ostringstream msg;
      msg << "Unsupported CALC_TARGET for -mode 3d: '" << snap.calc.target
          << "'. Supported targets include CUTOFF_RIGIDITY, DENSITY_SPECTRUM, "
          << "and CUTOFF_RIGIDITY+DENSITY_SPECTRUM.";
      throw std::runtime_error(msg.str());
    }

    if (doCutoff) {
      if (PIC::ThisThread == 0) {
        std::cout << "[Mode3D] Starting cutoff rigidity calculation for snapshot "
                  << (iSnapshot+1) << "/" << snapshotEpochs.size() << "...\n";
        std::cout.flush();
      }

      const int status = RunCutoffRigidity(snap,/*showProgressBar=*/true);
      if (status != 0) finalStatus = status;

      if (PIC::ThisThread == 0) {
        std::cout << "[Mode3D] Cutoff rigidity calculation complete for snapshot "
                  << (iSnapshot+1) << "/" << snapshotEpochs.size()
                  << " (status=" << status << ").\n";
        std::cout.flush();
      }
    }

    if (doDensityFlux) {
      if (PIC::ThisThread == 0) {
        std::cout << "[Mode3D] Starting density/flux calculation for snapshot "
                  << (iSnapshot+1) << "/" << snapshotEpochs.size() << "...\n";
        std::cout.flush();
      }

      const int status = RunDensityAndFlux(snap);
      if (status != 0) finalStatus = status;

      if (PIC::ThisThread == 0) {
        std::cout << "[Mode3D] Density/flux calculation complete for snapshot "
                  << (iSnapshot+1) << "/" << snapshotEpochs.size()
                  << " (status=" << status << ").\n";
        std::cout.flush();
      }
    }
  }

  // Reset the global suffix so a future in-process caller starts from the legacy
  // filename convention unless it explicitly installs another suffix.
  SetCutoffOutputFileSuffix("");
  SetDensityOutputFileSuffix("");

  return finalStatus;
}

} // namespace Mode3D
} // namespace Earth
