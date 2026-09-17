/*
This is a function meant to sample to Parallel velocity of neutral hydrogen along a LOS

Inputs: XYZ of a star in question
Outputs: A data file with the Parallel components of Velocity of hydrogen along the line of sight 

structure:
    Who knows yet


Plan for improvements:
1. Get it to compile using the Moon_Sample_Veloc... as a template
   a. I will be using a generic Coordinate in XYZ to as the target at first
  
2. Make it  use multiple XYZ coordinates in a text file perhaphs
3. Be able to give it an RA, Dec and have it translate to XYZ


*/




#include "pic.h"



OH::Sampling::LymanAlpha::cVelocitySampleBuffer *OH::Sampling::LymanAlpha::SampleBuffer=NULL;
OH::Sampling::LymanAlpha::cVelocitySampleBuffer *OH::Sampling::LymanAlpha::CountBuffer=NULL;
int OH::Sampling::LymanAlpha::nZenithPoints=0;
//double OH::Sampling::LymanAlpha::LymanAlphaSampleDirectionTable[3*OH::Sampling::LymanAlpha::LymanAlphaSampleDirectionTableLength];

double OH::Sampling::LymanAlpha::LymanAlphaSampleDirectionTable[]={0.0};
double OH::Sampling::LymanAlpha::LymanAlphaSampleStartLocation[3]={0.0};


void OH::Sampling::LymanAlpha::Init() {


  //This is written for a single direction  (X,Y,Z) = (1,1,1)
  SampleBuffer=new cVelocitySampleBuffer[LymanAlphaSampleDirectionTableLength];
  CountBuffer=new cVelocitySampleBuffer[LymanAlphaSampleDirectionTableLength];
  
  for (int i=0;i<LymanAlphaSampleDirectionTableLength;i++) {
    SampleBuffer[i].lGSE[0]=LymanAlphaSampleDirectionTable[0+3*i];
    SampleBuffer[i].lGSE[1]=LymanAlphaSampleDirectionTable[1+3*i];
    SampleBuffer[i].lGSE[2]=LymanAlphaSampleDirectionTable[2+3*i];

    Vector3D::Normalize(SampleBuffer[i].lGSE);

    CountBuffer[i].lGSE[0]=LymanAlphaSampleDirectionTable[0+3*i];
    CountBuffer[i].lGSE[1]=LymanAlphaSampleDirectionTable[1+3*i];
    CountBuffer[i].lGSE[2]=LymanAlphaSampleDirectionTable[2+3*i];

    Vector3D::Normalize(CountBuffer[i].lGSE);

  }
}

void OH::Sampling::LymanAlpha::Sampling() {


  //Initializing the varibles needed
  int Direction_i;
  int idim;
  double dl;
  //the ratio between the step of the integration procedure and the local cell size
  static const double IntegrationStep2CellSizeRatio=0.3;

  //initalizing the coordinates needed in the loop
  double startCoord[3]={0,0,0};

  //Adjust this for the coordinate that you need it to be
  double *l;
  
  //Leaving this as a for loop because I would like to implement multiple directions 
  for (Direction_i=0;Direction_i<LymanAlphaSampleDirectionTableLength;Direction_i++) {

    //temporary sampling buffer and the buffer for the particle data
    cVelocitySampleBuffer *tempSamplingBuffer=SampleBuffer+Direction_i;

    //temporary sampling buffer and the buffer for the particle data
    cVelocitySampleBuffer *tempCountingBuffer=CountBuffer+Direction_i;
 
    
    //determine the limits of integration and initalize the nodes
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* Node=NULL;
    
    l=tempSamplingBuffer->lGSE;
    memcpy(startCoord,LymanAlphaSampleStartLocation,3*sizeof(double));

    Node=PIC::Mesh::mesh->findTreeNode(startCoord); 

    while (Node!=NULL) {
      dl=IntegrationStep2CellSizeRatio*Node->GetCharacteristicCellSize();

      if (Node->Thread==PIC::ThisThread) {
        //find the cell for sampling
        int i,j,k;
        long int ncell,ptr;
        PIC::ParticleBuffer::byte *ParticleData;
        int spec;
        double *v,*x,LocalParticleWeight,VelocityLineOfSight,HeliocentricRadialVelocity,c,cellMeasure,rHeliocentric,Speed;

        ncell=PIC::Mesh::mesh->FindCellIndex(startCoord,i,j,k,Node);
        cellMeasure=Node->block->GetCenterNode(ncell)->Measure;

        //sample particles
        ptr=Node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

	//cycle through the particles
        while (ptr!=-1) {
	  //Get the paricle data
          ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

          spec=PIC::ParticleBuffer::GetI(ParticleData);
          v=PIC::ParticleBuffer::GetV(ParticleData);
          x=PIC::ParticleBuffer::GetX(ParticleData);

          LocalParticleWeight=Node->block->GetLocalParticleWeight(spec)/cellMeasure;
          LocalParticleWeight*=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

          //calculate velocity component in the direction of the line of sight
	  //rx*vx + ry*vy + rz*vz = v||r
          VelocityLineOfSight=Vector3D::DotProduct(v,l);

          i=(int)((VelocityLineOfSight+maxVelocityLimit)/VelocityBinWidth);
          if ((i>=0.0)&&(i<nVelocitySamplePoints)) {
	    tempSamplingBuffer->VelocityLineOfSight[spec][i]+=LocalParticleWeight*dl;
	    tempCountingBuffer->VelocityLineOfSight[spec][i]+=1;
	      
	      }

	  //Next particle
          ptr=PIC::ParticleBuffer::GetNext(ParticleData);
        }
      }   

      //next position along the line of sight
      for (idim=0;idim<3;idim++) startCoord[idim]+=dl*l[idim];

      Node=PIC::Mesh::mesh->findTreeNode(startCoord,Node);
    } 
  }
}

void OH::Sampling::LymanAlpha::OutputSampledData(int DataOutputFileNumber) {
  char fname[_MAX_STRING_LENGTH_PIC_];
  int iZone,s,i,cnt;
  FILE *fout=NULL;
  char fCountname[_MAX_STRING_LENGTH_PIC_];
  FILE *fCountout=NULL;

  
  const int mpiZoneExchangeBufferLength=PIC::nTotalSpecies*(nVelocitySamplePoints);
  double mpiZoneExchangeBuffer[mpiZoneExchangeBufferLength];

  const int mpiZoneExchangeCountBufferLength=PIC::nTotalSpecies*(nVelocitySamplePoints);
  double mpiZoneExchangeCountBuffer[mpiZoneExchangeBufferLength];

 
  if (PIC::ThisThread==0) {
    sprintf(fname,"%s/pic.LymanAlpha_Parr_Velocity_Dist.out=%i.dat",PIC::OutputDataFileDirectory,DataOutputFileNumber);
    fout=fopen(fname,"w");
    fprintf(fout,"VARIABLES=\"v\"");

    for (s=0;s<PIC::nTotalSpecies;s++) fprintf(fout,", \"f Velocity along the Line of Sight (%s) (in respect to the SUN)\" ",
					       PIC::MolecularData::GetChemSymbol(s));
    fprintf(fout,"\n");


    sprintf(fCountname,"%s/pic.LymanAlpha_Count_Dist.out=%i.dat",PIC::OutputDataFileDirectory,DataOutputFileNumber);
    fCountout=fopen(fCountname,"w");
    fprintf(fCountout,"VARIABLES=\"v\"");

    for (s=0;s<PIC::nTotalSpecies;s++) fprintf(fCountout,", \"f Velocity along the Line of Sight (%s) (in respect to the SUN)\" ",
					       PIC::MolecularData::GetChemSymbol(s));
    fprintf(fCountout,"\n");

    
  }



  for (iZone=0;iZone<LymanAlphaSampleDirectionTableLength;iZone++) {
    double *XYZ; 
   
    XYZ= SampleBuffer[iZone].lGSE;
    
    if (PIC::ThisThread==0) {
      fprintf(fout,"ZONE T=\"X=%e , Y=%e , Z=%e \",I=%i\n",XYZ[0],XYZ[1],XYZ[2],nVelocitySamplePoints);
      fprintf(fCountout,"ZONE T=\"X=%e , Y=%e , Z=%e \",I=%i\n",XYZ[0],XYZ[1],XYZ[2],nVelocitySamplePoints);
    }

    //combine all sampled data on the root processor and print them into a file
    for (cnt=0,s=0;s<PIC::nTotalSpecies;s++) {
      for (i=0;i<nVelocitySamplePoints;i++) {
        mpiZoneExchangeBuffer[cnt]=SampleBuffer[iZone].VelocityLineOfSight[s][i];
	mpiZoneExchangeCountBuffer[cnt]=CountBuffer[iZone].VelocityLineOfSight[s][i];
        cnt++;	
      }
    }


    if (PIC::ThisThread==0) {
      int thread;

      for (thread=1;thread<PIC::nTotalThreads;thread++) {
        MPI_Status status;

        MPI_Recv(mpiZoneExchangeBuffer,mpiZoneExchangeBufferLength,MPI_DOUBLE,thread,0,MPI_GLOBAL_COMMUNICATOR,&status);
        MPI_Recv(mpiZoneExchangeCountBuffer,mpiZoneExchangeCountBufferLength,MPI_DOUBLE,thread,0,MPI_GLOBAL_COMMUNICATOR,&status);

        for (cnt=0,s=0;s<PIC::nTotalSpecies;s++) {
          for (i=0;i<nVelocitySamplePoints;i++) {
            SampleBuffer[iZone].VelocityLineOfSight[s][i]+=mpiZoneExchangeBuffer[cnt];
            CountBuffer[iZone].VelocityLineOfSight[s][i]+=mpiZoneExchangeCountBuffer[cnt];
            cnt++;
          }
        }
      }

      //normalize the distribution function
      /*
      for (s=0;s<PIC::nTotalSpecies;s++) {
        double w;

        for (w=0.0,i=0;i<nVelocitySamplePoints;i++) w+=SampleBuffer[iZone].VelocityLineOfSight[s][i];
        if (w>0.0) for (i=0;i<nVelocitySamplePoints;i++) SampleBuffer[iZone].VelocityLineOfSight[s][i]/=w*VelocityBinWidth;
      }
      */

      //output sample data into a file
      for (i=0;i<nVelocitySamplePoints;i++) {
        fprintf(fout,"%e  ",-maxVelocityLimit+VelocityBinWidth*(i+0.5));
	fprintf(fCountout,"%e  ",-maxVelocityLimit+VelocityBinWidth*(i+0.5));
	
        for (s=0;s<PIC::nTotalSpecies;s++) {
	  fprintf(fout,"  %e  ", SampleBuffer[iZone].VelocityLineOfSight[s][i]);
	  fprintf(fCountout,"  %e  ", CountBuffer[iZone].VelocityLineOfSight[s][i]);

	}
       fprintf(fout,"\n");
       fprintf(fCountout,"\n");
      }
    } 
    else {
      MPI_Send(mpiZoneExchangeBuffer,mpiZoneExchangeBufferLength,MPI_DOUBLE,0,0,MPI_GLOBAL_COMMUNICATOR);
      MPI_Send(mpiZoneExchangeCountBuffer,mpiZoneExchangeCountBufferLength,MPI_DOUBLE,0,0,MPI_GLOBAL_COMMUNICATOR);
    }
    //clear the sampling buffer
    for (s=0;s<PIC::nTotalSpecies;s++) {
      for (i=0;i<nVelocitySamplePoints;i++){
	SampleBuffer[iZone].VelocityLineOfSight[s][i]=0.0;
	CountBuffer[iZone].VelocityLineOfSight[s][i]=0.0;
      }
      }

    MPI_Barrier(MPI_GLOBAL_COMMUNICATOR);
  }


  //close the file
  if (PIC::ThisThread==0) {
    fclose(fout);
    fclose(fCountout);
  }
}


