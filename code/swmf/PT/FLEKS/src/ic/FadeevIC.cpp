#include <cmath>

#include <AMReX.H>
#include <AMReX_MultiFab.H>

#include "FadeevIC.h"
#include "Particles.h"
#include "ReadParam.h"

using namespace amrex;

void FadeevIC::read_param(ReadParam& param) {
  param.read_optional("L", L_);
  param.read_optional("eps", eps_);
  param.read_optional("numIslands", numIslands_);
  param.read_optional("b0", b0_);
  param.read_optional("bg", bg_);
  param.read_optional("perturb", perturb_);
  param.read_optional("nb", nb_);
}

void FadeevIC::set_fields(PicICFields& fields) const {
  const int nLev = fields.n_lev();
  if (nLev == 0)
    return;

  for (int iLev = 0; iLev < nLev; ++iLev) {
    const auto& geom = fields.geom(iLev);
    const auto& prob_lo = geom.ProbLo();
    const auto& prob_hi = geom.ProbHi();
    const auto& dx = geom.CellSize();
    const amrex::Real Lx = prob_hi[0] - prob_lo[0];
    const amrex::Real Ly = prob_hi[1] - prob_lo[1];
    Lx_ = Lx;
    Ly_ = Ly;

    invLx_ = (Lx > 0.0) ? 1.0 / Lx : 0.0;
    invLy_ = (Ly > 0.0) ? 1.0 / Ly : 0.0;
    // profile_max at the O-points (x = +-pi*L, y = 0).
    profileMax_ = (1.0 + eps_) / (1.0 - eps_);

    // Overwrite uniform B with the Fadeev equilibrium + m=1 perturbation.
    const amrex::Real b0 = b0_;
    const amrex::Real bg = bg_;
    const amrex::Real eps = eps_;
    const amrex::Real L = L_;
    const amrex::Real perturb = perturb_;

    MultiFab& nodeB = fields.node_B(iLev);
    MultiFab& centerB = fields.center_B(iLev);

    nodeB.setVal(0.0);
    centerB.setVal(0.0);

    // Node-centered B (i,j,k on nodes).
    for (MFIter mfi(nodeB); mfi.isValid(); ++mfi) {
      FArrayBox& fab = nodeB[mfi];
      const Box& box = mfi.fabbox();
      const Array4<Real>& arrB = fab.array();
      ParallelFor(box, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
        const amrex::Real x = prob_lo[0] + dx[0] * i;
        const amrex::Real y = prob_lo[1] + dx[1] * j;
        const amrex::Real denom = std::cosh(y / L) + eps * std::cos(x / L);
        const amrex::Real denomInv = (denom > 0.0) ? 1.0 / denom : 0.0;
        // m=1 perturbation.
        const amrex::Real dbx = -perturb * b0 * Lx * invLy_ *
                                std::cos(2.0 * dPI * x * invLx_) *
                                std::sin(dPI * y * invLy_);
        const amrex::Real dbz = perturb * b0 * 2.0 *
                                std::sin(2.0 * dPI * x * invLx_) *
                                std::cos(dPI * y * invLy_);
        arrB(i, j, k, ix_) = b0 * std::sinh(y / L) * denomInv + dbx;
        arrB(i, j, k, iy_) = b0 * eps * std::sin(x / L) * denomInv + dbz;
        arrB(i, j, k, iz_) = b0 * bg;
      });
    }
    // Center-centered B (i+0.5, j+0.5).
    for (MFIter mfi(centerB); mfi.isValid(); ++mfi) {
      FArrayBox& fab = centerB[mfi];
      const Box& box = mfi.fabbox();
      const Array4<Real>& arrB = fab.array();
      ParallelFor(box, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
        const amrex::Real x = prob_lo[0] + dx[0] * (i + 0.5);
        const amrex::Real y = prob_lo[1] + dx[1] * (j + 0.5);
        const amrex::Real denom = std::cosh(y / L) + eps * std::cos(x / L);
        const amrex::Real denomInv = (denom > 0.0) ? 1.0 / denom : 0.0;
        const amrex::Real dbx = -perturb * b0 * Lx * invLy_ *
                                std::cos(2.0 * dPI * x * invLx_) *
                                std::sin(dPI * y * invLy_);
        const amrex::Real dbz = perturb * b0 * 2.0 *
                                std::sin(2.0 * dPI * x * invLx_) *
                                std::cos(dPI * y * invLy_);
        arrB(i, j, k, ix_) = b0 * std::sinh(y / L) * denomInv + dbx;
        arrB(i, j, k, iy_) = b0 * eps * std::sin(x / L) * denomInv + dbz;
        arrB(i, j, k, iz_) = b0 * bg;
      });
    }
  }

  fields.fill_boundary_E_B();
}

// Boost background density nb to the Fadeev sheet profile.
void FadeevIC::modify_particle_weight(ParticleICState& s) const {
  if (profileMax_ <= 0.0 || nb_ <= 0.0 || Ly_ <= 0.0)
    return;
  const amrex::Real denom = std::cosh(s.y / L_) + eps_ * std::cos(s.x / L_);
  const amrex::Real profile = (1.0 - eps_ * eps_) / (denom * denom);
  const amrex::Real nRatio = profile / profileMax_;
  const amrex::Real factor = (nb_ + (1.0 - nb_) * nRatio) / nb_;
  s.q *= (factor > 0.0) ? factor : 0.0;
}

// Diamagnetic drift u_s = -(T_s/q_s) (grad(n) x B)/(n B^2) for force balance.
void FadeevIC::modify_particle_velocity(ParticleICState& s) const {
  if (profileMax_ <= 0.0 || nb_ <= 0.0 || Ly_ <= 0.0)
    return;
  if (s.temperature <= 0.0 || s.charge == 0.0)
    return;

  const amrex::Real x = s.x;
  const amrex::Real y = s.y;
  const amrex::Real eps = eps_;
  const amrex::Real L = L_;
  const amrex::Real b0 = b0_;
  const amrex::Real nb = nb_;
  const amrex::Real perturb = perturb_;

  const amrex::Real D = std::cosh(y / L) + eps * std::cos(x / L);
  if (D <= 0.0)
    return;
  const amrex::Real profile = (1.0 - eps * eps) / (D * D);
  const amrex::Real nRatio = profile / profileMax_;
  const amrex::Real n = nb + (1.0 - nb) * nRatio;
  if (n <= 0.0)
    return;
  const amrex::Real dDdx = -(eps / L) * std::sin(x / L);
  const amrex::Real dDdy = std::sinh(y / L) / L;
  const amrex::Real dndx = (1.0 - nb) * nRatio * (-2.0 / D) * dDdx;
  const amrex::Real dndy = (1.0 - nb) * nRatio * (-2.0 / D) * dDdy;

  // Base Fadeev field plus the m=1 perturbation, as in set_fields.
  const amrex::Real dbx = -perturb * b0 * Lx_ * invLy_ *
                          std::cos(2.0 * dPI * x * invLx_) *
                          std::sin(dPI * y * invLy_);
  const amrex::Real dbz = perturb * b0 * 2.0 *
                          std::sin(2.0 * dPI * x * invLx_) *
                          std::cos(dPI * y * invLy_);
  const amrex::Real Bx = b0 * std::sinh(y / L) / D + dbx;
  const amrex::Real By = b0 * eps * std::sin(x / L) / D + dbz;
  const amrex::Real Bz = b0 * bg_;
  const amrex::Real B2 = Bx * Bx + By * By + Bz * Bz;
  if (B2 <= 0.0)
    return;

  // (grad(n) x B) is along z in this geometry.
  const amrex::Real gxBn = dndx * By - dndy * Bx;
  const amrex::Real uz = -(s.temperature / s.charge) * gxBn / (n * B2);
  s.wBulk += uz;
}
