#ifndef _UINTERP_H_
#define _UINTERP_H_

namespace amrex {

template <class T> class UInterp : public InterpBase {
public:
  virtual ~UInterp() = default;

  // Copied from AMReX_Interpolater.cpp
  Box CoarseBox(const Box& fine, int ratio) override {
    Box b = amrex::coarsen(fine, ratio);
    b.grow(1);
    for (int i = 0; i < AMREX_SPACEDIM; ++i) {
      if (b.length(i) < 2) {
        b.growHi(i, 1);
      }
    }
    return b;
  }

  // Copied from AMReX_Interpolater.cpp
  Box CoarseBox(const Box& fine, const IntVect& ratio) override {
    Box b = amrex::coarsen(fine, ratio);
    b.grow(1);
    for (int i = 0; i < AMREX_SPACEDIM; ++i) {
      if (b.length(i) < 2) {
        b.growHi(i, 1);
      }
    }

    return b;
  }

  virtual void interp(const BaseFab<T>& crse, int crse_comp, BaseFab<T>& fine,
                      int fine_comp, int ncomp, const Box& fine_region,
                      const IntVect& ratio, const Geometry& crse_geom,
                      const Geometry& fine_geom, Vector<BCRec> const& bcr,
                      int actual_comp, int actual_state, RunOn gpu_or_cpu) = 0;
};

template <class T> class UNodeBilinear : public UInterp<T> {
public:
  ~UNodeBilinear() override {}

  void interp(const BaseFab<T>& crse, int crse_comp, BaseFab<T>& fine,
              int fine_comp, int ncomp, const Box& fine_region,
              const IntVect& ratio, const Geometry& crse_geom,
              const Geometry& fine_geom, Vector<BCRec> const& bcr,
              int actual_comp, int actual_state, RunOn gpu_or_cpu) override {
    Array4<T const> const& crsearr = crse.const_array();
    Array4<T> const& finearr = fine.array();
    AMREX_HOST_DEVICE_PARALLEL_FOR_4D_FLAG(
        gpu_or_cpu, fine_region, ncomp, i, j, k, n, {
          node_bilinear_interp(i, j, k, n, finearr, fine_comp, crsearr,
                               crse_comp, ratio);
        });
  }

  AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE void node_bilinear_interp(
      int i, int j, int k, int n, Array4<T> const& fine, int fcomp,
      Array4<T const> const& crse, int ccomp, IntVect const& ratio) noexcept {
    if (SpaceDim == 2) {
      int ic = amrex::coarsen(i, ratio[0]);
      int jc = amrex::coarsen(j, ratio[1]);
      int ioff = i - ic * ratio[0];
      int joff = j - jc * ratio[1];
      Real rxinv = Real(1.0) / Real(ratio[0]);
      Real ryinv = Real(1.0) / Real(ratio[1]);
      if (ioff != 0 && joff != 0) {
        // Node on a X-Y face
        fine(i, j, 0, n + fcomp) =
            rxinv * ryinv *
            (crse(ic, jc, 0, n + ccomp) * (ratio[0] - ioff) *
                 (ratio[1] - joff) +
             crse(ic + 1, jc, 0, n + ccomp) * (ioff) * (ratio[1] - joff) +
             crse(ic, jc + 1, 0, n + ccomp) * (ratio[0] - ioff) * (joff) +
             crse(ic + 1, jc + 1, 0, n + ccomp) * (ioff) * (joff));
      } else if (ioff != 0) {
        // Node on X line
        fine(i, j, 0, n + fcomp) =
            rxinv * ((ratio[0] - ioff) * crse(ic, jc, 0, n + ccomp) +
                     (ioff)*crse(ic + 1, jc, 0, n + ccomp));
      } else if (joff != 0) {
        // Node on Y line
        fine(i, j, 0, n + fcomp) =
            ryinv * ((ratio[1] - joff) * crse(ic, jc, 0, n + ccomp) +
                     (joff)*crse(ic, jc + 1, 0, n + ccomp));
      } else {
        // Node coincident with coarse node
        fine(i, j, 0, n + fcomp) = crse(ic, jc, 0, n + ccomp);
      }
    } else if (SpaceDim == 3) {

      int ic = amrex::coarsen(i, ratio[0]);
      int jc = amrex::coarsen(j, ratio[1]);
      int kc = amrex::coarsen(k, ratio[2]);
      int ioff = i - ic * ratio[0];
      int joff = j - jc * ratio[1];
      int koff = k - kc * ratio[2];
      Real rxinv = Real(1.0) / Real(ratio[0]);
      Real ryinv = Real(1.0) / Real(ratio[1]);
      Real rzinv = Real(1.0) / Real(ratio[2]);
      if (ioff != 0 && joff != 0 && koff != 0) {
        // Fine node at center of cell
        fine(i, j, k, n + fcomp) =
            rxinv * ryinv * rzinv *
            (crse(ic, jc, kc, n + ccomp) * (ratio[0] - ioff) *
                 (ratio[1] - joff) * (ratio[2] - koff) +
             crse(ic + 1, jc, kc, n + ccomp) * (ioff) * (ratio[1] - joff) *
                 (ratio[2] - koff) +
             crse(ic, jc + 1, kc, n + ccomp) * (ratio[0] - ioff) * (joff) *
                 (ratio[2] - koff) +
             crse(ic + 1, jc + 1, kc, n + ccomp) * (ioff) * (joff) *
                 (ratio[2] - koff) +
             crse(ic, jc, kc + 1, n + ccomp) * (ratio[0] - ioff) *
                 (ratio[1] - joff) * (koff) +
             crse(ic + 1, jc, kc + 1, n + ccomp) * (ioff) * (ratio[1] - joff) *
                 (koff) +
             crse(ic, jc + 1, kc + 1, n + ccomp) * (ratio[0] - ioff) * (joff) *
                 (koff) +
             crse(ic + 1, jc + 1, kc + 1, n + ccomp) * (ioff) * (joff) *
                 (koff));
      } else if (joff != 0 && koff != 0) {
        // Node on a Y-Z face
        fine(i, j, k, n + fcomp) =
            ryinv * rzinv *
            (crse(ic, jc, kc, n + ccomp) * (ratio[1] - joff) *
                 (ratio[2] - koff) +
             crse(ic, jc + 1, kc, n + ccomp) * (joff) * (ratio[2] - koff) +
             crse(ic, jc, kc + 1, n + ccomp) * (ratio[1] - joff) * (koff) +
             crse(ic, jc + 1, kc + 1, n + ccomp) * (joff) * (koff));
      } else if (ioff != 0 && koff != 0) {
        // Node on a Z-X face
        fine(i, j, k, n + fcomp) =
            rxinv * rzinv *
            (crse(ic, jc, kc, n + ccomp) * (ratio[0] - ioff) *
                 (ratio[2] - koff) +
             crse(ic + 1, jc, kc, n + ccomp) * (ioff) * (ratio[2] - koff) +
             crse(ic, jc, kc + 1, n + ccomp) * (ratio[0] - ioff) * (koff) +
             crse(ic + 1, jc, kc + 1, n + ccomp) * (ioff) * (koff));
      } else if (ioff != 0 && joff != 0) {
        // Node on a X-Y face
        fine(i, j, k, n + fcomp) =
            rxinv * ryinv *
            (crse(ic, jc, kc, n + ccomp) * (ratio[0] - ioff) *
                 (ratio[1] - joff) +
             crse(ic + 1, jc, kc, n + ccomp) * (ioff) * (ratio[1] - joff) +
             crse(ic, jc + 1, kc, n + ccomp) * (ratio[0] - ioff) * (joff) +
             crse(ic + 1, jc + 1, kc, n + ccomp) * (ioff) * (joff));
      } else if (ioff != 0) {
        // Node on X line
        fine(i, j, k, n + fcomp) =
            rxinv * ((ratio[0] - ioff) * crse(ic, jc, kc, n + ccomp) +
                     (ioff)*crse(ic + 1, jc, kc, n + ccomp));
      } else if (joff != 0) {
        // Node on Y line
        fine(i, j, k, n + fcomp) =
            ryinv * ((ratio[1] - joff) * crse(ic, jc, kc, n + ccomp) +
                     (joff)*crse(ic, jc + 1, kc, n + ccomp));
      } else if (koff != 0) {
        // Node on Z line
        fine(i, j, k, n + fcomp) =
            rzinv * ((ratio[2] - koff) * crse(ic, jc, kc, n + ccomp) +
                     (koff)*crse(ic, jc, kc + 1, n + ccomp));
      } else {
        // Node coincident with coarse node
        fine(i, j, k, n + fcomp) = crse(ic, jc, kc, n + ccomp);
      }
    }
  }
};

template <class T> class UNode_FourthOrder : public UInterp<T> {
public:
  ~UNode_FourthOrder() override {}

  void interp(const BaseFab<T>& crse, int crse_comp, BaseFab<T>& fine,
              int fine_comp, int ncomp, const Box& fine_region,
              const IntVect& ratio, const Geometry& crse_geom,
              const Geometry& fine_geom, Vector<BCRec> const& bcr,
              int actual_comp, int actual_state, RunOn gpu_or_cpu) override {
    Array4<T const> const& crsearr = crse.const_array();
    Array4<T> const& finearr = fine.array();
    AMREX_HOST_DEVICE_PARALLEL_FOR_4D_FLAG(
        gpu_or_cpu, fine_region, ncomp, i, j, k, n, {
          node_fourth_order_interp(i, j, k, n, finearr, fine_comp, crsearr,
                                   crse_comp, ratio);
        });
  }

  AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE void node_fourth_order_interp(
      int i, int j, int k, int n, Array4<T> const& fine, int fcomp,
      Array4<T const> const& crse, int ccomp, IntVect const& ratio) noexcept {
    if (SpaceDim == 2) {
      int ic = amrex::coarsen(i, ratio[0]);
      int jc = amrex::coarsen(j, ratio[1]);
      int ioff = i - ic * ratio[0];
      int joff = j - jc * ratio[1];
      if (ioff != 0 && joff != 0) {
        // Node on a X-Y face
        fine(i, j, 0, n + fcomp) = 0.0;
        Real mult[2] = { 1.0, 1.0 };
        for (int ii = -1; ii <= 2; ++ii) {
          for (int jj = -1; jj <= 2; ++jj) {
            if (ii == -1 || ii == 2) {
              mult[0] = -1.0 / 16.0;
            } else if (ii == 0 || ii == 1) {
              mult[0] = 9.0 / 16.0;
            }
            if (jj == -1 || jj == 2) {
              mult[1] = -1.0 / 16.0;
            } else if (jj == 0 || jj == 1) {
              mult[1] = 9.0 / 16.0;
            }
            fine(i, j, 0, n + fcomp) +=
                mult[0] * mult[1] * crse(ic + ii, jc + jj, 0, n + ccomp);
          }
        }
      } else if (ioff != 0) {
        // Node on X line
        fine(i, j, 0, n + fcomp) =

            ((9.0 / 16.0) *
                 (crse(ic, jc, 0, n + ccomp) + crse(ic + 1, jc, 0, n + ccomp)) -
             (1.0 / 16.0) * (crse(ic - 1, jc, 0, n + ccomp) +
                             crse(ic + 2, jc, 0, n + ccomp)));

      } else if (joff != 0) {
        // Node on Y line

        fine(i, j, 0, n + fcomp) =

            ((9.0 / 16.0) *
                 (crse(ic, jc, 0, n + ccomp) + crse(ic, jc + 1, 0, n + ccomp)) -
             (1.0 / 16.0) * (crse(ic, jc - 1, 0, n + ccomp) +
                             crse(ic, jc + 2, 0, n + ccomp)));

      } else {
        // Node coincident with coarse node
        fine(i, j, 0, n + fcomp) = crse(ic, jc, 0, n + ccomp);
      }
    } else if (SpaceDim == 3) {

      int ic = amrex::coarsen(i, ratio[0]);
      int jc = amrex::coarsen(j, ratio[1]);
      int kc = amrex::coarsen(k, ratio[2]);
      int ioff = i - ic * ratio[0];
      int joff = j - jc * ratio[1];
      int koff = k - kc * ratio[2];

      if (ioff != 0 && joff != 0 && koff != 0) {
        // Fine node at center of cell
        fine(i, j, k, n + fcomp) = 0.0;
        Real mult[3] = { 1.0, 1.0, 1.0 };
        for (int ii = -1; ii <= 2; ++ii) {
          for (int jj = -1; jj <= 2; ++jj) {
            for (int kk = -1; kk <= 2; ++kk) {
              if (ii == -1 || ii == 2) {
                mult[0] = -1.0 / 16.0;
              } else if (ii == 0 || ii == 1) {
                mult[0] = 9.0 / 16.0;
              }
              if (jj == -1 || jj == 2) {
                mult[1] = -1.0 / 16.0;
              } else if (jj == 0 || jj == 1) {
                mult[1] = 9.0 / 16.0;
              }
              if (kk == -1 || kk == 2) {
                mult[2] = -1.0 / 16.0;
              } else if (kk == 0 || kk == 1) {
                mult[2] = 9.0 / 16.0;
              }
              fine(i, j, k, n + fcomp) +=
                  mult[0] * mult[1] * mult[2] *
                  crse(ic + ii, jc + jj, kc + kk, n + ccomp);
            }
          }
        }
      }

      else if (joff != 0 && koff != 0) {
        // Node on a Y-Z face
        fine(i, j, k, n + fcomp) = 0.0;
        Real mult[2] = { 1.0, 1.0 };
        for (int ii = -1; ii <= 2; ++ii) {
          for (int jj = -1; jj <= 2; ++jj) {
            if (ii == -1 || ii == 2) {
              mult[0] = -1.0 / 16.0;
            } else if (ii == 0 || ii == 1) {
              mult[0] = 9.0 / 16.0;
            }
            if (jj == -1 || jj == 2) {
              mult[1] = -1.0 / 16.0;
            } else if (jj == 0 || jj == 1) {
              mult[1] = 9.0 / 16.0;
            }
            fine(i, j, k, n + fcomp) +=
                mult[0] * mult[1] * crse(ic, jc + jj, kc + ii, n + ccomp);
          }
        }
      } else if (ioff != 0 && koff != 0) {
        // Node on a Z-X face
        fine(i, j, k, n + fcomp) = 0.0;
        Real mult[2] = { 1.0, 1.0 };
        for (int ii = -1; ii <= 2; ++ii) {
          for (int jj = -1; jj <= 2; ++jj) {
            if (ii == -1 || ii == 2) {
              mult[0] = -1.0 / 16.0;
            } else if (ii == 0 || ii == 1) {
              mult[0] = 9.0 / 16.0;
            }
            if (jj == -1 || jj == 2) {
              mult[1] = -1.0 / 16.0;
            } else if (jj == 0 || jj == 1) {
              mult[1] = 9.0 / 16.0;
            }
            fine(i, j, k, n + fcomp) +=
                mult[0] * mult[1] * crse(ic + ii, jc, kc + jj, n + ccomp);
          }
        }
      } else if (ioff != 0 && joff != 0) {
        // Node on a X-Y face
        fine(i, j, k, n + fcomp) = 0.0;
        Real mult[2] = { 1.0, 1.0 };
        for (int ii = -1; ii <= 2; ++ii) {
          for (int jj = -1; jj <= 2; ++jj) {
            if (ii == -1 || ii == 2) {
              mult[0] = -1.0 / 16.0;
            } else if (ii == 0 || ii == 1) {
              mult[0] = 9.0 / 16.0;
            }
            if (jj == -1 || jj == 2) {
              mult[1] = -1.0 / 16.0;
            } else if (jj == 0 || jj == 1) {
              mult[1] = 9.0 / 16.0;
            }
            fine(i, j, k, n + fcomp) +=
                mult[0] * mult[1] * crse(ic + ii, jc + jj, kc, n + ccomp);
          }
        }
      } else if (ioff != 0) {
        // Node on X line
        fine(i, j, k, n + fcomp) =

            ((9.0 / 16.0) * (crse(ic, jc, kc, n + ccomp) +
                             crse(ic + 1, jc, kc, n + ccomp)) -
             (1.0 / 16.0) * (crse(ic - 1, jc, kc, n + ccomp) +
                             crse(ic + 2, jc, kc, n + ccomp)));
      } else if (joff != 0) {
        // Node on Y line
        fine(i, j, k, n + fcomp) =

            ((9.0 / 16.0) * (crse(ic, jc, kc, n + ccomp) +
                             crse(ic, jc + 1, kc, n + ccomp)) -
             (1.0 / 16.0) * (crse(ic, jc - 1, kc, n + ccomp) +
                             crse(ic, jc + 2, kc, n + ccomp)));
      } else if (koff != 0) {
        // Node on Z line
        fine(i, j, k, n + fcomp) =

            ((9.0 / 16.0) * (crse(ic, jc, kc, n + ccomp) +
                             crse(ic, jc, kc + 1, n + ccomp)) -
             (1.0 / 16.0) * (crse(ic, jc, kc - 1, n + ccomp) +
                             crse(ic, jc, kc + 2, n + ccomp)));
      } else {
        // Node coincident with coarse node
        fine(i, j, k, n + fcomp) = crse(ic, jc, kc, n + ccomp);
      }
    }
  }
};
} // namespace amrex

#endif
