#include <cmath>

#include <AMReX.H>
#include <AMReX_MultiFab.H>

#include "Particles.h"
#include "ReadParam.h"
#include "WaveIC.h"

// Pure-EM lightwave: zero particles so only the field solver runs.
void WaveIC::apply_particle_override(ParticlesInfo& pInfo) const {
  if (profile_ == LightWave)
    pInfo.nPartPerCell = amrex::IntVect::Zero;
}

using namespace amrex;

std::string WaveIC::name() const {
  switch (profile_) {
    case LightWave:
      return "lightwave";
    case HybridWave:
      return "hybridwave";
    case ConvectionWave:
      return "convectionwave";
    case IonAcousticWave:
      return "ionacousticwave";
    case Generic:
      return "waveic";
  }
  return "waveic";
}

// Fill preset defaults for the active profile; #WAVEIC sub-params override.
void WaveIC::apply_preset() {
  switch (profile_) {
    case LightWave:
      seedE_ = true;
      seedB_ = true;
      oblique_ = true;
      guideField_ = false;
      velKick_ = false;
      seedWeight_ = false;
      dir_[0] = 0.6;
      dir_[1] = 0.8;
      dir_[2] = 0.0;
      waveLength_ = 48.0;
      frac_ = 0.0; // unused for lightwave
      pert_ = 0.0;
      break;
    case HybridWave:
      seedE_ = false;
      seedB_ = true;
      oblique_ = false;
      guideField_ = true;
      velKick_ = true;
      seedWeight_ = false;
      waveMode_ = 1;
      frac_ = 0.02;
      break;
    case ConvectionWave:
      seedE_ = false;
      seedB_ = true;
      oblique_ = false;
      guideField_ = true;
      velKick_ = false; // the distinguishing flag vs hybridwave
      seedWeight_ = false;
      waveMode_ = 1;
      frac_ = 0.20;
      break;
    case IonAcousticWave:
      seedE_ = false;
      seedB_ = false;
      oblique_ = false;
      guideField_ = false;
      velKick_ = false;
      seedWeight_ = true;
      waveMode_ = 1;
      pert_ = 0.1;
      break;
    case Generic:
      seedE_ = false;
      seedB_ = false;
      oblique_ = false;
      guideField_ = false;
      velKick_ = false;
      seedWeight_ = false;
      waveMode_ = 1;
      frac_ = 0.0;
      pert_ = 0.0;
      break;
  }
}

void WaveIC::read_param(ReadParam& param) {
  apply_preset();
  // All sub-parameters are optional.
  param.read_optional("seedE", seedE_);
  param.read_optional("seedB", seedB_);
  param.read_optional("oblique", oblique_);
  param.read_optional("guideField", guideField_);
  param.read_optional("velKick", velKick_);
  param.read_optional("seedWeight", seedWeight_);
  param.read_optional("dir", dir_[0]);
  param.read_optional("dir", dir_[1]);
  param.read_optional("dir", dir_[2]);
  param.read_optional("waveLength", waveLength_);
  param.read_optional("waveMode", waveMode_);
  param.read_optional("frac", frac_);
  param.read_optional("pert", pert_);
  param.read_optional("anisoTPerpOverTPar", anisoTPerpOverTPar_);
}

void WaveIC::set_fields(PicICFields& fields) const {
  const int nLev = fields.n_lev();
  if (nLev == 0)
    return;

  for (int iLev = 0; iLev < nLev; ++iLev) {
    // Guide field Bx0 already deposited by fill_E_B_fields().
    amrex::Real Bx0 = 1.0;
    if (guideField_) {
      MFIter mfi(fields.node_B(iLev));
      if (mfi.isValid()) {
        const Array4<Real const>& a = fields.node_B(iLev).array(mfi);
        const Box& b = mfi.validbox();
        Bx0 = a(b.smallEnd(), ix_);
      }
    }
    const amrex::Real B1 = frac_ * Bx0;
    B1_ = B1;

    const auto& prob_lo = fields.geom(iLev).ProbLo();
    const auto& dx = fields.geom(iLev).CellSize();
    const amrex::Real Lx = (fields.geom(iLev).ProbHi())[0] - prob_lo[0];
    Lx_ = Lx;

    // Phase wavenumber K.
    amrex::Real Kx, Ky, Kz;
    if (oblique_) {
      const amrex::Real kMag = 2.0 * dPI / waveLength_;
      Kx = kMag * dir_[0];
      Ky = kMag * dir_[1];
      Kz = kMag * dir_[2];
      kx_ = kMag;
    } else {
      kx_ = (Lx > 0.0) ? 2.0 * dPI * waveMode_ / Lx : 0.0;
      Kx = kx_;
      Ky = 0.0;
      Kz = 0.0;
    }

    MultiFab& nodeE = fields.node_E(iLev);
    MultiFab& nodeB = fields.node_B(iLev);
    MultiFab& centerB = fields.center_B(iLev);

    if (oblique_) {
      if (seedE_)
        nodeE.setVal(0.0);
      if (seedB_) {
        nodeB.setVal(0.0);
        centerB.setVal(0.0);
      }

      const amrex::Real n0 = dir_[0], n1 = dir_[1];

      if (seedE_ || seedB_) {
        for (MFIter mfi(nodeE); mfi.isValid(); ++mfi) {
          FArrayBox& fabE = nodeE[mfi];
          FArrayBox& fabB = nodeB[mfi];
          const Box& box = mfi.fabbox();
          const Array4<Real>& arrE = fabE.array();
          const Array4<Real>& arrB = fabB.array();
          ParallelFor(box, [&](int i, int j, int k) {
            const amrex::Real x = prob_lo[0] + dx[0] * i;
            const amrex::Real y = prob_lo[1] + dx[1] * j;
            const amrex::Real z = prob_lo[2] + dx[2] * k;
            const amrex::Real phase = Kx * x + Ky * y + Kz * z;
            const amrex::Real cphi = std::cos(phase);
            const amrex::Real sphi = std::sin(phase);
            if (seedE_) {
              arrE(i, j, k, ix_) = -n1 * sphi;
              arrE(i, j, k, iy_) = n0 * sphi;
              arrE(i, j, k, iz_) = -cphi;
            }
            if (seedB_) {
              arrB(i, j, k, ix_) = -n1 * cphi;
              arrB(i, j, k, iy_) = n0 * cphi;
              arrB(i, j, k, iz_) = sphi;
            }
          });
        }
      }
      if (seedB_) {
        for (MFIter mfi(centerB); mfi.isValid(); ++mfi) {
          FArrayBox& fabcB = centerB[mfi];
          const Box& box = mfi.fabbox();
          const Array4<Real>& arrcB = fabcB.array();
          ParallelFor(box, [&](int i, int j, int k) {
            const amrex::Real x = prob_lo[0] + dx[0] * (i + 0.5);
            const amrex::Real y = prob_lo[1] + dx[1] * (j + 0.5);
            const amrex::Real z = prob_lo[2] + dx[2] * (k + 0.5);
            const amrex::Real phase = Kx * x + Ky * y + Kz * z;
            const amrex::Real cphi = std::cos(phase);
            const amrex::Real sphi = std::sin(phase);
            arrcB(i, j, k, ix_) = -n1 * cphi;
            arrcB(i, j, k, iy_) = n0 * cphi;
            arrcB(i, j, k, iz_) = sphi;
          });
        }
      }
    } else {
      // Transverse circularly-polarized wave: B = (Bx0, B1 cos kx, B1 sin kx)
      if (seedB_) {
        nodeB.setVal(0.0);
        centerB.setVal(0.0);
        for (MFIter mfi(nodeB); mfi.isValid(); ++mfi) {
          FArrayBox& fab = nodeB[mfi];
          const Box& box = mfi.fabbox();
          const Array4<Real>& arrB = fab.array();
          ParallelFor(box, [&](int i, int j, int k) {
            const amrex::Real x = prob_lo[0] + dx[0] * i;
            const amrex::Real cphi = std::cos(kx_ * x);
            const amrex::Real sphi = std::sin(kx_ * x);
            arrB(i, j, k, ix_) = Bx0;
            arrB(i, j, k, iy_) = B1 * cphi;
            arrB(i, j, k, iz_) = B1 * sphi;
          });
        }
        for (MFIter mfi(centerB); mfi.isValid(); ++mfi) {
          FArrayBox& fab = centerB[mfi];
          const Box& box = mfi.fabbox();
          const Array4<Real>& arrB = fab.array();
          ParallelFor(box, [&](int i, int j, int k) {
            const amrex::Real x = prob_lo[0] + dx[0] * (i + 0.5);
            const amrex::Real cphi = std::cos(kx_ * x);
            const amrex::Real sphi = std::sin(kx_ * x);
            arrB(i, j, k, ix_) = Bx0;
            arrB(i, j, k, iy_) = B1 * cphi;
            arrB(i, j, k, iz_) = B1 * sphi;
          });
        }
      }
    }
  }

  fields.fill_boundary_E_B();
}

void WaveIC::modify_particle_weight(ParticleICState& s) const {
  if (!seedWeight_ || Lx_ <= 0.0)
    return;
  const amrex::Real factor = 1.0 + pert_ * std::sin(kx_ * s.x);
  s.q *= (factor > 0.0) ? factor : 0.0;
}

void WaveIC::modify_particle_velocity(ParticleICState& s) const {
  if (!velKick_)
    return;
  // Transverse Alfven velocity B = (0, B1 cos kx, B1 sin kx).
  const amrex::Real cphi = std::cos(kx_ * s.x);
  const amrex::Real sphi = std::sin(kx_ * s.x);
  s.vBulk -= B1_ * cphi;
  s.wBulk -= B1_ * sphi;
}

// Bi-Maxwellian seeding: treat #UNIFORMSTATE T as T_par (parallel draw, x)
// and inflate the two perpendicular draws by sqrt(T_perp/T_par).
void WaveIC::modify_particle_thermal_velocity(ParticleICState& s) const {
  if (anisoTPerpOverTPar_ <= 0.0)
    return;
  const amrex::Real scale = std::sqrt(anisoTPerpOverTPar_);
  s.vThermal *= scale;
  s.wThermal *= scale;
}
