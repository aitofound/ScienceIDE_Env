#include "radiation.h"


//particle weight == energy carried by a model particle [eV]


long int Radiation::PhotonFreqOffset=-1;
int Radiation::MaterialTemperatureOffset=-1;

int Radiation::AbsorptionCounterOffset=-1;
int Radiation::EmissionCounterOffset=-1;


void Radiation::ProcessCenterNodeAssociatedData(char *TargetBlockAssociatedData,char *SourceBlockAssociatedData) {
  double t;

  t=*(double*)(AbsorptionCounterOffset+SourceBlockAssociatedData);
  *(double*)(AbsorptionCounterOffset+TargetBlockAssociatedData)+=t;

  t=*(double*)(EmissionCounterOffset+SourceBlockAssociatedData);
  *(double*)(EmissionCounterOffset+TargetBlockAssociatedData)+=t;

  t=*(double*)(MaterialTemperatureOffset+SourceBlockAssociatedData); 
  *(double*)(MaterialTemperatureOffset+TargetBlockAssociatedData)=t;
}



void Radiation::ClearCellCounters() {
  int i,j,k;
  PIC::Mesh::cDataCenterNode *cell;

  for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=(thread==PIC::Mesh::mesh->ThisThread) ? PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread] : PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];

    if (node==NULL) continue;

    for (;node!=NULL;node=node->nextNodeThisThread) {
      PIC::Mesh::cDataBlockAMR *block=node->block;
      if (!block) continue;

      for (int ii=0;ii<_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;ii++) {
        int t=ii;

        k=t/(_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_);
        t=t%(_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_);

        j=t/_BLOCK_CELLS_X_;
        i=t%_BLOCK_CELLS_X_; 

        cell=node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

        *(double*)(AbsorptionCounterOffset+cell->GetAssociatedDataBufferPointer())=0.0;
        *(double*)(EmissionCounterOffset+cell->GetAssociatedDataBufferPointer())=0.0;
      }
    }
  }
}

void Radiation::Emission() {
  int s,i,j,k,idim;
  long int LocalCellNumber,ptr,ptrNext;

  #ifdef __CUDA_ARCH__
  int id=blockIdx.x*blockDim.x+threadIdx.x;
  int increment=gridDim.x*blockDim.x;
  #else
  int id=0,increment=1;
  #endif

  for (int iGlobalCell=id;iGlobalCell<PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;iGlobalCell+=increment) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    PIC::Mesh::cDataBlockAMR *block;

    int ii=iGlobalCell;
    int i,j,k;
    int iNode;
    int t;

    t=_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    iNode=ii/t;
    ii=ii%t;

    t=_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    k=ii/t;
    ii=ii%t;

    j=ii/_BLOCK_CELLS_X_;
    i=ii%_BLOCK_CELLS_X_;

    node=PIC::DomainBlockDecomposition::BlockTable[iNode];

    if (node->block!=NULL) {
      PIC::Mesh::cDataCenterNode *cell=node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
      
      double dU;
                    
      
      double T0=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());
      double sigma=Opasity::GetSigma(T0);

      double alpha,beta,f;
      
      alpha=0.5;
      beta=4*Material::RadiationConstant*pow(T0,3)/(Material::Density*Material::SpecificHeat);
      
      f=1.0/(1+alpha*beta*sigma*SpeedOfLight_cm*PIC::ParticleWeightTimeStep::GlobalTimeStep[0]);
      
      
      
      
      
      double I0=f*Opasity::GetSigma(T0)*Material::RadiationConstant*SpeedOfLight_cm*pow(T0,4); ///(4*Pi);
      
      double ParticleWeight=node->block->GetLocalParticleWeight(0);
      double LocalTimeStep=node->block->GetLocalTimeStep(0);
          
      dU=cell->Measure*I0*LocalTimeStep;
      double anpart=dU/ParticleWeight;  
  
      double WeightCorrectionFactor=1.0;
      double x[3],v[3];
      
      int npart=(int) anpart;
      if (anpart-npart>0.1) if (rnd()<anpart-npart) npart++;
      
      *(double*)(EmissionCounterOffset+cell->GetAssociatedDataBufferPointer())+=npart*ParticleWeight;
      
      for (int ii=0;ii<npart;ii++) {
        x[0]=node->xmin[0]+(node->xmax[0]-node->xmin[0])*(i+rnd())/_BLOCK_CELLS_X_; 
        x[1]=node->xmin[1]+(node->xmax[1]-node->xmin[1])*(j+rnd())/_BLOCK_CELLS_Y_;
        x[2]=node->xmin[2]+(node->xmax[2]-node->xmin[2])*(k+rnd())/_BLOCK_CELLS_Z_;

        Vector3D::Distribution::Uniform(v,SpeedOfLight_cm); 

        ptr=PIC::ParticleBuffer::GetNewParticle(node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],true);

        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(WeightCorrectionFactor,ptr);
        PIC::ParticleBuffer::SetX(x,ptr);
        PIC::ParticleBuffer::SetV(v,ptr);
        PIC::ParticleBuffer::SetI(0,ptr);
      }
      
      
      
      
      ///update material temperature
      double AbsorbedParticleWeight,EmittedParticleWeght;
      
      EmittedParticleWeght=*(double*)(EmissionCounterOffset+cell->GetAssociatedDataBufferPointer());
      AbsorbedParticleWeight=*(double*)(AbsorptionCounterOffset+cell->GetAssociatedDataBufferPointer());
           
      double dT=(AbsorbedParticleWeight-EmittedParticleWeght)/(Material::Density*cell->Measure)/Material::SpecificHeat;
      
      *(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer())+=dT;
      
      
      *(double*)(EmissionCounterOffset+cell->GetAssociatedDataBufferPointer())=0.0;
      *(double*)(AbsorptionCounterOffset+cell->GetAssociatedDataBufferPointer())=0.0;
    }
  }
}
      



void Radiation::Init() {
  //request a place in a particle's tate vector
  PIC::ParticleBuffer::RequestDataStorage(PhotonFreqOffset,sizeof(double));
  PIC::IndividualModelSampling::RequestStaticCellData.push_back(RequestStaticCellData);

  PIC::Parallel::CenterBlockBoundaryNodes::SetActiveFlag(true);
  PIC::Parallel::CenterBlockBoundaryNodes::ProcessCenterNodeAssociatedData=ProcessCenterNodeAssociatedData;
  

  //register output functions
  PIC::Mesh::AddVaraibleListFunction(PrintVariableList);
  PIC::Mesh::PrintDataCenterNode.push_back(PrintData);
  PIC::Mesh::InterpolateCenterNode.push_back(Interpolate);

  //particle interaction with the boundary of the domain
//  PIC::Mover::ProcessOutsideDomainParticles=ProcessParticlesBoundaryIntersection;

  //particle produced in thermal radiation
//  PIC::BC::UserDefinedParticleInjectionFunction=ThermalRadiation::InjectParticles;
}

void Radiation::PrintVariableList(FILE* fout,int DataSetNumber) {
  fprintf(fout,", \"Material Temparature\", \"I\", \"Equilibrium Temepature\"");
}

void Radiation::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode) {
  bool gather_output_data=false;

  if (pipe==NULL) gather_output_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_output_data=true;

  struct cDataExchengeBuffer {
    double MaterialTemparature;
    double I, EquilibriumTemparature;
  } buffer;

  if (gather_output_data==true) {
    buffer.MaterialTemparature=*((double*)(MaterialTemperatureOffset+CenterNode->GetAssociatedDataBufferPointer()));

    buffer.I=CenterNode->GetNumberDensity(0); //////*PIC::ParticleWeightTimeStep::GlobalParticleWeight[0];
    buffer.EquilibriumTemparature=pow(4.0*Pi*buffer.I/Material::RadiationConstant/SpeedOfLight_cm,0.25);  
  } 

  if ((PIC::ThisThread==0)||(pipe==NULL)) { 
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) { 
      pipe->recv((char*)&buffer,sizeof(cDataExchengeBuffer),CenterNodeThread);
    }

    fprintf(fout,"%e  %e  %e ",buffer.MaterialTemparature,buffer.I,buffer.EquilibriumTemparature);
  }
  else {
    pipe->send((char*)&buffer,sizeof(cDataExchengeBuffer));
  }
}

void Radiation::Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode) { 
  int i;
  double c,InterpolatedMaterialTemparature=0.0;

  //interpolate the sampled data
  for (i=0;i<nInterpolationCoeficients;i++) {
    c=InterpolationCoeficients[i];
    InterpolatedMaterialTemparature+=c*(*(double*)(MaterialTemperatureOffset+InterpolationList[i]->GetAssociatedDataBufferPointer()));
  }

  //store interpolated data
  *(double*)(MaterialTemperatureOffset+CenterNode->GetAssociatedDataBufferPointer())=InterpolatedMaterialTemparature; 
}

int Radiation::RequestStaticCellData(int offset) {
  MaterialTemperatureOffset=offset;
  offset+=sizeof(double);
  
  AbsorptionCounterOffset=offset;
  offset+=sizeof(double);
  
  EmissionCounterOffset=offset;
  offset+=sizeof(double);
  
  return offset;
}

//scatter model particles from the boundaries of the computational domain 
int Radiation::ProcessParticlesBoundaryIntersection(long int ptr,double* xInit,double* vInit,int nIntersectionFace,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) { //(long int ptr,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  int res;

  //particles are reflected from all boundaries exept that with face:
  if (nIntersectionFace==0) {
    res=_PARTICLE_DELETED_ON_THE_FACE_;
  }
  else {
    //the particle is reflected back into the domain  
    double v[3];
   
    PIC::ParticleBuffer::GetV(v,ptr);

    switch (nIntersectionFace) {
    case 1:
      v[0]*=-1.0;
      break;
    case 2:case 3:
      v[1]*=-1.0;
      break;
    case 4:case 5:
      v[2]*=-1.0;
    } 

    PIC::ParticleBuffer::SetV(v,ptr);
    res=_PARTICLE_REJECTED_ON_THE_FACE_;
  }

  return res;
}

bool Radiation::Injection::BoundingBoxParticleInjectionIndicator(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) { 
    if (ExternalFaces[0]==true) return true;
  } 

  return false;
}


long int Radiation::Injection::BoundingBoxInjection(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  int nface,idim;
  long int newParticle;
  PIC::ParticleBuffer::byte *newParticleData;
  long int nInjectedParticles=0;
  double c0,c1,TimeCounter,ModelParticlesInjectionRate,ParticleWeight,LocalTimeStep,x[3],v[3],vr,theta;
  double x0[3],e0[3],e1[3];

  if (startNode->Thread!=PIC::ThisThread) return 0;

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    if (ExternalFaces[0]==true) {
      //inject new particles 
      TimeCounter=0.0;
      ModelParticlesInjectionRate;

      ParticleWeight=startNode->block->GetLocalParticleWeight(spec);
      LocalTimeStep=startNode->block->GetLocalTimeStep(spec);

      PIC::Mesh::mesh->GetBlockFaceCoordinateFrame_3D(x0,e0,e1,0,startNode);
   
      
      double Vol=LocalTimeStep*SpeedOfLight_cm*Vector3D::Length(e0)*Vector3D::Length(e1);
      
      
       double T0=1.0; //equilibrium temeprature
       double I0=Opasity::GetSigma(T0)*Material::RadiationConstant*pow(T0,4);
       
//I0/=30;


//       double I0=10* Material::RadiationConstant*SpeedOfLight_cm*pow(T0,4)/(4.0*Pi); 


       double apart=I0*Vol/ParticleWeight;  
       int npart=(int) apart;




double EnergyFlux=Material::RadiationConstant*SpeedOfLight_cm*pow(T0,4)/4*LocalTimeStep*Vector3D::Length(e0)*Vector3D::Length(e1);
      apart=EnergyFlux/ParticleWeight;
      npart=(int) apart;



       
       if (apart-npart>rnd()) npart++;
       
       for (int ii=0;ii<npart;ii++) {

/*
         double dx=-rnd()*LocalTimeStep*SpeedOfLight_cm;
     
         Vector3D::Distribution::Uniform(v,SpeedOfLight_cm);
         
         if (dx+v[0]*LocalTimeStep>0.0) {
        //generate the new particle position on the face
        for (idim=0,c0=rnd(),c1=rnd();idim<DIM;idim++) x[idim]=x0[idim]+c0*e0[idim]+c1*e1[idim];
        
        x[0]=dx+v[0]*LocalTimeStep;
*/
        
        //find the localtion of the particle 
        int i,j,k;
        double vr,phi;

        v[0]=SpeedOfLight_cm*sqrt(rnd());
        vr=sqrt(SpeedOfLight_cm*SpeedOfLight_cm-v[0]*v[0]);
        phi=2.0*Pi*rnd();
        v[1]=vr*sin(phi);
        v[2]=vr*cos(phi);        


v[0]=SpeedOfLight_cm;
v[1]=0.0,v[2]=0.0;

        for (idim=0,c0=rnd(),c1=rnd();idim<DIM;idim++) x[idim]=x0[idim]+c0*e0[idim]+c1*e1[idim];


        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->findTreeNode(x,startNode);
        if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

        //generate a particle
        //newParticle=PIC::ParticleBuffer::GetNewParticle(node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],true); 

        newParticle=PIC::ParticleBuffer::GetNewParticle();


        newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
        nInjectedParticles++;

        //generate particles' velocity
        /*
        double theta=asin(rnd()); 
        v[0]=SpeedOfLight_cm*cos(theta); 
        
     //   v[0]=sqrt(rnd())*SpeedOfLight_cm; 
        vr=sqrt(SpeedOfLight_cm*SpeedOfLight_cm-v[0]*v[0]);

        theta=PiTimes2*rnd(); 
        v[1]=vr*sin(theta); 
        v[2]=vr*cos(theta);
*/

        PIC::ParticleBuffer::SetX(x,newParticleData);
        PIC::ParticleBuffer::SetV(v,newParticleData);
        PIC::ParticleBuffer::SetI(spec,newParticleData);
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,newParticleData);


        Radiation::Mover2(newParticle,rnd()*LocalTimeStep,node);

      }
    }
  } 

  return nInjectedParticles;
}


/*long int Radiation::Injection::BoundingBoxInjection(int spec,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  bool ExternalFaces[6];
  int nface,idim;
  long int newParticle;
  PIC::ParticleBuffer::byte *newParticleData;
  long int nInjectedParticles=0;
  double c0,c1,TimeCounter,ModelParticlesInjectionRate,ParticleWeight,LocalTimeStep,x[3],v[3],vr,theta;
  double x0[3],e0[3],e1[3];

  if (PIC::Mesh::mesh->ExternalBoundaryBlock(startNode,ExternalFaces)==_EXTERNAL_BOUNDARY_BLOCK_) {
    if (ExternalFaces[0]==true) {
      //inject new particles 
      TimeCounter=0.0;
      ModelParticlesInjectionRate;

      ParticleWeight=startNode->block->GetLocalParticleWeight(spec);
      LocalTimeStep=startNode->block->GetLocalTimeStep(spec);

      PIC::Mesh::mesh->GetBlockFaceCoordinateFrame_3D(x0,e0,e1,0,startNode);
   
      ModelParticlesInjectionRate=1.0;//kev* cm^2 /ns
      ModelParticlesInjectionRate*=Vector3D::Length(e0)*Vector3D::Length(e1); //kev/ns 
//      ModelParticlesInjectionRate/=_NANO_; // kev/s 



     double T0=1.0; //equilibrium temeprature
     double I0=Opasity::GetSigma(T0)*Material::RadiationConstant*SpeedOfLight_cm*pow(T0,4)/(4*Pi);
     double ModelParticleDensity=I0/ParticleWeight;  
     double ModelParticleFlux=ModelParticleDensity*SpeedOfLight_cm/4.0;


      ModelParticlesInjectionRate=ModelParticleFlux*Vector3D::Length(e0)*Vector3D::Length(e1);;


      while ((TimeCounter+=-log(rnd())/ModelParticlesInjectionRate)<LocalTimeStep) {
        //generate the new particle position on the face
        for (idim=0,c0=rnd(),c1=rnd();idim<DIM;idim++) x[idim]=x0[idim]+c0*e0[idim]+c1*e1[idim];

        //generate a particle
        newParticle=PIC::ParticleBuffer::GetNewParticle();
        newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);
        nInjectedParticles++;

        //generate particles' velocity
        double theta=asin(rnd()); 
        v[0]=SpeedOfLight_cm*cos(theta); 
        
     //   v[0]=sqrt(rnd())*SpeedOfLight_cm; 
        vr=sqrt(SpeedOfLight_cm*SpeedOfLight_cm-v[0]*v[0]);

        theta=PiTimes2*rnd(); 
        v[1]=vr*sin(theta); 
        v[2]=vr*cos(theta);

        PIC::ParticleBuffer::SetX(x,newParticleData);
        PIC::ParticleBuffer::SetV(v,newParticleData);
        PIC::ParticleBuffer::SetI(spec,newParticleData);
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,newParticleData);

        //inject the particle into the system
        _PIC_PARTICLE_MOVER__MOVE_PARTICLE_TIME_STEP_(newParticle,LocalTimeStep-TimeCounter,startNode);
      }
    }
  } 

  return nInjectedParticles;
}*/

void Radiation::IC::Set() {
  int iNode,i,j,k;
  PIC::Mesh::cDataBlockAMR *block;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
  PIC::Mesh::cDataCenterNode *CenterNode;

  for (int iNode=0;iNode<PIC::DomainBlockDecomposition::nLocalBlocks;iNode++) { 
    node=PIC::DomainBlockDecomposition::BlockTable[iNode];
    block=node->block;

    if (block==NULL) continue;

    for (i=0;i<_BLOCK_CELLS_X_;i++) for (j=0;j<_BLOCK_CELLS_Y_;j++) for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      CenterNode=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
 
      *(double*)(MaterialTemperatureOffset+CenterNode->GetAssociatedDataBufferPointer())=T;
    }
  } 
}




long int Radiation::Injection::BoundingBoxInjection(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode) {
  return BoundingBoxInjection(0,startNode);
}


_TARGET_HOST_ _TARGET_DEVICE_
int Radiation::Mover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double v[3],x[3];
  int idim;
  PIC::ParticleBuffer::byte *ParticleData;
  PIC::Mesh::cDataCenterNode *cell;

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetX(x,ParticleData);
  PIC::ParticleBuffer::GetV(v,ParticleData);

  int i,j,k;
  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");


  static int ncalls=0;
  ncalls++;


if (ncalls==25106) {
double d=33;
d+=33;
}

double xInit[3];
memcpy(xInit,x,3*sizeof(double));

  cell=node->block->GetCenterNode(i,j,k);

  double T0=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());
  double sigma=Opasity::GetSigma(T0);


  while (dtTotal>0.0) {
    //get the time interval to the next event 
    double dt=fabs(log(rnd())/(SpeedOfLight_cm*sigma));

    if (dt<dtTotal) {
      //the particle is absorbed 
      for (idim=0;idim<3;idim++) x[idim]+=dt*v[idim];

      for (idim=0;idim<3;idim++) {
        double delta;

        if (x[idim]<PIC::Mesh::mesh->xGlobalMin[idim]) {
          delta=PIC::Mesh::mesh->xGlobalMin[idim]-x[idim];

          x[idim]=PIC::Mesh::mesh->xGlobalMin[idim]+delta;
          v[idim]*=-1.0;
        }

        if (x[idim]>PIC::Mesh::mesh->xGlobalMax[idim]) {
          delta=x[idim]-PIC::Mesh::mesh->xGlobalMax[idim];

          x[idim]=PIC::Mesh::mesh->xGlobalMax[idim]-delta;
          v[idim]*=-1.0;
        }
      }


      node=PIC::Mesh::mesh->findTreeNode(x,node);

      if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
      cell=node->block->GetCenterNode(i,j,k);

      T0=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());

      double ParticleWeightCorrection=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);
      double GlobalWeight=node->block->GetLocalParticleWeight(0);

      double dT=ParticleWeightCorrection*GlobalWeight/(Material::Density*cell->Measure)/Material::SpecificHeat;
      T0+=dT;

      *(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer())=T0; 

      PIC::ParticleBuffer::DeleteParticle(ptr);
      return _PARTICLE_DELETED_ON_THE_FACE_;
    }
    else {
      //the particle survived  
      for (idim=0;idim<3;idim++) x[idim]+=dtTotal*v[idim];

      if (x[0]<PIC::Mesh::mesh->xGlobalMin[0]) {
        //the particle existed from the domain 
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_DELETED_ON_THE_FACE_;
      }

      //check if the particle ger scattered from the walls 
      for (idim=0;idim<3;idim++) {
        double delta;

        if (x[idim]<PIC::Mesh::mesh->xGlobalMin[idim]) {
          delta=PIC::Mesh::mesh->xGlobalMin[idim]-x[idim];

          x[idim]=PIC::Mesh::mesh->xGlobalMin[idim]+delta;
          v[idim]*=-1.0;
        }

        if (x[idim]>PIC::Mesh::mesh->xGlobalMax[idim]) {
          delta=x[idim]-PIC::Mesh::mesh->xGlobalMax[idim];

          x[idim]=PIC::Mesh::mesh->xGlobalMax[idim]-delta;
          v[idim]*=-1.0;
        }
      }

      //find a new location of the particle and add it to the processed particle list 
      node=PIC::Mesh::mesh->findTreeNode(x,node);

      if (node==NULL) {
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_DELETED_ON_THE_FACE_;
      }

      dtTotal=-1.0;
    } 
  }

  PIC::ParticleBuffer::SetX(x,ParticleData);
  PIC::ParticleBuffer::SetV(v,ParticleData);

  //attach the particle to the temporaty list
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  tempFirstCellParticlePtr=node->block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

  return _PARTICLE_MOTION_FINISHED_;
}

int Radiation::Mover2(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double v[3],x[3];
  int idim;
  PIC::ParticleBuffer::byte *ParticleData;
  PIC::Mesh::cDataCenterNode *cell;

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetX(x,ParticleData);
  PIC::ParticleBuffer::GetV(v,ParticleData);

  int i,j,k;
  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");


  static int ncalls=0;
  ncalls++;


if ((ncalls==463983)||(x[0]>0.03)) {
double d=33;
d+=33;
}

double xInit[3];
memcpy(xInit,x,3*sizeof(double));

  cell=node->block->GetCenterNode(i,j,k);

  double T0=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());
  double sigma=Opasity::GetSigma(T0);

  double alpha,beta,f;
  
  alpha=0.5;
  beta=4*Material::RadiationConstant*pow(T0,3)/(Material::Density*Material::SpecificHeat);
  
  f=1.0/(1+alpha*beta*sigma*SpeedOfLight_cm*dtTotal);

  while (dtTotal>0.0) {
    //get the time interval to the next event 
    double dt=fabs(-log(rnd())/(SpeedOfLight_cm*sigma));

//dt=1000000;

    if (dt<dtTotal) {
      //the particle is absorbed or scatered
      //     for (idim=0;idim<3;idim++) x[idim]+=dt*v[idim];

      if (x[0]<0.0) {
        //the particle left the domain -> remove it
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_DELETED_ON_THE_FACE_;
      }

      double dtmove=dt;
      int iface_last=-1;

      while (dtmove>0.0) {
        int iFaceMinTime=-1;
        double dtFlight,dtFlightmin=-1.0;

        for (int iface=0;iface<6;iface++) if (iface!=iface_last) {
          switch (iface) {
          case 0:
            dtFlight=(v[0]!=0.0) ? (PIC::Mesh::mesh->xGlobalMin[0]-x[0])/v[0] : 1000000;
            break;
          case 1: 
            dtFlight=(v[0]!=0.0) ? (PIC::Mesh::mesh->xGlobalMax[0]-x[0])/v[0] : 1000000;
            break;

          case 2:
            dtFlight=(v[1]!=0.0) ? (PIC::Mesh::mesh->xGlobalMin[1]-x[1])/v[1] : 1000000;
            break;
          case 3:
            dtFlight=(v[1]!=0.0) ? (PIC::Mesh::mesh->xGlobalMax[1]-x[1])/v[1] : 1000000;
            break;

          case 4:
            dtFlight=(v[2]!=0.0) ? (PIC::Mesh::mesh->xGlobalMin[2]-x[2])/v[2] : 1000000;
            break;
          case 5:
            dtFlight=(v[2]!=0.0) ? (PIC::Mesh::mesh->xGlobalMax[2]-x[2])/v[2] : 1000000;
            break;
          }

          if (dtFlight>0.0) {
          if (dtFlightmin<0.0) { 
            dtFlightmin=dtFlight;
            iFaceMinTime=iface;
          } 
          else {
            if (dtFlightmin>dtFlight) {
              dtFlightmin=dtFlight;
              iFaceMinTime=iface;
            }
          }
          }
        }


        if ((dtFlightmin<dtmove)&&(iFaceMinTime==0)) {
          //the particle left the domain -> remove it
          PIC::ParticleBuffer::DeleteParticle(ptr);
          return _PARTICLE_DELETED_ON_THE_FACE_;
        }

        if (dtmove<dtFlightmin) {
          for (idim=0;idim<3;idim++) x[idim]+=dtmove*v[idim];
          dtmove=0.0;
        }
        else {
          for (idim=0;idim<3;idim++) x[idim]+=dtFlightmin*v[idim]; 

          iface_last=iFaceMinTime;

          switch (iFaceMinTime) {
          case 0: case 1:
            v[0]*=-1.0;
            break;
          case 2: case 3:
            v[1]*=-1.0;
            break;
          case 4:case 5:
            v[2]*=-1.0;
          }

          dtmove-=dtFlightmin;
        }
      }


if ((ncalls==463983)||(x[0]>0.03)) {
double d=33;
d+=33;
}



        node=PIC::Mesh::mesh->findTreeNode(x,node);

     
      
      //determine whether the particle is absorbed or scattered
      if (rnd()<(1.0-f)) {
        //the particle is scattered -> get the new velocity direction for the particle
//        Vector3D::Distribution::Uniform(v,SpeedOfLight_cm);
        dtTotal-=dt;
      }
      else {
        //ther particle is absorbed -> increment the counter of absorptions
        
        if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
        cell=node->block->GetCenterNode(i,j,k);
        
        *(double*)(AbsorptionCounterOffset+cell->GetAssociatedDataBufferPointer())+=
            node->block->GetLocalParticleWeight(0)*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ptr);
        
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_DELETED_ON_THE_FACE_;
      }
      
      
//      node=PIC::Mesh::mesh->findTreeNode(x,node);
//
//      if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
//      cell=node->block->GetCenterNode(i,j,k);
//
//      T0=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());
//
//      double ParticleWeightCorrection=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);
//      double GlobalWeight=node->block->GetLocalParticleWeight(0);
//
//      double dT=ParticleWeightCorrection*GlobalWeight/(Material::Density*cell->Measure)/Material::SpecificHeat;
//      T0+=dT;
//
//      *(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer())=T0; 
//
//      PIC::ParticleBuffer::DeleteParticle(ptr);
//      return _PARTICLE_DELETED_ON_THE_FACE_;
    }
    else {
      //the particle survived  
      
      
      double dtmove=dtTotal;
      int iface_last=-1;

      while (dtmove>0.0) {
        int iFaceMinTime=-1;
        double dtFlight,dtFlightmin=-1.0;

        for (int iface=0;iface<6;iface++) if (iface!=iface_last) {
          switch (iface) {
          case 0:
            dtFlight=(v[0]!=0.0) ? (PIC::Mesh::mesh->xGlobalMin[0]-x[0])/v[0] : 1000000;
            break;
          case 1: 
            dtFlight=(v[0]!=0.0) ? (PIC::Mesh::mesh->xGlobalMax[0]-x[0])/v[0] : 1000000;
            break;

          case 2:
            dtFlight=(v[1]!=0.0) ? (PIC::Mesh::mesh->xGlobalMin[1]-x[1])/v[1] : 1000000;
            break;
          case 3:
            dtFlight=(v[1]!=0.0) ? (PIC::Mesh::mesh->xGlobalMax[1]-x[1])/v[1] : 1000000;
            break;

          case 4:
            dtFlight=(v[2]!=0.0) ? (PIC::Mesh::mesh->xGlobalMin[2]-x[2])/v[2] : 1000000;
            break;
          case 5:
            dtFlight=(v[2]!=0.0) ? (PIC::Mesh::mesh->xGlobalMax[2]-x[2])/v[2] : 1000000;
            break;
          }

          if (dtFlight>0.0) {
          if (dtFlightmin<0.0) { 
            dtFlightmin=dtFlight;
            iFaceMinTime=iface;
          } 
          else {
            if (dtFlightmin>dtFlight) {
              dtFlightmin=dtFlight;
              iFaceMinTime=iface;
            }
          }
          }

        }


        if ((dtFlightmin<dtmove)&&(iFaceMinTime==0)) {
          //the particle left the domain -> remove it
          PIC::ParticleBuffer::DeleteParticle(ptr);
          return _PARTICLE_DELETED_ON_THE_FACE_;
        }

        if (dtmove<dtFlightmin) {
          for (idim=0;idim<3;idim++) x[idim]+=dtmove*v[idim];
          dtmove=0.0;
        }
        else {
          for (idim=0;idim<3;idim++) x[idim]+=dtFlightmin*v[idim]; 

iface_last=iFaceMinTime;

          switch (iFaceMinTime) {
          case 0: case 1:
            v[0]*=-1.0;
            break;
          case 2: case 3:
            v[1]*=-1.0;
            break;
          case 4:case 5:
            v[2]*=-1.0;
          }

          dtmove-=dtFlightmin;
        }
      }
      
      
if ((ncalls==463983)||(x[0]>0.03)) {
double d=33;
d+=33;
}      
      
      
      
      
//      for (idim=0;idim<3;idim++) x[idim]+=dtTotal*v[idim];
//
//      if (x[0]<PIC::Mesh::mesh->xGlobalMin[0]) {
//        //the particle existed from the domain 
//        PIC::ParticleBuffer::DeleteParticle(ptr);
//        return _PARTICLE_DELETED_ON_THE_FACE_;
//      }
//
//      //check if the particle ger scattered from the walls 
//      for (idim=0;idim<3;idim++) {
//        double delta;
//
//        if (x[idim]<PIC::Mesh::mesh->xGlobalMin[idim]) {
//          delta=PIC::Mesh::mesh->xGlobalMin[idim]-x[idim];
//
//          x[idim]=PIC::Mesh::mesh->xGlobalMin[idim]+delta;
//          v[idim]*=-1.0;
//        }
//
//        if (x[idim]>PIC::Mesh::mesh->xGlobalMax[idim]) {
//          delta=x[idim]-PIC::Mesh::mesh->xGlobalMax[idim];
//
//          x[idim]=PIC::Mesh::mesh->xGlobalMax[idim]-delta;
//          v[idim]*=-1.0;
//        }
//      }

      //find a new location of the particle and add it to the processed particle list 
      node=PIC::Mesh::mesh->findTreeNode(x,node);

      if (node==NULL) {
        PIC::ParticleBuffer::DeleteParticle(ptr);
        return _PARTICLE_DELETED_ON_THE_FACE_;
      }

      dtTotal=-1.0;
    } 
  }


if ((ncalls==463983)||(x[0]>0.03)) {
double d=33;
d+=33;
}

  PIC::ParticleBuffer::SetX(x,ParticleData);
  PIC::ParticleBuffer::SetV(v,ParticleData);

  //attach the particle to the temporaty list
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  tempFirstCellParticlePtr=node->block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;

  return _PARTICLE_MOTION_FINISHED_;
}


/*
_TARGET_HOST_ _TARGET_DEVICE_
int Radiation::Mover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double v[3],x[3];
  int idim; 
  PIC::ParticleBuffer::byte *ParticleData;

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetX(x,ParticleData);
  PIC::ParticleBuffer::GetV(v,ParticleData);

  for (idim=0;idim<3;idim++) x[idim]+=dtTotal*v[idim];

  if (x[0]<PIC::Mesh::mesh->xGlobalMin[0]) {
    //the particle existed from the domain 
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_; 
  }

  //check if the particle ger scattered from the walls 
  for (idim=0;idim<3;idim++) {
     double delta;

    if (x[idim]<PIC::Mesh::mesh->xGlobalMin[idim]) {
      delta=PIC::Mesh::mesh->xGlobalMin[idim]-x[idim];

      x[idim]=PIC::Mesh::mesh->xGlobalMin[idim]+delta;
      v[idim]*=-1.0;
    }
    
    if (x[idim]>PIC::Mesh::mesh->xGlobalMax[idim]) {
      delta=x[idim]-PIC::Mesh::mesh->xGlobalMax[idim];

      x[idim]=PIC::Mesh::mesh->xGlobalMax[idim]-delta;
      v[idim]*=-1.0;
    }
  }

  //find a new location of the particle and add it to the processed particle list 
  node=PIC::Mesh::mesh->findTreeNode(x,node);

  if (node==NULL) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }

  PIC::ParticleBuffer::SetX(x,ParticleData);
  PIC::ParticleBuffer::SetV(v,ParticleData);

  //attach the particle to the temporaty list
  int i,j,k;
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  PIC::ParticleBuffer::SetPrev(-1,ParticleData);


#ifdef __CUDA_ARCH__
  int tptr=ptr;
  int *source=(int*)(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k));

  long int tempFirstCellParticle=atomicExch(source,tptr);

  if (sizeof(long int )>sizeof(int)) {
    *(source+1)=0;
  }

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,_GetParticleDataPointer(tempFirstCellParticle,data->ParticleDataLength,data->ParticleDataBuffer));
#else

  tempFirstCellParticlePtr=node->block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;
#endif

  return _PARTICLE_MOTION_FINISHED_;
}  


_TARGET_HOST_ _TARGET_DEVICE_
int Radiation::Mover(long int ptr,double dtTotal,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  double v[3],x[3];
  int idim; 
  PIC::ParticleBuffer::byte *ParticleData;

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetX(x,ParticleData);
  PIC::ParticleBuffer::GetV(v,ParticleData);

  for (idim=0;idim<3;idim++) x[idim]+=dtTotal*v[idim];

  if (x[0]<PIC::Mesh::mesh->xGlobalMin[0]) {
    //the particle existed from the domain 
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_; 
  }

  //check if the particle ger scattered from the walls 
  for (idim=0;idim<3;idim++) {
     double delta;

    if (x[idim]<PIC::Mesh::mesh->xGlobalMin[idim]) {
      delta=PIC::Mesh::mesh->xGlobalMin[idim]-x[idim];

      x[idim]=PIC::Mesh::mesh->xGlobalMin[idim]+delta;
      v[idim]*=-1.0;
    }
    
    if (x[idim]>PIC::Mesh::mesh->xGlobalMax[idim]) {
      delta=x[idim]-PIC::Mesh::mesh->xGlobalMax[idim];

      x[idim]=PIC::Mesh::mesh->xGlobalMax[idim]-delta;
      v[idim]*=-1.0;
    }
  }

  //find a new location of the particle and add it to the processed particle list 
  node=PIC::Mesh::mesh->findTreeNode(x,node);

  if (node==NULL) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
    return _PARTICLE_DELETED_ON_THE_FACE_;
  }

  PIC::ParticleBuffer::SetX(x,ParticleData);
  PIC::ParticleBuffer::SetV(v,ParticleData);

  //attach the particle to the temporaty list
  int i,j,k;
  long int tempFirstCellParticle,*tempFirstCellParticlePtr;

  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");

  PIC::ParticleBuffer::SetPrev(-1,ParticleData);


#ifdef __CUDA_ARCH__
  int tptr=ptr;
  int *source=(int*)(block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k));

  long int tempFirstCellParticle=atomicExch(source,tptr);

  if (sizeof(long int )>sizeof(int)) {
    *(source+1)=0;
  }

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,_GetParticleDataPointer(tempFirstCellParticle,data->ParticleDataLength,data->ParticleDataBuffer));
#else

  tempFirstCellParticlePtr=node->block->tempParticleMovingListTable+i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k);
  tempFirstCellParticle=(*tempFirstCellParticlePtr);

  PIC::ParticleBuffer::SetNext(tempFirstCellParticle,ParticleData);
  PIC::ParticleBuffer::SetPrev(-1,ParticleData);

  if (tempFirstCellParticle!=-1) PIC::ParticleBuffer::SetPrev(ptr,tempFirstCellParticle);
  *tempFirstCellParticlePtr=ptr;
#endif

  return _PARTICLE_MOTION_FINISHED_;
}  */

_TARGET_GLOBAL_ 
void Radiation::MoverManagerGPU() {
  int s,i,j,k,idim;
  long int LocalCellNumber,ptr,ptrNext;

//Radiation::ClearCellCounters();

  double dtTotal=PIC::ParticleWeightTimeStep::GlobalTimeStep[0];

  #ifdef __CUDA_ARCH__
  int id=blockIdx.x*blockDim.x+threadIdx.x;
  int increment=gridDim.x*blockDim.x;
  #else
  int id=0,increment=1;
  #endif

  for (int iGlobalCell=id;iGlobalCell<PIC::DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;iGlobalCell+=increment) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;
    PIC::Mesh::cDataBlockAMR *block;

    int ii=iGlobalCell;
    int i,j,k;
    int iNode;
    int t;

    t=_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    iNode=ii/t;
    ii=ii%t;

    t=_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;
    k=ii/t;
    ii=ii%t;

    j=ii/_BLOCK_CELLS_X_;
    i=ii%_BLOCK_CELLS_X_;

    node=PIC::DomainBlockDecomposition::BlockTable[iNode];

    if (node->block!=NULL) {
      long int ptr=node->block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
      long int ptr_next=ptr;

      while (ptr_next!=-1) {
        ptr=ptr_next;
        ptr_next=PIC::ParticleBuffer::GetNext(ptr);

        Radiation::Mover2(ptr,dtTotal,node); 
      }
    }

    #ifdef __CUDA_ARCH__
    __syncwarp;
    #endif
  }


  //update the particle lists 
  if (id==0)   for (int thread=0;thread<PIC::Mesh::mesh->nTotalThreads;thread++) {
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=(thread==PIC::Mesh::mesh->ThisThread) ? PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread] : PIC::Mesh::mesh->DomainBoundaryLayerNodesList[thread];

    if (node==NULL) continue;

    for (;node!=NULL;node=node->nextNodeThisThread) {
      PIC::Mesh::cDataBlockAMR *block=node->block;
      if (!block) continue;

      for (int ii=0;ii<_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_;ii++) {
        block->FirstCellParticleTable[ii]=block->tempParticleMovingListTable[ii];
        block->tempParticleMovingListTable[ii]=-1;
      }
    }
  }




  PIC::Parallel::CenterBlockBoundaryNodes::SetActiveFlag(true);
  PIC::Parallel::BPManager.isCorner = false;
  PIC::Parallel::BPManager.pointBufferSize = 2*sizeof(double);
  PIC::Parallel::BPManager.copy_node_to_buffer = copy_counters_to_buffer;
  PIC::Parallel::BPManager.add_buffer_to_node = add_counters_to_node;

  PIC::Parallel::ProcessBlockBoundaryNodes();
  PIC::Parallel::CenterBlockBoundaryNodes::SetActiveFlag(false);


  PIC::Parallel::ExchangeParticleData();
//  PIC::Parallel::ProcessCenterBlockBoundaryNodes_new();
}
 


void Radiation::copy_counters_to_buffer(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node, const int i, const int j, const int k, char *bufferPtr) {
  char *nodePtr = node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer();
  double SendData[2];

  SendData[0]=*(double*)(AbsorptionCounterOffset+nodePtr);
  SendData[1]=*(double*)(EmissionCounterOffset+nodePtr);

//  memcpy(bufferPtr, nodePtr, 2*sizeof(double));

memcpy(bufferPtr, SendData, 2*sizeof(double));

}


void Radiation::add_counters_to_node(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node, const int i, const int j, const int k, char *bufferPtr, double coef){
  char *nodePtr = node->block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k))->GetAssociatedDataBufferPointer();


  coef=1.0;

  *(double*)(AbsorptionCounterOffset+nodePtr)+=coef*((double*)bufferPtr)[0];
  *(double*)(EmissionCounterOffset+nodePtr)+=coef*((double*)bufferPtr)[1]; 
}




//absorption of radiation 
void Radiation::Absorption(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {



  int i,j,k,spec;
  PIC::Mesh::cDataCenterNode *cell;
  double w,x[3],ParticleWeight,MaterialTemparature;
  double LocalTimeStep,GlobalWeight,ParticleWeightCorrection;

  PIC::ParticleBuffer::byte *ParticleData;

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetX(x,ParticleData);
  spec=PIC::ParticleBuffer::GetI(ParticleData);
  ParticleWeightCorrection=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

  LocalTimeStep=node->block->GetLocalTimeStep(spec);
  GlobalWeight=node->block->GetLocalParticleWeight(spec);

  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
  cell=node->block->GetCenterNode(i,j,k);

double T0=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());

double sigma=Opasity::GetSigma(T0);
double p=exp(-sigma*SpeedOfLight_cm*LocalTimeStep);

if (rnd()<1-p) {
  //the particle was absorbed 
  double dU,dT;


  dU=GlobalWeight;

  dT=dU/(Material::Density*cell->Measure)/Material::SpecificHeat;  
  T0+=dT;
 
  *(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer())=T0;

  //remove the particle from the simulation
  PIC::ParticleBuffer::DeleteParticle(ptr);
}
  else {
    //keep the particle in the simulation 
    PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
    PIC::ParticleBuffer::SetPrev(-1,ptr);

    if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
    FirstParticleCell=ptr;
  }
}

/*
void Radiation::Absorption(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  int i,j,k,spec;
  PIC::Mesh::cDataCenterNode *cell;
  double w,x[3],ParticleWeight,MaterialTemparature;
  double LocalTimeStep,GlobalWeight,ParticleWeightCorrection;

  PIC::ParticleBuffer::byte *ParticleData;

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetX(x,ParticleData);
  spec=PIC::ParticleBuffer::GetI(ParticleData);
  ParticleWeightCorrection=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

  LocalTimeStep=node->block->GetLocalTimeStep(spec);
  GlobalWeight=node->block->GetLocalParticleWeight(spec);
  
  if (PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node,false)==-1) exit(__LINE__,__FILE__,"Error: cannot find the cellwhere the particle is located");
  cell=node->block->GetCenterNode(i,j,k);

double T0=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());

auto F = [&] (double T,double T0) {
  double res,dU;
  w=exp(-100/(T*T*T)*SpeedOfLight_cm*LocalTimeStep);

  if (w*ParticleWeightCorrection<1.0E-2) w=0.0;
  
  dU=(1.0-w)*ParticleWeightCorrection*GlobalWeight; //GJ   

  res=T0-T+dU/(Material::Density*cell->Measure)/Material::SpecificHeat;
  return res;
}; 

double T1,dU;

  MaterialTemparature=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());
  w=exp(-100/pow(MaterialTemparature,3)*SpeedOfLight_cm*LocalTimeStep);
  if (w*ParticleWeightCorrection<1.0E-2) w=0.0;
  
  dU=(1.0-w)*ParticleWeightCorrection*GlobalWeight; //GJ 
  T1=MaterialTemparature+dU/(Material::Density*cell->Measure)/Material::SpecificHeat; //keV 

double F0,F1,dT;

F0=F(MaterialTemparature,MaterialTemparature);

for (int ii=0;ii<5;ii++) {
  double dFdT,dT;

  F1=F(T1,MaterialTemparature);

  dFdT=(F1-F0)/(T1-MaterialTemparature);
  dT=-F1/dFdT;

  T1+=0.5*dT;
}
  
*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer())=T1;
*/

/*
if (true) {
  MaterialTemparature= *(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer());

  double dU,dT,sigma=Opasity::GetSigma(MaterialTemparature); 
  w=exp(-sigma*SpeedOfLight_cm*LocalTimeStep); 

  if (w*ParticleWeightCorrection<1.0E-2) w=0.0; 

  dU=(1.0-w)*ParticleWeightCorrection*GlobalWeight; //GJ 
  dT=dU/(Material::Density*cell->Measure)/Material::SpecificHeat;


  MaterialTemparature+=dU/(Material::Density*cell->Measure)/Material::SpecificHeat; //keV 
  *(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer())=MaterialTemparature;
}
*/

/*
  if (w==0.0) {
    //remove the particle from the simulation
    PIC::ParticleBuffer::DeleteParticle(ptr); 
  }
  else {
    //keep the particle in the simulation 
    PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
    PIC::ParticleBuffer::SetPrev(-1,ptr);

    PIC::ParticleBuffer::SetIndividualStatWeightCorrection(w*ParticleWeightCorrection,ParticleData);
    
    if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
    FirstParticleCell=ptr;
  }
}        
*/

long int Radiation::ThermalRadiation::InjectParticles() {
  int LocalCellNumber,i,j,k;
  long int res=0;

  for (cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];node!=NULL;node=node->nextNodeThisThread) {
    PIC::Mesh::cDataBlockAMR *block=node->block;

    if (!block) continue;

    for (k=0;k<_BLOCK_CELLS_Z_;k++) {
      for (j=0;j<_BLOCK_CELLS_Y_;j++) {
        for (i=0;i<_BLOCK_CELLS_X_;i++) {
          LocalCellNumber=_getCenterNodeLocalNumber(i,j,k);
          res+=InjectParticles(0,i,j,k,block->GetCenterNode(LocalCellNumber),block,node);
        }
      }
    }
  }

  return res;
} 

long int Radiation::ThermalRadiation::InjectParticles(int spec,int i,int j,int k,PIC::Mesh::cDataCenterNode* cell, PIC::Mesh::cDataBlockAMR* block, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) { 
  double MaterialTemparature;
  double anpart,dU,dU_kev,dU_GJ,ParticleWeight,WeightCorrectionFactor=1.0,LocalTimeStep,x[3],v[3];
  int npart;
  long int ptr;

  MaterialTemparature=*(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer()); 

  ParticleWeight=node->block->GetLocalParticleWeight(spec);
  LocalTimeStep=node->block->GetLocalTimeStep(spec);

//  dU=Opasity::GetSigma(MaterialTemparature)*Material::RadiationConstant*SpeedOfLight*100.0*pow(MaterialTemparature,4)/(4.0*Pi); //GJ/g/ns  


  double sigma=100/pow(MaterialTemparature,3); //cm^2/g
  double a=0.01372; //GJ/(cm^3*keV^4)
  double rho=3.0; //g/cm^3
  double SpecificHeat=0.1; //GJ/g/keV

//  dU=sigma*a*pow(Radiation::SpeedOfLight_cm,2)*pow(MaterialTemparature,4)/(4.0*Pi); //GJ/g/ns 

dU=sigma*a*Radiation::SpeedOfLight_cm*pow(MaterialTemparature,4); //GJ/g/n

  dU_GJ=dU*LocalTimeStep; //GJ/g 

  dU=dU_GJ*cell->Measure*Material::Density; //GJ

//  dU_kev=dU*1.0E9*J2eV/1.0E3;
  anpart=dU/ParticleWeight*SpeedOfLight_cm/(4.0*Pi);

/*
  dU=100.0/pow(MaterialTemparature,3)* 0.01372 * 29.98 *  pow(MaterialTemparature,4)/(4.0*Pi); //GJ/g/ns 


  dU_GJ=dU*LocalTimeStep/_NANO_; //GJ/g 

  dU=dU_GJ*cell->Measure*1.0E6*Material::Density; //GJ

  dU_kev=dU*1.0E9*J2eV/1.0E3; 
  anpart=dU_kev/ParticleWeight;
*/


  WeightCorrectionFactor=1.0;

//  if (anpart>100.0) {
//    WeightCorrectionFactor=anpart/100.0;
//    anpart=100.0;
//  }


  npart=(int)anpart;

  if (rnd()<npart-anpart) npart++;


  dU_GJ=npart*ParticleWeight/(cell->Measure*Material::Density); //GJ/g 

///  if (npart!=0) MaterialTemparature-=dU_GJ/Material::SpecificHeat * SpeedOfLight_cm/(4.0*Pi); //keV 

MaterialTemparature-=npart*ParticleWeight/(Material::SpecificHeat*cell->Measure*Material::Density) ; 



  *(double*)(MaterialTemperatureOffset+cell->GetAssociatedDataBufferPointer())=MaterialTemparature;

  for (int ii=0;ii<npart;ii++) {
    x[0]=node->xmin[0]+(node->xmax[0]-node->xmin[0])*(i+rnd())/_BLOCK_CELLS_X_; 
    x[1]=node->xmin[1]+(node->xmax[1]-node->xmin[1])*(j+rnd())/_BLOCK_CELLS_Y_;
    x[2]=node->xmin[2]+(node->xmax[2]-node->xmin[2])*(k+rnd())/_BLOCK_CELLS_Z_;

    Vector3D::Distribution::Uniform(v,SpeedOfLight_cm); 

    ptr=PIC::ParticleBuffer::GetNewParticle(block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)],true);

    PIC::ParticleBuffer::SetIndividualStatWeightCorrection(WeightCorrectionFactor,ptr);
    PIC::ParticleBuffer::SetX(x,ptr);
    PIC::ParticleBuffer::SetV(v,ptr);
    PIC::ParticleBuffer::SetI(spec,ptr);
  }  

  return npart;
}






