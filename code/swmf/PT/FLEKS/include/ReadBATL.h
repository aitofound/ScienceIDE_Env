#ifndef _READBATL_H_
#define _READBATL_H_

extern "C" {
void wrapamr_init_mpi();
void wrapamr_clean();
void wrapamr_read_header(char*, int, int);
void wrapamr_read_file(char*, int, int, int);
void wrapamr_get_ndim(int*);
void wrapamr_get_block_size(int*, int*, int*, int*);
void wrapamr_get_nvar(int*);
void wrapamr_get_namevar(char*, int*);
void wrapamr_get_nameunit(char*, int*);
void wrapamr_get_domain(double*, double*);
void wrapamr_get_block(double*, int*, int*, int*, double*, double*, double*,
                       double*);
void wrapamr_get_data(double*, double*, int*);
void wrapamr_get_data_cell(double*, double*, double*, int*);
void wrapamr_get_data_serial(double*, double*, int*);
void wrapamr_get_array_serial(int*, double*, double*);
}

#endif
