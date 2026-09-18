/*
 * main.cpp
 *
 on: Jun 21, 2012
 *      Author: fougere and vtenishe
 */


#include "pic.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <time.h>
#include <algorithm>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctime>

#include "RandNum.h"
#include "meshAMRcutcell.h"
#include "cCutBlockSet.h"
#include "meshAMRgeneric.h"

#include "../../srcInterface/LinearSystemCornerNode.h"
#include "linear_solver_wrapper_c.h"

#include "PeriodicBCTest.dfn"

#if _CUDA_MODE_ == _ON_
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#endif

//for lapenta mover

#include "pic.h"
#include "Exosphere.dfn"
#include "Exosphere.h"


int nVars=3; //number of variables in center associated data
double Background[3]={100.0,-20.0,10.0};


//#define _UNIFORM_MESH_ 1
//#define _NONUNIFORM_MESH_ 2

#ifndef _TEST_MESH_MODE_
#define _TEST_MESH_MODE_ _UNIFORM_MESH_
#endif

//double r_planet = 2.521e5;// radius of enceladus.
double r_planet = 126; //normalized
//double xmin[3]={-16.0*r_planet/4,-8.0*r_planet/4,-4.0*r_planet/4};
double xmin[3]={0,0,0};
double xmax[3]={2*16.0*r_planet/4,2*8.0*r_planet/4,2*4.0*r_planet/4};

int nCells[3] ={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};

double vel_bg[3]={8.8e-3,0,0};
double b_bg[3]={0.0,0.0, 325e-9 * 31930.3280322054};
double e_bg[3]={-vel_bg[1]*b_bg[2]+vel_bg[2]*b_bg[1], vel_bg[0]*b_bg[2]-vel_bg[2]*b_bg[0],
		-vel_bg[0]*b_bg[1]+vel_bg[1]*b_bg[0]};//-vXB


int CurrentCenterNodeOffset=-1,NextCenterNodeOffset=-1;
int CurrentCornerNodeOffset=-1,NextCornerNodeOffset=-1;

int iCase;

void GetGlobalCellIndex(int * index ,double * x, double * dx, double * xmin){
  //global cell index starts from 1 for true cells
  index[0] = round((x[0]-xmin[0])/dx[0]+0.5);
  index[1] = round((x[1]-xmin[1])/dx[1]+0.5);
  index[2] = round((x[2]-xmin[2])/dx[2]+0.5);

}

void GetGlobalCornerIndex(int * index ,double * x, double * dx, double * xmin){
  
  index[0] = round((x[0]-xmin[0])/dx[0]);
  index[1] = round((x[1]-xmin[1])/dx[1]);
  index[2] = round((x[2]-xmin[2])/dx[2]);

}



void SetParticleForCell(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,int iBlock,
                        int iCell,int jCell,int kCell,double * dx,double * xminBlock,
                        double * ParticleWeight,double CellVolume,long & nLocalInjectedParticles){

  //double cellParNumPerSp = PIC::CPLR::FLUID::npcelx[0]*PIC::CPLR::FLUID::npcely[0]*PIC::CPLR::FLUID::npcelz[0];
  int npcelx=6, npcely=7, npcelz=1;
  
  double cellParNumPerSp = npcelx* npcely* npcelz;
  int nSubCells[3]={npcelx,npcely,npcelz};
  int ind[3]={iCell,jCell,kCell};
  double xCenter[3],xCorner[3];

  int ionSpec=1, electronSpec=0;
  double ionMass = PIC::MolecularData::GetMass(ionSpec)/_AMU_;
  double electronMass = PIC::MolecularData::GetMass(electronSpec)/_AMU_;

  for (int idim=0; idim<3; idim++) {
    xCenter[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
    xCorner[idim]=xCenter[idim]-0.5*dx[idim];
  }


  // double BulkVel[PIC::nTotalSpecies][3];

  /*
  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++){
    
    BulkVel[iSp][0]= PIC::CPLR::FLUID::FluidInterface.getPICUx(iBlock,
                                                              xCenter[0],xCenter[1],xCenter[2],iSp);
    
    BulkVel[iSp][1]= PIC::CPLR::FLUID::FluidInterface.getPICUy(iBlock,
                                                              xCenter[0],xCenter[1],xCenter[2],iSp);
    
    BulkVel[iSp][2]= PIC::CPLR::FLUID::FluidInterface.getPICUz(iBlock,
                                                              xCenter[0],xCenter[1],xCenter[2],iSp);
    
    */

  /*
    if (iSp==0 && PIC::CPLR::FLUID::iCycle==0){
      ux0=BulkVel[iSp][0];
      uy0=BulkVel[iSp][1];
      uz0=BulkVel[iSp][2];
    }
    
    if (iSp==1 && PIC::CPLR::FLUID::iCycle==0){
      ux1=BulkVel[iSp][0];
      uy1=BulkVel[iSp][1];
      uz1=BulkVel[iSp][2];
    }
    
    
  } 
  */
  nLocalInjectedParticles += cellParNumPerSp*PIC::nTotalSpecies;
  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++){
    
    RandNum rndNum;
    int nxcg, nycg, nzcg, npcel, nRandom = 7;
    int index_G[3]; 
    //assuming it is uniform
    nxcg=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/dx[0];
    nycg=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/dx[1]; 
    nzcg=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/dx[2];
    
    
    GetGlobalCellIndex(index_G , xCenter, dx, PIC::Mesh::mesh->xGlobalMin);
    npcel = cellParNumPerSp;
    rndNum.set_seed((iSp + 3) * nRandom * npcel *
                            (nxcg * nycg * nzcg * PIC::CPLR::FLUID::iCycle + 
                             nycg * nzcg * index_G[0] + nzcg * index_G[1] + index_G[2]));
    
    //    double NumberDensity=fabs(PIC::CPLR::FLUID::FluidInterface.getPICRhoNum(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp));
    /*

    if (iSp==0 && iCell==1 && jCell==1 && kCell==1) {
      rho0=NumberDensity;
      p0 = PIC::CPLR::FLUID::FluidInterface.getPICP(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      ppar0 =  PIC::CPLR::FLUID::FluidInterface.getPICPpar(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      uth0 = PIC::CPLR::FLUID::FluidInterface.getPICUth(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
    } 
    
    if (iSp==1 && iCell==1 && jCell==1 && kCell==1) {
      
      rho1=NumberDensity;
      p1 = PIC::CPLR::FLUID::FluidInterface.getPICP(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      uth1 = PIC::CPLR::FLUID::FluidInterface.getPICUth(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      ppar1 =  PIC::CPLR::FLUID::FluidInterface.getPICPpar(iBlock,xCenter[0],xCenter[1],xCenter[2],iSp);
      
    }
      
    */
    //    double weightCorrection=NumberDensity*CellVolume/ParticleWeight[iSp]/cellParNumPerSp;
        
    for (int ii = 0; ii < npcelx; ii++){
      for (int jj = 0; jj < npcely; jj++){
        for (int kk = 0; kk < npcelz; kk++){
          
          double xPar[3],rndLoc[3];
          int index_sub[3]={ii,jj,kk};
          for (int idim=0; idim<3; idim++){
            rndLoc[idim]=rndNum();
            xPar[idim] = (index_sub[idim] + rndLoc[idim]) * (dx[idim]/nSubCells[idim])
              + xCorner[idim];
            //xPar[idim] = (index_sub[idim] + 0.5) * (dx[idim]/nSubCells[idim])
            //  + xCorner[idim];
         
          }

	  double rho_i= 1.102274e-03;
	  double rho_e = 2.755684e-06;
	  double p = 8.221618e-07;
	  double pe = 1.908590e-09;

          //double NumberDensity=fabs(PIC::CPLR::FLUID::FluidInterface.getPICRhoNum(iBlock,xPar[0],xPar[1],xPar[2],iSp));
	  double NumberDensity =rho_i/ionMass;
	  
          double weightCorrection=NumberDensity*CellVolume/ParticleWeight[iSp]/cellParNumPerSp;
    
          
          double ParVel_D[3];
          double uth[3];
	  double uth0;



	  if (iSp==0){
	    uth0 = sqrt(pe/rho_e);//1.3 eV       
	  }else{
	    uth0 = sqrt(p/rho_i);//35 eV
	  }

	  //printf("isp:%d, uth0:%e\n",iSp, uth0);

	  double rand[4];
	  for (int irand=0; irand<4; irand++) rand[irand]=rndNum();
	  double harvest = rand[0];
	  double prob = sqrt(-2.0 * log(1.0 - .999999999 * harvest));
	  harvest = rand[1];
	  double theta = 2.0 * Pi * harvest;
	  uth[0] = uth0 * prob * cos(theta);
	  uth[1] = uth0 * prob * sin(theta);

	  // w = Z velocity
	  harvest = rand[2];
	  prob = sqrt(-2.0 * log(1.0 - .999999999 * harvest));
	  harvest = rand[3];
	  theta = 2.0 * Pi * harvest;
	  uth[2] = uth0 * prob * cos(theta);

	  
	  /*
          if ((iSp!=0 || PIC::CPLR::FLUID::FluidInterface.get_useElectronFluid()) && PIC::CPLR::FLUID::FluidInterface.getUseAnisoP()){
            
            double rand[4];
            for (int irand=0; irand<4; irand++) rand[irand]=rndNum();
            
            PIC::CPLR::FLUID::FluidInterface.setPICAnisoUth(
                                                         iBlock,xPar[0],xPar[1],xPar[2],
                                                         uth,uth+1,uth+2,
                                                         rand[0], rand[1], rand[2], rand[3],iSp);
            
          }else{
            double rand[4];
            for (int irand=0; irand<4; irand++) rand[irand]=rndNum();
            
            PIC::CPLR::FLUID::FluidInterface.setPICIsoUth(
                                                         iBlock,xPar[0],xPar[1],xPar[2],
                                                         uth,uth+1,uth+2,
                                                         rand[0], rand[1], rand[2], rand[3],iSp);        
          }
	  */
          double BulkVel[3];
            
	  //BulkVel[0]= PIC::CPLR::FLUID::FluidInterface.getPICUx(iBlock,
          //                                                            xPar[0],xPar[1],xPar[2],iSp);
    
	//BulkVel[1]= PIC::CPLR::FLUID::FluidInterface.getPICUy(iBlock,
	//xPar[0],xPar[1],xPar[2],iSp);
            
      //BulkVel[2]= PIC::CPLR::FLUID::FluidInterface.getPICUz(iBlock,
      //                                                                xPar[0],xPar[1],xPar[2],iSp);
	  BulkVel[0]=vel_bg[0];
	  BulkVel[1]=vel_bg[1];
	  BulkVel[2]=vel_bg[2];
	  
            
            for (int idim=0;idim<3;idim++)
              ParVel_D[idim] = BulkVel[idim]+uth[idim];
          

          PIC::ParticleBuffer::InitiateParticle(xPar, ParVel_D,&weightCorrection,&iSp,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
          
        }
      }
    }
  }
  
  //  fclose(fPartData);

}


void SetParticleForCell_fixed(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,int iBlock,
                        int iCell,int jCell,int kCell,double * dx,double * xminBlock,
                        double * ParticleWeight,double CellVolume,long & nLocalInjectedParticles){

  //double cellParNumPerSp = PIC::CPLR::FLUID::npcelx[0]*PIC::CPLR::FLUID::npcely[0]*PIC::CPLR::FLUID::npcelz[0];
  int npcelx=6, npcely=7, npcelz=1;
  
  double cellParNumPerSp = npcelx* npcely* npcelz;
  int nSubCells[3]={npcelx,npcely,npcelz};
  int ind[3]={iCell,jCell,kCell};
  double xCenter[3],xCorner[3];

  int ionSpec=1, electronSpec=0;
  double ionMass = PIC::MolecularData::GetMass(ionSpec)/_AMU_;
  double electronMass = PIC::MolecularData::GetMass(electronSpec)/_AMU_;

  for (int idim=0; idim<3; idim++) {
    xCenter[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
    xCorner[idim]=xCenter[idim]-0.5*dx[idim];
  }


  nLocalInjectedParticles += cellParNumPerSp*PIC::nTotalSpecies;
  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++){
    
    RandNum rndNum;
    int nxcg, nycg, nzcg, npcel, nRandom = 7;
    int index_G[3]; 
    //assuming it is uniform
    nxcg=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/dx[0];
    nycg=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/dx[1]; 
    nzcg=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/dx[2];
    
    
    GetGlobalCellIndex(index_G , xCenter, dx, PIC::Mesh::mesh->xGlobalMin);
    npcel = cellParNumPerSp;
    rndNum.set_seed((iSp + 3) * nRandom * npcel *
                            (nxcg * nycg * nzcg * PIC::CPLR::FLUID::iCycle + 
                             nycg * nzcg * index_G[0] + nzcg * index_G[1] + index_G[2]));
    
        
    for (int ii = 0; ii < npcelx; ii++){
      for (int jj = 0; jj < npcely; jj++){
        for (int kk = 0; kk < npcelz; kk++){
          
          double xPar[3],rndLoc[3];
          int index_sub[3]={ii,jj,kk};
          for (int idim=0; idim<3; idim++){
            rndLoc[idim]=rndNum();
            xPar[idim] = (index_sub[idim] + rndLoc[idim]) * (dx[idim]/nSubCells[idim])
              + xCorner[idim];
            //xPar[idim] = (index_sub[idim] + 0.5) * (dx[idim]/nSubCells[idim])
            //  + xCorner[idim];
         
          }

	  double rho_i= 1.102274e-03;
	  double rho_e = 2.755684e-06;
	  double p = 8.221618e-07;
	  double pe = 1.908590e-09;

          //double NumberDensity=fabs(PIC::CPLR::FLUID::FluidInterface.getPICRhoNum(iBlock,xPar[0],xPar[1],xPar[2],iSp));
	  double NumberDensity =rho_i/ionMass;
	  
          double weightCorrection=NumberDensity*CellVolume/ParticleWeight[iSp]/cellParNumPerSp;
    
          
          double ParVel_D[3];
          double uth[3];
	  double uth0;



	  if (iSp==0){
	    uth0 = sqrt(pe/rho_e);//1.3 eV       
	  }else{
	    uth0 = sqrt(p/rho_i);//35 eV
	  }

	  //printf("isp:%d, uth0:%e\n",iSp, uth0);

	  double rand[4];
	  for (int irand=0; irand<4; irand++) rand[irand]=rndNum();
	  double harvest = rand[0];
	  double prob = sqrt(-2.0 * log(1.0 - .999999999 * harvest));
	  harvest = rand[1];
	  double theta = 2.0 * Pi * harvest;
	  uth[0] = uth0 * prob * cos(theta);
	  uth[1] = uth0 * prob * sin(theta);

	  // w = Z velocity
	  harvest = rand[2];
	  prob = sqrt(-2.0 * log(1.0 - .999999999 * harvest));
	  harvest = rand[3];
	  theta = 2.0 * Pi * harvest;
	  uth[2] = uth0 * prob * cos(theta);

	  
	  /*
          if ((iSp!=0 || PIC::CPLR::FLUID::FluidInterface.get_useElectronFluid()) && PIC::CPLR::FLUID::FluidInterface.getUseAnisoP()){
            
            double rand[4];
            for (int irand=0; irand<4; irand++) rand[irand]=rndNum();
            
            PIC::CPLR::FLUID::FluidInterface.setPICAnisoUth(
                                                         iBlock,xPar[0],xPar[1],xPar[2],
                                                         uth,uth+1,uth+2,
                                                         rand[0], rand[1], rand[2], rand[3],iSp);
            
          }else{
            double rand[4];
            for (int irand=0; irand<4; irand++) rand[irand]=rndNum();
            
            PIC::CPLR::FLUID::FluidInterface.setPICIsoUth(
                                                         iBlock,xPar[0],xPar[1],xPar[2],
                                                         uth,uth+1,uth+2,
                                                         rand[0], rand[1], rand[2], rand[3],iSp);        
          }
	  */
          double BulkVel[3];
            
	  BulkVel[0]=vel_bg[0];
	  BulkVel[1]=vel_bg[1];
	  BulkVel[2]=vel_bg[2];
	  
            
            for (int idim=0;idim<3;idim++) { 
              ParVel_D[idim] = BulkVel[idim]+uth[idim];

	    //  ParVel_D[idim] = 0.0;
	    }
          

          PIC::ParticleBuffer::InitiateParticle(xPar, ParVel_D,&weightCorrection,&iSp,NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
          
        }
      }
    }
  }
  

}


void SetParticleForCell_float(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,int iBlock, int iFace,
                        int iCell,int jCell,int kCell,double * dx,double * xminBlock,
                        double * ParticleWeight,double CellVolume,long & nLocalInjectedParticles){


  int ind_dest[3]={iCell,jCell,kCell};
  int ind_src[3]={iCell,jCell,kCell};
  double xCenter[3],xCorner[3];

  int idim = iFace/2;
  int LeftorRight = iFace- 2*idim;
  ind_src[idim] = ind_src[idim] + 1 -2*LeftorRight; // Iface:0 i+1; 1: i-1; 2:j+1; 3:j-1; 4:k+1; 5:k-1

  /*
  for (int idim=0; idim<3; idim++) {
    xCenter[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
    xCorner[idim]=xCenter[idim]-0.5*dx[idim];
  }
  */

  PIC::Mesh::cDataBlockAMR *block=node->block;
  
  long int *FirstCellParticleTable=block->FirstCellParticleTable;
  long int ptr_src=FirstCellParticleTable[ind_src[0]+_BLOCK_CELLS_X_*(ind_src[1]+_BLOCK_CELLS_Y_*ind_src[2])];

  //long int ptr_dest=FirstCellParticleTable[ind_dest[0]+_BLOCK_CELLS_X_*(ind_dest[1]+_BLOCK_CELLS_Y_*ind_dest[2])];

  //copy particles from source cell to destination cell with a displacement of dx
  if (ptr_src!=-1) {

    double vInit[3]={0.0,0.0,0.0},xInit[3]={0.0,0.0,0.0};
    int spec;

    long int ptrNext=ptr_src;
    PIC::ParticleBuffer::byte *ParticleData,*ParticleDataNext;
    ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptr_src);

    	    while (ptrNext!=-1) {
              
              //double LocalParticleWeight;
	      double ind_correction;
	      
	      ParticleData=ParticleDataNext;	  	    
	     
              spec=PIC::ParticleBuffer::GetI(ParticleData);
	      PIC::ParticleBuffer::GetV(vInit,ParticleData);
	      PIC::ParticleBuffer::GetX(xInit,ParticleData);
	      xInit[idim] =xInit[idim]  -  (1 -2*LeftorRight)*dx[idim]; //convert location of src to destination
              //LocalParticleWeight=block->GetLocalParticleWeight(spec);
              //LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);
	      ind_correction = PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);


	      PIC::ParticleBuffer::InitiateParticle(xInit, vInit, &ind_correction, &spec, NULL,_PIC_INIT_PARTICLE_MODE__ADD2LIST_,(void*)node);
			
	      nLocalInjectedParticles++;
	      
              ptrNext=PIC::ParticleBuffer::GetNext(ParticleData);

              if (ptrNext!=-1) {
                ParticleDataNext=PIC::ParticleBuffer::GetParticleDataPointer(ptrNext);
                PIC::ParticleBuffer::PrefertchParticleData_Basic(ParticleDataNext);
              }
            }

  }

}

// Regenerate particles in cells next to the domain boundary using
// PIC::Mesh::InitBoundaryCellVector (no face scanning).
// Returns: (#created - #deleted), same convention as setParticle_BC().
long int setParticle_BC_new() {
  using PIC::Mesh::cDataBlockAMR;

  PIC::Debugger::BuildAndPrintCellParticleDistribution();

  std::vector<PIC::Mesh::cBoundaryCellInfo> boundaryCells;
  if (!PIC::Mesh::InitBoundaryCellVector(&boundaryCells)) {
    std::printf("$PREFIX: SetParticle_BC_new: ERROR: InitBoundaryCellVector failed\n");
    return 0;
  }

  long nParticleDeleted = 0;
  long nParticleCreated = 0;

  // Walk all local boundary cells
  for (const auto& bc : boundaryCells) {
    auto* node = bc.node;
    if (!node || !node->block) continue;

    // Per-node geometry
    const double* xminBlock = node->xmin;
    const double* xmaxBlock = node->xmax;

    double dx[3] = {0,0,0};
    int nCells[3] = {_BLOCK_CELLS_X_, _BLOCK_CELLS_Y_,
#if _MESH_DIMENSION_ == 3
                     _BLOCK_CELLS_Z_
#else
                     1
#endif
    };

    for (int d=0; d<3; ++d) {
#if _MESH_DIMENSION_ == 1
      if (d>0) { dx[d]=1.0; continue; }
#endif
#if _MESH_DIMENSION_ == 2
      if (d==2) { dx[d]=1.0; continue; }
#endif
      dx[d] = (xmaxBlock[d]-xminBlock[d]) / nCells[d];
    }

    // Cell volume
    double cellVol = 1.0;
    for (int d=0; d<3; ++d) cellVol *= dx[d];

    // Species weights local to the block
    double ParticleWeight[PIC::nTotalSpecies];
    for (int sp=0; sp<PIC::nTotalSpecies; ++sp)
      ParticleWeight[sp] = node->block->GetLocalParticleWeight(sp);

    // 1) Delete all particles in this cell (same deletion pattern as setParticle_BC)
    //    (first-cell table + while-loop delete) :contentReference[oaicite:0]{index=0}
    if (node->block->FirstCellParticleTable != nullptr) {
      const int i = bc.i, j = bc.j, k = bc.k;
      long* ptr = node->block->FirstCellParticleTable
                + (i + _BLOCK_CELLS_X_ * (j + _BLOCK_CELLS_Y_ * k));
      while (*ptr != -1) {
        PIC::ParticleBuffer::DeleteParticle(*ptr, *ptr);
        ++nParticleDeleted;
      }
    }

    // 2) Regenerate according to your existing injection routine
    //    (same call site style as setParticle_BC uses) :contentReference[oaicite:1]{index=1}
    SetParticleForCell_fixed(
      node, /*iBlock*/ 0,
      bc.i, bc.j, bc.k,
      dx,
      const_cast<double*>(xminBlock),
      ParticleWeight,
      cellVol,
      nParticleCreated
    );
  }

  PIC::Debugger::BuildAndPrintCellParticleDistribution();

  std::printf("$PREFIX: SetParticle_BC_new: boundary cells processed=%zu, deleted=%ld, created=%ld\n",
              boundaryCells.size(), nParticleDeleted, nParticleCreated);

  return nParticleCreated - nParticleDeleted;
}


long int setParticle_BC(){
  

  //printf("setFixedBC, iCycle:%ld\n",PIC::CPLR::FLUID::iCycle);

  // if (PIC::CPLR::FLUID::iCycle==0) return 0;

  static int cnt=0;
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;  
  //set field for one layer ghost corners 

  char fullname[STRING_LENGTH];
  sprintf(fullname,"test_fixedbc_before_iter=%d.dat",cnt);
  //PIC::Mesh::mesh->outputMeshDataTECPLOT(fullname,0);                                                                          
  
  long nParticleDeleted=0, nParticleCreated=0;
    
  int nAllocatedBlocks =0;
  int ** countedFaceBC;

 
  
  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    if(!node->block) continue;    
    nAllocatedBlocks++; 
  }
  countedFaceBC = new int * [nAllocatedBlocks];
  countedFaceBC[0] = new int [nAllocatedBlocks*6]; 

  for (int iBlk=0; iBlk<nAllocatedBlocks;iBlk++){
    countedFaceBC[iBlk] =  countedFaceBC[0]+6*iBlk;
    for (int iFace=0; iFace<6; iFace++) countedFaceBC[iBlk][iFace]=0;
  } 
 
  
  for (int iFace=0;iFace<6;iFace++){
    
    int iBlk=-1;
    for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*   node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
      //for (int iBlk=0; iBlk<PIC::DomainBlockDecomposition::nLocalBlocks;iBlk++){

      //cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node = PIC::DomainBlockDecomposition::BlockTable[iBlk];

       
      if (!node->block) continue;
      iBlk++;
      //if (node->GetNeibFace(iFace,0,0,PIC::Mesh::mesh)!=NULL) continue;
      //if (node->GetNeibFace(iFace,0,0,PIC::Mesh::mesh)!=NULL && node->GetNeibFace(iFace,0,0)->Thread!=-1) continue;
      if (node->GetNeibFace(iFace,0,0,PIC::Mesh::mesh)!=NULL && node->GetNeibFace(iFace,0,0,PIC::Mesh::mesh)->IsUsedInCalculationFlag!=false) continue;
      //printf("iFace:%d, iBlk:%d, node:%p,nodeXmin:%e,%e,%e\n",iFace,iBlk,node,
      //       node->xmin[0],node->xmin[1],node->xmin[2]);
      
      // if (node->GetNeibFace(iFace,0,0)->Thread!=-1) continue;

      int ind_beg[3]={0,0,0}, ind_end[3] = {nCells[0]-1, nCells[1]-1,nCells[2]-1};

    
      int * iFaceMin[6]={&ind_beg[0],&ind_end[0],&ind_beg[0],&ind_beg[0],&ind_beg[0],&ind_beg[0]};
      int * iFaceMax[6]={&ind_beg[0],&ind_end[0],&ind_end[0],&ind_end[0],&ind_end[0],&ind_end[0]};
      int * jFaceMin[6]={&ind_beg[1],&ind_beg[1],&ind_beg[1],&ind_end[1],&ind_beg[1],&ind_beg[1]};
      int * jFaceMax[6]={&ind_end[1],&ind_end[1],&ind_beg[1],&ind_end[1],&ind_end[1],&ind_end[1]};
      int * kFaceMin[6]={&ind_beg[2],&ind_beg[2],&ind_beg[2],&ind_beg[2],&ind_beg[2],&ind_end[2]};
      int * kFaceMax[6]={&ind_end[2],&ind_end[2],&ind_end[2],&ind_end[2],&ind_beg[2],&ind_end[2]};
  
    
  
      double dx[3];
      double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
          
      for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
 
      long int *  FirstCellParticleTable=node->block->FirstCellParticleTable;
      double CellVolume=1;
      for (int idim=0;idim<3;idim++) CellVolume *= dx[idim];
      double ParticleWeight[PIC::nTotalSpecies];
      for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++)
        ParticleWeight[iSp]=node->block->GetLocalParticleWeight(iSp);
      
      //    for (int iFace=0;iFace<6;iFace++){
      //  if (node->neibNodeFace[iFace]!=NULL && node->neibNodeFace[iFace]->Thread!=-1) continue;

      for (int jface=0; jface<iFace; jface+=2) ind_beg[jface/2]+=countedFaceBC[iBlk][jface]; 
      for (int jface=1; jface<iFace; jface+=2) ind_end[(jface-1)/2]-=countedFaceBC[iBlk][jface]; 
      
      for (int i=*iFaceMin[iFace];i<=*iFaceMax[iFace];i++)
        for (int j=*jFaceMin[iFace];j<=*jFaceMax[iFace];j++)
          for (int k=*kFaceMin[iFace];k<=*kFaceMax[iFace];k++){
            
      /*
      for (int i=0;i<=nI-1;i++)
        for (int j=0;j<=nJ-1;j++)
          for (int k=0;k<=nK-1;k++){
      */

            //for (int i=0;i<nCells[0];i++) for (int j=0;j<nCells[1];j++) for (int k=0;k<nCells[2];k++) {
      
            double x[3];
            int ind[3]={i,j,k};
            
            for (int idim=0; idim<3; idim++) {
              x[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
            }
 
            
            //if (!isBoundaryCell(x, dx, PIC::Mesh::mesh->xGlobalMin, PIC::Mesh::mesh->xGlobalMax, 0, 0)) continue;
            if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCell(x,dx,node)) continue;
            
            // printf("boundary cell x:%e,%e,%e, isBoundaryCell:%s\n",x[0],x[1],x[2], 
            //       PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCell(x,dx,node)?"T":"F");
            //printf("fixRho at x:%e,%e,%e\n", x[0],x[1],x[2]);
              
           

            if (FirstCellParticleTable!=NULL){
              long int * ptr=FirstCellParticleTable+(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k));
              while ((*ptr)!=-1) {
                PIC::ParticleBuffer::DeleteParticle(*ptr,*ptr);
                nParticleDeleted++;
              }
            }
              

	    if (iFace==0) {
	      SetParticleForCell_fixed(node,iBlk,i,j,k,dx,xminBlock,ParticleWeight,CellVolume, nParticleCreated);
	    }else{
	      SetParticleForCell_fixed(node,iBlk,i,j,k,dx,xminBlock,ParticleWeight,CellVolume, nParticleCreated);
	      //SetParticleForCell_float(node,iBlk,iFace,i,j,k,dx,xminBlock,ParticleWeight,CellVolume, nParticleCreated);
	    }
          }//for (int i=0;i<nCells[0];i++) for (int j=0;j<nCells[1];j++) for (int k=0;k<nCells[2];k++)
          
      countedFaceBC[iBlk][iFace]++;
   
    }

  }

  sprintf(fullname,"test_fixedbc_after_iter=%d.dat",cnt);
  //PIC::Mesh::mesh->outputMeshDataTECPLOT(fullname,0);                                                                cnt++;         
  if (PIC::ThisThread==0) printf("setfixed bc done\n");

  //fclose(fPartData);

  delete [] countedFaceBC[0];
  delete [] countedFaceBC;
  return  nParticleCreated-nParticleDeleted;
}







void CleanParticles(){
  
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

  for (node=PIC::Mesh::mesh->BranchBottomNodeList;node!=NULL;node=node->nextBranchBottomNode) if (node->block!=NULL) {
   
     long int *  FirstCellParticleTable=node->block->FirstCellParticleTable;
     if (FirstCellParticleTable==NULL) continue;
     for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
       for (int j=0;j<_BLOCK_CELLS_Y_;j++)  {
	 for (int i=0;i<_BLOCK_CELLS_X_;i++) {
	   long int * ptr=FirstCellParticleTable+(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k));
	   while ((*ptr)!=-1) PIC::ParticleBuffer::DeleteParticle(*ptr,*ptr);

//////
/*
long int next,ptr=FirstCellParticleTable[(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k))]; 

while (ptr!=-1) {

  next=PIC::ParticleBuffer::GetNext(ptr);
  PIC::ParticleBuffer::DeleteParticle(ptr);
  ptr=next; 
}

FirstCellParticleTable[(i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k))]=-1;
*/

/////	   
	 }   
       }
     }
     
  }

}



void setFixedFloatE_BC_half(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;
        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
    // for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    //PIC::Mesh::cDataBlockAMR *block = node->block;
    
    for (int i=-1;i<=nCells[0];i++) for (int j=-1;j<=nCells[1];j++) for (int k=-1;k<=nCells[2];k++) {
          //-1 set bc at subdomain boundaries
          //if (CornerNode==block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))) {
          
          //}
          double x[3];
          int ind[3]={i,j,k};
             
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+ind[idim]*dx[idim];
          }

	  int code=PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node);
          //if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node)) continue;
	  if (!code) continue;
	  

          PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
          if (CornerNode!=NULL){

	    int idim, LeftorRight=-1;// left:0, right 1;
	    /*
	    // 0:    not boundary corner
	    // 1:001 x-direction face  
	    // 2:010 y-direction face
	    // 4:100 z-direction face
	    // 6:110 x-direction edge
	    // 5:101 y-direction edge
	    // 3:011 z-direction edge
	    // 7:111 corner
	    // 8: outside block and outside domain
	     */
	    // 1, 5, 3, 7 -> idim 0
	    // 2, 6 -> idim 1
	    // 4 -> idim 2
	    // 8 -> error
	    switch (code) {

	    case 1:
	    case 5:
	    case 3:
	    case 7:
	      idim = 0;
	      break;
	    case 2:
	    case 6:
	      idim = 1;
	      break;
	    case 4:
	      idim = 2;
	      break;
	    default:
	      for (idim=0;idim<3;idim++){
		if ((fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMin[idim]-dx[idim]))<0.5*dx[idim])){
		  LeftorRight=0;
		  break;
		}
		if ((fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMax[idim]+dx[idim]))<0.5*dx[idim])){
		  LeftorRight=1;
		  break;
		}		
	      }
	      
	      if (idim==3) exit(__LINE__,__FILE__,"Error: the point is outside domain not implemented");
	    }
            char *  offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

	    int ind_src[3]={i,j,k};

	    if (LeftorRight==-1){
	      if (fabs(x[idim]-PIC::Mesh::mesh->xGlobalMin[idim])<0.5*dx[idim]){
		LeftorRight =0;
		ind_src[idim] = ind_src[idim]+1;
	      }else{
		LeftorRight =1;
		ind_src[idim] = ind_src[idim]-1;
	      }
	    }
	    
	    // if (idim==0 && LeftorRight==0){
	      //left boudary is fixed
	    //fixed boundary
	    double Ex=e_bg[0],Ey=e_bg[1],Ez=e_bg[2];
                 
            //Ex = PIC::CPLR::FLUID::FluidInterface.getEx(iBlock,x[0],x[1],x[2]);
            //Ey = PIC::CPLR::FLUID::FluidInterface.getEy(iBlock,x[0],x[1],x[2]);
            //Ez = PIC::CPLR::FLUID::FluidInterface.getEz(iBlock,x[0],x[1],x[2]);

            ((double*)(offset+OffsetE_HalfTimeStep))[ExOffsetIndex]=Ex;
            ((double*)(offset+OffsetE_HalfTimeStep))[EyOffsetIndex]=Ey;
            ((double*)(offset+OffsetE_HalfTimeStep))[EzOffsetIndex]=Ez;

	    /* }else{
	      //float boundary
	     PIC::Mesh::cDataCornerNode *CornerNode_src= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(ind_src[0],ind_src[1],ind_src[2]));
	     char *  offset_src=CornerNode_src->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;


	     ((double*)(offset+OffsetE_HalfTimeStep))[ExOffsetIndex]= ((double*)(offset_src+OffsetE_HalfTimeStep))[ExOffsetIndex];
	     ((double*)(offset+OffsetE_HalfTimeStep))[EyOffsetIndex]= ((double*)(offset_src+OffsetE_HalfTimeStep))[EyOffsetIndex];
	     ((double*)(offset+OffsetE_HalfTimeStep))[EzOffsetIndex]= ((double*)(offset_src+OffsetE_HalfTimeStep))[EzOffsetIndex];
	     

	     }*/
            //if (isTest) printf("test E bc: i,j,k:%d,%d,%d; x:%e %e %e; E:%e,%e,%e\n", i,j,k,x[0],x[1],x[2],Ex,Ey,Ez);
          }
        }// for (int i=iFaceMin_n[iface];i<=iFaceMax_n[iface];i++)...

    iBlock++;
  }

}



void setFixedFloatE_BC_curr(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;
        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
    // for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    //PIC::Mesh::cDataBlockAMR *block = node->block;
    
    for (int i=-1;i<=nCells[0];i++) for (int j=-1;j<=nCells[1];j++) for (int k=-1;k<=nCells[2];k++) {
          //-1 set bc at subdomain boundaries
          //if (CornerNode==block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k))) {
          
          //}
          double x[3];
          int ind[3]={i,j,k};
             
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+ind[idim]*dx[idim];
          }

	  int code=PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node);
          //if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node)) continue;
	  if (!code) continue;
	  

          PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
          if (CornerNode!=NULL){

	    int idim, LeftorRight=-1;// left:0, right 1;
	    /*
	    // 0:    not boundary corner
	    // 1:001 x-direction face  
	    // 2:010 y-direction face
	    // 4:100 z-direction face
	    // 6:110 x-direction edge
	    // 5:101 y-direction edge
	    // 3:011 z-direction edge
	    // 7:111 corner
	    // 8: outside block and outside domain
	     */
	    // 1, 5, 3, 7 -> idim 0
	    // 2, 6 -> idim 1
	    // 4 -> idim 2
	    // 8 -> error
	    switch (code) {

	    case 1:
	    case 5:
	    case 3:
	    case 7:
	      idim = 0;
	      break;
	    case 2:
	    case 6:
	      idim = 1;
	      break;
	    case 4:
	      idim = 2;
	      break;
	    default:
	      for (idim=0;idim<3;idim++){
		if ((fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMin[idim]-dx[idim]))<0.5*dx[idim])){
		  LeftorRight=0;
		  break;
		}
		if ((fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMax[idim]+dx[idim]))<0.5*dx[idim])){
		  LeftorRight=1;
		  break;
		}		
	      }
	      
	      if (idim==3) exit(__LINE__,__FILE__,"Error: the point is outside domain not implemented");
	    }
            char *  offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;

	    int ind_src[3]={i,j,k};

	    if (LeftorRight==-1){
	      if (fabs(x[idim]-PIC::Mesh::mesh->xGlobalMin[idim])<0.5*dx[idim]){
		LeftorRight =0;
		ind_src[idim] = ind_src[idim]+1;
	      }else{
		LeftorRight =1;
		ind_src[idim] = ind_src[idim]-1;
	      }
	    }
	    
	    //if (idim==0 && LeftorRight==0){
	    
	    //fixed boundary
	    double Ex=e_bg[1],Ey=e_bg[1],Ez=e_bg[2];
                 
            //Ex = PIC::CPLR::FLUID::FluidInterface.getEx(iBlock,x[0],x[1],x[2]);
            //Ey = PIC::CPLR::FLUID::FluidInterface.getEy(iBlock,x[0],x[1],x[2]);
            //Ez = PIC::CPLR::FLUID::FluidInterface.getEz(iBlock,x[0],x[1],x[2]);

            ((double*)(offset+CurrentEOffset))[ExOffsetIndex]=Ex;
            ((double*)(offset+CurrentEOffset))[EyOffsetIndex]=Ey;
            ((double*)(offset+CurrentEOffset))[EzOffsetIndex]=Ez;

	    /* }else{
	      //float boundary
	     PIC::Mesh::cDataCornerNode *CornerNode_src= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(ind_src[0],ind_src[1],ind_src[2]));
	     char *  offset_src=CornerNode_src->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;


	     ((double*)(offset+OffsetE_HalfTimeStep))[ExOffsetIndex]= ((double*)(offset_src+CurrentEOffset))[ExOffsetIndex];
	     ((double*)(offset+OffsetE_HalfTimeStep))[EyOffsetIndex]= ((double*)(offset_src+CurrentEOffset))[EyOffsetIndex];
	     ((double*)(offset+OffsetE_HalfTimeStep))[EzOffsetIndex]= ((double*)(offset_src+CurrentEOffset))[EzOffsetIndex];
	     

	     }*/
            //if (isTest) printf("test E bc: i,j,k:%d,%d,%d; x:%e %e %e; E:%e,%e,%e\n", i,j,k,x[0],x[1],x[2],Ex,Ey,Ez);
          }
        }// for (int i=iFaceMin_n[iface];i<=iFaceMax_n[iface];i++)...

    iBlock++;
  }

}




void setFixedFloatB_center_BC(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;
        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
    // for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    //PIC::Mesh::cDataBlockAMR *block = node->block;
  
    for (int i=-1;i<nCells[0]+1;i++) for (int j=-1;j<nCells[1]+1;j++) for (int k=-1;k<nCells[2]+1;k++) {
      

          double x[3];
          int ind[3]={i,j,k};
            
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];
          }

          //if (!isBoundaryCell(x, dx, PIC::Mesh::mesh->xGlobalMin, PIC::Mesh::mesh->xGlobalMax, 0, 0)) continue;
	  int code = PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCell(x,dx,node);
          //if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCell(x,dx,node)) continue;
	  if (!code) continue;

          PIC::Mesh::cDataCenterNode *CenterNode= node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));
          if (CenterNode!=NULL){

	    int idim, LeftorRight;// left:0, right 1;
	    /*
	    // 0:    not boundary corner
	    // 1:001 x-direction face  
	    // 2:010 y-direction face
	    // 4:100 z-direction face
	    // 6:110 x-direction edge
	    // 5:101 y-direction edge
	    // 3:011 z-direction edge
	    // 7:111 corner
	    // 8: outside block and outside domain
	     */
	    // 1, 5, 3, 7 -> idim 0
	    // 2, 6 -> idim 1
	    // 4 -> idim 2
	    // 8 -> error
	    switch (code) {

	    case 1:
	    case 5:
	    case 3:
	    case 7:
	      idim = 0;
	      break;
	    case 2:
	    case 6:
	      idim = 1;
	      break;
	    case 4:
	      idim = 2;
	      break;
	    default:
	    
		for (idim=0;idim<3;idim++){
		  if ((fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMin[idim]-0.5*dx[idim]))<0.5*dx[idim])){
		    LeftorRight=0;
		    break;
		  }
		  if ((fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMax[idim]+0.5*dx[idim]))<0.5*dx[idim])){
		    LeftorRight=1;
		    break;
		  }		
		}
	   
		if (idim==3) exit(__LINE__,__FILE__,"Error: the point is outside domain not implemented");
	    }
	    
	    
            char *  offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

	    
	    int ind_src[3]={i,j,k};
	    if (LeftorRight==-1){
	      if (fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMin[idim]+0.5*dx[idim]))<0.5*dx[idim]){
		LeftorRight =0;
		ind_src[idim] = ind_src[idim]+1;
	      }else{
		LeftorRight =1;
		ind_src[idim] = ind_src[idim]-1;
	      }
	    }
	    
	    //if (idim==0 && LeftorRight==0){

	      //fixed boundary
            double Bx=  b_bg[0], By=  b_bg[1],Bz= b_bg[2];

	    //Bz = 325e-9 * 31930.3280322054; 
            //Bx = PIC::CPLR::FLUID::FluidInterface.getBx(iBlock,x[0],x[1],x[2]);
            //By = PIC::CPLR::FLUID::FluidInterface.getBy(iBlock,x[0],x[1],x[2]);
            //Bz = PIC::CPLR::FLUID::FluidInterface.getBz(iBlock,x[0],x[1],x[2]);
            
            /*
            if (fabs(x[0]-31.5)<0.01 && fabs(x[1]-7.5)<0.01 && fabs(x[2]-3.5)<0.01){
              printf("test center b:%e,%e,%e\n", Bx, By, Bz);

            }
            */
            ((double*)(offset+CurrentBOffset))[BxOffsetIndex]=Bx;
            ((double*)(offset+CurrentBOffset))[ByOffsetIndex]=By;
            ((double*)(offset+CurrentBOffset))[BzOffsetIndex]=Bz;
	    
            ((double*)(offset+PrevBOffset))[BxOffsetIndex]=Bx;
            ((double*)(offset+PrevBOffset))[ByOffsetIndex]=By;
            ((double*)(offset+PrevBOffset))[BzOffsetIndex]=Bz;
	    /* }else{
	      //float boundary
	      PIC::Mesh::cDataCenterNode *CenterNode_src= node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(ind_src[0],ind_src[1],ind_src[2]));
	      char *  offset_src=CenterNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;
	      
	      ((double*)(offset+CurrentBOffset))[BxOffsetIndex]=((double*)(offset_src+CurrentBOffset))[BxOffsetIndex];
	      ((double*)(offset+CurrentBOffset))[ByOffsetIndex]=((double*)(offset_src+CurrentBOffset))[ByOffsetIndex];
	      ((double*)(offset+CurrentBOffset))[BzOffsetIndex]=((double*)(offset_src+CurrentBOffset))[BzOffsetIndex];
	      
	      ((double*)(offset+PrevBOffset))[BxOffsetIndex]=((double*)(offset_src+PrevBOffset))[BxOffsetIndex];
	      ((double*)(offset+PrevBOffset))[ByOffsetIndex]=((double*)(offset_src+PrevBOffset))[ByOffsetIndex];
	      ((double*)(offset+PrevBOffset))[BzOffsetIndex]=((double*)(offset_src+PrevBOffset))[BzOffsetIndex];
           	      
	    }*/
	  }
	}
    iBlock++;
  }

}


void setFixedFloatB_corner_BC(){
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

    if (!node->block) continue;
        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
    // for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    //PIC::Mesh::cDataBlockAMR *block = node->block;
  

    for (int i=-1;i<=nCells[0];i++) for (int j=-1;j<=nCells[1];j++) for (int k=-1;k<=nCells[2];k++) {

          double x[3];
          int ind[3]={i,j,k};
            
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+(ind[idim])*dx[idim];
          }

	  int code = PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node);
          //if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node)) continue;
	  if (!code) continue;

          PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
          if (CornerNode!=NULL){

	    int idim, LeftorRight;// left:0, right 1;
	    /*
	    // 0:    not boundary corner
	    // 1:001 x-direction face  
	    // 2:010 y-direction face
	    // 4:100 z-direction face
	    // 6:110 x-direction edge
	    // 5:101 y-direction edge
	    // 3:011 z-direction edge
	    // 7:111 corner
	    // 8: outside block and outside domain
	     */
	    // 1, 5, 3, 7 -> idim 0
	    // 2, 6 -> idim 1
	    // 4 -> idim 2
	    // 8 -> error
	    switch (code) {

	    case 1:
	    case 5:
	    case 3:
	    case 7:
	      idim = 0;
	      break;
	    case 2:
	    case 6:
	      idim = 1;
	      break;
	    case 4:
	      idim = 2;
	      break;
	    default:
	      for (idim=0;idim<3;idim++){
		if ((fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMin[idim]-dx[idim]))<0.5*dx[idim])){
		  LeftorRight=0;
		  break;
		}
		if ((fabs(x[idim]-(PIC::Mesh::mesh->xGlobalMax[idim]+dx[idim]))<0.5*dx[idim])){
		  LeftorRight=1;
		  break;
		}		
	      }
	    
	      if (idim==3) exit(__LINE__,__FILE__,"Error: the point is outside domain not implemented");
	    }
	    
            char *  offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner;

	    int ind_src[3]={i,j,k};

	    if (LeftorRight==-1){
	      if (fabs(x[idim]-PIC::Mesh::mesh->xGlobalMin[idim])<0.5*dx[idim]){
		LeftorRight =0;
		ind_src[idim] = ind_src[idim]+1;
	      }else{
		LeftorRight =1;
		ind_src[idim] = ind_src[idim]-1;
	      }
	    }
	    
	    // if (idim==0 && LeftorRight==0){
	      //fixed boundary  
            double Bx= b_bg[0],By= b_bg[1],Bz= b_bg[2];

	    //Bz = 325e-9 * 31930.3280322054; 
            //Bx = PIC::CPLR::FLUID::FluidInterface.getBx(iBlock,x[0],x[1],x[2]);
            //By = PIC::CPLR::FLUID::FluidInterface.getBy(iBlock,x[0],x[1],x[2]);
            //Bz = PIC::CPLR::FLUID::FluidInterface.getBz(iBlock,x[0],x[1],x[2]);
            /*
            if (fabs(x[0]-31)<0.01 && fabs(x[1]-8)<0.01 && fabs(x[2]-4)<0.01){
              printf("test corner b :%e,%e,%e\n", Bx, By, Bz);
              
            }
            */
            ((double*)(offset+CurrentBOffset))[BxOffsetIndex]=Bx;
            ((double*)(offset+CurrentBOffset))[ByOffsetIndex]=By;
            ((double*)(offset+CurrentBOffset))[BzOffsetIndex]=Bz;
            
            ((double*)(offset+PrevBOffset))[BxOffsetIndex]=Bx;
            ((double*)(offset+PrevBOffset))[ByOffsetIndex]=By;
            ((double*)(offset+PrevBOffset))[BzOffsetIndex]=Bz;

	    /* }else{
	      //float boundary
	      PIC::Mesh::cDataCornerNode *CornerNode_src= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(ind_src[0],ind_src[1],ind_src[2]));

	      char *  offset_src = CornerNode_src->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner;
	      
	      ((double*)(offset+CurrentBOffset))[BxOffsetIndex]=((double*)(offset_src+CurrentBOffset))[BxOffsetIndex];
	      ((double*)(offset+CurrentBOffset))[ByOffsetIndex]=((double*)(offset_src+CurrentBOffset))[ByOffsetIndex];
	      ((double*)(offset+CurrentBOffset))[BzOffsetIndex]=((double*)(offset_src+CurrentBOffset))[BzOffsetIndex];
	      
	      ((double*)(offset+PrevBOffset))[BxOffsetIndex]=((double*)(offset_src+PrevBOffset))[BxOffsetIndex];
	      ((double*)(offset+PrevBOffset))[ByOffsetIndex]=((double*)(offset_src+PrevBOffset))[ByOffsetIndex];
	      ((double*)(offset+PrevBOffset))[BzOffsetIndex]=((double*)(offset_src+PrevBOffset))[BzOffsetIndex];


	    }*/
          }//if (CornerNode!=NULL)
        }
    iBlock++;
  }

}




long int PrepopulateDomain() {
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iCell,jCell,kCell;
  //cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataCenterNode *cell;
  long int nd,nGlobalInjectedParticles,nLocalInjectedParticles=0;
  double Velocity[3];
  /*
  //local copy of the block's cells
  int cellListLength=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::ThisThread]->block->GetCenterNodeListLength();
  PIC::Mesh::cDataCenterNode *cellList[cellListLength];
  */
  //particle ejection parameters
  //double ParticleWeight;//beta=PIC::MolecularData::GetMass(spec)/(2*Kbol*Temperature);
  double waveNumber[3]={0.0,0.0,0.0};
  double lambda=32.0;


  double ParticleWeight[PIC::nTotalSpecies];
  //  for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++)
  //  ParticleWeight[iSp]=node->block->GetLocalParticleWeight(iSp);
    
  
  waveNumber[0]=2*Pi/lambda;

  double *ParticleDataTable=NULL,*ParticleDataTable_dev=NULL;
  int ParticleDataTableIndex=0,ParticleDataTableLength=0;

  
  int nBlock[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};

  //the boundaries of the block and middle point of the cell
  double *xminBlock,*xmaxBlock;
  double v[3],anpart;
  int npart;
  char * offset=NULL;
  int ionSpec=1, electronSpec=0;
  double ionMass = PIC::MolecularData::GetMass(ionSpec)/_AMU_;
  double electronMass = PIC::MolecularData::GetMass(electronSpec)/_AMU_;
  //  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
  //  {
        for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
    if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
      bool BoundaryBlock=false;
      
      for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
	  //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
	  BoundaryBlock=true;
	  break;
	}
      
      if (BoundaryBlock==true) continue;
    }

    if (node->Thread!=PIC::ThisThread) continue;


    // }

    // PIC::Mesh::cDataCenterNode *cellList[cellListLength];
  
    //memcpy(cellList,node->block->GetCenterNodeList(),cellListLength*sizeof(PIC::Mesh::cDataCenterNode*));

    xminBlock=node->xmin,xmaxBlock=node->xmax;
    double dx[3];
    double CellVolume=1;
    for (int idim=0;idim<3;idim++) {
      dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nBlock[idim];
      CellVolume *= dx[idim];
    }

    for (int iSp=0;iSp<PIC::nTotalSpecies;iSp++)
       ParticleWeight[iSp]=node->block->GetLocalParticleWeight(iSp);


    for (kCell=0;kCell<nBlock[2];kCell++) for (jCell=0;jCell<nBlock[1];jCell++) for (iCell=0;iCell<nBlock[0];iCell++) {

	  int ind[3]={iCell,jCell,kCell};
	  double x[3];
	  for (int idim=0; idim<3; idim++) x[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];


	  SetParticleForCell_fixed(node, 0,
                        iCell, jCell, kCell, dx,  xminBlock,
			ParticleWeight, CellVolume,nLocalInjectedParticles);

	  /*
	  void SetParticleForCell_fixed(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,int iBlock,
                        int iCell,int jCell,int kCell,double * dx,double * xminBlock,
                        double * ParticleWeight,double CellVolume,long & nLocalInjectedParticles){
	  */
	  
	  /*
	  double rho = 720;
	  double p = 8.064, pe = 0.01872;
	  double ux =0.0;//26.4
	  */


      //end of the particle injection block
      //std::cout<<"finished injecting npart: "<<npart<<std::endl;
      
      
      
	}
   }



  MPI_Allreduce(&nLocalInjectedParticles,&nGlobalInjectedParticles,1,MPI_LONG,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
  printf("particles prepopulated!\n");
  return nGlobalInjectedParticles;
}



void SetIC() {
  
    int i,j,k;
    char *offset;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    double cPi = 3.14159265;
    double waveNumber[3]={0.0,0.0,0.0};
    double lambda=32.0;
   
    waveNumber[0]=2*cPi/lambda;
  
    double x[3];
   
    using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
    int nBreak=0;

    printf("User Set IC called\n");

    int nBlocks[3] ={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
    for (int nLocalNode=0;nLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      node=PIC::DomainBlockDecomposition::BlockTable[nLocalNode];
      
      if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
	bool BoundaryBlock=false;
	
	for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
	    //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
	    BoundaryBlock=true;
	    break;
	  }
	
	if (BoundaryBlock==true) continue;
      }
      
      double dx[3];
      double *xminBlock= node->xmin, *xmaxBlock= node->xmax;

      for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nBlocks[idim];
      
     
      for (k=0;k<_BLOCK_CELLS_Z_+1;k++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (i=0;i<_BLOCK_CELLS_X_+1;i++) {
	    
            int ind[3]={i,j,k};
            
	    PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
	    if (CornerNode!=NULL){
	      offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
	     
              
	      for (int idim=0; idim<3; idim++) x[idim]=xminBlock[idim]+ind[idim]*dx[idim];
	      double Ex=e_bg[0], Ey=e_bg[1], Ez=e_bg[2];
	   
	   
              ((double*)(offset+CurrentEOffset))[ExOffsetIndex]=Ex;
              ((double*)(offset+CurrentEOffset))[EyOffsetIndex]=Ey;
              ((double*)(offset+CurrentEOffset))[EzOffsetIndex]=Ez;
	  
              ((double*)(offset+OffsetE_HalfTimeStep))[ExOffsetIndex]=Ex;
              ((double*)(offset+OffsetE_HalfTimeStep))[EyOffsetIndex]=Ey;
              ((double*)(offset+OffsetE_HalfTimeStep))[EzOffsetIndex]=Ez;
          

	    // ((double*)(offset+CurrentCornerNodeOffset))[EzOffsetIndex]=i+j*_BLOCK_CELLS_X_+k*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_+nLocalNode*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;


	    
	    //((double*)(offset+NextCornerNodeOffset))[EzOffsetIndex]=i+j*_BLOCK_CELLS_X_+k*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_+nLocalNode*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;
	    }//
	  }//for (k=0;k<_BLOCK_CELLS_Z_+1;k++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (i=0;i<_BLOCK_CELLS_X_+1;i++) 
      for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) {
            int ind[3]={i,j,k};
	    PIC::Mesh::cDataCenterNode *CenterNode= node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k));
	    if (CenterNode!=NULL){
	      offset=node->block->GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset;

              for (int idim=0; idim<3; idim++) x[idim]=xminBlock[idim]+(ind[idim]+0.5)*dx[idim];

	      //double B = 0.004*sin(waveNumber[0]*(x[0]-xmin[0])+waveNumber[1]*(x[1]-xmin[1])+waveNumber[2]*(x[2]-xmin[2]));
              double Bx = b_bg[0], By=b_bg[1], Bz=b_bg[2];
              
	      

	      //printf("IC Bz:%e\n",Bz*Bfactor);
              ((double*)(offset+CurrentBOffset))[BxOffsetIndex]=Bx;
              ((double*)(offset+CurrentBOffset))[ByOffsetIndex]=By;
              ((double*)(offset+CurrentBOffset))[BzOffsetIndex]=Bz;
		
		
              ((double*)(offset+PrevBOffset))[BxOffsetIndex]=Bx;
              ((double*)(offset+PrevBOffset))[ByOffsetIndex]=By;
              ((double*)(offset+PrevBOffset))[BzOffsetIndex]=Bz;
            }// if (CenterNode!=NULL)
	  }//for (k=0;k<_BLOCK_CELLS_Z_;k++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (i=0;i<_BLOCK_CELLS_X_;i++) 
    }
   
    switch (_PIC_BC__PERIODIC_MODE_) {
    case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange();
      break;
      
    case _PIC_BC__PERIODIC_MODE_ON_:
      PIC::BC::ExternalBoundary::UpdateData();
      break;
    }
}


double localTimeStep(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
    double CellSize;
    double CharacteristicSpeed;
    double dt;


    CellSize=startNode->GetCharacteristicCellSize();
    //return 0.3*CellSize/CharacteristicSpeed;

    //return 0.05;
    return 0.07*3e3;
}


double BulletLocalResolution(double *x) {                                                                                           
  double dist = xmax[0]-xmin[0];

#ifndef _UNIFORM_MESH_
#error ERROR: _UNIFORM_MESH_ is used but not defined
#endif
#ifndef _TEST_MESH_MODE_
#error ERROR: _TEST_MESH_MODE_ is used but not defined
#endif
#if _TEST_MESH_MODE_==_UNIFORM_MESH_  
  double res = 3;
#endif

#ifndef _NONUNIFORM_MESH_
#error ERROR: _NONUNIFORM_MESH_ is used but not defined
#endif
#ifndef _TEST_MESH_MODE_
#error ERROR: _TEST_MESH_MODE_ is used but not defined
#endif
#if _TEST_MESH_MODE_==_NONUNIFORM_MESH_
  double highRes = dist/32.0, lowRes= dist/2.0;     
  double res =(5-1)/dist*(x[0]-xmin[0])+1;  
#endif

  res=sqrt(3)+0.1;
  res= res* r_planet/4;
  return res;
}
                       

int main(int argc,char **argv) {
  
   time_t TimeValue=time(NULL);
   tm *ct=localtime(&TimeValue);

   double radius;

   radius = r_planet;
   printf("radius:%e\n", radius);
   printf("start: (%i/%i %i:%i:%i)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec);
  

  PIC::InitMPI();
  PIC::Init_BeforeParser();

  
  int RelativeOffset=0;
  
#ifndef _NONUNIFORM_MESH_
#error ERROR: _NONUNIFORM_MESH_ is used but not defined
#endif
#ifndef _TEST_MESH_MODE_
#error ERROR: _TEST_MESH_MODE_ is used but not defined
#endif
#if _TEST_MESH_MODE_==_NONUNIFORM_MESH_
  printf("non-uniform mesh!\n");
#endif
#ifndef _UNIFORM_MESH_
#error ERROR: _UNIFORM_MESH_ is used but not defined
#endif
#ifndef _TEST_MESH_MODE_
#error ERROR: _TEST_MESH_MODE_ is used but not defined
#endif
#if _TEST_MESH_MODE_==_UNIFORM_MESH_
  printf("uniform mesh!\n");
#endif


#ifndef _PIC_MODE_ON_
#error ERROR: _PIC_MODE_ON_ is used but not defined
#endif
#ifndef _CURRENT_MODE_
#error ERROR: _CURRENT_MODE_ is used but not defined
#endif
#if _CURRENT_MODE_==_PIC_MODE_ON_
  printf("current on!\n");
#endif
#ifndef _PIC_MODE_OFF_
#error ERROR: _PIC_MODE_OFF_ is used but not defined
#endif
#ifndef _CURRENT_MODE_
#error ERROR: _CURRENT_MODE_ is used but not defined
#endif
#if _CURRENT_MODE_==_PIC_MODE_OFF_
  printf("current mode off!\n");
#endif

  PIC::Mesh::mesh->PopulateOutsideDomainNodesFlag=true;

  
  //seed the random number generator
  rnd_seed(100);

  //generate mesh or read from file
  char mesh[_MAX_STRING_LENGTH_PIC_]="none";  ///"amr.sig=0xd7058cc2a680a3a2.mesh.bin";
  sprintf(mesh,"amr.sig=%s.mesh.bin","test_mesh");

  PIC::Mesh::mesh->AllowBlockAllocation=false;
  if(_PIC_BC__PERIODIC_MODE_== _PIC_BC__PERIODIC_MODE_ON_){
  PIC::BC::ExternalBoundary::Periodic::Init(xmin,xmax,BulletLocalResolution);
  }else{
    PIC::Mesh::mesh->init(xmin,xmax,BulletLocalResolution);
  }
  PIC::Mesh::mesh->memoryAllocationReport();


  if (PIC::ThisThread==0) printf("xmax:%e,%e,%e\n",xmax[0],xmax[1],xmax[2]);
  //generate mesh or read from file
  bool NewMeshGeneratedFlag=false;

  char fullname[STRING_LENGTH];
  sprintf(fullname,"%s/%s",PIC::UserModelInputDataPath,mesh);

  FILE *fmesh=NULL;

  fmesh=fopen(fullname,"r");

  if (fmesh!=NULL) {
    fclose(fmesh);
    PIC::Mesh::mesh->readMeshFile(fullname);
  }
  else {
    NewMeshGeneratedFlag=true;

    if (PIC::Mesh::mesh->ThisThread==0) {
       PIC::Mesh::mesh->buildMesh();
       PIC::Mesh::mesh->saveMeshFile("mesh.msh");
       MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
    }
    else {
       MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
       PIC::Mesh::mesh->readMeshFile("mesh.msh");
    }
  }


  //if the new mesh was generated => rename created mesh.msh into amr.sig=0x%lx.mesh.bin
  if (NewMeshGeneratedFlag==true) {
    unsigned long MeshSignature=PIC::Mesh::mesh->getMeshSignature();

    if (PIC::Mesh::mesh->ThisThread==0) {
      char command[300];

      sprintf(command,"mv mesh.msh amr.sig=0x%lx.mesh.bin",MeshSignature);
      system(command);
    }
  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  

  PIC::Mesh::initCellSamplingDataBuffer();

  PIC::Mesh::mesh->CreateNewParallelDistributionLists();

  PIC::Mesh::mesh->AllowBlockAllocation=true;
  PIC::Mesh::mesh->AllocateTreeBlocks();
  PIC::Mesh::mesh->InitCellMeasure();

  PIC::Init_AfterParser();
  PIC::Mover::Init();

  //set up the time step
  PIC::ParticleWeightTimeStep::LocalTimeStep=localTimeStep;
  PIC::ParticleWeightTimeStep::initTimeStep();

  if (PIC::ThisThread==0) printf("test1\n");
  PIC::Mesh::mesh->outputMeshTECPLOT("mesh_test.dat");
  
  if(_PIC_BC__PERIODIC_MODE_== _PIC_BC__PERIODIC_MODE_ON_){
  PIC::BC::ExternalBoundary::Periodic::InitBlockPairTable();
  }
  //-387.99e2
  int s,i,j,k;


  if (PIC::ThisThread==0) printf("test2\n");
 
  // PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(0);
  //PIC::ParticleWeightTimeStep::initParticleWeight_ConstantWeight(1);

  double weight_est;
  weight_est = 6.889211e-05*pow(r_planet/4,3)/40;
  //weight_est = 6.889211e-05*pow(r_planet/4,3)/1;
 
  
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(0,weight_est);
  PIC::ParticleWeightTimeStep::SetGlobalParticleWeight(1,weight_est);

  PIC::DomainBlockDecomposition::UpdateBlockTable();

  //solve the transport equation
  //set the initial conditions for the transport equation
  //  TransportEquation::SetIC(3);
 

  switch (_PIC_BC__PERIODIC_MODE_) {
  case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange();
      break;
      
  case _PIC_BC__PERIODIC_MODE_ON_:
    PIC::BC::ExternalBoundary::UpdateData();
      break;
  }
  //PIC::FieldSolver::Init(); 
  PIC::FieldSolver::Electromagnetic::ECSIM::SetIC=SetIC;

  PIC::FieldSolver::Electromagnetic::ECSIM::setParticle_BC=
    setParticle_BC;//called every pic_time_step


  PIC::FieldSolver::Electromagnetic::ECSIM::setParticle_BC=
    setParticle_BC_new;//called every pic_time_step


  PIC::FieldSolver::Electromagnetic::ECSIM::setE_half_BC=
    setFixedFloatE_BC_half;//called in ECSIM::TimeStep()
  PIC::FieldSolver::Electromagnetic::ECSIM::setE_curr_BC=
    setFixedFloatE_BC_curr;//called in ECSIM::TimeStep()  
  PIC::FieldSolver::Electromagnetic::ECSIM::setB_center_BC=
    setFixedFloatB_center_BC;//called in ECSIM::TimeStep()  
  PIC::FieldSolver::Electromagnetic::ECSIM::setB_corner_BC=
    setFixedFloatB_corner_BC;//called in ECSIM::TimeStep()  
  //PIC::FieldSolver::Electromagnetic::ECSIM::setBlockParticle=NULL;//used for init new blocks


  
  int  totalIter,CaseNumber;
  //PIC::FieldSolver::Init();
  PIC::FieldSolver::Electromagnetic::ECSIM::Init_IC();
  PIC::CPLR::FLUID::EFieldTol = 1.0e-8;

  totalIter=500;
     
    switch (_PIC_BC__PERIODIC_MODE_) {
    case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange();
      break;
      
    case _PIC_BC__PERIODIC_MODE_ON_:
      PIC::BC::ExternalBoundary::UpdateData();
      break;
    }
    PIC::Mesh::mesh->outputMeshDataTECPLOT("ic.dat",0);
  

      int LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
      int GlobalParticleNumber;
      MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
      printf("Before cleaning, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);
      std::cout<<"LocalParticleNumber: "<<LocalParticleNumber<<" GlobalParticleNumber:"<<GlobalParticleNumber<<std::endl;

      CleanParticles();
      LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
      MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
      printf("After cleaning, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);

      PrepopulateDomain();
      PIC::Debugger::BuildAndPrintCellParticleDistribution();

      LocalParticleNumber=PIC::ParticleBuffer::GetAllPartNum();
      MPI_Allreduce(&LocalParticleNumber,&GlobalParticleNumber,1,MPI_INT,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
      printf("After prepopulating, LocalParticleNumber,GlobalParticleNumber,iThread:%d,%d,%d\n",LocalParticleNumber,GlobalParticleNumber,PIC::ThisThread);
      std::cout<<"LocalParticleNumber: "<<LocalParticleNumber<<" GlobalParticleNumber:"<<GlobalParticleNumber<<std::endl;
   
    switch (_PIC_BC__PERIODIC_MODE_) {
    case _PIC_BC__PERIODIC_MODE_OFF_:
      PIC::Mesh::mesh->ParallelBlockDataExchange();
      break;
      
    case _PIC_BC__PERIODIC_MODE_ON_:
      PIC::BC::ExternalBoundary::UpdateData();
      break;
    }
    
    //PIC::Sampling::Sampling();
    PIC::FieldSolver::Electromagnetic::ECSIM::UpdateJMassMatrix();
    
    for (int niter=0;niter<totalIter;niter++) {

      //PIC::Mesh::mesh->outputMeshDataTECPLOT("1.dat",0);
    
      //TransportEquation::TimeStep();
  
      PIC::TimeStep();
      //PIC::FieldSolver::Electromagnetic::ECSIM::TimeStep();

      //PIC::Mesh::mesh->outputMeshDataTECPLOT("2.dat",0);


      PIC::Debugger::BuildAndPrintCellParticleDistribution();

      switch (_PIC_BC__PERIODIC_MODE_) {
      case _PIC_BC__PERIODIC_MODE_OFF_:
	PIC::Mesh::mesh->ParallelBlockDataExchange();
	break;

      case _PIC_BC__PERIODIC_MODE_ON_:
	PIC::BC::ExternalBoundary::UpdateData();
	break;
      }

    }
  
  PIC::RunTimeSystemState::CumulativeTiming::Print();
  MPI_Finalize();
  
  TimeValue=time(NULL);
  ct=localtime(&TimeValue);
  
  printf("end: (%i/%i %i:%i:%i)\n",ct->tm_mon+1,ct->tm_mday,ct->tm_hour,ct->tm_min,ct->tm_sec);

  cout << "End of the run" << endl;
  return EXIT_SUCCESS;


}
