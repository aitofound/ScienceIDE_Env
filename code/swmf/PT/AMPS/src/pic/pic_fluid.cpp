//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//$Id$
//the interface between AMPS and SWMF

#include "mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "pic.h"

using namespace std;

bool PIC::CPLR::FLUID::FirstCouplingOccured=false;
list<PIC::CPLR::FLUID::fSendCenterPointData> PIC::CPLR::FLUID::SendCenterPointData;

FluidPicInterface PIC::CPLR::FLUID::FluidInterface;
long int PIC::CPLR::FLUID::iCycle = 0;
int PIC::CPLR::FLUID::nBlockProc  = 0;
bool PIC::CPLR::FLUID::IsRestart = false;
int PIC::CPLR::FLUID::nCells[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
int *PIC::CPLR::FLUID::npcelx, *PIC::CPLR::FLUID::npcely,*PIC::CPLR::FLUID::npcelz;

int nI_Gh = _GHOST_CELLS_X_< 2 ? 2:_GHOST_CELLS_X_;
int nJ_Gh = _GHOST_CELLS_Y_< 2 ? 2:_GHOST_CELLS_Y_;
int nK_Gh = _GHOST_CELLS_Z_< 2 ? 2:_GHOST_CELLS_Z_;

double PIC::CPLR::FLUID::dt  = 0;

// #EFIELDSOLVER
double PIC::CPLR::FLUID::EFieldTol = 1.0e-6;
double PIC::CPLR::FLUID::EFieldIter = 200;

void PIC::CPLR::FLUID::ConvertMpiCommunicatorFortran2C(signed int* iComm,signed int* iProc,signed int* nProc) {
  MPI_GLOBAL_COMMUNICATOR=MPI_Comm_f2c(*iComm);
  PIC::InitMPI();

  if (PIC::ThisThread==0) {
    printf("AMPS: MPI Communicatior is imported from SWMF, size=%i\n",PIC::nTotalThreads);
  }
}

/*
void PIC::CPLR::SWMF::ResetCenterPointProcessingFlag() {
  int thread,i,j,k;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCenterNode *cell;

  //init the cell processing flags
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    block=node->block;

    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
          if (cell!=NULL) cell->nodeDescriptor.nodeProcessedFlag=_OFF_AMR_MESH_;
        }
    }
  }

  for (thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) for (node=PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];node!=NULL;node=node->nextNodeThisThread) {
    block=node->block;

    for (i=-_GHOST_CELLS_X_;i<_BLOCK_CELLS_X_+_GHOST_CELLS_X_;i++) {
      for (j=-_GHOST_CELLS_Y_;j<_BLOCK_CELLS_Y_+_GHOST_CELLS_Y_;j++)
        for (k=-_GHOST_CELLS_Z_;k<_BLOCK_CELLS_Z_+_GHOST_CELLS_Z_;k++) {
          cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
          if (cell!=NULL) cell->nodeDescriptor.nodeProcessedFlag=_OFF_AMR_MESH_;
        }
    }
  }
}
*/

void PIC::CPLR::FLUID::GetCornerPointNumber(int *nCornerPoints) {
  int thread,i,j,k;
  //cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCornerNode *corner;

  //in case the BlockTable is not initialized yet -> init BlockTable
  if (PIC::DomainBlockDecomposition::BlockTable==NULL) {
    PIC::DomainBlockDecomposition::UpdateBlockTable();

    // recompute bc_type flags on the new owners
    if (_PIC_FIELD_SOLVER_MODE_!=_PIC_FIELD_SOLVER_MODE__OFF_) {
      PIC::FieldSolver::Electromagnetic::DomainBC.RecomputeBCType();
    }
  }

  *nCornerPoints=0;

  nBlockProc = 0; 
  //count the number of the corner points
  //for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
  for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];

    if(!isTrueBlock(node) || !node->block) continue;
    block=node->block;
    nBlockProc++; 
    for (i=-nI_Gh;i<_BLOCK_CELLS_X_+nI_Gh+1;i++) {
      for (j=-nJ_Gh;j<_BLOCK_CELLS_Y_+nJ_Gh+1;j++) {
        for (k=-nK_Gh;k<_BLOCK_CELLS_Z_+nK_Gh+1;k++) {
          // corner=block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));
	  (*nCornerPoints)++;
        }
      }
    }    
  }

}


void PIC::CPLR::FLUID::GetCornerPointCoordinates(double *x) {
  int thread,i,j,k,cnt=0;
  //cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCornerNode *corner;
  
  int nCells[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};

  //get coordinated of the center points
  //for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
  for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];

    if(!isTrueBlock(node) || !node->block) continue;
    block=node->block;
        
    double * xminBlock=node->xmin;
    double * xmaxBlock=node->xmax;
    double dx[3];
    
    for (int idim=0;idim<3;idim++) 
      dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    
    for (i=-nI_Gh;i<_BLOCK_CELLS_X_+nI_Gh+1;i++) {
      for (j=-nJ_Gh;j<_BLOCK_CELLS_Y_+nJ_Gh+1;j++) {
        for (k=-nK_Gh;k<_BLOCK_CELLS_Z_+nK_Gh+1;k++) {
	  double xTemp[3];
          int ind[3]={i,j,k};
          for (int idim=0;idim<3;idim++) xTemp[idim]=xminBlock[idim]+ind[idim]*dx[idim];          
          FluidInterface.pic_to_Mhd_Vec(xTemp, x+3*cnt);
	  cnt++;
        }
      }
    }
  }

}

void PIC::CPLR::FLUID::ReceiveCornerPointData(char* ValiableList, int nVarialbes, double *data,int *index) {

  // Set the range/cell size of used block. --------------

  // Call clear method first, because nBlockProc may change. 
  FluidInterface.BlockMin_BD.clear(); 
  FluidInterface.BlockMax_BD.clear();
  FluidInterface.CellSize_BD.clear();

  const int nDimMax = 3;
  FluidInterface.BlockMin_BD.init(nBlockProc, nDimMax);
  FluidInterface.BlockMax_BD.init(nBlockProc, nDimMax);
  FluidInterface.CellSize_BD.init(nBlockProc, nDimMax);

  int BlockSize_D[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};  

  int iBlock = 0; 
  //cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;   
  double * xMin, *xMax; 
  
  //for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
  for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];
    
    if(!isTrueBlock(node) || !node->block) continue;
    xMin = node->xmin; // The ghost cells are excluded.
    xMax = node->xmax; 

    for(int iDim = 0; iDim < nDimMax; iDim++){
      FluidInterface.BlockMin_BD(iBlock,iDim) = xMin[iDim]; 
      FluidInterface.BlockMax_BD(iBlock,iDim) = xMax[iDim]; 
      FluidInterface.CellSize_BD(iBlock,iDim) = (xMax[iDim] - xMin[iDim])/BlockSize_D[iDim]; 
    }
    iBlock++; 
  }
  
  FluidInterface.nG_D[0] = nI_Gh; 
  FluidInterface.nG_D[1] = nJ_Gh; 
  FluidInterface.nG_D[2] = nK_Gh; 
  
  //------------------------------------------------------------------
  
  FluidInterface.set_State_BGV(nBlockProc, 
			       _BLOCK_CELLS_X_ + 2*nI_Gh + 1, 
			       _BLOCK_CELLS_Y_ + 2*nJ_Gh + 1, 
			       _BLOCK_CELLS_Z_ + 2*nK_Gh + 1, 
			       data, index);
  FirstCouplingOccured=true;
}


bool PIC::CPLR::FLUID::isTrueBlock(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node){

  bool isTrueBlk = true;   
  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) {
    bool BoundaryBlock=false;
    
    for (int iface=0;iface<6;iface++) if (node->GetNeibFace(iface,0,0,PIC::Mesh::mesh)==NULL) {
        //the block is at the domain boundary, and thresefor it is a 'ghost' block that is used to impose the periodic boundary conditions
        BoundaryBlock=true;
        break;
      }
    
    isTrueBlk = !BoundaryBlock;
  }     
  return isTrueBlk;   

}



void PIC::CPLR::FLUID::set_FluidInterface(){
  FluidInterface.writers_init();
}



void PIC::CPLR::FLUID::read_param(){

  ReadParam & readParam = FluidInterface.readParam;
  readParam.set_verbose(PIC::ThisThread==0);

  bool doSaveBinary = true; 

  double *qom; 
  qom = new double[1];
  qom[0] = -100;

  //int *npcelx,*npcely,*npcelz;

  std::string command; 
  while (readParam.get_next_command(command)) {
    if (command == "#SAVEIDL") {
      /*
        1) The command name should be #SAVEPLOT
        2) Each pic region should has its own parameters.

        Example:
        #SAVEIDL
        4                       nPlotFile
        sat_satInputFile.sat  var ascii si        StringPlot
        -1                      DnOutput
        3.0                     DtOutput
        0.                      DxOutput
        rhoS0 rhoS1 rho pxx pxxS0 pxxS1 Ex Ey Ez Bx By Bz
        sat_satInputFile.sat particles0 real4 si   StringPlot
        1                       DnOutput
        -0.05                   DtOutput
        10.          DxOutput: output one particle of every DxOutput particles.
        z=0 var ascii si        StringPlot
        -1                      DnOutput
        3.0                     DtOutput
        0.                      DxOutput
        rhoS0 rhoS1 rho pxx pxxS0 pxxS1 Ex Ey Ez Bx By Bz
        x=0 var ascii pic       StringPlot
        1                       DnOutput
        -0.05                   DtOutput
        0.                      DxOutput
        rhoS0 rhoS1 bx by pxx          PlotVar
        3d all real4  planet    StringPlot
        1                       DnOutput
        -0.05                   DtOutput
        0.                      DxOutput
        cut all real8 si        StringPlot
        1                       DnOutput
        -0.05                   DtOutput
        0                       xMin
        1                       xMax
        2                       yMin
        3                       yMax
        4                       zMin
        5                       zMax
        0.                      DxOutput
        cut particles1 real4 si  StringPlot
        1                       DnOutput
        -0.05                   DtOutput
        0                       xMin
        1                       xMax
        2                       yMin
        3                       yMax
        4                       zMin
        5                       zMax
        1.            DxOutput: output one particle of every DxOutput particles.
        3d particles0 real4 si region0 regioin1  StringPlot
        1                       DnOutput
        -0.05                   DtOutput
        10.          DxOutput: output one particle of every DxOutput particles.


        Note:
        1) Available output variables are listed in EMfields3D.cpp::getVar().
        2) DxOutput is only functional for particles and 3d field output now.
        3) The position for "cut", "x=", "y="... is in BATSRUS coordinate.????
        4) Output variable 'particles' only works for 'cut', '3d' and 'sat'.
        5) If the keyword 'region' exists, only output the specified region,
	otherwise, all regions are saved.
        6) If the PIC box is aligned (doRotate == false) with MHD XYZ
	coordinates, PIC save its node
	location in MHD coordinates, otherwise, PIC save locations in PIC
	coordinates.
      */

      int nPlotFileMax; // nPlotFile <= nPlotFileMax
      string::size_type pos;
      readParam.read_var("nPlotFile", nPlotFileMax);


      int dnOutput;
      double dtOutput, plotDx; 
      std::string plotVar; 

      int nPlotFile = 0;
      for (int iPlot = 0; iPlot < nPlotFileMax; iPlot++) {
        string plotString;
        readParam.read_var("plotString", plotString);

        // Check save this region or not.
        bool doSaveThisRegion;
        doSaveThisRegion = true;
        if (plotString.find("region") != string::npos) {
          doSaveThisRegion =
	    plotString.find((FluidInterface.getsRegion()).c_str()) != string::npos;
        }

        if (doSaveThisRegion) {
          pos = plotString.find_first_not_of(' ');
          if (pos != string::npos)
            plotString.erase(0, pos);
        }

        if (doSaveThisRegion) {
          readParam.read_var("dnSavePlot", dnOutput);
          readParam.read_var("dtSavePlot", dtOutput);
        } else {
          // Is there a better way to skip the unnecessary lines?
          readParam.skip_lines(2);
        }

        // if (plotString.substr(0, 3) == "cut") {
        //   for (int i = 0; i < nDimMax; i++) {
        //     // Always read 3 dimension.
        //     // plotRangeMin_ID should be in normalized BATSRUS unit.
        //     if (doSaveThisRegion) {
        //       readParam.read_var("CoordMin", plotRangeMin_ID(nPlotFile,i));
        //       readParam.read_var("CoordMax", plotRangeMax_ID(nPlotFile,i));
        //     } else {
        //       readParam.skip_lines(2);
        //     }
        //   }
        // }

        if (doSaveThisRegion) {
          readParam.read_var("dxSavePlot", plotDx);
        } else {
          readParam.skip_lines(1);
        }
	
        pos = plotString.find("var");
        if (pos != string::npos) {
          if (doSaveThisRegion) {
            readParam.read_var("plotVar", plotVar);
          } else {
            readParam.skip_lines(1);
          }
        }


        if (doSaveThisRegion){
	  Writer writerTmp(nPlotFile, plotString, 
	   		   dnOutput, dtOutput, 
	   		   plotDx, plotVar);
	  FluidInterface.writer_I.push_back(writerTmp);
          nPlotFile++;
	}

      } // iPlot
    } else if (command == "#SAVEBINARY") {
      readParam.read_var("doSaveBinary", doSaveBinary);
    } else if (command == "#ELECTRON") {
      // iones info comes from BATSRUS
      readParam.read_var("qom", qom[0]);
    } else if (command == "#DISCRETIZATION") {
      //isDisParamSet = true;
      double th,gradRhoRatio,cDiff,ratioDivC2C; 
      readParam.read_var("th", th);
      readParam.read_var("gradRhoRatio", gradRhoRatio);
      readParam.read_var("cDiff", cDiff);
      readParam.read_var("ratioDivC2C", ratioDivC2C);

      if (ratioDivC2C>1.0 || ratioDivC2C<0.0)
	exit(__LINE__,__FILE__,"Error: the range of ratioDivC2C is [0,1]!");

      if (ratioDivC2C>1e-6 && _PIC_STENCIL_NUMBER_!=375)
	exit(__LINE__,__FILE__,"Error: please set the right divE discretization type in input file!");
      PIC::FieldSolver::Electromagnetic::ECSIM::corrCoeff = ratioDivC2C;
      PIC::FieldSolver::Electromagnetic::ECSIM::theta = th;
    } else if (command == "#DIVE") {
      /*
        Examples:
         1) #DIVE
            weight_estimate

         2) #DIVE
            weight          divECleanType
            1              nPower
            1e-8            divECleanTol
            50              divECleanIter

         3) #DIVE
            position_light  divECleanType
            1          nPower
            1e-8            divECleanTol
            50              divECleanIter
            3               nIterNonLinear

         4) #DIVE
            position_all    divECleanType
            1               nPower
            1e-8            divECleanTol
            50              divECleanIter
            3               nIterNonLinear

         5) #DIVE
            position_estimate_phi  divECleanType
            1                      nPower
            1e-2                   divECleanTol
            20                     divECleanIter

            #DIVE
            position_estimate      divECleanType

	  6) #DIVE
	     F                //Turn divEClean off. 

      */
      string divECleanType;
      readParam.read_var("divECleanType",divECleanType);

       if (divECleanType == "F") {
 	 PIC::FieldSolver::Electromagnetic::ECSIM::DoDivECorrection = false; 
       }else{
          PIC::FieldSolver::Electromagnetic::ECSIM::DoDivECorrection = true;
       }
//         if (divECleanType.substr(0, 15) != "weight_estimate" &&
// 	    divECleanType != "position_estimate") {
// 	  readParam.read_var("nPower", nPowerWeight);
// 	  readParam.read_var("divECleanTol", divECleanTol);
// 	  readParam.read_var("divECleanIter", divECleanIter);
// 	  if (divECleanType == "position_light" ||
// 	      divECleanType == "position_all")
// 	    readParam.read_var("nIterNonLinear", nIterNonLinear);
// 	}

// 	if (divECleanType == "weight" ||
// 	    divECleanType.substr(0, 15) == "weight_estimate" ||
// 	    divECleanType == "position_light" ||
// 	    divECleanType == "position_all" ||
// 	    divECleanType == "position_estimate" ||
// 	    divECleanType == "position_estimate_phi") {
// 	  doCleanDivE = true;
// 	  DoCalcRhocDirectly = true;
// 	} else {
// 	  cout << "Error: divECleanType = " << divECleanType
// 	       << " is not recognized!" << endl;
// 	  abort();
// 	}
// 	if (divECleanType == "position_estimate_phi")
// 	  correctionRatio = 0.9;
// 	if (divECleanType == "position_estimate")
// 	  correctionRatio = 0.5;
//       }

    } else if (command == "#RESTART") {
      string restartString;
      readParam.read_var("IsRestart",restartString);
      
      if (restartString == "F") {
        PIC::CPLR::FLUID::IsRestart = false;
      }else{
        PIC::CPLR::FLUID::IsRestart = true;
      }
      
    } else if (command == "#PARTICLES") {
      int nCommand = 3; // Number of commands for each region.
      readParam.skip_lines(nCommand * FluidInterface.getiRegion());

      npcelx = new int[1];
      npcely = new int[1];
      npcelz = new int[1];

      readParam.read_var("npcelx", npcelx[0]);
      readParam.read_var("npcely", npcely[0]);
      readParam.read_var("npcelz", npcelz[0]);


    } else if (command == "#EFIELDSOLVER") {
      readParam.read_var("EFieldTol", EFieldTol);
      readParam.read_var("EFieldIter", EFieldIter);    
    } else if (command == "#TIMESTEPPING") {
      // These three variables are not used so far in MHD-AMPS. 
      bool useSWMFDt, useFixedDt;
      double cflLimit, fixedDt;
      readParam.read_var("useSWMFDt", useSWMFDt);
      if (!useSWMFDt) {
        readParam.read_var("useFixedDt", useFixedDt);
        if (useFixedDt) {
          readParam.read_var("fixedDt", fixedDt); // In SI unit
	  dt = fixedDt*FluidInterface.getSi2NoT(); // Convert to normalized unit
	  
        } else {
          readParam.read_var("CFL", cflLimit);
        }
      }
    }
  }  // while


  FluidInterface.set_doSaveBinary(doSaveBinary);

  int ns = 2; 
  FluidInterface.fixPARAM(qom, npcelx, npcely, npcelz, &ns);

  if(ns !=2 )
    exit(__LINE__,__FILE__,"Error: more than 2 species are not supported so far!");
  
  for(int iSpecies = 0; iSpecies<ns; ++iSpecies){
    // Set the mass and charge for the PIC code. 
    PIC::MolecularData::SetMass(FluidInterface.getMiSpecies(iSpecies)*_AMU_,iSpecies);  
    PIC::MolecularData::SetElectricCharge(FluidInterface.getQiSpecies(iSpecies)*ElectronCharge,iSpecies); 
  }
  
}



void PIC::CPLR::FLUID::find_output_list(const Writer& writerIn, long int & nPointAllProc, 
		      VectorPointList & pointList_II, 
		      std::array<double, 3> & xMin_D,
		      std::array<double, 3> & xMax_D){
  std::string nameSub = "find_output_list";

  /*Something like this. Note: the node shared by two blocks 
    should be count once only !!*/
  
  // int nCells[3]={_BLOCK_CELLS_X_,_BLOCK_CELLS_Y_,_BLOCK_CELLS_Z_};
  int thread,i,j,k,cnt=0;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataBlockAMR *block;
  PIC::Mesh::cDataCornerNode *corner;
  int iBlock=0, iLocalNode=-1;

  double xMinL_I[3] = {PIC::Mesh::mesh->xGlobalMax[0],PIC::Mesh::mesh->xGlobalMax[1],PIC::Mesh::mesh->xGlobalMax[2]};
  double xMaxL_I[3] = {PIC::Mesh::mesh->xGlobalMin[0],PIC::Mesh::mesh->xGlobalMin[1],PIC::Mesh::mesh->xGlobalMin[2]};
  
  for (node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    
    //for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {                                        
    bool isAllocated=false;
    if (node->IsUsedInCalculationFlag==true) {
      iLocalNode++;
      isAllocated=true;
    }
    //cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];      
    //if(!isTrueBlock(node) || !node->block) continue;
    if(!isTrueBlock(node)) continue;
   
    double dx[3];
    for (int idim=0;idim<3;idim++)
      dx[idim]= (node->xmax[idim]-node->xmin[idim])/nCells[idim];
 
    int nI = fabs(node->xmax[0]-PIC::Mesh::mesh->xGlobalMax[0])>PIC::Mesh::mesh->EPS ? _BLOCK_CELLS_X_:_BLOCK_CELLS_X_+1;
    int nJ = fabs(node->xmax[1]-PIC::Mesh::mesh->xGlobalMax[1])>PIC::Mesh::mesh->EPS ? _BLOCK_CELLS_Y_:_BLOCK_CELLS_Y_+1;
    int nK = fabs(node->xmax[2]-PIC::Mesh::mesh->xGlobalMax[2])>PIC::Mesh::mesh->EPS ? _BLOCK_CELLS_Z_:_BLOCK_CELLS_Z_+1;


    for (i=0;i<nI;i++) {
      for (j=0;j<nJ;j++)
        for (k=0;k<nK;k++) {
	  //   corner=block->GetCornerNode(_getCornerNodeLocalNumber(i,j,k));
	  int index_b[3]={i,j,k};
	  double xTemp[3];
	  for (int idim=0;idim<3;idim++) xTemp[idim]=node->xmin[idim]+dx[idim]*index_b[idim];
          //corner->GetX(xTemp);
          int index_G[3];
          double xp,yp,zp;

          GetGlobalCornerIndex(index_G, xTemp, dx, PIC::Mesh::xmin);
          xp = xTemp[0], yp = xTemp[1],zp = xTemp[2];
          
          if (writerIn.is_inside_plot_region(index_G[0], index_G[1], index_G[2], xp, yp, zp)) {
	    int iBlockIn=-2;
	    if (isAllocated) iBlockIn = iLocalNode;
            pointList_II.push_back({{(double)i, (double)j, (double)k,    xp,
                    yp,         zp,         (double)iBlockIn} });          
          
            if (xp < xMinL_I[0])
              xMinL_I[0] = xp;
            if (yp < xMinL_I[1])
              xMinL_I[1] = yp;
            if (zp < xMinL_I[2])
              xMinL_I[2] = zp;
            
            if (xp > xMaxL_I[0])
              xMaxL_I[0] = xp;
            if (yp > xMaxL_I[1])
            xMaxL_I[1] = yp;
            if (zp > xMaxL_I[2])
              xMaxL_I[2] = zp;      

          }

	  // Look for IPIC3D2/src/main/iPic3Dlib.cpp:line1784 for reference.

        }
    }    
  }

  long nPointLocal = pointList_II.size();
  MPI_Allreduce(&nPointLocal, &nPointAllProc, 1, MPI_LONG, MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
  
  // Global min/max
  double xMinG_D[nDimMax], xMaxG_D[nDimMax];
  
  MPI_Allreduce(xMinL_I, xMinG_D, nDimMax, MPI_DOUBLE, MPI_MIN, MPI_GLOBAL_COMMUNICATOR);
  MPI_Allreduce(xMaxL_I, xMaxG_D, nDimMax, MPI_DOUBLE, MPI_MAX, MPI_GLOBAL_COMMUNICATOR);
  
  for(int iDim = 0; iDim<nDimMax; ++iDim){
    xMin_D[iDim] = xMinG_D[iDim];
    xMax_D[iDim] = xMaxG_D[iDim];
  }

}



void PIC::CPLR::FLUID::get_field_var(const VectorPointList & pointList_II,
		   const std::vector<std::string> & sVar_I, 
		   MDArray<double> & var_II){
  std::string nameSub = "get_field_var";
  //std::cout<<nameSub<<" is called"<<std::endl;

  bool isCoord = false;
  int ix_=0, iy_=1, iz_=2,ixx_=3,iyy_=4,izz_=5, iBlk_=6;
  int ix, iy, iz, iBlock; 

  long nPoint = pointList_II.size();
  int nVar = sVar_I.size();   


  for(long iPoint = 0; iPoint<nPoint; ++iPoint){
    ix = pointList_II[iPoint][ix_];
    iy = pointList_II[iPoint][iy_];
    iz = pointList_II[iPoint][iz_];
    iBlock = pointList_II[iPoint][iBlk_];
   
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node=NULL;
    if (iBlock>=0) node=PIC::DomainBlockDecomposition::BlockTable[iBlock];


    double xPoint[3];
    if (iBlock>=0){
      xPoint[0] =node->xmin[0]+(node->xmax[0]-node->xmin[0])/nCells[0]*ix;
      xPoint[1] =node->xmin[1]+(node->xmax[1]-node->xmin[1])/nCells[1]*iy;
      xPoint[2] =node->xmin[2]+(node->xmax[2]-node->xmin[2])/nCells[2]*iz;
    }else{
      xPoint[0]=pointList_II[iPoint][ixx_];
      xPoint[1]=pointList_II[iPoint][iyy_];
      xPoint[2]=pointList_II[iPoint][izz_];
    }

    PIC::InterpolationRoutines::CellCentered::cStencil CenterStencil(false);

    if (node){
      //interpolate the magnetic field from center nodes to particle location                       
       
      
      CenterStencil=*(PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(xPoint,node));
      
      PIC::Mesh::cDataCenterNode *CenterNode= node->block->
        GetCenterNode(PIC::Mesh::mesh->getCenterNodeLocalNumber(ix,iy,iz));
      char * centerDataPtr =  CenterNode->GetAssociatedDataBufferPointer();

      PIC::Mesh::cDataCornerNode *CornerNode= node->block->
	GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(ix,iy,iz));
      char * DataPtr = CornerNode->GetAssociatedDataBufferPointer();

      for(int iVar = 0; iVar < nVar; ++iVar){
	var_II(iPoint, iVar) = get_var(sVar_I[iVar], DataPtr,xPoint,centerDataPtr, isCoord);
      }
    }else{      
      for(int iVar = 0; iVar < nVar; ++iVar){
	var_II(iPoint, iVar) = get_var(sVar_I[iVar], NULL,xPoint, NULL, isCoord);
      }
    }

  }
}



void PIC::CPLR::FLUID::write_output(double timeNow, bool doForceOutput){
  //  fix_plasma_node_boundary();
  timeNow *= FluidInterface.getNo2SiT();
  FluidInterface.writers_write(timeNow, iCycle, doForceOutput, find_output_list,
                               get_field_var);
}

void PIC::CPLR::FLUID::fix_plasma_node_boundary(){
  // Fix the plasma variables (rho, u, p) at the boundary nodes. 
  

  if (_PIC_BC__PERIODIC_MODE_==_PIC_BC__PERIODIC_MODE_ON_) return;
 
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  int iBlock=0;

  //for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*  node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {

  for (int iLocalNode=0;iLocalNode<PIC::DomainBlockDecomposition::nLocalBlocks;iLocalNode++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> * node=PIC::DomainBlockDecomposition::BlockTable[iLocalNode];
      
    if (!node->block) continue;        
    double dx[3];
    double *xminBlock= node->xmin, *xmaxBlock= node->xmax;
       
    for (int idim=0;idim<3;idim++) dx[idim]=(xmaxBlock[idim]-xminBlock[idim])/nCells[idim];
    PIC::Mesh::cDataBlockAMR *block = node->block;
    
    for (int i=0;i<=nCells[0];i++) for (int j=0;j<=nCells[1];j++) for (int k=0;k<=nCells[2];k++) {
              
          double x[3];
          int ind[3]={i,j,k};
             
          for (int idim=0; idim<3; idim++) {
            x[idim]=xminBlock[idim]+ind[idim]*dx[idim];
          }


          if (!PIC::FieldSolver::Electromagnetic::ECSIM::isBoundaryCorner(x,node)) continue;
    
          PIC::Mesh::cDataCornerNode *CornerNode= node->block->GetCornerNode(PIC::Mesh::mesh->getCornerNodeLocalNumber(i,j,k));
          if (CornerNode!=NULL){
            char *  offset=CornerNode->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset;
                
	    for (int spec = 0; spec<PIC::nTotalSpecies; spec++){
	    double * SpeciesData_I = ((double*)offset)+SpeciesDataIndex[0];
	    
	    int tempOffset = 10*spec;
	    double mass = PIC::MolecularData::GetMass(spec)/_AMU_;
	    double charge = PIC::MolecularData::GetElectricCharge(spec)/ElectronCharge;
	    double inVqom = mass/charge; 
	    SpeciesData_I[tempOffset+Rho_] = mass*FluidInterface.getPICRhoNum(iBlock, x[0], x[1], x[2], spec);
	    SpeciesData_I[tempOffset+RhoUx_] = inVqom*FluidInterface.getPICJx(iBlock, x[0], x[1], x[2], spec);
	    SpeciesData_I[tempOffset+RhoUy_] = inVqom*FluidInterface.getPICJy(iBlock, x[0], x[1], x[2], spec);
	    SpeciesData_I[tempOffset+RhoUz_] = inVqom*FluidInterface.getPICJz(iBlock, x[0], x[1], x[2], spec);

	    SpeciesData_I[tempOffset+RhoUxUx_] = inVqom*FluidInterface.getPICPxx(iBlock, x[0], x[1], x[2], spec);
	    SpeciesData_I[tempOffset+RhoUyUy_] = inVqom*FluidInterface.getPICPyy(iBlock, x[0], x[1], x[2], spec);
	    SpeciesData_I[tempOffset+RhoUzUz_] = inVqom*FluidInterface.getPICPzz(iBlock, x[0], x[1], x[2], spec);
	    SpeciesData_I[tempOffset+RhoUxUy_] = inVqom*FluidInterface.getPICPxy(iBlock, x[0], x[1], x[2], spec);
	    SpeciesData_I[tempOffset+RhoUxUz_] = inVqom*FluidInterface.getPICPxz(iBlock, x[0], x[1], x[2], spec);
	    SpeciesData_I[tempOffset+RhoUyUz_] = inVqom*FluidInterface.getPICPyz(iBlock, x[0], x[1], x[2], spec);

	    }
          }
        }// for (int i=iFaceMin_n[iface];i<=iFaceMax_n[iface];i++)...

    iBlock++;
  }


}


bool PIC::CPLR::FLUID::isBoundaryCorner(double *x, double *dx, double * xmin, double * xmax, int minIndex, int maxIndex){

  if ( maxIndex < minIndex)
    exit(__LINE__,__FILE__,"Error: minIndex is greater than maxIndex");
  
  int indexBoundary[3]; //index count from the boundary

  for (int idim=0; idim<3; idim++) {
    indexBoundary[idim]=
      (fabs(x[idim]-xmin[idim])<fabs(x[idim]-xmax[idim])?
       round((x[idim]-xmin[idim])/dx[idim]):round((xmax[idim]-x[idim])/dx[idim]));
    //minus value means outside the domain
    //positive value inside
    for (int idx=minIndex;idx<=maxIndex; idx++){
      if (indexBoundary[idim]==idx) return true;
    }
  }

  return false;
}


double PIC::CPLR::FLUID::get_var(std::string var, 
				 char * DataPtr, double * x,
                                 char * centerDataPtr,
				 bool isCoord){
  // Something like this. May be iBlock index is needed.
  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;
  double value = 0; 
  int ns = PIC::CPLR::FLUID::FluidInterface.get_nS();
 

   if (var.substr(0, 1) == "X") {
     value = x[0] + PIC::CPLR::FLUID::FluidInterface.getFluidStartX();
     return value;
   } else if (var.substr(0, 1) == "Y") {
    value = x[1] + PIC::CPLR::FLUID::FluidInterface.getFluidStartY();
    return value;
   } else if (var.substr(0, 1) == "Z") {
    value = x[2] + PIC::CPLR::FLUID::FluidInterface.getFluidStartZ();      
    return value;
  }
   

   if (!DataPtr) return value;


   if (var.substr(0, 2) == "Ex") {
     double  *  currE=(double *)(DataPtr+
                                PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+CurrentEOffset);
     value = currE[0];
     
    //value = Ex[i][j][k];
  } else if (var.substr(0, 2) == "Ey") {
    //value = Ey[i][j][k];
    double  *  currE=(double *)(DataPtr+
                                PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+CurrentEOffset);
    value = currE[1];
   
    
  } else if (var.substr(0, 2) == "Ez") {
    //value = Ez[i][j][k];
    double  *  currE=(double *)(DataPtr+
                                PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+CurrentEOffset);
    value = currE[2];


  } else if (var.substr(0, 2) == "Bx") {

    //value =  CellInterpolatedVar("Bx", centerStencilPtr);
    double  *  currB=(double *)(DataPtr+
                                PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner+CurrentBOffset);
    value = currB[0];


    //value = Bxn[i][j][k];
  } else if (var.substr(0, 2) == "By") {    
     
     double  *  currB=(double *)(DataPtr+
                                 PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner+CurrentBOffset);
     value = currB[1];
    
     //value = CellInterpolatedVar("By", centerStencilPtr);
     //value = Byn[i][j][k];
  } else if (var.substr(0, 2) == "Bz") {
    
    double  *  currB=(double *)(DataPtr+
                                PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset+OffsetB_corner+CurrentBOffset);
    value = currB[2];


    //value = CellInterpolatedVar("Bz", centerStencilPtr);

    //value = Bzn[i][j][k];
  } else if (var.substr(0, 4) == "rhoc") {
    // rhocS0, rhocS1... rhoc


  } else if (var.substr(0, 3) == "rho") {
    // "rho", "rhoS0", "rhoS1"...
    if (var.size() == 3) {
      value = 0;
      for (int is = 0; is < ns; is++) {        
        // value += CellInterpolatedVar("Rho",centerStencilPtr,is)
        //  /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(is));
        value += GetCornerVar("Rho",DataPtr,is);
      
      }
    } else {
      string::size_type pos;
      stringstream ss;
      int is;
      pos = var.find_first_of("0123456789");
      ss << var.substr(pos);
      ss >> is;
      if (is >= ns) {
        value = 0;
      } else {
        //value = CellInterpolatedVar("Rho",centerStencilPtr,is)
        //  /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(is));
        //       printf("x:%e,%e,%e, Rho:%e\n",x[0],x[1],x[2],value);
        
        value = GetCornerVar("Rho",DataPtr,is);
      }
    }
  }else if (var.substr(0, 2) == "pS") {
    // pS0, pS1...
    bool isFluidP = true;
    string::size_type pos;
    stringstream ss;
    int is;
    pos = var.find_first_of("0123456789");
    ss << var.substr(pos);
    ss >> is;
    if (is >= ns) {
      value = 0;
    } else {
      //value=(CellInterpolatedVar("Pxx", centerStencilPtr,is)
      //      +CellInterpolatedVar("Pyy", centerStencilPtr,is)
      //      +CellInterpolatedVar("Pzz", centerStencilPtr,is))
      //       /3.0;
      value = (GetCornerVar("Pxx",DataPtr,is)
               +GetCornerVar("Pyy",DataPtr,is)
               +GetCornerVar("Pzz",DataPtr,is))
        /3.0;

    }
  } else if (var.substr(0, 3) == "pXX") {
    bool isFluidP;
    isFluidP = var.substr(0, 1) == "p";
    if (var.size() == 3) {
      value = 0;
      for (int is = 0; is < ns; is++) {
        // value += CellInterpolatedVar("Pxx", centerStencilPtr,is);
        value += GetCornerVar("Pxx",DataPtr,is);
      }
    } else {
      string::size_type pos;
      stringstream ss;
      int is;
      pos = var.find_first_of("0123456789");
      ss << var.substr(pos);
      ss >> is;
      if (is >= ns) {
        value = 0;
      } else {
        //value = CellInterpolatedVar("Pxx", centerStencilPtr,is);
        value = GetCornerVar("Pxx",DataPtr,is);
      }
    }
  } else if (var.substr(0, 3) == "pYY") {
    bool isFluidP;
    isFluidP = var.substr(0, 1) == "p";
    if (var.size() == 3) {
      value = 0;
      for (int is = 0; is < ns; is++) {
        //value += CellInterpolatedVar("Pyy", centerStencilPtr,is);
        value += GetCornerVar("Pyy",DataPtr,is);
      }
    } else {
      string::size_type pos;
      stringstream ss;
      int is;
      pos = var.find_first_of("0123456789");
      ss << var.substr(pos);
      ss >> is;
      if (is >= ns) {
        value = 0;
      } else {
        //value = CellInterpolatedVar("Pyy", centerStencilPtr,is);
        value = GetCornerVar("Pyy",DataPtr,is);
      }
    }
  } else if (var.substr(0, 3) == "pZZ") {
    bool isFluidP;
    isFluidP = var.substr(0, 1) == "p";
    if (var.size() == 3) {
      value = 0;
      for (int is = 0; is < ns; is++) {
        //value += CellInterpolatedVar("Pzz", centerStencilPtr,is);
        value += GetCornerVar("Pzz",DataPtr,is);
      }
    } else {
      string::size_type pos;
      stringstream ss;
      int is;
      pos = var.find_first_of("0123456789");
      ss << var.substr(pos);
      ss >> is;
      if (is >= ns) {
        value = 0;
      } else {
        //value = CellInterpolatedVar("Pzz", centerStencilPtr,is);
        value = GetCornerVar("Pzz",DataPtr,is);
      }
    }
  } else if (var.substr(0, 3) == "pXY") {
    bool isFluidP;
    isFluidP = var.substr(0, 1) == "p";
    if (var.size() == 3) {
      value = 0;
      for (int is = 0; is < ns; is++) {
        //value += CellInterpolatedVar("Pxy", centerStencilPtr,is);
        value +=GetCornerVar("Pxy",DataPtr,is);
      }
    } else {
      string::size_type pos;
      stringstream ss;
      int is;
      pos = var.find_first_of("0123456789");
      ss << var.substr(pos);
      ss >> is;
      if (is >= ns) {
        value = 0;
      } else {
        //value = CellInterpolatedVar("Pxy", centerStencilPtr,is);
        value =GetCornerVar("Pxy",DataPtr,is);
      }
    }
  } else if (var.substr(0, 3) == "pYZ") {
    bool isFluidP;
    isFluidP = var.substr(0, 1) == "p";
    if (var.size() == 3) {
      value = 0;
      for (int is = 0; is < ns; is++) {
        //value += CellInterpolatedVar("Pyz", centerStencilPtr,is);
        value +=GetCornerVar("Pyz",DataPtr,is);
      }
    } else {
      string::size_type pos;
      stringstream ss;
      int is;
      pos = var.find_first_of("0123456789");
      ss << var.substr(pos);
      ss >> is;
      if (is >= ns) {
        value = 0;
      } else {
        //value = CellInterpolatedVar("Pyz", centerStencilPtr,is);
        value =GetCornerVar("Pyz",DataPtr,is);
      }
    }
  } else if (var.substr(0, 3) == "pXZ") {
    bool isFluidP;
    isFluidP = var.substr(0, 1) == "p";
    if (var.size() == 3) {
      value = 0;
      for (int is = 0; is < ns; is++) {
        //value += CellInterpolatedVar("Pxz", centerStencilPtr,is);
        value +=GetCornerVar("Pxz",DataPtr,is);
      }
    } else {
      string::size_type pos;
      stringstream ss;
      int is;
      pos = var.find_first_of("0123456789");
      ss << var.substr(pos);
      ss >> is;
      if (is >= ns) {
        value = 0;
      } else {
        //value = CellInterpolatedVar("Pxz", centerStencilPtr,is);
        value +=GetCornerVar("Pxz",DataPtr,is);
      }
    }
  } else if (var.substr(0, 2) == "ux") {
    string::size_type pos;
    stringstream ss;
    int is;
    pos = var.find_first_of("0123456789");
    ss << var.substr(pos);
    ss >> is;
    value = 0;
    if (is < ns) {
      // value = CellInterpolatedVar("Ux", centerStencilPtr,is);
      value =GetCornerVar("Ux",DataPtr,is);
    }
  } else if (var.substr(0, 2) == "uy") {
    string::size_type pos;
    stringstream ss;
    int is;
    pos = var.find_first_of("0123456789");
    ss << var.substr(pos);
    ss >> is;
    value = 0;
    if (is < ns) {
      //value = CellInterpolatedVar("Uy", centerStencilPtr,is);     
      value =GetCornerVar("Uy",DataPtr,is);
  
    }
  } else if (var.substr(0, 2) == "uz") {
    string::size_type pos;
    stringstream ss;
    int is;
    pos = var.find_first_of("0123456789");
    ss << var.substr(pos);
    ss >> is;
    value = 0;
    if (is < ns) {
      value =GetCornerVar("Uz",DataPtr,is);
      //value = CellInterpolatedVar("Uz", centerStencilPtr,is);     
    }
   }else if (var.substr(0, 5) == "divEc") {
    value = ((double *)(centerDataPtr+   
                        PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset))[divEIndex];    
   }else if (var.substr(0, 2) == "qc") {
    value = 0.51*((double *)(centerDataPtr+   
                        PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset))[netChargeNewIndex]+
      0.49*((double *)(centerDataPtr+   
                       PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset))[netChargeOldIndex];
      
   }else if (var.substr(0, 3) == "phi") {
    value = ((double *)(centerDataPtr+   
                        PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset))[phiIndex];    
   } else {
    value = 0;
   }
  return value;

  //~/SWMF/PC/IPIC3D2/src/fields/EMfields3D.cpp
}



double PIC::CPLR::FLUID::CellInterpolatedVar(std::string var,PIC::InterpolationRoutines::CellCentered::cStencil * centerStencilPtr){

  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

  double value =0;
  if (var.substr(0, 2) == "Bx"){    

    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double * B_ptr = (double *)(centerStencilPtr->cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset);
      value+=centerStencilPtr->Weight[iStencil]*B_ptr[0];      
    }

  }else if (var.substr(0, 2) == "By"){

    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double * B_ptr = (double *)(centerStencilPtr->cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset);
      value+=centerStencilPtr->Weight[iStencil]*B_ptr[1];      
    } 

  }else if (var.substr(0, 2) == "Bz"){

    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double * B_ptr = (double *)(centerStencilPtr->cell[iStencil]->GetAssociatedDataBufferPointer()+PIC::CPLR::DATAFILE::Offset::MagneticField.RelativeOffset+CurrentBOffset);
      value+=centerStencilPtr->Weight[iStencil]*B_ptr[2];      
    } 

  }

  return value;

}

double PIC::CPLR::FLUID::CellInterpolatedVar(std::string var,PIC::InterpolationRoutines::CellCentered::cStencil * centerStencilPtr,int iSp){

  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

  double value =0;
  if (var.substr(0, 3) == "Rho"){    
    //number density
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      value += centerStencilPtr->Weight[iStencil]*
        centerStencilPtr->cell[iStencil]->GetDatumAverage(PIC::Mesh::DatumNumberDensity, iSp);
      double xTemp[3];
      centerStencilPtr->cell[iStencil]->GetX(xTemp);
    }
  }else if (var.substr(0, 2) == "Ux"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3];
      centerStencilPtr->cell[iStencil]->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      value += centerStencilPtr->Weight[iStencil]*vTemp[0];
    }
  }else if (var.substr(0, 2) == "Uy"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3];
      centerStencilPtr->cell[iStencil]->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      value += centerStencilPtr->Weight[iStencil]*vTemp[1];
    }
  }else if (var.substr(0, 2) == "Uz"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3];
      centerStencilPtr->cell[iStencil]->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      value += centerStencilPtr->Weight[iStencil]*vTemp[2];
    }
  }else if (var.substr(0, 3) == "Pxx"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2Temp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2,v2Temp,iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2Temp[0]-vTemp[0]*vTemp[0]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pyy"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2Temp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2,v2Temp,iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2Temp[1]-vTemp[1]*vTemp[1]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pzz"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2Temp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2,v2Temp,iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2Temp[2]-vTemp[2]*vTemp[2]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pxy"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2TensorTemp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2Tensor, v2TensorTemp, iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2TensorTemp[0]-vTemp[0]*vTemp[1]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pyz"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2TensorTemp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2Tensor, v2TensorTemp, iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2TensorTemp[1]-vTemp[1]*vTemp[2]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }else if (var.substr(0, 3) == "Pxz"){
    for (int iStencil=0;iStencil<centerStencilPtr->Length;iStencil++) {
      double vTemp[3],v2TensorTemp[3];
      PIC::Mesh::cDataCenterNode *cell= centerStencilPtr->cell[iStencil]; 
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity,vTemp,iSp);
      cell->GetDatumAverage(PIC::Mesh::DatumParticleVelocity2Tensor, v2TensorTemp, iSp);
      value += centerStencilPtr->Weight[iStencil]*(v2TensorTemp[2]-vTemp[0]*vTemp[2]);
    }
    value *= CellInterpolatedVar("Rho",centerStencilPtr,iSp)
      /fabs(PIC::CPLR::FLUID::FluidInterface.get_qom(iSp));
  }

  return value;
}


double PIC::CPLR::FLUID::GetCornerVar(std::string var,char * DataPtr,int iSp){

  using namespace PIC::FieldSolver::Electromagnetic::ECSIM;

  double value =0;
  if (var.substr(0, 3) == "Rho"){    
    value =((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+Rho_];
  }else if (var.substr(0, 2) == "Ux"){
    double rhoTemp= ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+Rho_];

    if (rhoTemp==0) value=0.0;
    else{
      value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
	[SpeciesDataIndex[iSp]+RhoUx_];
      value /= rhoTemp;
    }

  }else if (var.substr(0, 2) == "Mx"){
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUx_];
  }else if (var.substr(0, 2) == "Uy"){
    double rhoTemp= ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+Rho_];
    
    if (rhoTemp==0) value=0.0;
    else{
      value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
	[SpeciesDataIndex[iSp]+RhoUy_];
      value /= rhoTemp;
    }
  }else if (var.substr(0, 2) == "My"){
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUy_];
  }else if (var.substr(0, 2) == "Uz"){
    double rhoTemp= ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+Rho_];
    
    if (rhoTemp==0) value=0.0;
    else{
      value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
	[SpeciesDataIndex[iSp]+RhoUz_];
      value /= rhoTemp;
    }

  }else if (var.substr(0, 2) == "Mz"){
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUz_];
  }else if (var.substr(0, 3) == "Pxx"){
    double uTemp = GetCornerVar("Ux",DataPtr,iSp);
    double rhoTemp = GetCornerVar("Rho",DataPtr,iSp);
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUxUx_] - rhoTemp*uTemp*uTemp;
  }else if (var.substr(0, 3) == "Pyy"){
    double uTemp = GetCornerVar("Uy",DataPtr,iSp);
    double rhoTemp = GetCornerVar("Rho",DataPtr,iSp);
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUyUy_] - rhoTemp*uTemp*uTemp;
  }else if (var.substr(0, 3) == "Pzz"){
    double uTemp = GetCornerVar("Uz",DataPtr,iSp);
    double rhoTemp = GetCornerVar("Rho",DataPtr,iSp);
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUzUz_] - rhoTemp*uTemp*uTemp;
  }else if (var.substr(0, 3) == "Pxy"){
    double uTemp = GetCornerVar("Uy",DataPtr,iSp);    
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUxUy_] - 
      ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUx_]*uTemp;


  }else if (var.substr(0, 3) == "Pyz"){
    double uTemp = GetCornerVar("Uz",DataPtr,iSp);    
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUyUz_] - 
      ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUy_]*uTemp;

  }else if (var.substr(0, 3) == "Pxz"){
    double uTemp = GetCornerVar("Uz",DataPtr,iSp);    
    value = ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUxUz_] - 
      ((double *)(DataPtr+PIC::CPLR::DATAFILE::Offset::ElectricField.RelativeOffset))
      [SpeciesDataIndex[iSp]+RhoUx_]*uTemp;
  }

  return value;
}


void PIC::CPLR::FLUID::GetGlobalCornerIndex(int * index ,double * x, double * dx, double * xmin){
  index[0] = round((x[0]-xmin[0])/dx[0]);
  index[1] = round((x[1]-xmin[1])/dx[1]);
  index[2] = round((x[2]-xmin[2])/dx[2]);
}

void PIC::CPLR::FLUID::check_max_mem_usage(string tag) {
  double memLocal = read_mem_usage();
  double memMax = memLocal; 

  MPI_Allreduce(&memLocal, &memMax, 1, MPI_DOUBLE, MPI_MAX, MPI_GLOBAL_COMMUNICATOR);
  if (fabs(memLocal - memMax) < 1e-6) {
    cout << tag << " Maximum memory usage = " << memLocal
	 << "MB on thread = " << PIC::ThisThread << endl;
  } 
}

double PIC::CPLR::FLUID::read_mem_usage(){  
  // This function returns the resident set size (RSS) of
  // this processor in unit MB. This function works for most
  // Linux distributions, but not Mac OS X. 
  
  // From wiki: 
  // RSS is the portion of memory occupied by a process that is 
  // held in main memory (RAM).

  double rssMB = 0.0;                              

  ifstream stat_stream("/proc/self/stat", ios_base::in);

  if (!stat_stream.fail()) {
    // Dummy vars for leading entries in stat that we don't care about         
    string pid, comm, state, ppid, pgrp, session, tty_nr;
    string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    string utime, stime, cutime, cstime, priority, nice;
    string O, itrealvalue, starttime;
                            
    // Two values we want                                                       
    unsigned long vsize;                
    unsigned long rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >>
      tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >>
      stime >> cutime >> cstime >> priority >> nice >> O >> itrealvalue >> 
      starttime >> vsize >> rss; // Ignore the rest       
    stat_stream.close(); 

    rssMB = rss*sysconf(_SC_PAGE_SIZE)/1024.0/1024.0;
  }

  return rssMB; 
}

