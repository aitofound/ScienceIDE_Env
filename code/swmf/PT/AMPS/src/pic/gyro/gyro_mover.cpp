//====================================================================================
//  PIC::GYROKINETIC guiding-center (GC) particle movers — 3-D, non-relativistic
//====================================================================================
//
// OVERVIEW
// --------
// This file provides a clean, self-contained implementation of guiding-center
// particle advance for strongly magnetized species (e.g., electrons in ECSIM),
// where the fast gyromotion is not resolved. The particle state is represented by
//  - position: x(t) in 3-D
//  - parallel velocity: v_parallel(t) = v · b
//  - perpendicular speed magnitude: v_normal(t) = |v_perp|
//
// with magnetic moment µ assumed invariant:
//
//   µ = (m * v_perp^2) / (2 |B|)          (non-relativistic)
//
// The gyro-phase is not represented and not needed. The mover advances x and v_parallel;
// v_normal is reconstructed from µ and |B| at the evaluation point.
//
//
// GOVERNING EQUATIONS (NON-REL GC MODEL)
// -------------------------------------
// Let b = B/|B| be the unit vector along the magnetic field.
// Let v_parallel be the signed velocity component along b.
// Let v_drift be the perpendicular drift velocity of the guiding center.
//
// Guiding-center velocity decomposition:
//   dx/dt = v_gc = v_drift + b * v_parallel
//
// Drift velocity modeled as sum of:
//   (1) E×B drift:
//       v_E = (E × B) / |B|^2
//
//   (2) grad-B drift:
//       v_∇B = (µ / q) * (B × ∇|B|) / |B|^2
//
//   (3) curvature drift (non-rel form consistent with legacy AMPS algebra):
//       v_curv = (m v_parallel^2 / q) * ( B × ( (B · ∇) B ) ) / |B|^4
//
// Parallel acceleration (Lorentz + mirror force):
//   dv_parallel/dt = (q/m) * E_parallel  -  (µ/m) * (b · ∇|B|)
//
// where E_parallel = E · b.
//
//
// NUMERICS (TIME INTEGRATION)
// ---------------------------
// Two explicit integrators are provided:
//
// 1) First-order (Euler / Forward Euler)
//    ----------------------------------
//    Evaluate RHS at time level n and update:
//      x^{n+1}      = x^n      + dt * ( v_drift^n + b^n * vpar^n )
//      vpar^{n+1}   = vpar^n   + dt * ( dvpar_dt^n )
//
//    This is first-order accurate in time. It is cheap but more dissipative and
//    sensitive to dt. Use when you want robust baseline behavior or very small dt.
//
// 2) Second-order (RK2 midpoint / explicit midpoint)
//    -----------------------------------------------
//    Stage A (at n):
//      kx0   = v_drift(x^n,vpar^n) + b(x^n)*vpar^n
//      kv0   = dvpar_dt(x^n,vpar^n)
//
//    Predict midpoint:
//      x^{n+1/2}    = x^n    + (dt/2) * kx0
//      vpar^{n+1/2} = vpar^n + (dt/2) * kv0
//
//    Stage B (at n+1/2):
//      kxH   = v_drift(xH,vparH) + b(xH)*vparH
//      kvH   = dvpar_dt(xH,vparH)
//
//    Full update:
//      x^{n+1}      = x^n      + dt * kxH
//      vpar^{n+1}   = vpar^n   + dt * kvH
//
//    This is second-order accurate in time for smooth fields and sufficiently small dt.
//
//
// IMPLEMENTATION DETAILS / CONTRACTS
// ---------------------------------
// A) 3-D ONLY, NO FIELD-LINE MODE
//    This mover is strictly 3-D. We do not rely on _PIC_FIELD_LINE_MODE_. We treat
//    particle velocity as a true 3-vector in the standard particle buffer layout.
//
// B) REDUCED STATE STORAGE AND COMMIT REQUIREMENTS
//    The project intent requires we store v_parallel and v_normal as dedicated fields
//    in the particle buffer (via PIC::GYROKINETIC offsets and getters/setters).
//
//    At the end of the mover call we MUST:
//      - commit v_parallel  (SetVParallel)
//      - commit v_normal    (SetVNormal)      [reconstructed from µ and |B|]
//      - compute and commit full 3D velocity vector v = b*v_parallel
//      - compute and commit drift velocity (SetV_drift)
//
// C) FIELD ACCESS
//    This code supports two field sources:
//
//    1) ECSIM field solver:
//         PIC::FieldSolver::Electromagnetic::ECSIM::GetElectricField(E,x,node)
//         PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticField(B,x,node)
//         PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticFieldGradient(gradB,x,node)
//
//    2) Coupler (CPLR) background fields:
//         PIC::CPLR::InitInterpolationStencil(x,node)
//         PIC::CPLR::GetBackgroundElectricField(E)
//         PIC::CPLR::GetBackgroundMagneticField(B)
//         PIC::CPLR::GetBackgroundMagneticFieldGradient(gradB,NAN)
//
//    The gradient tensor layout is assumed to be:
//         gradB[3*i + j] = ∂B_i / ∂x_j      (i component, j coordinate)
//
//    This is consistent with the algebra used in legacy code in this codebase.
//
// D) OUT-OF-DOMAIN / INVALID FIELD HANDLING
//    If node==NULL or |B|<=0 or non-finite results occur, we set the RHS terms to zero.
//    For the actual mover, if the new position cannot be located inside the mesh,
//    we delete the particle and return _PARTICLE_LEFT_THE_DOMAIN_ (same style as other movers).
//
// E) UNITS / NORMALIZATION
//    Species tables are stored in SI. If your solver uses normalized field units
//    (_PIC_FIELD_SOLVER_INPUT_UNIT_ == _PIC_FIELD_SOLVER_INPUT_UNIT_NORM_),
//    we convert q and m to normalized units using picunits::si2no_q/m exactly like
//    the Boris mover does.
//
// F) INTERNAL BOUNDARY / TRAJECTORY TRACKER
//    The code mirrors the standard internal-boundary check and particle tracker
//    recording logic used by other movers.
//
// G) PARTICLE MOVING LIST PLACEMENT
//    At the end of a successful move, the particle is inserted into the block’s
//    temp moving list using the same logic as the reference mover:
//
//      FindCellIndex -> tempParticleMovingListTable insertion with compilation-mode branches.
//
//====================================================================================


#include "../pic.h"

namespace PIC {
namespace GYROKINETIC {

  //----------------------------------------------------------------------------------
  // Helper: get species mass/charge in the same unit system as fields
  //----------------------------------------------------------------------------------
  inline void GetSpeciesMassCharge(double &m,double &q,int spec) {
    // The AMPS species property tables are SI by default.
    // When the field solver input is normalized, convert m and q consistently.
    if (_PIC_FIELD_SOLVER_INPUT_UNIT_==_PIC_FIELD_SOLVER_INPUT_UNIT_NORM_) {
      q = picunits::si2no_q(PIC::MolecularData::ElectricChargeTable[spec],PIC::Units::Factors);
      m = picunits::si2no_m(PIC::MolecularData::MolMass[spec],PIC::Units::Factors);
    }
    else {
      q = PIC::MolecularData::ElectricChargeTable[spec];
      m = PIC::MolecularData::MolMass[spec];
    }
  }

  //----------------------------------------------------------------------------------
  // Helper: get E, B, and gradB at position x inside node
  //----------------------------------------------------------------------------------
  inline bool GetEBandGradB(double E[3],double B[3],double gradB[9],
                           const double *x,
                           cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
    // If node is null, the point is outside domain.
    if (node==NULL) return false;

    // ECSIM path: fields come from field solver directly.
    if (_PIC_FIELD_SOLVER_MODE_==_PIC_FIELD_SOLVER_MODE__ELECTROMAGNETIC__ECSIM_) {
      PIC::FieldSolver::Electromagnetic::ECSIM::GetElectricField(E,(double*)x,node);
      PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticField(B,(double*)x,node);
      PIC::FieldSolver::Electromagnetic::ECSIM::GetMagneticFieldGradient(gradB,(double*)x,node);
      return true;
    }

    // Coupler path: use background interpolation stencil (must be initialized here).
    if (_PIC_COUPLER_MODE_==_PIC_COUPLER_MODE__OFF_) {
      // This mover expects either ECSIM or CPLR fields. If neither is available, abort.
      exit(__LINE__,__FILE__,"PIC::GYROKINETIC::GetEBandGradB(): neither ECSIM nor CPLR is available");
    }

    PIC::CPLR::InitInterpolationStencil((double*)x,node);
    PIC::CPLR::GetBackgroundElectricField(E);
    PIC::CPLR::GetBackgroundMagneticField(B);

    // No explicit time input in this module; pass NAN (DATAFILE interprets as "current").
    PIC::CPLR::GetBackgroundMagneticFieldGradient(gradB,NAN);

    return true;
  }

  //----------------------------------------------------------------------------------
  // Helper: compute |B| and unit vector b = B/|B|
  //----------------------------------------------------------------------------------
  inline bool GetAbsBAndUnitB(const double B[3],double &absB,double b[3]) {
    const double absB2 = B[0]*B[0] + B[1]*B[1] + B[2]*B[2];
    if (absB2<=0.0) {
      absB=0.0;
      b[0]=0.0; b[1]=0.0; b[2]=0.0;
      return false;
    }

    absB = sqrt(absB2);
    const double inv = 1.0/absB;
    b[0]=B[0]*inv; b[1]=B[1]*inv; b[2]=B[2]*inv;
    return true;
  }

  //----------------------------------------------------------------------------------
  // Helper: compute grad(|B|) from gradB and B
  //   gradB[3*i + j] = dB_i/dx_j
  //   grad|B|_j = (B · dB/dx_j) / |B|
  //----------------------------------------------------------------------------------
  inline void GetGradAbsB(double gradAbsB[3],const double B[3],const double gradB[9],double absB) {
    if (absB<=0.0) {
      gradAbsB[0]=0.0; gradAbsB[1]=0.0; gradAbsB[2]=0.0;
      return;
    }

    const double invAbsB = 1.0/absB;
    for (int j=0;j<3;j++) {
      // gradAbsB[j] = (B0 dB0/dxj + B1 dB1/dxj + B2 dB2/dxj) / |B|
      gradAbsB[j] =
        (B[0]*gradB[0*3+j] + B[1]*gradB[1*3+j] + B[2]*gradB[2*3+j]) * invAbsB;
    }
  }

  //----------------------------------------------------------------------------------
  // Helper: evaluate RHS terms for guiding-center ODE system
  //
  // Inputs:
  //   x, node         : evaluation location (must be inside domain)
  //   vpar, mu, spec  : GC state parameters
  //
  // Outputs:
  //   absB, b         : |B| and unit vector b
  //   vdrift          : perpendicular drift velocity (E×B + grad-B + curvature)
  //   dvpar_dt        : parallel acceleration (q/m E_parallel - mu/m b·grad|B|)
  //
  // Safety:
  //   If evaluation fails or yields invalid values, outputs are set to zero.
  //----------------------------------------------------------------------------------
  inline void EvalRHS(const double *x,
                      cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,
                      double vpar,double mu,int spec,
                      double &absB,double b[3],
                      double vdrift[3],double &dvpar_dt) {
    // Initialize outputs defensively.
    absB=0.0;
    b[0]=0.0; b[1]=0.0; b[2]=0.0;
    vdrift[0]=0.0; vdrift[1]=0.0; vdrift[2]=0.0;
    dvpar_dt=0.0;

    double E[3],B[3],gradB[9];

    // If we cannot evaluate fields here, keep RHS as zeros.
    if (GetEBandGradB(E,B,gradB,x,node)==false) return;

    // If B is zero, GC model is undefined; keep RHS zeros.
    if (GetAbsBAndUnitB(B,absB,b)==false) return;

    // Species properties (units consistent with E,B)
    double m,q;
    GetSpeciesMassCharge(m,q,spec);

    // Precompute |B| powers
    const double absB2 = absB*absB;
    const double invAbsB2 = 1.0/absB2;

    // Compute grad|B|
    double gradAbsB[3];
    GetGradAbsB(gradAbsB,B,gradB,absB);

    // Parallel electric field E_parallel = E·b
    const double Epar = E[0]*b[0] + E[1]*b[1] + E[2]*b[2];

    // Mirror term uses b·grad|B|
    const double bDotGradAbsB = b[0]*gradAbsB[0] + b[1]*gradAbsB[1] + b[2]*gradAbsB[2];

    // Parallel acceleration:
    //   dvpar/dt = (q/m) E_parallel - (mu/m) (b·grad|B|)
    dvpar_dt = (q/m)*Epar - (mu/m)*bDotGradAbsB;

    // (1) E×B drift:
    //   vE = (E×B)/|B|^2
    double ExB[3];
    Vector3D::CrossProduct(ExB,E,B);
    vdrift[0] = ExB[0]*invAbsB2;
    vdrift[1] = ExB[1]*invAbsB2;
    vdrift[2] = ExB[2]*invAbsB2;

    // (2) grad-B drift:
    //   v_gradB = (mu/q) (B×grad|B|)/|B|^2
    if (q!=0.0 && mu!=0.0) {
      double BxGradAbsB[3];
      Vector3D::CrossProduct(BxGradAbsB,B,gradAbsB);
      const double c = (mu/q)*invAbsB2;
      vdrift[0] += c*BxGradAbsB[0];
      vdrift[1] += c*BxGradAbsB[1];
      vdrift[2] += c*BxGradAbsB[2];
    }

    // (3) curvature drift:
    //
    // Compute (B·∇)B (a vector):
    //   BB_i = sum_j B_j * ∂B_i/∂x_j
    //
    // Using layout gradB[3*i + j] = ∂B_i/∂x_j.
    double BB[3];
    BB[0] = B[0]*gradB[0*3+0] + B[1]*gradB[0*3+1] + B[2]*gradB[0*3+2];
    BB[1] = B[0]*gradB[1*3+0] + B[1]*gradB[1*3+1] + B[2]*gradB[1*3+2];
    BB[2] = B[0]*gradB[2*3+0] + B[1]*gradB[2*3+1] + B[2]*gradB[2*3+2];

    // Curvature drift:
    //   v_curv = (m vpar^2 / q) * (B×BB) / |B|^4
    if (q!=0.0 && vpar!=0.0) {
      double BxBB[3];
      Vector3D::CrossProduct(BxBB,B,BB);
      const double invAbsB4 = 1.0/(absB2*absB2);
      const double c = (m*vpar*vpar/q)*invAbsB4;
      vdrift[0] += c*BxBB[0];
      vdrift[1] += c*BxBB[1];
      vdrift[2] += c*BxBB[2];
    }

    // Clamp any non-finite results to zero for robustness.
    if (!isfinite(vdrift[0]) || !isfinite(vdrift[1]) || !isfinite(vdrift[2]) || !isfinite(dvpar_dt)) {
      vdrift[0]=0.0; vdrift[1]=0.0; vdrift[2]=0.0;
      dvpar_dt=0.0;
    }
  }

  //----------------------------------------------------------------------------------
  // Helper: commit reduced state and required outputs after a successful move
  //----------------------------------------------------------------------------------
  inline void CommitReducedStateAndVelocity(long int ptr,
                                            PIC::ParticleBuffer::byte *ParticleData,
                                            int spec,
                                            const double *x,
                                            cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,
                                            double vpar,
                                            double mu,
                                            const double vdrift_to_store[3]) {
    namespace PB = PIC::ParticleBuffer;
    // Evaluate B at final position to reconstruct v_perp from µ and to construct v=b*vpar.
    double absB=0.0,b[3]={0.0,0.0,0.0};
    double vdrift_dummy[3]={0.0,0.0,0.0};
    double dvpar_dt_dummy=0.0;

    EvalRHS(x,node,vpar,mu,spec,absB,b,vdrift_dummy,dvpar_dt_dummy);

    // Species mass for v_perp reconstruction
    double m,q;
    GetSpeciesMassCharge(m,q,spec);

    // v_perp = sqrt( 2 µ |B| / m )
    double vperp = 0.0;
    if (absB>0.0 && mu>0.0) {
      const double v2 = 2.0*mu*absB/m;
      vperp = (v2>0.0) ? sqrt(v2) : 0.0;
    }

    // Commit reduced state
    PB::SetVParallel(vpar,ParticleData);
    PB::SetVNormal(vperp,ParticleData);

    // Commit full 3D velocity vector v = b*v_parallel (requested)
    // Note: this is the *parallel* velocity vector, not including drifts.
    // If you want total GC velocity, add vdrift separately in diagnostics.
    double v[3];
    PB::GetV(v,ParticleData);
    v[0]=b[0]*vpar+vdrift_to_store[0];
    v[1]=b[1]*vpar+vdrift_to_store[1];
    v[2]=b[2]*vpar+vdrift_to_store[2];
    PB::SetV(v,ParticleData);

    // Commit drift velocity (requested)
    SetV_drift((double*)vdrift_to_store,ParticleData);
  }

  //==================================================================================
  // Mover_FirstOrder: Explicit Euler, 1st order accurate
  //==================================================================================
  int Mover_FirstOrder(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  namespace PB = PIC::ParticleBuffer;
    //--------------------------------------------------------------------------
    // 1) Load particle state and ensure GC reduced-state fields are initialized
    //--------------------------------------------------------------------------
    PIC::ParticleBuffer::byte *ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
    double *x = PIC::ParticleBuffer::GetX(ParticleData);
    int spec  = PIC::ParticleBuffer::GetI(ParticleData);

    // µ is invariant in this model
    const double mu   = PB::GetMagneticMoment(ptr);

    // Current v_parallel (stored in gyrokinetic slots)
    double vpar = PB::GetVParallel(ParticleData);

    //--------------------------------------------------------------------------
    // 2) Evaluate RHS at (x^n, vpar^n)
    //--------------------------------------------------------------------------
    double absB=0.0,b[3]={0.0,0.0,0.0};
    double vdrift[3]={0.0,0.0,0.0};
    double dvpar_dt=0.0;

    EvalRHS(x,startNode,vpar,mu,spec,absB,b,vdrift,dvpar_dt);

    //--------------------------------------------------------------------------
    // 3) Euler update
    //--------------------------------------------------------------------------
    // Position update:
    //   x^{n+1} = x^n + dt * (vdrift^n + b^n*vpar^n)
    x[0] += dtTotal*(vdrift[0] + b[0]*vpar);
    x[1] += dtTotal*(vdrift[1] + b[1]*vpar);
    x[2] += dtTotal*(vdrift[2] + b[2]*vpar);

    // Parallel velocity update:
    //   vpar^{n+1} = vpar^n + dt * dvpar_dt^n
    vpar += dtTotal*dvpar_dt;

    //--------------------------------------------------------------------------
    // 4) Check domain and locate new node
    //--------------------------------------------------------------------------
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode = PIC::Mesh::Search::FindBlock(x);
    if (newNode==NULL) {
      // Particle left computational domain: delete and return standard code
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    //--------------------------------------------------------------------------
    // 5) Re-evaluate drift at final position and commit required fields
    //--------------------------------------------------------------------------
    // We store drift velocity evaluated at the final point (x^{n+1}, vpar^{n+1}).
    double absB1=0.0,b1[3]={0.0,0.0,0.0};
    double vdrift1[3]={0.0,0.0,0.0};
    double dvpar_dt_dummy=0.0;

    EvalRHS(x,newNode,vpar,mu,spec,absB1,b1,vdrift1,dvpar_dt_dummy);

    // Commit:
    //   - v_parallel
    //   - v_normal reconstructed from µ and |B|
    //   - velocity vector v=b*vpar
    //   - drift velocity storage SetV_drift(vdrift1)
    CommitReducedStateAndVelocity(ptr,ParticleData,spec,x,newNode,vpar,mu,vdrift1);

    //--------------------------------------------------------------------------
    // 6) Internal boundary handling (mirrors other movers)
    //--------------------------------------------------------------------------
    #if defined(_TARGET_) 
    if  ((_TARGET_ID_(_TARGET_) != _TARGET_NONE__ID_) && (_INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_)) {
      double r2 = Vector3D::DotProduct(x,x);
      if (r2 < _RADIUS_(_TARGET_)*_RADIUS_(_TARGET_)) {
        // The reference movers often return _PARTICLE_DELETED_ON_THE_FACE_ logic here.
        // If your boundary handler differs, adjust accordingly.
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }
      else {
        newNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
      }
    }
    else {
      newNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
    }
    #else 
    newNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
    #endif
    
    startNode=newNode;

    //--------------------------------------------------------------------------
    // 7) Trajectory tracker (if enabled)
    //--------------------------------------------------------------------------
    if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
      double v[3];
      PIC::ParticleBuffer::GetV(v,ParticleData);
      PIC::ParticleTracker::RecordTrajectoryPoint(x,v,spec,ParticleData,(void*)startNode);

      if (_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_) {
        PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,ParticleData,(void*)startNode);
      }
    }

    //--------------------------------------------------------------------------
    // 8) Insert particle into temp moving list (same as reference movers)
    //--------------------------------------------------------------------------
    int i,j,k;
    PIC::Mesh::cDataBlockAMR *block;

    if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,newNode,false)==-1) {
      exit(__LINE__,__FILE__,"PIC::GYROKINETIC::Mover_FirstOrder(): cannot find cell index for moved particle");
    }

    if ((block=newNode->block)==NULL) {
      exit(__LINE__,__FILE__,"PIC::GYROKINETIC::Mover_FirstOrder(): destination block is empty; dt may be too large");
    }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    long int tempFirstCellParticle = atomic_exchange(
      block->tempParticleMovingListTable + i + _BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),
      ptr
    );

    PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
    if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
    long int tempFirstCellParticle,*tempFirstCellParticlePtr;

    tempFirstCellParticlePtr = block->tempParticleMovingListTable + i + _BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
    tempFirstCellParticle    = (*tempFirstCellParticlePtr);

    PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
    *tempFirstCellParticlePtr = ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable::cEntry *ThreadTemp =
      block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

    PIC::ParticleBuffer::SetNext(ThreadTemp->first,ParticleData);
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    if (ThreadTemp->last==-1) ThreadTemp->last=ptr;
    if (ThreadTemp->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTemp->first);
    ThreadTemp->first=ptr;

#else
#error The option is unknown
#endif

    return _PARTICLE_MOTION_FINISHED_;
  }

  //==================================================================================
  // Mover_SecondOrder: Explicit midpoint RK2, 2nd order accurate
  //==================================================================================
  int Mover_SecondOrder(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode) {
  namespace PB = PIC::ParticleBuffer;
    //--------------------------------------------------------------------------
    // 1) Load particle state and ensure reduced state is initialized
    //--------------------------------------------------------------------------
    PIC::ParticleBuffer::byte *ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
    double x[3];
    PB::GetX(x,ParticleData);
    int spec  = PB::GetI(ParticleData);

    const double mu   = PB::GetMagneticMoment(ptr);
    const double vpar0 = PB::GetVParallel(ParticleData);

    if (PB::IsParticleAllocated(ptr)==false) exit(__LINE__,__FILE__,"error: a particle is not allocated");

    // Save initial position (x^n)
    const double x0[3] = {x[0],x[1],x[2]};

    //--------------------------------------------------------------------------
    // 2) Stage A: evaluate RHS at (x^n, vpar^n)
    //--------------------------------------------------------------------------
    double absB0=0.0,b0[3]={0.0,0.0,0.0};
    double vdrift0[3]={0.0,0.0,0.0};
    double dvpar_dt0=0.0;

    EvalRHS(x0,startNode,vpar0,mu,spec,absB0,b0,vdrift0,dvpar_dt0);

    //--------------------------------------------------------------------------
    // 3) Midpoint prediction (x^{n+1/2}, vpar^{n+1/2})
    //--------------------------------------------------------------------------
    double xHalf[3];
    xHalf[0] = x0[0] + 0.5*dtTotal*(vdrift0[0] + b0[0]*vpar0);
    xHalf[1] = x0[1] + 0.5*dtTotal*(vdrift0[1] + b0[1]*vpar0);
    xHalf[2] = x0[2] + 0.5*dtTotal*(vdrift0[2] + b0[2]*vpar0);

    const double vparHalf = vpar0 + 0.5*dtTotal*dvpar_dt0;

    // Midpoint must be inside domain to evaluate fields there
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* nodeHalf = PIC::Mesh::Search::FindBlock(xHalf);
    if (nodeHalf==NULL) {
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    //--------------------------------------------------------------------------
    // 4) Stage B: evaluate RHS at midpoint
    //--------------------------------------------------------------------------
    double absBH=0.0,bH[3]={0.0,0.0,0.0};
    double vdriftH[3]={0.0,0.0,0.0};
    double dvpar_dtH=0.0;

    EvalRHS(xHalf,nodeHalf,vparHalf,mu,spec,absBH,bH,vdriftH,dvpar_dtH);

    //--------------------------------------------------------------------------
    // 5) Full update using midpoint slope
    //--------------------------------------------------------------------------
    x[0] = x0[0] + dtTotal*(vdriftH[0] + bH[0]*vparHalf);
    x[1] = x0[1] + dtTotal*(vdriftH[1] + bH[1]*vparHalf);
    x[2] = x0[2] + dtTotal*(vdriftH[2] + bH[2]*vparHalf);
    PB::SetX(x,ParticleData);

    const double vpar1 = vpar0 + dtTotal*dvpar_dtH;

    //--------------------------------------------------------------------------
    // 6) Check domain and locate final node
    //--------------------------------------------------------------------------
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* newNode = PIC::Mesh::Search::FindBlock(x);
    if (newNode==NULL) {
      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_LEFT_THE_DOMAIN_;
    }

    //--------------------------------------------------------------------------
    // 7) Re-evaluate drift at final point and commit required outputs
    //--------------------------------------------------------------------------
    double absB1=0.0,b1[3]={0.0,0.0,0.0};
    double vdrift1[3]={0.0,0.0,0.0};
    double dvpar_dt_dummy=0.0;

    EvalRHS(x,newNode,vpar1,mu,spec,absB1,b1,vdrift1,dvpar_dt_dummy);

    CommitReducedStateAndVelocity(ptr,ParticleData,spec,x,newNode,vpar1,mu,vdrift1);

    

    //--------------------------------------------------------------------------
    // 8) Internal boundary handling (same style as other movers)
    //--------------------------------------------------------------------------
    #if defined(_TARGET_) 
    if  ((_TARGET_ID_(_TARGET_) != _TARGET_NONE__ID_) && (_INTERNAL_BOUNDARY_MODE_ == _INTERNAL_BOUNDARY_MODE_ON_)) {
      double r2 = Vector3D::DotProduct(x,x);
      if (r2 < _RADIUS_(_TARGET_)*_RADIUS_(_TARGET_)) {
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_LEFT_THE_DOMAIN_;
      }
      else {
        newNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
      }
    }
    else {
      newNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
    }
    #else  
    newNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
    #endif

    startNode=newNode;

    //--------------------------------------------------------------------------
    // 9) Trajectory tracker (if enabled)
    //--------------------------------------------------------------------------
    if (_PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_) {
      double v[3];
      PIC::ParticleBuffer::GetV(v,ParticleData);
      PIC::ParticleTracker::RecordTrajectoryPoint(x,v,spec,ParticleData,(void*)startNode);

      if (_PIC_PARTICLE_TRACKER__TRACKING_CONDITION_MODE__DYNAMICS_ == _PIC_MODE_ON_) {
        PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,ParticleData,(void*)startNode);
      }
    }

    //--------------------------------------------------------------------------
    // 10) Insert into temp moving list (same as reference movers)
    //--------------------------------------------------------------------------
    int i,j,k;
    PIC::Mesh::cDataBlockAMR *block;

    if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,newNode,false)==-1) {
      exit(__LINE__,__FILE__,"PIC::GYROKINETIC::Mover_SecondOrder(): cannot find cell index for moved particle");
    }

    if ((block=newNode->block)==NULL) {
      exit(__LINE__,__FILE__,"PIC::GYROKINETIC::Mover_SecondOrder(): destination block is empty; dt may be too large");
    }

#if _PIC_MOVER__MPI_MULTITHREAD_ == _PIC_MODE_ON_
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    long int tempFirstCellParticle = atomic_exchange(
      block->tempParticleMovingListTable + i + _BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k),
      ptr
    );

    PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
    if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__MPI_
    long int tempFirstCellParticle,*tempFirstCellParticlePtr;

    tempFirstCellParticlePtr = block->tempParticleMovingListTable + i + _BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
    tempFirstCellParticle    = (*tempFirstCellParticlePtr);

    PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
    *tempFirstCellParticlePtr = ptr;

#elif _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    PIC::Mesh::cDataBlockAMR::cTempParticleMovingListMultiThreadTable::cEntry *ThreadTemp =
      block->GetTempParticleMovingListMultiThreadTable(omp_get_thread_num(),i,j,k);

    PIC::ParticleBuffer::SetNext(ThreadTemp->first,ParticleData);
    PIC::ParticleBuffer::SetPrev(-1,ParticleData);

    if (ThreadTemp->last==-1) ThreadTemp->last=ptr;
    if (ThreadTemp->first!=-1) PIC::ParticleBuffer::SetPrev(ptr,ThreadTemp->first);
    ThreadTemp->first=ptr;

#else
#error The option is unknown
#endif

    return _PARTICLE_MOTION_FINISHED_;
  }

} // namespace GYROKINETIC
} // namespace PIC

