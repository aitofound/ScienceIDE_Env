#include <AMReX_Utility.H>

#include "TestParticles.h"

using namespace amrex;

namespace {

ParticlesInfo make_test_particles_info() {
  ParticlesInfo info;
  info.nPartPerCell = IntVect(AMREX_D_DECL(2, 2, 2));
  return info;
}

} // namespace

TestParticles::TestParticles(Grid* gridIn, FluidInterface* const fluidIn,
                             TimeCtr* const tcIn, const int speciesID,
                             const Real charge, const Real mass, int id)
    : Particles(gridIn, fluidIn, tcIn, speciesID, charge, mass,
                make_test_particles_info(), PartMode::PIC) {
  gridID = id;

  gridName = std::string("FLEKS") + std::to_string(gridID);
  printPrefix = gridName + ": ";

  outputDir = component + "/plots/test_particles";

  nInitPart = 0;
}

//==========================================================
void TestParticles::move_and_save_particles(const MultiFab& nodeEMF,
                                            const MultiFab& nodeBMF, Real dt,
                                            Real dtNext, Real tNowSI,
                                            bool doSave) {
  if (is_neutral()) {
    move_and_save_neutrals(dt, tNowSI, doSave);
  } else {
    move_and_save_charged_particles(nodeEMF, nodeBMF, dt, dtNext, tNowSI,
                                    doSave);
  }
}

//==========================================================
void TestParticles::move_and_save_particles_cell_centered(
    const MultiFab& centerEMF, const MultiFab& centerBMF, Real dt, Real dtNext,
    Real tNowSI, bool doSave) {
  if (is_neutral()) {
    move_and_save_neutrals(dt, tNowSI, doSave);
  } else {
    move_and_save_charged_particles_cell_centered(centerEMF, centerBMF, dt,
                                                  dtNext, tNowSI, doSave);
  }
}

//==========================================================
void TestParticles::move_and_save_charged_particles(const MultiFab& nodeEMF,
                                                    const MultiFab& nodeBMF,
                                                    Real dt, Real dtNext,
                                                    Real tNowSI, bool doSave) {
  timing_func("TestParticles::move_charged_particles");

  const Real dtLoc = 0.5 * (dt + dtNext);

  const Real qdto2mc = charge / mass * 0.5 * dt;

  const int iLev = 0;
  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    const Array4<Real const>& nodeEArr = nodeEMF[pti].array();
    const Array4<Real const>& nodeBArr = nodeBMF[pti].array();

    auto& particles = pti.GetArrayOfStructs();

    const Box& bx = cell_status(iLev)[pti].box();
    const Array4<int const>& status = cell_status(iLev)[pti].array();

    const IntVect lowCorner = bx.smallEnd();
    const IntVect highCorner = bx.bigEnd();

    const Dim3 lo = init_dim3(0);
    const Dim3 hi = init_dim3(1);

    for (auto& p : particles) {
      if (p.idata(iRecordCount_) >= nPTRecord) {
        Abort("Error: there is not enough allocated memory to store the "
              "particle record!!");
      }

      Real up = p.rdata(iup_);
      Real vp = p.rdata(ivp_);
      Real wp = p.rdata(iwp_);
      const Real xp = p.pos(ix_);
      const Real yp = p.pos(iy_);
      const Real zp = nDim > 2 ? p.pos(iz_) : 0;

      //-----calculate interpolation coef begin-----------
      IntVect loIdx;
      RealVect dShift;
      find_node_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                      loIdx, dShift);

      Real coef[2][2][2];
      linear_interpolation_coef(dShift, coef);
      //-----calculate interpolation coef end-------------

      Real bp[3] = { 0, 0, 0 };
      Real ep[3] = { 0, 0, 0 };
      for (int k = lo.z; k <= hi.z; ++k)
        for (int j = lo.y; j <= hi.y; ++j)
          for (int i = lo.x; i <= hi.x; ++i) {
            IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + i, loIdx[iy_] + j,
                                         loIdx[iz_] + k) };
            for (int iDim = 0; iDim < nDim3; iDim++) {
              bp[iDim] += nodeBArr(ijk, iDim) * coef[i][j][k];
              ep[iDim] += nodeEArr(ijk, iDim) * coef[i][j][k];
            }
          }

      Real gamma = 1;
      Real invGamma = 1. / gamma;

      if (isRelativistic) {
        // Convert: vel -> gamma*vel
        const Real v2 = up * up + vp * vp + wp * wp;
        if (v2 > 1) {
          Abort("Error: particle speed is fast than light!");
        }
        invGamma = sqrt(1 - v2);
        gamma = 1 / invGamma;
        up *= gamma;
        vp *= gamma;
        wp *= gamma;
      }

      // Half step acceleration
      const Real ut = up + qdto2mc * ep[ix_];
      const Real vt = vp + qdto2mc * ep[iy_];
      const Real wt = wp + qdto2mc * ep[iz_];

      if (isRelativistic) {
        const Real p2 = ut * ut + vt * vt + wt * wt;
        gamma = sqrt(1 + p2);
        invGamma = 1. / gamma;
      }

      const Real omx = qdto2mc * bp[ix_] * invGamma;
      const Real omy = qdto2mc * bp[iy_] * invGamma;
      const Real omz = qdto2mc * bp[iz_] * invGamma;

      const Real denom = 1.0 / (1.0 + omx * omx + omy * omy + omz * omz);
      const Real udotOm = ut * omx + vt * omy + wt * omz;
      // Solve the velocity equation
      const Real uavg = (ut + (vt * omz - wt * omy + udotOm * omx)) * denom;
      const Real vavg = (vt + (wt * omx - ut * omz + udotOm * omy)) * denom;
      const Real wavg = (wt + (ut * omy - vt * omx + udotOm * omz)) * denom;

      Real unp1 = 2.0 * uavg - up;
      Real vnp1 = 2.0 * vavg - vp;
      Real wnp1 = 2.0 * wavg - wp;

      if (isRelativistic) {
        // Convert: gamma*vel -> vel
        const Real p2 = unp1 * unp1 + vnp1 * vnp1 + wnp1 * wnp1;
        gamma = sqrt(1 + p2);
        invGamma = 1. / gamma;

        unp1 *= invGamma;
        vnp1 *= invGamma;
        wnp1 *= invGamma;
      }

      p.rdata(iup_) = unp1;
      p.rdata(ivp_) = vnp1;
      p.rdata(iwp_) = wnp1;

      p.pos(ix_) = xp + unp1 * dtLoc;
      p.pos(iy_) = yp + vnp1 * dtLoc;
      if (nDim > 2)
        p.pos(iz_) = zp + wnp1 * dtLoc;

      if (doSave) {
        const int i0 = record_var_index(p.idata(iRecordCount_));
        p.rdata(i0 + iTPt_) = tNowSI;
        p.rdata(i0 + iTPu_) = unp1;
        p.rdata(i0 + iTPv_) = vnp1;
        p.rdata(i0 + iTPw_) = wnp1;
        p.rdata(i0 + iTPx_) = xp + unp1 * 0.5 * dt;
        p.rdata(i0 + iTPy_) = yp + vnp1 * 0.5 * dt;
        p.rdata(i0 + iTPz_) = zp + wnp1 * 0.5 * dt;

        if (ptRecordSize > iTPBx_) {
          p.rdata(i0 + iTPBx_) = bp[ix_];
          p.rdata(i0 + iTPBy_) = bp[iy_];
          p.rdata(i0 + iTPBz_) = bp[iz_];
        }

        if (ptRecordSize > iTPEx_) {
          p.rdata(i0 + iTPEx_) = ep[ix_];
          p.rdata(i0 + iTPEy_) = ep[iy_];
          p.rdata(i0 + iTPEz_) = ep[iz_];
        }

        if (ptRecordSize > iTPdBxdx_) {
          Real gradB[3][3] = { { 0.0 } };
          // The gradient calculation is based on the derivative of the
          // trilinear interpolation shape functions. B(x,y,z) = sum_{i,j,k}
          // B_{i,j,k} * W_i(x) * W_j(y) * W_k(z) dB/dx = sum_{i,j,k} B_{i,j,k}
          // * (dW_i(x)/dx) * W_j(y) * W_k(z) dW_0/dx = -1/dx, dW_1/dx = 1/dx
          // W_0(x) = 1-dShift.x, W_1(x) = dShift.x
          const Real* invDx = Geom(iLev).InvCellSize();

          // B at 8 nodes
          Real b[3][2][2][2];
          for (int k = 0; k < 2; ++k)
            for (int j = 0; j < 2; ++j)
              for (int i = 0; i < 2; ++i) {
                IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + i, loIdx[iy_] + j,
                                             loIdx[iz_] + k) };
                for (int iDim = 0; iDim < nDim3; iDim++) {
                  b[iDim][i][j][k] = nodeBArr(ijk, iDim);
                }
              }

          Real sx = dShift[ix_];
          Real sy = dShift[iy_];
          Real sz = dShift[iz_];

          // dB/dx
          for (int iDim = 0; iDim < nDim3; iDim++) {
            gradB[iDim][ix_] =
                (((b[iDim][1][0][0] - b[iDim][0][0][0]) * (1 - sy) +
                  (b[iDim][1][1][0] - b[iDim][0][1][0]) * sy) *
                     (1 - sz) +
                 ((b[iDim][1][0][1] - b[iDim][0][0][1]) * (1 - sy) +
                  (b[iDim][1][1][1] - b[iDim][0][1][1]) * sy) *
                     sz) *
                invDx[ix_];
          }
          // dB/dy
          for (int iDim = 0; iDim < nDim3; iDim++) {
            gradB[iDim][iy_] =
                (((b[iDim][0][1][0] - b[iDim][0][0][0]) * (1 - sx) +
                  (b[iDim][1][1][0] - b[iDim][1][0][0]) * sx) *
                     (1 - sz) +
                 ((b[iDim][0][1][1] - b[iDim][0][0][1]) * (1 - sx) +
                  (b[iDim][1][1][1] - b[iDim][1][0][1]) * sx) *
                     sz) *
                invDx[iy_];
          }
#if (AMREX_SPACEDIM == 3)
          // dB/dz
          for (int iDim = 0; iDim < nDim3; iDim++) {
            gradB[iDim][iz_] =
                (((b[iDim][0][0][1] - b[iDim][0][0][0]) * (1 - sx) +
                  (b[iDim][1][0][1] - b[iDim][1][0][0]) * sx) *
                     (1 - sy) +
                 ((b[iDim][0][1][1] - b[iDim][0][1][0]) * (1 - sx) +
                  (b[iDim][1][1][1] - b[iDim][1][1][0]) * sx) *
                     sy) *
                invDx[iz_];
          }
#endif
          p.rdata(i0 + iTPdBxdx_) = gradB[0][0];
          p.rdata(i0 + iTPdBxdy_) = gradB[0][1];
          p.rdata(i0 + iTPdBxdz_) = gradB[0][2];
          p.rdata(i0 + iTPdBydx_) = gradB[1][0];
          p.rdata(i0 + iTPdBydy_) = gradB[1][1];
          p.rdata(i0 + iTPdBydz_) = gradB[1][2];
          p.rdata(i0 + iTPdBzdx_) = gradB[2][0];
          p.rdata(i0 + iTPdBzdy_) = gradB[2][1];
          p.rdata(i0 + iTPdBzdz_) = gradB[2][2];
        }

        p.idata(iRecordCount_)++;
      }
      // Mark for deletion
      if (is_outside_active_region(p, status, lowCorner, highCorner, iLev)) {
        p.id() = -1;
      }
    } // for p
  } // for pti

  redistribute_particles();
}

//==========================================================
// Cell-centred gather (hybrid); same Boris push as the node-centred version.
void TestParticles::move_and_save_charged_particles_cell_centered(
    const MultiFab& centerEMF, const MultiFab& centerBMF, Real dt, Real dtNext,
    Real tNowSI, bool doSave) {
  timing_func("TestParticles::move_charged_particles_cell_centered");

  const Real dtLoc = 0.5 * (dt + dtNext);

  const Real qdto2mc = charge / mass * 0.5 * dt;

  const int iLev = 0;
  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    const Array4<Real const>& centerEArr = centerEMF[pti].array();
    const Array4<Real const>& centerBArr = centerBMF[pti].array();

    auto& particles = pti.GetArrayOfStructs();

    const Box& bx = cell_status(iLev)[pti].box();
    const Array4<int const>& status = cell_status(iLev)[pti].array();

    const IntVect lowCorner = bx.smallEnd();
    const IntVect highCorner = bx.bigEnd();

    const Dim3 lo = init_dim3(0);
    const Dim3 hi = init_dim3(1);

    for (auto& p : particles) {
      if (p.idata(iRecordCount_) >= nPTRecord) {
        Abort("Error: there is not enough allocated memory to store the "
              "particle record!!");
      }

      Real up = p.rdata(iup_);
      Real vp = p.rdata(ivp_);
      Real wp = p.rdata(iwp_);
      const Real xp = p.pos(ix_);
      const Real yp = p.pos(iy_);
      const Real zp = nDim > 2 ? p.pos(iz_) : 0;

      //-----calculate interpolation coef begin-----------
      IntVect loIdx;
      RealVect dShift;
      find_cell_index(p.pos(), Geom(iLev).ProbLo(), Geom(iLev).InvCellSize(),
                      loIdx, dShift);

      // Linear cell-centred gather; offset-2 coef is unused (zeroed).
      Real coef[3][3][3];
      Real coefLin[2][2][2];
      linear_interpolation_coef(dShift, coefLin);
      for (int k = 0; k <= 2; ++k)
        for (int j = 0; j <= 2; ++j)
          for (int i = 0; i <= 2; ++i)
            coef[i][j][k] =
                (i <= 1 && j <= 1 && k <= 1) ? coefLin[i][j][k] : 0.0;
      //-----calculate interpolation coef end-------------

      Real bp[3] = { 0, 0, 0 };
      Real ep[3] = { 0, 0, 0 };
      for (int k = lo.z; k <= hi.z; ++k)
        for (int j = lo.y; j <= hi.y; ++j)
          for (int i = lo.x; i <= hi.x; ++i) {
            IntVect ijk = { AMREX_D_DECL(loIdx[ix_] + i, loIdx[iy_] + j,
                                         loIdx[iz_] + k) };
            const Real& c0 = coef[i - lo.x][j - lo.y][k - lo.z];
            for (int iDim = 0; iDim < nDim3; iDim++) {
              bp[iDim] += centerBArr(ijk, iDim) * c0;
              ep[iDim] += centerEArr(ijk, iDim) * c0;
            }
          }

      Real gamma = 1;
      Real invGamma = 1. / gamma;

      if (isRelativistic) {
        // Convert: vel -> gamma*vel
        const Real v2 = up * up + vp * vp + wp * wp;
        if (v2 > 1) {
          Abort("Error: particle speed is fast than light!");
        }
        invGamma = sqrt(1 - v2);
        gamma = 1 / invGamma;
        up *= gamma;
        vp *= gamma;
        wp *= gamma;
      }

      // Half step acceleration
      const Real ut = up + qdto2mc * ep[ix_];
      const Real vt = vp + qdto2mc * ep[iy_];
      const Real wt = wp + qdto2mc * ep[iz_];

      if (isRelativistic) {
        const Real p2 = ut * ut + vt * vt + wt * wt;
        gamma = sqrt(1 + p2);
        invGamma = 1. / gamma;
      }

      const Real omx = qdto2mc * bp[ix_] * invGamma;
      const Real omy = qdto2mc * bp[iy_] * invGamma;
      const Real omz = qdto2mc * bp[iz_] * invGamma;

      const Real denom = 1.0 / (1.0 + omx * omx + omy * omy + omz * omz);
      const Real udotOm = ut * omx + vt * omy + wt * omz;
      // Solve the velocity equation
      const Real uavg = (ut + (vt * omz - wt * omy + udotOm * omx)) * denom;
      const Real vavg = (vt + (wt * omx - ut * omz + udotOm * omy)) * denom;
      const Real wavg = (wt + (ut * omy - vt * omx + udotOm * omz)) * denom;

      Real unp1 = 2.0 * uavg - up;
      Real vnp1 = 2.0 * vavg - vp;
      Real wnp1 = 2.0 * wavg - wp;

      if (isRelativistic) {
        // Convert: gamma*vel -> vel
        const Real p2 = unp1 * unp1 + vnp1 * vnp1 + wnp1 * wnp1;
        gamma = sqrt(1 + p2);
        invGamma = 1. / gamma;

        unp1 *= invGamma;
        vnp1 *= invGamma;
        wnp1 *= invGamma;
      }

      p.rdata(iup_) = unp1;
      p.rdata(ivp_) = vnp1;
      p.rdata(iwp_) = wnp1;

      p.pos(ix_) = xp + unp1 * dtLoc;
      p.pos(iy_) = yp + vnp1 * dtLoc;
      if (nDim > 2)
        p.pos(iz_) = zp + wnp1 * dtLoc;

      if (doSave) {
        const int i0 = record_var_index(p.idata(iRecordCount_));
        p.rdata(i0 + iTPt_) = tNowSI;
        p.rdata(i0 + iTPu_) = unp1;
        p.rdata(i0 + iTPv_) = vnp1;
        p.rdata(i0 + iTPw_) = wnp1;
        p.rdata(i0 + iTPx_) = xp + unp1 * 0.5 * dt;
        p.rdata(i0 + iTPy_) = yp + vnp1 * 0.5 * dt;
        p.rdata(i0 + iTPz_) = zp + wnp1 * 0.5 * dt;

        if (ptRecordSize > iTPBx_) {
          p.rdata(i0 + iTPBx_) = bp[ix_];
          p.rdata(i0 + iTPBy_) = bp[iy_];
          p.rdata(i0 + iTPBz_) = bp[iz_];
        }

        if (ptRecordSize > iTPEx_) {
          p.rdata(i0 + iTPEx_) = ep[ix_];
          p.rdata(i0 + iTPEy_) = ep[iy_];
          p.rdata(i0 + iTPEz_) = ep[iz_];
        }

        if (ptRecordSize > iTPdBxdx_) {
          // dB/dx gradient is unavailable for the cell-centred gather (zeroed).
          p.rdata(i0 + iTPdBxdx_) = 0.0;
          p.rdata(i0 + iTPdBxdy_) = 0.0;
          p.rdata(i0 + iTPdBxdz_) = 0.0;
          p.rdata(i0 + iTPdBydx_) = 0.0;
          p.rdata(i0 + iTPdBydy_) = 0.0;
          p.rdata(i0 + iTPdBydz_) = 0.0;
          p.rdata(i0 + iTPdBzdx_) = 0.0;
          p.rdata(i0 + iTPdBzdy_) = 0.0;
          p.rdata(i0 + iTPdBzdz_) = 0.0;
        }

        p.idata(iRecordCount_)++;
      }
      // Mark for deletion
      if (is_outside_active_region(p, status, lowCorner, highCorner, iLev)) {
        p.id() = -1;
      }
    } // for p
  } // for pti

  redistribute_particles();
}

//==========================================================
void TestParticles::move_and_save_neutrals(Real dt, Real tNowSI, bool doSave) {
  timing_func("TestParticles::move_neutrals");

  const int iLev = 0;
  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    auto& particles = pti.GetArrayOfStructs();

    const Box& bx = cell_status(iLev)[pti].box();
    const Array4<int const>& status = cell_status(iLev)[pti].array();

    const IntVect lowCorner = bx.smallEnd();
    const IntVect highCorner = bx.bigEnd();

    for (auto& p : particles) {
      if (p.idata(iRecordCount_) >= nPTRecord) {
        Abort("Error: there is not enough allocated memory to store the "
              "particle record!!");
      }

      Real up = p.rdata(iup_);
      Real vp = p.rdata(ivp_);
      Real wp = p.rdata(iwp_);
      const Real xp = p.pos(ix_);
      const Real yp = p.pos(iy_);
      const Real zp = nDim > 2 ? p.pos(iz_) : 0;

      p.pos(ix_) = xp + up * dt;
      p.pos(iy_) = yp + vp * dt;
      if (nDim > 2)
        p.pos(iz_) = zp + wp * dt;

      if (doSave) {
        const int i0 = record_var_index(p.idata(iRecordCount_));
        p.rdata(i0 + iTPt_) = tNowSI;
        p.rdata(i0 + iTPu_) = p.rdata(iup_);
        p.rdata(i0 + iTPv_) = p.rdata(ivp_);
        p.rdata(i0 + iTPw_) = p.rdata(iwp_);
        p.rdata(i0 + iTPx_) = p.pos(ix_);
        p.rdata(i0 + iTPy_) = p.pos(iy_);
        p.rdata(i0 + iTPz_) = p.pos(iz_);

        p.idata(iRecordCount_)++;
      }

      // Mark for deletion
      if (is_outside_active_region(p, status, lowCorner, highCorner, iLev)) {
        p.id() = -1;
      }
    }
  }

  redistribute_particles();
}

//======================================================================
void TestParticles::read_test_particle_list(
    const Vector<std::string>& listFiles) {
  std::string funcName = "TP::read_test_particle_list";
  timing_func(funcName);

  std::string listName;
  for (int i = 0; i < listFiles.size(); ++i) {
    // Assume the particle list file's name is *N.dat, where N is
    // the species ID (Usually 0 or 1).
    if (listFiles[i].find(std::to_string(speciesID) + ".dat") !=
        std::string::npos) {
      listName = listFiles[i];
      break;
    }
  }

  if (listName.empty())
    return;

  vIDs.clear();

  // Read particle IDs
  std::ifstream source;
  source.open(listName, std::ifstream::in);
  PID id;
  while (source >> id.cpu >> id.id) {
    id.flag = true;
    vIDs.push_back(id);
  }

  std::sort(vIDs.begin(), vIDs.end());
}

//======================================================================
/**
 * @brief Trace the PIC particles that are in the list (listFiles).
 *
 * This function adds test particles from the given PIC particles object (`pts`)
 * to the current test particles object. It traces the particles that are in the
 * list of particle IDs (`vIDs`).
 *
 * @param pts Pointer to the PicParticles object containing the particles to be
 * traced.
 */
void TestParticles::add_test_particles_from_pic(PicParticles* pts) {
  std::string funcName = "TP::add_test_particles_from_pic";
  timing_func(funcName);

  if (vIDs.size() == 0)
    return;

  const int iLev = 0;

  const auto& lOther = pts->GetParticles(iLev);

  // Loop through the PIC particles.
  for (MFIter mfi = pts->MakeMFIter(iLev); mfi.isValid(); ++mfi) {
    auto index = std::make_pair(mfi.index(), mfi.LocalTileIndex());

    if (lOther.find(index) == lOther.end())
      continue;

    const auto& tileOther = lOther.at(index);

    if (tileOther.numParticles() == 0)
      continue;

    auto& particles = GetParticles(iLev)[index];

    const auto& aosOther = tileOther.GetArrayOfStructs();

    for (auto pOther : aosOther) {
      PID id;
      id.cpu = pOther.cpu();
      id.id = pOther.id();

      auto it = std::lower_bound(vIDs.begin(), vIDs.end(), id);

      // This particle is on the test particle list. Generate a new test
      // particle.
      if (*it == id) {
        auto p = make_particle();
        p.cpu() = pOther.cpu();
        p.id() = pOther.id();
        p.pos(ix_) = pOther.pos(ix_);
        p.pos(iy_) = pOther.pos(iy_);
        p.pos(iz_) = pOther.pos(iz_);
        p.rdata(iup_) = pOther.rdata(iup_);
        p.rdata(ivp_) = pOther.rdata(ivp_);
        p.rdata(iwp_) = pOther.rdata(iwp_);
        p.rdata(iqp_) = pOther.rdata(iqp_);

        p.idata(iRecordCount_) = 0;

        particles.push_back(p);

        // Remove this id from the test particle list later.
        (*it).flag = false;
      }
    }
  }

  { // If a test particle has been generated on one proc, all procs should
    // remove this particle from the test particle list (vIDs). Use a bit array
    // to do mpi_allreduce to improve performance.
    BitArray ba(vIDs.size());
    for (unsigned int i = 0; i < vIDs.size(); ++i) {
      if (vIDs[i].flag) {
        ba.set(i, 1);
      }
    }

    MPI_Allreduce(MPI_IN_PLACE, ba.get(), ba.size_int(), MPI_INT, MPI_BAND,
                  ParallelDescriptor::Communicator());

    for (unsigned int i = 0; i < vIDs.size(); ++i) {
      if (ba.get(i) == 0) {
        vIDs[i].flag = false;
      }
    }
  }

  // After move, vIDs is empty.
  std::vector<PID> tmp = std::move(vIDs);

  for (auto t : tmp) {
    if (t.flag)
      vIDs.push_back(t);
  }
}

//======================================================================
/**
 * @brief Add test particles from fluid states.
 *
 * This function adds test particles from the given fluid states (`tpStates`)
 * to the current test particles object. It initializes the test particles
 * based on the specified fluid states and the current cell status.
 *
 * @param tpStates Vector of fluid states from which test particles are to be
 * added.
 */
void TestParticles::add_test_particles_from_fluid(const Vector<Vel>& tpStates) {
  std::string funcName = "TP::add_test_particles_from_fluid";
  timing_func(funcName);
  Print() << funcName << " : nInitPart = " << nInitPart
          << " : current number = " << TotalNumberOfParticles(true, false)
          << std::endl;

  const int iLev = 0;

  Vel tpVel;
  for (const auto& state : tpStates) {
    if (state.tag == speciesID) {
      tpVel = state;
    }
  }

  for (MFIter mfi = MakeMFIter(iLev, false); mfi.isValid(); ++mfi) {
    const auto& status = cell_status(iLev)[mfi].array();
    const Box& bx = mfi.validbox();
    const auto lo = lbound(bx);
    const auto hi = ubound(bx);

    for (int i = lo.x; i <= hi.x; i += nIntervalCell[ix_])
      for (int j = lo.y; j <= hi.y; j += nIntervalCell[iy_])
#if (AMREX_SPACEDIM == 3)
        for (int k = lo.z; k <= hi.z; k += nIntervalCell[iz_])
#else
        for (int k = 0; k <= 0; ++k)
#endif
        {
          if (iPartRegion == iRegionUniform_ ||
              (iPartRegion == iRegionBoundary_ &&
               bit::is_lev_edge(status(i, j, k)))) {
            add_particles_cell(iLev, mfi, IntVect{ AMREX_D_DECL(i, j, k) }, fi,
                               false, IntVect(0), tpVel);
          } else if (iPartRegion == iRegionUser_) {
            amrex::Real xyz[nDim];
            Geom(iLev).CellCenter({ AMREX_D_DECL(i, j, k) }, xyz);
            if (tpRegions.is_inside(xyz)) {
              add_particles_cell(iLev, mfi, IntVect{ AMREX_D_DECL(i, j, k) },
                                 fi, false, IntVect(0), tpVel);
            }
          }
        }

    if (iPartRegion == iRegionSideXp_) {
      for (int j = lo.y; j <= hi.y; j += nIntervalCell[iy_])
        for (int k = lo.z; k <= hi.z; k += nIntervalCell[iz_]) {
          const int i = hi.x;
          if (bit::is_lev_edge(status(i, j, k)))
            add_particles_cell(iLev, mfi, IntVect{ AMREX_D_DECL(i, j, k) }, fi,
                               false, IntVect(0), tpVel);
        }
    }
  }
}

//======================================================================
bool TestParticles::write_particles(int cycle) {
  std::string funcName = "TP::write_particles";
  timing_func(funcName);

  int nPartLoc = TotalNumberOfParticles(false, true);
  int nPartAhead = 0;

  gather_accumulate_and_scatter(nPartLoc, nPartAhead);

  unsigned long long int nByteLoc = loop_particles("count_record_size");
  unsigned long long int nByteAhead = 0;
  gather_accumulate_and_scatter(nByteLoc, nByteAhead);

  Vector<char> dataBuffer;
  dataBuffer.resize(nByteLoc);
  loop_particles("copy_record", dataBuffer.data(), dataBuffer.size());

  // cpu + id + loc;
  constexpr int listUnitSize = 2 * sizeof(int) + sizeof(unsigned long long);
  Vector<char> partList;
  partList.resize(nPartLoc * listUnitSize);
  loop_particles("get_record_loc", partList.data(), partList.size(),
                 nByteAhead);

  std::stringstream ss;
  ss << "n" << std::setfill('0') << std::setw(8) << cycle;

  UtilCreateDirectory(outputDir, 0755);
  std::string fileNamePartList = outputDir + "/" + gridName +
                                 "_particle_list_species_" +
                                 std::to_string(speciesID) + "_" + ss.str();

  std::string fileNamePartRecord = outputDir + "/" + gridName +
                                   "_particle_species_" +
                                   std::to_string(speciesID) + "_" + ss.str();

  Print() << printPrefix << "Saving test particles into " << fileNamePartRecord
          << std::endl;

  MPI_File recordFile, listFile;
  MPI_Status status;

  MPI_File_open(ParallelDescriptor::Communicator(), fileNamePartList.c_str(),
                MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &listFile);
  MPI_File_write_at_all(listFile, nPartAhead * listUnitSize, partList.data(),
                        partList.size(), MPI_CHAR, &status);
  MPI_File_close(&listFile);

  MPI_File_open(ParallelDescriptor::Communicator(), fileNamePartRecord.c_str(),
                MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &recordFile);
  MPI_File_write_at_all(recordFile, nByteAhead, dataBuffer.data(),
                        dataBuffer.size(), MPI_CHAR, &status);
  MPI_File_close(&recordFile);

  loop_particles("reset_record_counter", partList.data(), partList.size(),
                 nByteAhead);

  {
    if (ParallelDescriptor::IOProcessor()) {
      std::string headerName = outputDir + "/Header";

      std::ofstream headerFile;
      headerFile.open(headerName.c_str(), std::ios::out | std::ios::trunc);

      if (!headerFile.good())
        FileOpenFailed(headerName);

      headerFile << ptRecordSize << "\n";
    }
  }

  return true;
}

unsigned long long int TestParticles::loop_particles(
    std::string action, char* buff, unsigned long long int sizeLimit,
    unsigned long long int shift) {

  unsigned long long int nByteCount = 0;

  bool doCopyData = (action == "copy_record");
  bool doGetLoc = (action == "get_record_loc");
  bool doResetRecordCounter = (action == "reset_record_counter");

  int iPartCount = 0;
  constexpr int listUnitSize = 2 * sizeof(int) + sizeof(unsigned long long);

  const int iLev = 0;
  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    auto& particles = pti.GetArrayOfStructs();
    for (auto& p : particles) {
      int nRecord = p.idata(iRecordCount_);

      // int: cpu + id + nRecord
      // Real: weight + ptRecordSize*nRecord
      int nBytePerPart =
          3 * sizeof(int) + (1 + ptRecordSize * nRecord) * sizeof(float);

      if (doResetRecordCounter) {
        p.idata(iRecordCount_) = 0;
      }

      if (doGetLoc) {
        int i2[2];
        i2[0] = p.cpu();
        i2[1] = p.id();
        memcpy(buff + iPartCount * listUnitSize, i2, 2 * sizeof(int));

        unsigned long long tmp = nByteCount + shift;
        memcpy(buff + iPartCount * listUnitSize + 2 * sizeof(int), &tmp,
               sizeof(unsigned long long));
      }

      if (doCopyData) {
        int iCountLoc = 0, sizeLoc = 0;
        int i3[3];
        i3[0] = p.cpu();
        i3[1] = p.id();
        i3[2] = nRecord;

        // cpu + id + nRecord
        sizeLoc = 3 * sizeof(int);
        memcpy(buff + nByteCount + iCountLoc, i3, sizeLoc);
        iCountLoc += sizeLoc;

        // weight
        sizeLoc = sizeof(float);
        float weight = (float)(p.rdata(iqp_) / charge * mass * no2outM);
        memcpy(buff + nByteCount + iCountLoc, &weight, sizeLoc);
        iCountLoc += sizeLoc;

        sizeLoc = sizeof(float) * ptRecordSize;
        for (int i = 0; i < nRecord; ++i) {
          float recordData[ptRecordSize];

          const int i0 = record_var_index(i);

          // time is already in SI unit
          recordData[iTPt_] = (float)p.rdata(i0 + iTPt_);
          recordData[iTPx_] = (float)(p.rdata(i0 + iTPx_) * no2outL);
          recordData[iTPy_] = (float)(p.rdata(i0 + iTPy_) * no2outL);
          recordData[iTPz_] = (float)(p.rdata(i0 + iTPz_) * no2outL);
          recordData[iTPu_] = (float)(p.rdata(i0 + iTPu_) * no2outV);
          recordData[iTPv_] = (float)(p.rdata(i0 + iTPv_) * no2outV);
          recordData[iTPw_] = (float)(p.rdata(i0 + iTPw_) * no2outV);

          if (ptRecordSize > iTPBx_) {
            recordData[iTPBx_] = (float)(p.rdata(i0 + iTPBx_) * no2outB);
            recordData[iTPBy_] = (float)(p.rdata(i0 + iTPBy_) * no2outB);
            recordData[iTPBz_] = (float)(p.rdata(i0 + iTPBz_) * no2outB);
          }

          if (ptRecordSize > iTPEx_) {
            recordData[iTPEx_] = (float)(p.rdata(i0 + iTPEx_) * no2outE);
            recordData[iTPEy_] = (float)(p.rdata(i0 + iTPEy_) * no2outE);
            recordData[iTPEz_] = (float)(p.rdata(i0 + iTPEz_) * no2outE);
          }

          if (ptRecordSize > iTPdBxdx_) {
            // no2out for gradient is no2outB/no2outL
            Real no2outG = no2outB / no2outL;
            recordData[iTPdBxdx_] = (float)(p.rdata(i0 + iTPdBxdx_) * no2outG);
            recordData[iTPdBxdy_] = (float)(p.rdata(i0 + iTPdBxdy_) * no2outG);
            recordData[iTPdBxdz_] = (float)(p.rdata(i0 + iTPdBxdz_) * no2outG);
            recordData[iTPdBydx_] = (float)(p.rdata(i0 + iTPdBydx_) * no2outG);
            recordData[iTPdBydy_] = (float)(p.rdata(i0 + iTPdBydy_) * no2outG);
            recordData[iTPdBydz_] = (float)(p.rdata(i0 + iTPdBydz_) * no2outG);
            recordData[iTPdBzdx_] = (float)(p.rdata(i0 + iTPdBzdx_) * no2outG);
            recordData[iTPdBzdy_] = (float)(p.rdata(i0 + iTPdBzdy_) * no2outG);
            recordData[iTPdBzdz_] = (float)(p.rdata(i0 + iTPdBzdz_) * no2outG);
          }

          memcpy(buff + nByteCount + iCountLoc, &recordData, sizeLoc);
          iCountLoc += sizeLoc;
        }

        if (nByteCount + iCountLoc > sizeLimit) {
          AllPrint() << "nByteCount = " << nByteCount
                     << " iCountLoc = " << iCountLoc
                     << " sizeLimit = " << sizeLimit << std::endl;
          Abort("Error: memory may leaked!!");
        }
      }

      iPartCount++;
      nByteCount += nBytePerPart;
    }
  }
  return nByteCount;
}

void TestParticles::print_record_buffer(char* buffer,
                                        unsigned long long int nBuffer) {
  unsigned long long int count = 0;

  while (count < nBuffer) {
    AllPrint() << "-----Record-----------" << std::endl;
    AllPrint() << "Record: cpu = " << (*(int*)(buffer + count));
    count += sizeof(int);
    AllPrint() << " id = " << (*(int*)(buffer + count));
    count += sizeof(int);

    int nRecord = (*(int*)(buffer + count));
    AllPrint() << " nRecord = " << nRecord;
    count += sizeof(int);

    AllPrint() << " weight = " << (*(float*)(buffer + count)) << std::endl;
    count += sizeof(float);

    for (int i = 0; i < nRecord; ++i) {
      for (int ii = 0; ii < ptRecordSize; ++ii) {
        AllPrint() << " data" << ii << " = " << (*(float*)(buffer + count));
        count += sizeof(float);
      }
      AllPrint() << std::endl;
    }
    AllPrint() << "----------------" << std::endl;
  }
}

void TestParticles::reset_record_counter() {
  const int iLev = 0;
  for (PIter pti(*this, iLev); pti.isValid(); ++pti) {
    auto& particles = pti.GetArrayOfStructs();
    for (auto& p : particles) {
      p.idata(iRecordCount_) = 0;
    }
  }
}
