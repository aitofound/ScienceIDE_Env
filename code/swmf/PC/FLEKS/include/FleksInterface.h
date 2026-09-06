#ifndef _FLEKSINTERFACE_H_
#define _FLEKSINTERFACE_H_

#include <sstream>

#include <mpi.h>

extern "C" {
int fleks_init_mpi_(MPI_Fint *iComm, signed int *iProc, signed int *nProc);
int fleks_init_(double *timeNow);
int fleks_finalize_init_();
int fleks_run_(double *time);
int fleks_save_restart_(char *nameRestartOutDirComp, int *nChar);
int fleks_end_();
int fleks_read_param_(char *param, int *nlines, int *ncharline, int *iProc);
int fleks_from_gm_init_(int *iParam, double *paramreal, char *NameVar);
int fleks_get_ngridpoints_(int *nPoint);
int fleks_get_grid_(double *Pos_DI, int *n);
int fleks_set_state_var_(double *Data_VI, int *iPoint_I, int *nVar,
                         char *nameVar, int *nChar);
int fleks_cal_dt_(double *dt);
int fleks_find_points_(int *nPoint, double *Xyz_DI, int *iProc_I);
int fleks_get_state_var_(int *nDim, int *nPoint, double *Xyz_I, double *data_I,
                         int *nVar);
int fleks_get_for_oh_(int *nDim, int *nPoint, double *Xyz_I, double *data_I,
                      int *nVar);
int fleks_get_grid_info_(int *fluidDim, int *iGrid, int *iDecomp);
int fleks_set_grid_info_(int *nInt, int *accumulatedSize, int *status);
int fleks_turn_on_all_cells_();
}
#endif
