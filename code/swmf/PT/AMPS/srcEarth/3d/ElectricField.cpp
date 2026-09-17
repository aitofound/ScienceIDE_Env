#include "ElectricField.h"

#include <cmath>
#include <algorithm>
#include <string>

#include "pic.h"

//--------------------------------------------------------------------------------------
// Tsyganenko interfaces and live SWMF coupling are mutually exclusive in this source.
//--------------------------------------------------------------------------------------
// In _PIC_COUPLER_MODE__SWMF_ builds the authoritative background fields are the
// cell-centered fields imported from SWMF into the AMPS coupler buffers.  Therefore this
// file must not include or call the Tsyganenko wrappers in that build: doing so would
// introduce invalid model selection and unnecessary Geopack/Tsyganenko link dependencies.
//
// In non-SWMF builds we preserve the existing behavior for the supported standalone
// magnetic models used by the 3D electric-field initialization path.
#if _PIC_COUPLER_MODE_ != _PIC_COUPLER_MODE__SWMF_
#include "../../interface/T96Interface.h"
#include "../../interface/T05Interface.h"
#include "../../interface/TA16Interface.h"
#include "GeopackInterface.h"
#endif

#include "../gridless/DipoleInterface.h"

namespace {
inline double sqr(double x) { return x*x; }
const double kOmegaEarth = 7.2921159e-5;

void Cross(const double a[3],const double b[3],double c[3]) {
  c[0]=a[1]*b[2]-a[2]*b[1];
  c[1]=a[2]*b[0]-a[0]*b[2];
  c[2]=a[0]*b[1]-a[1]*b[0];
}

void EvalDipoleSI(double B[3],const double x[3],const EarthUtil::AmpsParam& prm) {
  // Reuse the analytic dipole helper that already exists in srcEarth/gridless.
  // That interface owns the canonical dipole constants and the tilt semantics
  // used elsewhere in srcEarth, so using it here keeps the 3D initialization
  // path consistent with the dipole-only cutoff/density workflows.
  //
  // IMPORTANT:
  // The gridless dipole interface does not expose a free function named
  // `GetMagneticFieldDipole(...)`. Instead, it keeps a small shared state in
  // `Earth::GridlessMode::Dipole::gParams` and provides the public helpers
  // `SetMomentScale`, `SetTiltDeg`, and `GetB_Tesla`.
  //
  // ConfigureBackgroundFieldModel() freezes the dipole parameters before mesh
  // initialization starts.  Do not call the setters here: this evaluator may be
  // entered concurrently by temporary POSIX field-initialization workers.
  (void)prm;
  Earth::GridlessMode::Dipole::GetB_Tesla(x,B);
}

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
//--------------------------------------------------------------------------------------
// Live SWMF-coupled field access
//--------------------------------------------------------------------------------------
// The direct SWMF coupler does not evaluate T96/T05/TA16 in this source.  Instead, SWMF
// sends the MHD state to AMPS, AMPS stores B and plasma velocity in the cell-centered
// coupler buffers, and PIC::CPLR exposes the same interpolated fields that the standard
// AMPS particle movers use.
//
// Magnetic field:
//   PIC::CPLR::GetBackgroundMagneticField(B) dispatches to
//   PIC::CPLR::SWMF::GetBackgroundMagneticField(...) and interpolates the SWMF B field
//   from the current AMPS interpolation stencil.
//
// Electric field:
//   PIC::CPLR::GetBackgroundElectricField(E) dispatches to the SWMF coupler path, where
//   the field is reconstructed as the ideal-MHD field E = -v x B from the SWMF plasma
//   velocity and magnetic field stored in the same cell-centered buffers.
//
// This helper mirrors the access pattern used by AMPS movers: find the AMR block that
// contains x, initialize the coupler interpolation stencil, then call the global PIC::CPLR
// accessor.  A thread-local node cache avoids repeatedly searching from the AMR root when
// this routine is called for many neighboring mesh points during 3D initialization.
static cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* FindSWMFNodeOrDie(const double x[3],
                                                                const char* caller) {
  if (PIC::Mesh::mesh == NULL) {
    exit(__LINE__,__FILE__,
         "Error in Earth::Mode3D::ElectricField: PIC::Mesh::mesh is NULL; "
         "cannot interpolate SWMF-coupled background fields");
  }

  // InitInterpolationStencil takes a non-const pointer, so work with a local copy.
  // The coordinates themselves remain in the same SI/GSM system used by AMPS.
  double xLocal[3]={x[0],x[1],x[2]};

  static thread_local cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* swmfLastNode = NULL;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node =
    (swmfLastNode != NULL)
      ? PIC::Mesh::mesh->findTreeNode(xLocal,swmfLastNode)
      : PIC::Mesh::mesh->findTreeNode(xLocal);

  // If the cached-node search fails, retry from the root before reporting a true
  // out-of-mesh condition.  This makes the cache safe across large jumps between
  // initialization points or changes in AMR-block ownership.
  if (node == NULL) node = PIC::Mesh::mesh->findTreeNode(xLocal);

  if (node == NULL) {
    (void)caller;
    exit(__LINE__,__FILE__,
         "Error in Earth::Mode3D::ElectricField: point is outside the AMPS mesh; "
         "cannot interpolate SWMF-coupled background fields");
  }

  swmfLastNode = node;
  return node;
}

static void EvalSWMFBkgMagneticFieldSI(double B[3],const double x[3]) {
  double xLocal[3]={x[0],x[1],x[2]};
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node =
    FindSWMFNodeOrDie(xLocal,"EvalSWMFBkgMagneticFieldSI");

  PIC::CPLR::InitInterpolationStencil(xLocal,node);
  PIC::CPLR::GetBackgroundMagneticField(B);
}

static void EvalSWMFBkgElectricFieldSI(double E[3],const double x[3]) {
  double xLocal[3]={x[0],x[1],x[2]};
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node =
    FindSWMFNodeOrDie(xLocal,"EvalSWMFBkgElectricFieldSI");

  PIC::CPLR::InitInterpolationStencil(xLocal,node);
  PIC::CPLR::GetBackgroundElectricField(E);
}
#endif
} // namespace

namespace Earth {
namespace Mode3D {

void EvaluateBackgroundMagneticFieldSI(double B[3],const double xGSM_SI[3],const EarthUtil::AmpsParam& prm) {
  const std::string model=EarthUtil::ToUpper(prm.field.model);

  // The analytic dipole remains available in every build.  It is useful for
  // controlled validation runs and does not depend on either Tsyganenko/Geopack or
  // SWMF-coupler state.
  if (model=="DIPOLE") { EvalDipoleSI(B,xGSM_SI,prm); return; }

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // Live SWMF-coupled path.
  //
  // T96/T05/TA16 are standalone empirical models and must not be used when AMPS is
  // compiled as an SWMF-coupled component.  In that mode the magnetic field used by
  // AMPS is the SWMF-supplied field stored in the coupler data buffers and exposed
  // through PIC::CPLR::GetBackgroundMagneticField().  Use that same accessor here so
  // mesh initialization, diagnostics, and particle movers all sample the same B.
  (void)prm;
  EvalSWMFBkgMagneticFieldSI(B,xGSM_SI);
  return;
#else
  // Standalone field coordinates are GSM.
  //
  // ConfigureBackgroundFieldModel() initializes IGRF/T96/T05/TA16 explicitly
  // in GSM before the AMR mesh is populated.  The xGSM_SI position passed below
  // is therefore in the same frame expected by every standalone field wrapper.
  //
  // IGRF-only standalone field.
  // ConfigureBackgroundFieldModel() calls Geopack::Init(epoch,"GSM") once before
  // the AMR mesh is populated.  The reentrant Geopack IGRF evaluator then reads
  // the frozen context at the supplied GSM/SI position; no Tsyganenko external
  // contribution is added.  This is the field definition required by the
  // Smart--Shea, CARI-7, and Gerontidou C6 reference tables.
  if (model=="IGRF") {
    Geopack::IGRF::GetMagneticField(B,const_cast<double*>(xGSM_SI));
    return;
  }
  if (model=="T96") { ::T96::GetMagneticField(B,const_cast<double*>(xGSM_SI)); return; }
  if (model=="T05") { ::T05::GetMagneticField(B,const_cast<double*>(xGSM_SI)); return; }
  if (model=="TA16") { ::TA16::GetMagneticField(B,const_cast<double*>(xGSM_SI)); return; }
  B[0]=B[1]=B[2]=0.0;
#endif
}

// Corotation: E = -(Omega x r) x B. This is a clean SI-space implementation
// of Earth corotation suitable for initializing a mesh field. The literature
// often discusses corotation together with the Volland-Stern family of inner-
// magnetosphere convection models: Volland (1973), Stern (1975), Maynard &
// Chen (1975), and later model comparisons such as Pierrard et al. (2008).
static void EvalCorotationSI(double E[3],const double x[3],const EarthUtil::AmpsParam& prm) {
  double B[3],vx[3],e[3],omega[3]={0.0,0.0,kOmegaEarth};
  EvaluateBackgroundMagneticFieldSI(B,x,prm);
  Cross(omega,x,vx);
  Cross(vx,B,e);
  E[0]=-prm.efield.corotationScale*e[0];
  E[1]=-prm.efield.corotationScale*e[1];
  E[2]=-prm.efield.corotationScale*e[2];
}
// Scalar potential for the corotation electric field.
//
// Derivation (equatorial-plane, aligned dipole):
//   E_corot = -(Ω×r)×B  →  E_r = -Ω_E · B_eq(r)· r (cylindrical)
//
//   For a dipole: B_z(equator) = +B_eq · (R_E/r)³, so
//     E_r = -Ω_E · B_eq · R_E³ / r²
//     → ∂Φ/∂r = Ω_E · B_eq · R_E³ / r²
//     → Φ_corot(r) = -Ω_E · B_eq · R_E³ / r      (Volland 1973, Stern 1975)
//
// For non-dipole models (T96/T05/TA16) the same formula is used with the IGRF
// dipole reference: corotation is dominated by the internal (dipolar) field at
// L < 4, where this approximation is accurate to a few percent.
//
// Scale: at r = R_E, |Φ_corot| ≈ 92 kV for the standard IGRF dipole moment —
// consistent with published ionospheric corotation voltage estimates.
//
// corotationScale mirrors the factor used in EvalCorotationSI so that the
// potential and its gradient are mutually consistent.
static double EvalCorotationPotentialV(const double x[3],const EarthUtil::AmpsParam& prm) {
  // Reference IGRF equatorial surface field (~3.12e-5 T).  prm.field.dipoleMoment_Me
  // is the scale factor applied by EvalDipoleSI → SetMomentScale; use it here
  // to keep the potential consistent with the dipole-model E field.
  const double kBequatorial_SI = 3.12e-5;  // T
  const double B_eq = (prm.field.dipoleMoment_Me > 0.0)
                        ? prm.field.dipoleMoment_Me * kBequatorial_SI
                        : kBequatorial_SI;

  const double r = std::sqrt(sqr(x[0]) + sqr(x[1]) + sqr(x[2]));
  if (r < 1.0) return 0.0;  // guard against r ≈ 0; 1 m << R_E is safe

  return -prm.efield.corotationScale
       * kOmegaEarth * B_eq * std::pow(_EARTH__RADIUS_, 3) / r;
}

// Volland-Stern-type volumetric potential. Classical published forms are most
// commonly given in equatorial/L-shell variables. For AMPS mesh initialization
// we need an everywhere-defined 3D recipe, so we use a simple cylindrical
// extension with the standard shielded two-cell local-time dependence.
static double EvalVSPotentialV(const double x[3],const EarthUtil::AmpsParam& prm) {
  const double rho_m=std::sqrt(sqr(x[0])+sqr(x[1]));
  const double rho_re=std::max(rho_m/_EARTH__RADIUS_,prm.efield.lMin);
  const double phi=std::atan2(x[1],x[0]);
  const double ampV=1000.0*prm.efield.vsPotential_kV*prm.efield.vsScale;
  return ampV*std::pow(prm.efield.vsReferenceL/rho_re,prm.efield.vsGamma)*std::sin(phi);
}

static void EvalVollandSternSI(double E[3],const double x[3],const EarthUtil::AmpsParam& prm) {
  const double r=std::sqrt(sqr(x[0])+sqr(x[1])+sqr(x[2]));
  const double h=std::max(1.0e3,1.0e-5*std::max(r,_EARTH__RADIUS_));
  E[0]=E[1]=E[2]=0.0;
  for (int idim=0;idim<3;idim++) {
    double xp[3]={x[0],x[1],x[2]},xm[3]={x[0],x[1],x[2]};
    xp[idim]+=h; xm[idim]-=h;
    E[idim]=-(EvalVSPotentialV(xp,prm)-EvalVSPotentialV(xm,prm))/(2.0*h);
  }
}

void EvaluateElectricFieldSI(double E[3],const double xGSM_SI[3],const EarthUtil::AmpsParam& prm) {
  E[0]=E[1]=E[2]=0.0;

#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // Live SWMF-coupled path.
  //
  // In the direct AMPS/SWMF coupling mode the background electric field used by the
  // AMPS particle movers is obtained through PIC::CPLR::GetBackgroundElectricField().
  // For SWMF this accessor reconstructs the ideal-MHD electric field E = -v x B from
  // the plasma velocity and magnetic field imported from SWMF into AMPS cell buffers.
  //
  // Therefore, do not evaluate the local corotation/Volland-Stern models here in an
  // SWMF-coupled build.  Returning the coupled field keeps this 3D initialization /
  // diagnostic path consistent with the live AMPS particle-tracing path.
  (void)prm;
  EvalSWMFBkgElectricFieldSI(E,xGSM_SI);
  return;
#else
  const std::string model=EarthUtil::ToUpper(prm.efield.model);
  if (model=="NONE" || model.empty()) return;
  const double r=std::sqrt(sqr(xGSM_SI[0])+sqr(xGSM_SI[1])+sqr(xGSM_SI[2]));
  if (r<std::max(1.0,prm.efield.rMin_km*1000.0)) return;
  if (model=="COROTATION") { EvalCorotationSI(E,xGSM_SI,prm); return; }
  if (model=="VOLLAND_STERN") { EvalVollandSternSI(E,xGSM_SI,prm); return; }
  if (model=="COROTATION_VOLLAND_STERN") {
    double a[3],b[3];
    EvalCorotationSI(a,xGSM_SI,prm);
    EvalVollandSternSI(b,xGSM_SI,prm);
    for (int i=0;i<3;i++) E[i]=a[i]+b[i];
  }
#endif
}

double EvaluateElectricPotential_V(const double xGSM_SI[3],const EarthUtil::AmpsParam& prm) {
#if _PIC_COUPLER_MODE_ == _PIC_COUPLER_MODE__SWMF_
  // The SWMF-coupled electric field is the interpolated ideal-MHD field E = -v x B.
  // In general that field is not represented in AMPS by a scalar electrostatic
  // potential.  Return zero rather than reporting an inconsistent analytic potential
  // from the standalone corotation/Volland-Stern models.
  (void)xGSM_SI;
  (void)prm;
  return 0.0;
#else
  const std::string model=EarthUtil::ToUpper(prm.efield.model);
  if (model=="COROTATION") return EvalCorotationPotentialV(xGSM_SI,prm);
  if (model=="VOLLAND_STERN") return EvalVSPotentialV(xGSM_SI,prm);
  if (model=="COROTATION_VOLLAND_STERN") return EvalCorotationPotentialV(xGSM_SI,prm)+EvalVSPotentialV(xGSM_SI,prm);
  return 0.0;
#endif
}

} // namespace Mode3D
} // namespace Earth
