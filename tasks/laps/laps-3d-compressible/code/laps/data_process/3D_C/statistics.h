#ifndef _STATISTICS_
#define _STATISTICS_

void theta_ub_zpm_perp_only(double *uu, int nt, int method, 
    int nx, int ny, int nz, int nvar, int axis);

void *pthread_theta_ub_zpm_perp_only(void *args);

#endif