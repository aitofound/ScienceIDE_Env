//$Id$
//the restart procedures of AMPS

#include "pic.h"

int PIC::Restart::ParticleRestartAutosaveIterationInterval=1;
char PIC::Restart::SamplingData::RestartFileName[_MAX_STRING_LENGTH_PIC_]="SampleData.restart";

char PIC::Restart::saveParticleDataRestartFileName[200]="ParticleData.restart";
char PIC::Restart::recoverParticleDataRestartFileName[_MAX_STRING_LENGTH_PIC_]="ParticleData.restart";
bool PIC::Restart::ParticleDataRestartFileOverwriteMode=true;

int PIC::Restart::SamplingData::minReadFileNumber=-1;
int PIC::Restart::SamplingData::maxReadFileNumber=-1;
PIC::Restart::SamplingData::fDataRecoveryManager PIC::Restart::SamplingData::DataRecoveryManager=NULL;
bool PIC::Restart::SamplingData::PreplotRecoveredData=false;

//additional used-defined data saved in the restart file
PIC::Restart::fUserAdditionalRestartData PIC::Restart::UserAdditionalRestartDataSave=NULL;
PIC::Restart::fUserAdditionalRestartData PIC::Restart::UserAdditionalRestartDataRead=NULL;

char PIC::Restart::UserAdditionalRestartDataCompletedMarker[PIC::Restart::UserAdditionalRestartDataCompletedMarkerLength]="PARTICLE-RESTART-FILE--END-USER-ADDITIONAL";

//flag: read particle restart file when run as a component of the SWMF
bool PIC::Restart::LoadRestartSWMF=false;

//-------------------------------------- Set user function for saving.reading additional restart data ---------------------------------
void PIC::Restart::SetUserAdditionalRestartData(PIC::Restart::fUserAdditionalRestartData fRead,PIC::Restart::fUserAdditionalRestartData fSave) {
  UserAdditionalRestartDataSave=fSave;
  UserAdditionalRestartDataRead=fRead;
}

//-------------------------------------- Save/Load Sampling Data Restart File ---------------------------------------------------------
//save sampling data
void PIC::Restart::SamplingData::Save(const char* fname) {
  FILE *fRestart=NULL;
  int BlockDataSize=0,mpiBufferSize=0;


  //determine the size of the data vector associated with a block
  BlockDataSize=_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*PIC::Mesh::sampleSetDataLength;
  mpiBufferSize=max((int)10.0*BlockDataSize,(int)10000000);

  //init the MPI channel and open the restart file
  char fname_full[500];  
  
  sprintf(fname_full,"%s.thread=%i.tmp",fname,PIC::ThisThread);
  fRestart=fopen(fname_full,"w");

  if (fRestart==NULL) exit(__LINE__,__FILE__,"Error: cannot open file");

  if (PIC::ThisThread==0) {
    if (fwrite(&PIC::LastSampleLength,sizeof(PIC::LastSampleLength),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
    if (fwrite(&PIC::DataOutputFileNumber,sizeof(PIC::DataOutputFileNumber),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 

    if (fwrite(&PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,sizeof(PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
    if (fwrite(&PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,sizeof(PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
  }
  
  //save the restart information
  SaveBlock(PIC::Mesh::mesh->rootTree,fRestart);


  fclose (fRestart);
  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

  //combine separated restart files in a single files 
  if (ThisThread==0) {
    std::ofstream  dst(fname,   std::ios::binary);
  
    for (int thread=0;thread<PIC::nTotalThreads;thread++) {
      sprintf(fname_full,"%s.thread=%i.tmp",fname,thread);

      std::ifstream  src(fname_full, std::ios::binary);
      dst<<src.rdbuf();

      src.close();
      remove(fname_full); 
    }

    dst.close();
  }
  
  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
}

void PIC::Restart::SamplingData::SaveBlock(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,FILE* fRestart) {
  int nAllocatedCells,nAllocatedCorners,i,j,k;

  //save the data
  if (node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    if (node->Thread==PIC::ThisThread) {
      int LocalCellNumber, LocalCornerNumber;
      PIC::Mesh::cDataCenterNode *cell;
      PIC::Mesh::cDataCornerNode *corner;
      char* SamplingData;

      //Calculate the number of the allocated cells in the block and send it to the root processor
      nAllocatedCells=0;
      nAllocatedCorners=0;
      
      if (node->block!=NULL) for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
        cell=node->block->GetCenterNode(LocalCellNumber);              

        if (cell!=NULL) nAllocatedCells++;
      }
        
      if (node->block!=NULL) for (i=0;i<_BLOCK_CELLS_X_+1;i++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (k=0;k<_BLOCK_CELLS_Z_+1;k++) {
        LocalCornerNumber=_getCornerNodeLocalNumber(i,j,k);
        corner=node->block->GetCornerNode(LocalCornerNumber);              

        if (corner!=NULL) nAllocatedCorners++;
      }
      

      if (fwrite(&node->AMRnodeID,sizeof(cAMRnodeID),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
      if (fwrite(&nAllocatedCells,sizeof(int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
      if (fwrite(&nAllocatedCorners,sizeof(int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed");      

      //save the sampling data
      if (node->block!=NULL) for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
        LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
        cell=node->block->GetCenterNode(LocalCellNumber);

        if (cell==NULL) continue;

        SamplingData=cell->GetAssociatedDataBufferPointer();
        if (fwrite(SamplingData,sizeof(char),PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,fRestart)!=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
      }
        
      if (node->block!=NULL) for (i=0;i<_BLOCK_CELLS_X_+1;i++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (k=0;k<_BLOCK_CELLS_Z_+1;k++) {
        LocalCornerNumber=_getCornerNodeLocalNumber(i,j,k);
        corner=node->block->GetCornerNode(LocalCornerNumber);
              
        if (corner==NULL) continue;
             
        SamplingData=corner->GetAssociatedDataBufferPointer();
        if (fwrite(SamplingData,sizeof(char),PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,fRestart)!=PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength) exit(__LINE__,__FILE__,"Error: fwrite has failed");  
      }
    }
  }
  else {
    for (int nDownNode=0;nDownNode<(1<<3);nDownNode++) if (node->downNode[nDownNode]!=NULL) SamplingData::SaveBlock(node->downNode[nDownNode],fRestart);
  }
}

void PIC::Restart::SamplingData::Read(const char* fname) {
  FILE *fRestart=NULL;

  fRestart=fopen(fname,"r");

  if (fRestart==NULL) {
    char msg[200];

    sprintf(msg,"Error: restart file %s is not found",fname);
    exit(__LINE__,__FILE__,msg);
  }

  if (fread(&PIC::LastSampleLength,sizeof(PIC::LastSampleLength),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread has failed"); 
  if (fread(&PIC::DataOutputFileNumber,sizeof(PIC::DataOutputFileNumber),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread has failed"); 

  int t;

  if (fread(&t,sizeof(int),1, fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread has failed"); 
  if (t!=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength) exit(__LINE__,__FILE__,"Error: PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength has changed");

  if (fread(&t,sizeof(int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread has failed"); 
  if (t!=PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength) exit(__LINE__,__FILE__,"Error: PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength has changed");

  SamplingData::ReadBlock(fRestart);
  fclose(fRestart);

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

  //reset the sampling buffer
  PIC::CollectingSampleCounter=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (!block) continue;
    for (int k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (int j=0;j<_BLOCK_CELLS_Y_;j++) {
        for (int i=0;i<_BLOCK_CELLS_X_;i++) {
          int LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
          PIC::Mesh::flushCollectingSamplingBuffer(block->GetCenterNode(LocalCellNumber));
        }

        if (DIM==1) break;
      }

      if ((DIM==1)||(DIM==2)) break;
    }
  }

}

void PIC::Restart::SamplingData::ReadBlock(FILE* fRestart) {
  int nAllocatedCells, nAllocatedCorners;
  cAMRnodeID NodeId;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node;


  while (feof(fRestart)==0) {
    if (fread(&NodeId,sizeof(cAMRnodeID),1,fRestart)==0) return;
    if (fread(&nAllocatedCells,sizeof(int),1,fRestart)==0) return;
    if (fread(&nAllocatedCorners,sizeof(int),1,fRestart)==0) return;

    Node=PIC::Mesh::mesh->findAMRnodeWithID(NodeId);

    if (Node->Thread!=PIC::ThisThread) {
      if (fseek(fRestart,PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength*nAllocatedCells,SEEK_CUR)!=0) return;
      if (fseek(fRestart,PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength*nAllocatedCorners,SEEK_CUR)!=0) return;
 
      continue;
    }

    //read the data for this block
    int i,j,k,LocalCellNumber, LocalCornerNumber;
    PIC::Mesh::cDataCenterNode *cell;
    PIC::Mesh::cDataCornerNode *corner;
    char* SamplingData;

    for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
      cell=Node->block->GetCenterNode(LocalCellNumber);
            
      if (cell!=NULL) {
        SamplingData=cell->GetAssociatedDataBufferPointer();
        if (fread(SamplingData,sizeof(char),PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength,fRestart)!=PIC::Mesh::cDataCenterNode_static_data::totalAssociatedDataLength) exit(__LINE__,__FILE__,"Error: fread failed");
      }
    }
      
    for (i=0;i<_BLOCK_CELLS_X_+1;i++) for (j=0;j<_BLOCK_CELLS_Y_+1;j++) for (k=0;k<_BLOCK_CELLS_Z_+1;k++) {
      LocalCornerNumber=_getCornerNodeLocalNumber(i,j,k);
      corner=Node->block->GetCornerNode(LocalCornerNumber);
            
      if (corner!=NULL) {
        SamplingData=corner->GetAssociatedDataBufferPointer();
        if (fread(SamplingData,sizeof(char),PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength,fRestart)!=PIC::Mesh::cDataCornerNode_static_data::totalAssociatedDataLength) exit(__LINE__,__FILE__,"Error: fread failed");
      }
    }
  }
}

//-------------------------------------- Save/Load Particle Data Restart File ---------------------------------------------------------
void PIC::Restart::SaveParticleData(const char* fname) {
  FILE *fRestart=NULL;
  int nSavedParticles,nTotalSavedParticles,nSavedBlocks=0,nTotalSavedBlocks,nSavedBytes=0,nTotalSavedBytes;

  //open the restart file
  char fname_full[300];

  sprintf(fname_full,"%s.thread=%i.tmp",fname,PIC::ThisThread);
  fRestart=fopen(fname_full,"w");

  //call the user-defined function for saving additional data into the restart file
  if (PIC::ThisThread==0) {
    if (UserAdditionalRestartDataSave!=NULL)  UserAdditionalRestartDataSave(fRestart);

    if (fwrite(UserAdditionalRestartDataCompletedMarker,sizeof(char),UserAdditionalRestartDataCompletedMarkerLength,fRestart)!=UserAdditionalRestartDataCompletedMarkerLength) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
    nSavedBytes+=sizeof(char)*UserAdditionalRestartDataCompletedMarkerLength;

    if (fwrite(&PIC::ParticleBuffer::ParticleDataLength,sizeof(long int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed");  
    nSavedBytes+=sizeof(long int);

    //save the particle weight table
    if (_SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_) {
      if (fwrite(PIC::ParticleWeightTimeStep::GlobalParticleWeight,sizeof(double),_TOTAL_SPECIES_NUMBER_,fRestart)!=_TOTAL_SPECIES_NUMBER_) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
      nSavedBytes+=sizeof(double)*_TOTAL_SPECIES_NUMBER_;
    }
    else {
      exit(__LINE__,__FILE__,"Error: not implemented");
    } 
  }

  //save the restart information
  nSavedParticles=SaveParticleDataBlock(PIC::Mesh::mesh->rootTree,fRestart,nSavedBlocks,nSavedBytes);
  fclose(fRestart);

  //combine the multiple files in a single restart file
  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  MPI_Reduce(&nSavedParticles,&nTotalSavedParticles,1,MPI_INT,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Reduce(&nSavedBlocks,&nTotalSavedBlocks,1,MPI_INT,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);
  MPI_Reduce(&nSavedBytes,&nTotalSavedBytes,1,MPI_INT,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);

  if (ThisThread==0) {
    std::ofstream  dst(fname,   std::ios::binary);

    printf("$PREFIX: Saving restart file: particles saved - %i; blocks - %i; bytes - %i\n", nTotalSavedParticles,nTotalSavedBlocks,nTotalSavedBytes);

    for (int thread=0;thread<PIC::nTotalThreads;thread++) {
      sprintf(fname_full,"%s.thread=%i.tmp",fname,thread);

      std::ifstream  src(fname_full, std::ios::binary);
      dst<<src.rdbuf();

      src.close();
      remove(fname_full);
    }

    dst.close();
  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread==0) printf("$PREFIX: Saving particle restart file: %s\n",fname);
  GetParticleDataCheckSum();
}

int PIC::Restart::SaveParticleDataBlock(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,FILE* fRestart,int &nSavedBlocks,int &nSavedBytes) {
  int res=0;

  //save the data
  if (node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    if (node->Thread==PIC::ThisThread) {
      int i,j,k;
      long int ptr;

      //determine the number of the model particle in the each cell of the block
      int nTotalParticleNumber=0;
      int ParticleNumberTable[_BLOCK_CELLS_X_][_BLOCK_CELLS_Y_][_BLOCK_CELLS_Z_];
      long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];

      if (node->block!=NULL) {
        nSavedBlocks++;
        memcpy(FirstCellParticleTable,node->block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

        for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
          ParticleNumberTable[i][j][k]=0;
          ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

          while (ptr!=-1) {
            ParticleNumberTable[i][j][k]++;
            nTotalParticleNumber++;
            ptr=PIC::ParticleBuffer::GetNext(ptr);
          }
        }

        //save the particle number into the restart file
        if (nTotalParticleNumber!=0) {
          if (fwrite(&node->AMRnodeID,sizeof(cAMRnodeID),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
          nSavedBytes+=sizeof(cAMRnodeID); 

          if (fwrite(&nTotalParticleNumber,sizeof(int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
          nSavedBytes+=sizeof(int);

          if (fwrite(&ParticleNumberTable[0][0][0],sizeof(int),_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_,fRestart)!=_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
	  nSavedBytes+=sizeof(int)*_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;

          //save the particle data into the restart file
          //IMPORTANT: save the partilce data in the reverse order so, when thay are read back from the restart file they are in the seme order as
          //in the memory before saving --> the checksum of the particle data is the same
          for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
            char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];
            int n;

            for (n=0;n<ParticleNumberTable[i][j][k];n++) {
              if (n==0) {
                ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
                for (int t=0;t<ParticleNumberTable[i][j][k]-1;t++) ptr=PIC::ParticleBuffer::GetNext(ptr);
              }
              else ptr=PIC::ParticleBuffer::GetPrev(ptr);

              memcpy(tempParticleData,PIC::ParticleBuffer::GetParticleDataPointer(ptr),PIC::ParticleBuffer::ParticleDataLength);
              if (fwrite(tempParticleData,sizeof(char),PIC::ParticleBuffer::ParticleDataLength,fRestart)!=PIC::ParticleBuffer::ParticleDataLength) exit(__LINE__,__FILE__,"Error: fwrite has failed"); 
	      nSavedBytes+=sizeof(char)*PIC::ParticleBuffer::ParticleDataLength;

              ++res;
            }
          }
        }
      }
    }
  }
  else {
    for (int nDownNode=0;nDownNode<(1<<3);nDownNode++) if (node->downNode[nDownNode]!=NULL) res+=SaveParticleDataBlock(node->downNode[nDownNode],fRestart,nSavedBlocks,nSavedBytes);
  }

  return res;
}


long int PIC::Restart::GetRestartFileParticleNumber(const char *fname) {
  long int res=0;
  FILE* fRestart;

  fRestart=fopen(fname,"r");

  if (UserAdditionalRestartDataRead!=NULL) UserAdditionalRestartDataRead(fRestart);

  char msg[UserAdditionalRestartDataCompletedMarkerLength];
  if (fread(msg,sizeof(char),UserAdditionalRestartDataCompletedMarkerLength,fRestart)!=UserAdditionalRestartDataCompletedMarkerLength) exit(__LINE__,__FILE__,"Error: fread failed"); 

  long int t;
  if (fread(&t,sizeof(long int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 

  //read the particle weight table
  if (_SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_) {
    double tt[_TOTAL_SPECIES_NUMBER_];

    if (fread(tt,sizeof(double),_TOTAL_SPECIES_NUMBER_,fRestart)!=_TOTAL_SPECIES_NUMBER_) exit(__LINE__,__FILE__,"Error: fread failed"); 
  }
  else {
    exit(__LINE__,__FILE__,"Error: not implemented");
  }
  

  while (feof(fRestart)==0) {
    int nTotalParticleNumber=0;

    fseek(fRestart,sizeof(cAMRnodeID),SEEK_CUR);
    if (fread(&nTotalParticleNumber,sizeof(int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 

    res+=nTotalParticleNumber;

    if (nTotalParticleNumber!=0) {
      //skip the data for this block
      fseek(fRestart,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(int)+
        nTotalParticleNumber*PIC::ParticleBuffer::ParticleDataLength*sizeof(char),SEEK_CUR);
    }
  }

  fclose(fRestart);

  return res;
}

int PIC::Restart::ReadParticleDataBlock(FILE* fRestart,int &nReadBlocks,int &nReadBytes,int st_size) {
  int res=0;


  //read the data
  while ((!feof(fRestart))&&(nReadBytes<st_size)) {
    int nTotalParticleNumber=0;
    int ParticleNumberTable[_BLOCK_CELLS_X_][_BLOCK_CELLS_Y_][_BLOCK_CELLS_Z_];
    long int *FirstCellParticleTable;
    cAMRnodeID NodeId;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node;

    if (fread(&NodeId,sizeof(cAMRnodeID),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
    nReadBytes+=sizeof(cAMRnodeID);

    if (fread(&nTotalParticleNumber,sizeof(int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
    nReadBytes+=sizeof(int);

    if (nTotalParticleNumber!=0) {
      Node=PIC::Mesh::mesh->findAMRnodeWithID(NodeId);

      if (Node->Thread!=PIC::ThisThread) {
        //skip the data for this block
	nReadBytes+=_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(int)+
	  nTotalParticleNumber*PIC::ParticleBuffer::ParticleDataLength*sizeof(char); 

        if (fseek(fRestart,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(int)+
          nTotalParticleNumber*PIC::ParticleBuffer::ParticleDataLength*sizeof(char),SEEK_CUR)!=0) return res;
      }
      else {
        //read the data for this block
        nReadBlocks++;	
	
        if (fread(&ParticleNumberTable[0][0][0],sizeof(int),_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_,fRestart)!=_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_) exit(__LINE__,__FILE__,"Error: file reading error");
        FirstCellParticleTable=Node->block->FirstCellParticleTable;
	nReadBytes+=_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(int);

        int i,j,k,np;
        char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];
        long int ptr;

        //block is allocated -> march through the cells and save them into the restart file
        for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
          for (np=0;np<ParticleNumberTable[i][j][k];np++) {
            if (fread(tempParticleData,sizeof(char),PIC::ParticleBuffer::ParticleDataLength,fRestart)!=PIC::ParticleBuffer::ParticleDataLength) exit(__LINE__,__FILE__,"Error: file reading error");
            ptr=PIC::ParticleBuffer::GetNewParticle(FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)]);
            res++; 
	    nReadBytes+=PIC::ParticleBuffer::ParticleDataLength;

            PIC::ParticleBuffer::CloneParticle((PIC::ParticleBuffer::byte*) PIC::ParticleBuffer::GetParticleDataPointer(ptr),(PIC::ParticleBuffer::byte*) tempParticleData);

            //in case when particle tracking is on -> apply the particle tracking conditions if needed
            if (_PIC_PARTICLE_TRACKER_MODE_ ==_PIC_MODE_ON_) {
              PIC::ParticleBuffer::byte *ParticleData;
              PIC::ParticleTracker::cParticleData *DataRecord;

              ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
              DataRecord=(PIC::ParticleTracker::cParticleData*)(PIC::ParticleTracker::ParticleDataRecordOffset+(PIC::ParticleBuffer::byte*)ParticleData);
              DataRecord->TrajectoryTrackingFlag=false;


              if (_PIC_PARTICLE_TRACKER__RESTART_LOADED_PARTICLES__APPLY_TRACKING_CONDITION_MODE_==_PIC_MODE_ON_) {
                //apply the particle tracking condition if needed
                double x[3],v[3];
                int spec;

                PIC::ParticleBuffer::GetX(x,ParticleData);
                PIC::ParticleBuffer::GetV(v,ParticleData);
                spec=PIC::ParticleBuffer::GetI(ParticleData);

                PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,(void *)ParticleData,(void*)Node);
              }

            }
          }
        }

      }
    }
  }

  return res;
}


void PIC::Restart::ReadParticleData(const char* fname) {
  FILE *fRestart=NULL;
  struct stat st;
  int nReadBlocks=0,nReadBytes=0;


  if (stat(fname, &st) != 0) {
    exit(__LINE__,__FILE__,"Error getting file info");
  }

  fRestart=fopen(fname,"r");

  if (fRestart==NULL) {
    char msg[200];

    sprintf(msg,"Error: restart file %s is not found",fname);
    exit(__LINE__,__FILE__,msg);
  }

  //call the user function for recovering the additional user information
  if (UserAdditionalRestartDataRead!=NULL) UserAdditionalRestartDataRead(fRestart);

  //read the end-of-the-user-data-marker
  char msg[UserAdditionalRestartDataCompletedMarkerLength];
  if (fread(msg,sizeof(char),UserAdditionalRestartDataCompletedMarkerLength,fRestart)!=UserAdditionalRestartDataCompletedMarkerLength) exit(__LINE__,__FILE__,"Error: fread failed"); 
  nReadBytes+=sizeof(char)*UserAdditionalRestartDataCompletedMarkerLength;

  long int t;
  if (fread(&t,sizeof(long int),1,fRestart)!=1) exit(__LINE__,__FILE__,"Error: fread failed"); 
  nReadBytes+=sizeof(long int);
  if (t!=PIC::ParticleBuffer::ParticleDataLength) exit(__LINE__,__FILE__,"Error: the value of the PIC::ParticleBuffer::ParticleDataLength haschanged");

  //save the particle weight table
  if (_SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_) {
    if (fread(PIC::ParticleWeightTimeStep::GlobalParticleWeight,sizeof(double),_TOTAL_SPECIES_NUMBER_,fRestart)!=_TOTAL_SPECIES_NUMBER_) exit(__LINE__,__FILE__,"Error: fread failed"); 
    nReadBytes+=sizeof(double)*_TOTAL_SPECIES_NUMBER_;
  }
  else {
    exit(__LINE__,__FILE__,"Error: not implemented");
  }

  if (memcmp(msg,UserAdditionalRestartDataCompletedMarker,sizeof(char)*UserAdditionalRestartDataCompletedMarkerLength)!=0) {
    exit(__LINE__,__FILE__,"Error: the end-of-the additional used data in the input file is mislocated. Something wrong with the user-defined additional restart data save/read procedures.");
  }

  int nLoadedParticles,nTotalLoadedParticles;

  nLoadedParticles=ReadParticleDataBlock(fRestart,nReadBlocks,nReadBytes,st.st_size);
  fclose(fRestart);

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  MPI_Reduce(&nLoadedParticles,&nTotalLoadedParticles,1,MPI_INT,MPI_SUM,0,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread==0) {
    printf("$PREFIX: Reading particle restart file: %s\n",fname);
    printf("$PREFIX: Reading particle restart file: the total number of locaded particles is %i\n",nTotalLoadedParticles);
  } 

  GetParticleDataCheckSum();
}

//calculate the check sum of the particle data
//-------------------------------------- Calculate the checlsum of the particle buffer ------------------------------------------------
unsigned long PIC::Restart::GetParticleDataCheckSum() {
  CRC32 CheckSum;

  //the thread number that was processed last: the checkSum object is sent from PrevNodeThread directly to node->Thread
  //at the begining of the calculation CheckSum is located on the root thread
  int PrevNodeThread=0;

  GetParticleDataBlockCheckSum(PIC::Mesh::mesh->rootTree,&CheckSum,PrevNodeThread);
  MPI_Bcast(&CheckSum,sizeof(CRC32),MPI_CHAR,PrevNodeThread,MPI_GLOBAL_COMMUNICATOR);

  if (PIC::ThisThread==0) {
    printf("$PREFIX:The particle data CRC32 checksum=0x%lx (%i@%s):\n",CheckSum.checksum(),__LINE__,__FILE__);
  }

  MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  return CheckSum.checksum();
}


void PIC::Restart::GetParticleDataBlockCheckSum(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node,CRC32* CheckSum,int &PrevNodeThread) {
  //save the data
  if (node->lastBranchFlag()==_BOTTOM_BRANCH_TREE_) {
    //recieve the CheckSum object

    if ((node->Thread==PIC::ThisThread)||(PIC::ThisThread==PrevNodeThread)) {
      if (node->Thread!=PrevNodeThread) {
        if (PIC::ThisThread==PrevNodeThread) {
          //send CheckSum to node->Thread
          MPI_Send(CheckSum,sizeof(CRC32),MPI_CHAR,node->Thread,0,MPI_GLOBAL_COMMUNICATOR);
        }
        else {
          //recieve CheckSum from PrevNodeThread
          MPI_Status status;
          MPI_Recv(CheckSum,sizeof(CRC32),MPI_CHAR,PrevNodeThread,0,MPI_GLOBAL_COMMUNICATOR,&status);

        }
      }
    }

    PrevNodeThread=node->Thread;

    //calculate the chekc sum
    if (node->Thread==PIC::ThisThread) {
      int i,j,k;
      long int ptr;
      long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_];
      char tempParticleData[PIC::ParticleBuffer::ParticleDataLength];

      if (node->block!=NULL) {
        memcpy(FirstCellParticleTable,node->block->FirstCellParticleTable,_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_*sizeof(long int));

        for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
          ptr=FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

          while (ptr!=-1) {
            memcpy(tempParticleData,PIC::ParticleBuffer::GetParticleDataPointer(ptr),PIC::ParticleBuffer::ParticleDataLength);

            PIC::ParticleBuffer::GetParticleSignature(ptr,CheckSum,false);

     //       CheckSum->add(tempParticleData,sizeof(unsigned char));
     //       CheckSum->add(tempParticleData+sizeof(unsigned char)+2*sizeof(long int),PIC::ParticleBuffer::ParticleDataLength-sizeof(unsigned char)-2*sizeof(long int)); 

            ptr=PIC::ParticleBuffer::GetNext(ptr);
          }
        }
      }

    }
  }
  else {
    for (int nDownNode=0;nDownNode<(1<<3);nDownNode++) if (node->downNode[nDownNode]!=NULL) GetParticleDataBlockCheckSum(node->downNode[nDownNode],CheckSum,PrevNodeThread);
  }
}








