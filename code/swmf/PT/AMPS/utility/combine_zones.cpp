
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <iostream>
#include <fstream>


#include "AMPS/src/general/ifileopr.h"

int main () {
  char fname[100]="pic.O.s=0.out=8.dat";
  char str[1000],str1[1000];

  CiFileOperations ifile;
  ifile.openfile(fname); 

  char fname_variables[400],fname_data[400],fname_connectivity[400];
  FILE *fVariables,*fData,*fConnectivity;

  sprintf(fname_variables,"%s.variables",fname);
  fVariables=fopen(fname_variables,"w");

  sprintf(fname_data,"%s.data",fname);
  fData=fopen(fname_data,"w");

  sprintf(fname_connectivity,"%s.connectivity",fname);  
  fConnectivity=fopen(fname_connectivity,"w");

  int _variables=0;
  int _zone=1;
  int _data=2;
  int _connectivity=3;


  int code=_variables;  

  int npoints_zone,ncells_zone,npoints_offset=0,ntotal_cells=0,ntotal_points=0;
  int npoints_output=0,ncells_output=0;
  char *endptr;

  while (ifile.eof()==false) {
    ifile.GetInputStr(str,sizeof(str));

    if (code==_variables) {
      fprintf(fVariables,"%s\n",str);
      fclose(fVariables);

      code=_zone;
    }
    else if (code==_zone) {
      ifile.CutInputStr(str1,str);
      ifile.CutInputStr(str1,str);
      ifile.CutInputStr(str1,str);

      npoints_zone=strtol(str1,&endptr,10);
      ntotal_points+=npoints_zone;

      ifile.CutInputStr(str1,str);
      ifile.CutInputStr(str1,str);  

      ncells_zone=strtol(str1,&endptr,10);
      ntotal_cells+=ncells_zone;

      code=_data;
    }
    else if (code==_data) {
      int iline;
      
      for (iline=0;iline<npoints_zone;iline++) { 
        npoints_output++;
        fprintf(fData,"%s\n",str);       
        
        if (iline!=npoints_zone-1) ifile.GetInputStr(str,sizeof(str));
      }
      
      
      code=_connectivity;
    }
    else if (code==_connectivity) {
      int c,i,iz;
      
      
      for (iz=0;iz<ncells_zone;iz++) {
        for (i=0;i<8;i++) {
          c=npoints_offset+strtol(str1,&endptr,10);

          fprintf(fConnectivity,"%i ",c);
        }

        fprintf(fConnectivity,"\n");
        ncells_output++;

        if (iz!=ncells_zone-1) ifile.GetInputStr(str,sizeof(str));
      }

      npoints_offset+=npoints_zone;
      code=_zone;
    }
  }

  fclose(fConnectivity);
  fclose(fData);
  
  //combine all files into a single file
  std::ofstream  dst("res.dat",   std::ios::binary);
  std::ifstream  src_variables(fname_variables, std::ios::binary);
  std::ifstream  src_data(fname_data, std::ios::binary);
  std::ifstream  src_connectivity(fname_connectivity, std::ios::binary);
    
  dst<<src_variables.rdbuf();  
  dst << "ZONE N=" << ntotal_points <<", E=" << ntotal_cells << ", DATAPACKING=POINT, ZONETYPE=FEBRICK\n";
  
  
  if ((ncells_output!=ntotal_cells)||(npoints_output!=ntotal_points)) {
    printf("Oops...\n");
    exit(0);
  }
  
  dst<<src_data.rdbuf();
  dst<<src_connectivity.rdbuf();
  
  src_variables.close();
  src_data.close();
  src_connectivity.close();
  
  dst.close();
  
  remove(fname_variables);
  remove(fname_data);
  remove(fname_connectivity);

  return 1;
}  










 
