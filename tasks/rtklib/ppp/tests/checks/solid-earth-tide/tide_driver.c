#include <stdio.h>
#include <stdlib.h>
#include "rtklib.h"

/* Inputs and solid-Earth-only mode from test/utest/t_ppp.c:utest3.
 * A thin public-API driver exposes displacement, not the assertion residual.
 */
int main(int argc, char **argv)
{
    double ep[6]={2010,6,7,1,2,3};
    double rr[3],dr[3]={0};
    int i;
    if (argc!=4) { fprintf(stderr,"usage: tide_driver X Y Z\n"); return 2; }
    for (i=0;i<3;i++) rr[i]=atof(argv[i+1]);
    tidedisp(epoch2time(ep),rr,1,NULL,NULL,dr);
    printf("%.17g %.17g %.17g\n",dr[0],dr[1],dr[2]);
    return 0;
}
