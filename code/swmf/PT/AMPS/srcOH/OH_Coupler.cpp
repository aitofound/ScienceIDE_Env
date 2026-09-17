/*
 * OH_Coupler.cpp
 *
 *  Created on: Mar 20, 2015
 *      Author: vtenishe
 */
//$Id$
//Coupler functions for OH

#include "OH.h"
double OH::Coupling::TimeAfterCoupling[PIC::nTotalSpecies] = {0.0};

void OH::Coupling::Send(char *NameVar, int *nVarIn, int *nDimIn, int *nPoint, double *Xyz_DI, double *Data_VI) {
  int i0=0,i1=0;
  char vname[200];

  int density_offset=0;
  int momentum_offset=1;
  int energy_offset=4;
  int package_length=5;

  //save the data
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=NULL;
  PIC::Mesh::cDataCenterNode *cell;
  double weight;
  char *AssociatedDataPointer;
  int i,j,k,LocalCellNumber;

  for (int pt=0;pt<(*nPoint);pt++) {
    double x[3]={0.0,0.0,0.0}; //the location of the point; to be sure that it has all 3 components
    PIC::InterpolationRoutines::CellCentered::cStencil CenterBasedStencil;

    //find the block
    memcpy(x,Xyz_DI+pt*(*nDimIn),(unsigned int)(*nDimIn)*sizeof(double));
    if ((node=PIC::Mesh::mesh->findTreeNode(x,node))==NULL) exit(__LINE__,__FILE__,"Error: can not find the block");
    if (node->Thread!=PIC::ThisThread) exit(__LINE__,__FILE__,"Error: the point data is located on another processor");


    PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,CenterBasedStencil);

    //find the cell
    if ((LocalCellNumber=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false))==-1) exit(__LINE__,__FILE__,"Error: cannot find the cell");

    cell=node->block->GetCenterNode(LocalCellNumber);
    AssociatedDataPointer=cell->GetAssociatedDataBufferPointer();

    int ifluid=0,fluid_data_offset=0;

    for (ifluid=0;ifluid<PIC::CPLR::SWMF::nCommunicatedIonFluids;ifluid++) {
      Data_VI[fluid_data_offset+density_offset  +pt*(*nVarIn)]=0.0;
      Data_VI[fluid_data_offset+0+momentum_offset+pt*(*nVarIn)]=0.0;
      Data_VI[fluid_data_offset+1+momentum_offset+pt*(*nVarIn)]=0.0;
      Data_VI[fluid_data_offset+2+momentum_offset+pt*(*nVarIn)]=0.0;
      Data_VI[fluid_data_offset+energy_offset   +pt*(*nVarIn)]=0.0;

      if (PIC::LastSampleLength!=0) for (int icell=0; icell<CenterBasedStencil.Length; icell++) {
        AssociatedDataPointer=CenterBasedStencil.cell[icell]->GetAssociatedDataBufferPointer();
        weight=CenterBasedStencil.Weight[icell];  

        // copy density source
        Data_VI[fluid_data_offset+density_offset+pt*(*nVarIn)]=
          weight*(*(ifluid+(double*)(AssociatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Output::ohSourceDensityOffset)))/PIC::LastSampleLength;

        // copy momentum source
        Data_VI[fluid_data_offset+0+momentum_offset+pt*(*nVarIn)]=
          weight*(*(0+3*ifluid+(double*)(AssociatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Output::ohSourceMomentumOffset)))/PIC::LastSampleLength;

        Data_VI[fluid_data_offset+1+momentum_offset+pt*(*nVarIn)]=
          weight*(*(1+3*ifluid+(double*)(AssociatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Output::ohSourceMomentumOffset)))/PIC::LastSampleLength;

        Data_VI[fluid_data_offset+2+momentum_offset+pt*(*nVarIn)]=
          weight*(*(2+3*ifluid+(double*)(AssociatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Output::ohSourceMomentumOffset)))/PIC::LastSampleLength;

        // copy energy source
        Data_VI[fluid_data_offset+energy_offset+pt*(*nVarIn)]=
          weight*(*(ifluid+(double*)(AssociatedDataPointer+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Output::ohSourceEnergyOffset)))/PIC::LastSampleLength;


        if ((isfinite(Data_VI[fluid_data_offset+density_offset+pt*(*nVarIn)])==false)||(isfinite(Data_VI[fluid_data_offset+energy_offset+pt*(*nVarIn)])==false)) exit(__LINE__,__FILE__);  

        for (int i=0;i<3;i++) if (isfinite(Data_VI[fluid_data_offset+i+momentum_offset+pt*(*nVarIn)])==false) exit(__LINE__,__FILE__);
      }


      if (isfinite(Data_VI[fluid_data_offset+density_offset  +pt*(*nVarIn)])==false) exit(__LINE__,__FILE__);
      if (isfinite(Data_VI[fluid_data_offset+0+momentum_offset+pt*(*nVarIn)])==false) exit(__LINE__,__FILE__);
      if (isfinite(Data_VI[fluid_data_offset+1+momentum_offset+pt*(*nVarIn)])==false) exit(__LINE__,__FILE__); 
      if (isfinite(Data_VI[fluid_data_offset+2+momentum_offset+pt*(*nVarIn)])==false) exit(__LINE__,__FILE__); 
      if (isfinite(Data_VI[fluid_data_offset+energy_offset   +pt*(*nVarIn)])==false) exit(__LINE__,__FILE__); 

      fluid_data_offset+=package_length;
    }
  }

  // reset time after last coupling session
  for(int spec=0; spec < PIC::nTotalSpecies; spec++)
    OH::Coupling::TimeAfterCoupling[spec] = 0.0;
}



