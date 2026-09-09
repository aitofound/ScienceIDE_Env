#ifndef _GRIDINFO_H_
#define _GRIDINFO_H_

#include <cmath>
#include <iostream>

extern "C" {
void reset_grid_info(int* gridInfo, int nx, int ny, int nz);

void set_point_status(int* gridInfo, int nx, int ny, int nz, int i, int j,
                      int k, int val);

void get_point_status(int* gridInfo, int nx, int ny, int nz, int i, int j,
                      int k, int* Val);
}

int get_point_status(int* gridInfo, int nx, int ny, int nz, int i, int j,
                     int k);

class GridInfo {
public:
  constexpr static int iPicOn_ = 1, iPicOff_ = 0;

private:
  static const int ix = 0, iy = 1, iz = 2;
  // Number of cells
  int nCell[3];
  // Number of cells per patch
  int patchSize[3] = { 1, 1, 1 };
  // Number of patches
  int nPatch[3];

  int nInt;
  int* data;
  bool isNewGrid;

public:
  GridInfo() {
    data = nullptr;
    isNewGrid = false;
  }

  ~GridInfo() {
    if (data != nullptr) {
      delete[] data;
    }
  }

  void is_grid_new(bool in) { isNewGrid = in; }

  bool is_grid_new() const { return isNewGrid; }

  int get_patch_size(int iDim) { return patchSize[iDim]; }

  void init(int nx, int ny, int nz, int nPatchIn) {
    nCell[ix] = nx;
    nCell[iy] = ny;
    nCell[iz] = nz;

    int patchTotal = 1;
    for (int i = 0; i < 3; i++) {
      patchSize[i] = nCell[i] > 1 ? nPatchIn : 1;
      nPatch[i] = nCell[i] / patchSize[i];
      patchTotal *= nPatch[i];
    }

    nInt = ceil(patchTotal / 8.0 / sizeof(int));
    data = new int[nInt];

    for (int i = 0; i < nInt; i++) {
      data[i] = 0;
    }
    isNewGrid = true;
  }

  void set_status(int* statusIn) {
    if (statusIn == nullptr) {
      turn_on_all_cells();
      return;
    }
    isNewGrid = false;
    for (int i = 0; i < nInt; i++) {
      const int tmp = statusIn[i];
      if (data[i] != tmp)
        isNewGrid = true;
      data[i] = tmp;
    }
  }

  void turn_on_all_cells() {
    int iOn = 1;
    for (int i = 0; i < nPatch[ix]; i++)
      for (int j = 0; j < nPatch[iy]; j++)
        for (int k = 0; k < nPatch[iz]; k++) {
          set_point_status(data, nPatch[ix], nPatch[iy], nPatch[iz], i, j, k,
                           iOn);
        }
    return;
  }

  int get_status(int i, int j, int k = 0) {
    // i, j, k: FLEKS cell index. NOT patch index.
    int iPatch = i / patchSize[ix];
    int jPatch = j / patchSize[iy];
    int kPatch = k / patchSize[iz];

    return get_point_status(data, nPatch[ix], nPatch[iy], nPatch[iz], iPatch,
                            jPatch, kPatch);
  }

  void print() {
    for (int i = 0; i < nPatch[ix]; i++)
      for (int j = 0; j < nPatch[iy]; j++)
        for (int k = 0; k < nPatch[iz]; k++) {
          int status = get_point_status(data, nPatch[ix], nPatch[iy],
                                        nPatch[iz], i, j, k);
          printf("patch idx i=%d,j=%d,k=%d,status=%d\n", i, j, k, status);
        }
  }
};

#endif
