//$Id$
//functionality of sampling of the distribution of the particle velocity direction and energy that car reach the near Earth' regions


/*
 * DomainBoundaryParticlePropertyTable.cpp
 *
 *  Created on: Sep 16, 2017
 *      Author: vtenishe
 */

#include "pic.h"
#include "specfunc.h"
#include "array_3d.h"
#include "array_4d.h"
#include "array_5d.h"

double Earth::CutoffRigidity::DomainBoundaryParticleProperty::logEmax=log(Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit);
double Earth::CutoffRigidity::DomainBoundaryParticleProperty::logEmin=log(Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit);

int Earth::CutoffRigidity::DomainBoundaryParticleProperty::nAzimuthIntervals=50;
int Earth::CutoffRigidity::DomainBoundaryParticleProperty::nCosZenithIntervals=50;
int Earth::CutoffRigidity::DomainBoundaryParticleProperty::nLogEnergyLevels=50;

double Earth::CutoffRigidity::DomainBoundaryParticleProperty::dCosZenithAngle=1.0/Earth::CutoffRigidity::DomainBoundaryParticleProperty::nCosZenithIntervals;
double Earth::CutoffRigidity::DomainBoundaryParticleProperty::dAzimuthAngle=2.0*Pi/Earth::CutoffRigidity::DomainBoundaryParticleProperty::nAzimuthIntervals;
double Earth::CutoffRigidity::DomainBoundaryParticleProperty::dLogE=(logEmax-logEmin)/Earth::CutoffRigidity::DomainBoundaryParticleProperty::nLogEnergyLevels;

bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleDomainBoundaryParticleProperty=false;

double Earth::CutoffRigidity::DomainBoundaryParticleProperty::dX[6][3]={{0.0,0.0},{0.0,0.0},{0.0,0.0},{0.0,0.0},{0.0,0.0},{0.0,0.0}};
array_5d<cBitwiseFlagTable> Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable;
//cBitwiseFlagTable Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleTable[PIC::nTotalSpecies][6][Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection][Earth::CutoffRigidity::DomainBoundaryParticleProperty::SampleMaskNumberPerSpatialDirection];

//parameters of the phase space sampling procedure
//ActiveFlag defines whether sampling of the phaswe space is turned on
//LastActiveOutputCycleNumber is the number of output cycles during which the phase space is sampled
//LastActiveOutputCycleResetParticleBuffer is the flag that defined whether the particle buffer need to be reset at the end of sampling of the phase space
bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::ActiveFlag=false;
int Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::LastActiveOutputCycleNumber=-1;
bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::LastActiveOutputCycleResetParticleBuffer=false;
double Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::OccupiedPhaseSpaceTableFraction=0.2;

bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::ApplyInjectionPhaseSpaceLimiting=false;
bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::EnableSampleParticleProperty=true;

//de-allocate sampling buffers
void Earth::CutoffRigidity::DomainBoundaryParticleProperty::Deallocate() {
  SampleTable.remove();
}

//allocate the particle property sample table
void Earth::CutoffRigidity::DomainBoundaryParticleProperty::Allocate(int nlocs) {
  int s,iface,i,j,iThreadOpenMP,iTestLocation;

  if (SampleDomainBoundaryParticleProperty==false) return;

  logEmax=log(Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit);
  logEmin=log(Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit);
  dLogE=(logEmax-logEmin)/nLogEnergyLevels;

  int offset;

  //de-allocate the sampling tables in case they have been allocated
  SampleTable.remove();

  //allocate the sampling tables
  SampleTable.init(PIC::nTotalSpecies,nlocs,6,SampleMaskNumberPerSpatialDirection,SampleMaskNumberPerSpatialDirection);


  for (iTestLocation=0;iTestLocation<nlocs;iTestLocation++) {
    for (s=0;s<PIC::nTotalSpecies;s++) for (iface=0;iface<6;iface++) for (i=0;i<SampleMaskNumberPerSpatialDirection;i++) for (j=0;j<SampleMaskNumberPerSpatialDirection;j++) {
      for (iThreadOpenMP=0;iThreadOpenMP<SampleTable(s,iTestLocation,iface,i,j).nThreadsOpenMP;iThreadOpenMP++) {
        SampleTable.GetPtr(s,iTestLocation,iface,i,j)->AllocateTable(nAzimuthIntervals*nCosZenithIntervals*nLogEnergyLevels,iThreadOpenMP);
      }
    }
  }
}

//get the global index that corresponds to the velocity vector
int Earth::CutoffRigidity::DomainBoundaryParticleProperty::GetVelocityVectorIndex(int spec,double *v,int iface) {
  double t,l[3],Energy,Speed=0.0;
  int idim,iCosZenithInterval,iAzimuthInterval,iLogEnergyLevel;

  //getermine the speed
  for (idim=0;idim<3;idim++) Speed+=v[idim]*v[idim];

  Speed=sqrt(Speed);
  t=1.0/Speed;

  for (idim=0;idim<3;idim++) l[idim]=t*v[idim];

  //getermine the energy level
  Energy=Relativistic::Speed2E(Speed,PIC::MolecularData::GetMass(spec));
  iLogEnergyLevel=(int)((log(Energy)-logEmin)/dLogE);

  if (iLogEnergyLevel<0) iLogEnergyLevel=0;
  if (iLogEnergyLevel>=nLogEnergyLevels) iLogEnergyLevel=nLogEnergyLevels-1;

  //determine the azimuth angle level
  double c0=0.0,c1=0.0,c2=0.0;

  for (int i=0;i<3;i++) c0-=v[i]*e0FaceFrame[iface][i],c1-=v[i]*e1FaceFrame[iface][i],c2-=v[i]*InternalFaceNorm[iface][i];  //the reversed direction of the velocity vector is important

  //determine the zenith angle level
  if (c2>0.0) {
    iCosZenithInterval=c2/(Speed*dCosZenithAngle);
    if (iCosZenithInterval>=nCosZenithIntervals) iCosZenithInterval=nCosZenithIntervals-1;
  }
  else iCosZenithInterval=nCosZenithIntervals-1;

  //determine the azimuth angle level
  double phi=acos(c0/Speed);

  if (c1<0.0) phi=PiTimes2-phi;
  iAzimuthInterval=(int)(phi/dAzimuthAngle);
  if (iAzimuthInterval>=nAzimuthIntervals) iAzimuthInterval=nAzimuthIntervals-1;

  //return the index corresponding to the particle velocity
  return iAzimuthInterval+(iCosZenithInterval+iLogEnergyLevel*nCosZenithIntervals)*nAzimuthIntervals;
}

void Earth::CutoffRigidity::DomainBoundaryParticleProperty::ConvertVelocityVectorIndex2Velocity(int spec,double *v,int iface,int Index) {
  int idim,iCosZenithInterval,iAzimuthInterval,iLogEnergyLevel;

  iLogEnergyLevel=Index/(nCosZenithIntervals*nAzimuthIntervals);
  Index=Index%(nCosZenithIntervals*nAzimuthIntervals);

  iCosZenithInterval=Index/nAzimuthIntervals;
  iAzimuthInterval=Index%nAzimuthIntervals;

  double Speed,Energy,ZenithAngle,AzimuthAngle,cosZenithAngle,sinZenithAngle,cosAzimuthAngle,sinAzimuthAngle;

  Energy=exp((0.5+iLogEnergyLevel)*dLogE+logEmin);
  ZenithAngle=acos((iCosZenithInterval+0.5)*dCosZenithAngle);
  AzimuthAngle=(iAzimuthInterval+0.5)*dAzimuthAngle;

  cosZenithAngle=cos(ZenithAngle);
  sinZenithAngle=sin(ZenithAngle);

  cosAzimuthAngle=cos(AzimuthAngle);
  sinAzimuthAngle=sin(AzimuthAngle);

  Speed=Relativistic::E2Speed(Energy,PIC::MolecularData::GetMass(spec));

  for (idim=0;idim<3;idim++) v[idim]=Speed*(InternalFaceNorm[iface][idim]*sinZenithAngle+cosZenithAngle*(e0FaceFrame[iface][idim]*cosAzimuthAngle+e1FaceFrame[iface][idim]*sinAzimuthAngle));
}


//init the namespace
void Earth::CutoffRigidity::DomainBoundaryParticleProperty::Init() {
  int iface;

  for (iface=0;iface<6;iface++) {
    switch (iface) {
    case 0:case 1:
      dX[iface][0]=0.0;
      dX[iface][1]=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/SampleMaskNumberPerSpatialDirection;
      dX[iface][2]=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/SampleMaskNumberPerSpatialDirection;
      break;
    case 2:case 3:
      dX[iface][0]=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/SampleMaskNumberPerSpatialDirection;
      dX[iface][1]=0.0;
      dX[iface][2]=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/SampleMaskNumberPerSpatialDirection;
      break;
    case 4:case 5:
      dX[iface][0]=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/SampleMaskNumberPerSpatialDirection;
      dX[iface][1]=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/SampleMaskNumberPerSpatialDirection;
      dX[iface][2]=0.0;
      break;
    }
  }
}

//register properties of the particles that cross the external boundary
void Earth::CutoffRigidity::DomainBoundaryParticleProperty::RegisterParticleProperties(int spec,double *x,double *v,int iOriginLocation,int iface) {
  int Index;

  if (EnableSampleParticleProperty==true) {
    //determine on which Samplking Table to refister the particle velocity
    int iTable,jTable;

    switch (iface) {
    case 0:case 1:
      iTable=(x[1]-PIC::Mesh::mesh->xGlobalMin[1])/dX[iface][1];
      jTable=(x[2]-PIC::Mesh::mesh->xGlobalMin[2])/dX[iface][2];
      break;
    case 2:case 3:
      iTable=(x[0]-PIC::Mesh::mesh->xGlobalMin[0])/dX[iface][0];
      jTable=(x[2]-PIC::Mesh::mesh->xGlobalMin[2])/dX[iface][2];
      break;
    case 4:case 5:
      iTable=(x[0]-PIC::Mesh::mesh->xGlobalMin[0])/dX[iface][0];
      jTable=(x[1]-PIC::Mesh::mesh->xGlobalMin[1])/dX[iface][1];
      break;
    }

    if (iTable>=SampleMaskNumberPerSpatialDirection) iTable=SampleMaskNumberPerSpatialDirection-1;
    if (jTable>=SampleMaskNumberPerSpatialDirection) jTable=SampleMaskNumberPerSpatialDirection-1;
    if (iTable<0) iTable=0;
    if (jTable<0) jTable=0;


    //register the particle velocity vector
    Index=GetVelocityVectorIndex(spec,v,iface);

    if (Index<0) exit(__LINE__,__FILE__,"Error: Index must be positive");

    SampleTable(spec,iOriginLocation,iface,iTable,jTable).SetFlag(true,Index);
  }
}

//gather the flags from all processors
void Earth::CutoffRigidity::DomainBoundaryParticleProperty::Gather() {
  for (int iTestLocation=0;iTestLocation<std::max(1,Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength);iTestLocation++) for (int s=0;s<PIC::nTotalSpecies;s++) for (int iface=0;iface<6;iface++) {
    for (int i=0;i<SampleMaskNumberPerSpatialDirection;i++) for (int j=0;j<SampleMaskNumberPerSpatialDirection;j++) {
      SampleTable(s,iTestLocation,iface,i,j).Gather();
    }
  }
}

//test the particle properties
bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::TestInjectedParticleProperties(int spec,double *x,double *v,int iTestLocation,int iface) {
  int Index,iTable,jTable;

  //determine on which Sampling Table to refister the particle velocity
  switch (iface) {
  case 0:case 1:
    iTable=(x[1]-PIC::Mesh::mesh->xGlobalMin[1])/dX[iface][1];
    jTable=(x[2]-PIC::Mesh::mesh->xGlobalMin[2])/dX[iface][2];
    break;
  case 2:case 3:
    iTable=(x[0]-PIC::Mesh::mesh->xGlobalMin[0])/dX[iface][0];
    jTable=(x[2]-PIC::Mesh::mesh->xGlobalMin[2])/dX[iface][2];
    break;
  case 4:case 5:
    iTable=(x[0]-PIC::Mesh::mesh->xGlobalMin[0])/dX[iface][0];
    jTable=(x[1]-PIC::Mesh::mesh->xGlobalMin[1])/dX[iface][1];
    break;
  }

  if (iTable>=SampleMaskNumberPerSpatialDirection) iTable=SampleMaskNumberPerSpatialDirection-1;
  if (jTable>=SampleMaskNumberPerSpatialDirection) jTable=SampleMaskNumberPerSpatialDirection-1;

  //Test Particle Properties
  Index=GetVelocityVectorIndex(spec,v,iface);
  return SampleTable(spec,iTestLocation,iface,iTable,jTable).Test(Index);
}

//Smooth the sampled distribution
void Earth::CutoffRigidity::DomainBoundaryParticleProperty::SmoothSampleTable() {
  int spec,i,j,k,iMin,iMax,jMin,jMax,Index,iTable,jTable,nSetPoints,IndexMax,iLogEnergyLevel,iface,iCosZenithInterval,iAzimuthInterval;

  IndexMax=nAzimuthIntervals*nCosZenithIntervals*nLogEnergyLevels;

  for (int iTestLocation=0;iTestLocation<std::max(1,Earth::CutoffRigidity::IndividualLocations::xTestLocationTableLength);iTestLocation++) for (spec=0;spec<PIC::nTotalSpecies;spec++) for (iface=0;iface<6;iface++) {
    for (iTable=0;iTable<SampleMaskNumberPerSpatialDirection;iTable++) for (jTable=0;jTable<SampleMaskNumberPerSpatialDirection;jTable++) {
      //loop through all sampling tables

      for (Index=0,nSetPoints=0;Index<IndexMax;Index++) if (SampleTable(spec,iTestLocation,iface,iTable,jTable).Test(Index)==true) nSetPoints++;

      if (nSetPoints!=0) {
        //expand the set of the sampled points so a given fraction of the points is occupied
        bool ProcessFlagTable[nCosZenithIntervals][nAzimuthIntervals][nLogEnergyLevels];

        while (nSetPoints<SamplingParameters::OccupiedPhaseSpaceTableFraction*IndexMax) {
          nSetPoints=0;

          //reset the flag table
          for (iCosZenithInterval=0;iCosZenithInterval<nCosZenithIntervals;iCosZenithInterval++) for (iAzimuthInterval=0;iAzimuthInterval<nAzimuthIntervals;iAzimuthInterval++) {
            for (iLogEnergyLevel=0;iLogEnergyLevel<nLogEnergyLevels;iLogEnergyLevel++) {
              ProcessFlagTable[iCosZenithInterval][iAzimuthInterval][iLogEnergyLevel]=false;
            }
          }

          //scan through the sampled distribution
          for (iCosZenithInterval=0;iCosZenithInterval<nCosZenithIntervals;iCosZenithInterval++) for (iAzimuthInterval=0;iAzimuthInterval<nAzimuthIntervals;iAzimuthInterval++) {
            for (iLogEnergyLevel=0;iLogEnergyLevel<nLogEnergyLevels;iLogEnergyLevel++) {
              Index=iAzimuthInterval+(iCosZenithInterval+iLogEnergyLevel*nCosZenithIntervals)*nAzimuthIntervals;

              if ((SampleTable(spec,iTestLocation,iface,iTable,jTable).Test(Index)==true)&&(ProcessFlagTable[iCosZenithInterval][iAzimuthInterval][iLogEnergyLevel]==false)) {
                //check if the distribution souble be expanded
                ProcessFlagTable[iCosZenithInterval][iAzimuthInterval][iLogEnergyLevel]=true;
                nSetPoints++;

                //scan through the neibourhood of the found point
                for (i=-1;i<=1;i++) if ((iCosZenithInterval+i>=0)&&(iCosZenithInterval+i<nCosZenithIntervals)) {
                  for (j=-1;j<=1;j++)  if ((iAzimuthInterval+j>=0)&&(iAzimuthInterval+i<nAzimuthIntervals)) {
                    for (k=-1;k<=1;k++) if ((iLogEnergyLevel+k>=0)&&(iLogEnergyLevel+k<nLogEnergyLevels)) {
                      if (ProcessFlagTable[iCosZenithInterval+i][iAzimuthInterval+j][iLogEnergyLevel+k]==false) {
                        ProcessFlagTable[iCosZenithInterval+i][iAzimuthInterval+j][iLogEnergyLevel+j]=true;
                        Index=(iAzimuthInterval+j)+((iCosZenithInterval+i)+(iLogEnergyLevel+k)*nCosZenithIntervals)*nAzimuthIntervals;

                        if (SampleTable(spec,iTestLocation,iface,iTable,jTable).Test(Index)==false) {
                          SampleTable(spec,iTestLocation,iface,iTable,jTable).SetFlag(true,Index);
                        }

                        nSetPoints++;
                      }
                    }
                  }
                }
              }
            }
          }

        }
      }

    }
  }
}


double Earth::CutoffRigidity::DomainBoundaryParticleProperty::GetTotalSourceRate(int spec,int iface,int iTable,int jTable,bool AccountReachabilityFactor) {
  double lnEmax,lnEmin,f,mass;
  int iTest;


  const int nTotalTests=1000;

  mass=PIC::MolecularData::GetMass(spec);

  double c1=mass*pow(SpeedOfLight,2);
  double TotalInjectionRate=0.0;

  lnEmin=log(Earth::Sampling::Fluency::minSampledEnergy);
  lnEmax=log(Earth::Sampling::Fluency::maxSampledEnergy);

  //intefrate the differential flux to calcualte the total source rate
  int nIntervals=10000;
  double lx,ly,lz,dE=(Earth::Sampling::Fluency::maxSampledEnergy-Earth::Sampling::Fluency::minSampledEnergy)/nIntervals;
  double TotalIntegral=0.0,LimitedIntegral=0.0;

  for (int i=0;i<nIntervals;i++) {
    double E,DiffFlux;

    E=(i+0.5)*dE+Earth::Sampling::Fluency::minSampledEnergy;
    DiffFlux=GCR_BADAVI2011ASR::Hydrogen::GetDiffFlux(E);

    TotalInjectionRate+=DiffFlux*dE;
  }

  switch (iface) {
  case 0:case 1:
    ly=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/SampleMaskNumberPerSpatialDirection;
    lz=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/SampleMaskNumberPerSpatialDirection;

    TotalInjectionRate*=ly*lz;
    break;
  case 2:case 3:
    lx=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/SampleMaskNumberPerSpatialDirection;
    lz=(PIC::Mesh::mesh->xGlobalMax[2]-PIC::Mesh::mesh->xGlobalMin[2])/SampleMaskNumberPerSpatialDirection;

    TotalInjectionRate*=lx*lz;
    break;
  case 4:case 5:
    lx=(PIC::Mesh::mesh->xGlobalMax[0]-PIC::Mesh::mesh->xGlobalMin[0])/SampleMaskNumberPerSpatialDirection;
    ly=(PIC::Mesh::mesh->xGlobalMax[1]-PIC::Mesh::mesh->xGlobalMin[1])/SampleMaskNumberPerSpatialDirection;

    TotalInjectionRate*=ly*lz;
  }

  for (iTest=0;iTest<nTotalTests;iTest++) {
    double f,lnE,theta,phi,E;


    phi=rnd()*2.0*Pi;
    theta=rnd()*Pi;

    lnE=rnd()*(lnEmax-lnEmin)+lnEmin;
    E=exp(lnE);


    f = exp( (2 * lnE)) * 0.3141592654e1 *  (c1 * c1) * (exp( lnE) +  (2 * c1)) * pow(exp( lnE) * (exp( lnE) +  (2 * c1)),
        -0.1e1 / 0.2e1) / (exp( (4 * lnE)) + 0.4e1 * exp( (3 * lnE)) *  c1 + 0.6e1 * exp( (2 * lnE)) *  (c1 * c1) +
            0.4e1 * exp( lnE) *  pow( c1,  3) +  pow( c1,  4));

    f*=GCR_BADAVI2011ASR::Hydrogen::GetDiffFlux(E);
    TotalIntegral+=f;


    if (AccountReachabilityFactor==true) {
      double E,speed,v[3];
      int Index;

      E=exp(lnE);
      speed=Relativistic::E2Speed(E,mass);

      switch (iface) {
      case 0:
        v[0]=speed*cos(theta);
        v[1]=speed*sin(theta)*cos(phi);
        v[2]=speed*sin(theta)*sin(phi);
        break;

      case 1:
        v[0]=-speed*cos(theta);
        v[1]=speed*sin(theta)*cos(phi);
        v[2]=speed*sin(theta)*sin(phi);
        break;


      case 2:
        v[0]=speed*sin(theta)*cos(phi);
        v[1]=speed*cos(theta);
        v[2]=speed*sin(theta)*sin(phi);
        break;

      case 3:
        v[0]=speed*sin(theta)*cos(phi);
        v[1]=-speed*cos(theta);
        v[2]=speed*sin(theta)*sin(phi);
        break;


      case 4:
        v[0]=speed*sin(theta)*cos(phi);
        v[1]=speed*sin(theta)*sin(phi);
        v[2]=speed*cos(theta);
        break;

      case 5:
        v[0]=speed*sin(theta)*cos(phi);
        v[1]=speed*sin(theta)*sin(phi);
        v[2]=-speed*cos(theta);
        break;
      }

      //convert velocity vector in a index and chack it in the table
      Index=GetVelocityVectorIndex(spec,v,iface);

      int i,size=SampleTable.size(1);

      for (i=0;i<size;i++) if (SampleTable(spec,i,iface,iTable,jTable).Test(Index)==true) {
        LimitedIntegral+=f;
        break;
      }
    }
  }


  double GlobalTotalIntegral,GlobalLimitedIntegral;

  MPI_Allreduce(&TotalIntegral,&GlobalTotalIntegral,1,MPI_DOUBLE,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);
  MPI_Allreduce(&LimitedIntegral,&GlobalLimitedIntegral,1,MPI_DOUBLE,MPI_SUM,MPI_GLOBAL_COMMUNICATOR);

  return TotalInjectionRate*GlobalLimitedIntegral/GlobalTotalIntegral;
}


bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::GenerateParticleProperty(double *x,double *v,double &WeightCorrectionFactor,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* &startNode,int spec,int iface,int iTable,int jTable) {
  double xmin[3],xmax[3],mass;
  int idim;
  int NormAxis=-1;
  double NormAxisDirection=-1.0;

  mass=PIC::MolecularData::GetMass(spec);

  //generate location of the new particle
  switch (iface) {
  case 0:
    xmin[0]=PIC::Mesh::mesh->xGlobalMin[0];
    xmax[0]=PIC::Mesh::mesh->xGlobalMin[0];

    xmin[1]=PIC::Mesh::mesh->xGlobalMin[1]+iTable*dX[iface][1];
    xmax[1]=PIC::Mesh::mesh->xGlobalMin[1]+(iTable+1)*dX[iface][1];

    xmin[2]=PIC::Mesh::mesh->xGlobalMin[2]+jTable*dX[iface][2];
    xmax[2]=PIC::Mesh::mesh->xGlobalMin[2]+(jTable+1)*dX[iface][2];

    NormAxis=0,NormAxisDirection=1.0;
    break;

  case 1:
    xmin[0]=PIC::Mesh::mesh->xGlobalMax[0];
    xmax[0]=PIC::Mesh::mesh->xGlobalMax[0];

    xmin[1]=PIC::Mesh::mesh->xGlobalMin[1]+iTable*dX[iface][1];
    xmax[1]=PIC::Mesh::mesh->xGlobalMin[1]+(iTable+1)*dX[iface][1];

    xmin[2]=PIC::Mesh::mesh->xGlobalMin[2]+jTable*dX[iface][2];
    xmax[2]=PIC::Mesh::mesh->xGlobalMin[2]+(jTable+1)*dX[iface][2];

    NormAxis=0,NormAxisDirection=-1.0;
    break;


  case 2:
    xmin[0]=PIC::Mesh::mesh->xGlobalMin[0]+iTable*dX[iface][0];
    xmax[0]=PIC::Mesh::mesh->xGlobalMin[0]+(iTable+1)*dX[iface][0];

    xmin[1]=PIC::Mesh::mesh->xGlobalMin[1];
    xmax[1]=PIC::Mesh::mesh->xGlobalMin[1];

    xmin[2]=PIC::Mesh::mesh->xGlobalMin[2]+jTable*dX[iface][2];
    xmax[2]=PIC::Mesh::mesh->xGlobalMin[2]+(jTable+1)*dX[iface][2];

    NormAxis=1,NormAxisDirection=1.0;
    break;

  case 3:
    xmin[0]=PIC::Mesh::mesh->xGlobalMin[0]+iTable*dX[iface][0];
    xmax[0]=PIC::Mesh::mesh->xGlobalMin[0]+(iTable+1)*dX[iface][0];

    xmin[1]=PIC::Mesh::mesh->xGlobalMax[1];
    xmax[1]=PIC::Mesh::mesh->xGlobalMax[1];

    xmin[2]=PIC::Mesh::mesh->xGlobalMin[2]+jTable*dX[iface][2];
    xmax[2]=PIC::Mesh::mesh->xGlobalMin[2]+(jTable+1)*dX[iface][2];

    NormAxis=1,NormAxisDirection=-1.0;
    break;


  case 4:
    xmin[0]=PIC::Mesh::mesh->xGlobalMin[0]+iTable*dX[iface][0];
    xmax[0]=PIC::Mesh::mesh->xGlobalMin[0]+(iTable+1)*dX[iface][0];

    xmin[1]=PIC::Mesh::mesh->xGlobalMin[1]+jTable*dX[iface][1];
    xmax[1]=PIC::Mesh::mesh->xGlobalMin[1]+(jTable+1)*dX[iface][1];

    xmin[2]=PIC::Mesh::mesh->xGlobalMin[2];
    xmax[2]=PIC::Mesh::mesh->xGlobalMin[2];

    NormAxis=2,NormAxisDirection=1.0;
    break;


  case 5:
    xmin[0]=PIC::Mesh::mesh->xGlobalMin[0]+iTable*dX[iface][0];
    xmax[0]=PIC::Mesh::mesh->xGlobalMin[0]+(iTable+1)*dX[iface][0];

    xmin[1]=PIC::Mesh::mesh->xGlobalMin[1]+jTable*dX[iface][1];
    xmax[1]=PIC::Mesh::mesh->xGlobalMin[1]+(jTable+1)*dX[iface][1];

    xmin[2]=PIC::Mesh::mesh->xGlobalMax[2];
    xmax[2]=PIC::Mesh::mesh->xGlobalMax[2];

    NormAxis=2,NormAxisDirection=-1;
    break;
  }


  for (idim=0;idim<DIM;idim++) x[idim]=xmin[idim]+rnd()*(xmax[idim]-xmin[idim]);

  x[NormAxis]+=NormAxisDirection*PIC::Mesh::mesh->EPS;

  //determine if the particle belongs to this processor
  startNode=PIC::Mesh::mesh->findTreeNode(x,startNode);
  if (startNode->Thread!=PIC::Mesh::mesh->ThisThread) return false;


  //evaluate fmax
  double f,fmax,E,speed,Index,t;
  static array_4d<double> fMaxTable; //   [spec,iface,iTable,jTable],
  static array_4d<bool> fMaxInitTable;
  bool static InitFlag=false;

  static double lnEmin=log(Earth::Sampling::Fluency::minSampledEnergy);
  static double lnEmax=log(Earth::Sampling::Fluency::maxSampledEnergy);

  if (InitFlag==false) {
    InitFlag=true;

    fMaxTable.init(PIC::nTotalSpecies,6,SampleMaskNumberPerSpatialDirection,SampleMaskNumberPerSpatialDirection);
    fMaxInitTable.init(PIC::nTotalSpecies,6,SampleMaskNumberPerSpatialDirection,SampleMaskNumberPerSpatialDirection);

    fMaxTable=0.0;
    fMaxInitTable=false;
  }

  if (fMaxInitTable(spec,iface,iTable,jTable)==false) {
    //evaluate fmax for given spec,iface,iTable,jTable
    int itest;
    const int nTotalTests=1000000;

    fMaxInitTable(spec,iface,iTable,jTable)=true;

    for (itest=0,fmax=0.0;itest<nTotalTests;itest++) {
      E=exp(lnEmin+rnd()*(lnEmax-lnEmin));
      speed=Relativistic::E2Speed(E,mass);

      Vector3D::Distribution::Uniform(v,speed);

      if (NormAxisDirection*v[NormAxis]<0.0) v[NormAxis]=-v[NormAxis];

      Index=GetVelocityVectorIndex(spec,v,iface);

      int i,size=SampleTable.size(1);
      bool flag=false;

      for (i=0;i<size;i++) if (SampleTable(spec,i,iface,iTable,jTable).Test(Index)==true) {
        flag=true;
        break;
      }

      if (flag==false) continue;

      f=fabs(v[NormAxis])/speed*GCR_BADAVI2011ASR::Hydrogen::GetDiffFlux(E);
      if (f>fmax) fmax=f;
    }

    fMaxTable(spec,iface,iTable,jTable)=fmax;
  }


  //generate velocity of the new particle
  fmax=fMaxTable(spec,iface,iTable,jTable);

  if (fmax==0.0) return false;

  do {
    E=exp(lnEmin+rnd()*(lnEmax-lnEmin));
    speed=Relativistic::E2Speed(E,mass);

    Vector3D::Distribution::Uniform(v,speed);

    if (NormAxisDirection*v[NormAxis]<0.0) v[NormAxis]=-v[NormAxis];

    Index=GetVelocityVectorIndex(spec,v,iface);

    int i,size=SampleTable.size(1);
    bool flag=false;

    for (i=0;i<size;i++) if (SampleTable(spec,i,iface,iTable,jTable).Test(Index)==true) {
      flag=true;
      break;
    }

    f=(flag==true) ? fabs(v[NormAxis])/speed*GCR_BADAVI2011ASR::Hydrogen::GetDiffFlux(E) : 0.0;
  }
  while (f/fmax<rnd());


  WeightCorrectionFactor=1.0;
  return true;
}

//=======================================================================================================================
//inject particles from the boundary of the computational domain
void Earth::CutoffRigidity::DomainBoundaryParticleProperty::InjectParticlesDomainBoundary(int spec) {
  int iTable,jTable,iface,index;


  //estimate the total source rate

  auto GetGlobalSampleMaskIndex = [](int iTable,int jTable,int iface) {
    return iTable+SampleMaskNumberPerSpatialDirection*jTable+SampleMaskNumberPerSpatialDirection*SampleMaskNumberPerSpatialDirection*iface;
  };

  auto ProcessGlobalSampleMaskIndex = [&]  (int& iTable,int& jTable,int& iface, int Index) {
    iface=Index/(SampleMaskNumberPerSpatialDirection*SampleMaskNumberPerSpatialDirection);

    Index-=iface*SampleMaskNumberPerSpatialDirection*SampleMaskNumberPerSpatialDirection;
    jTable=Index/SampleMaskNumberPerSpatialDirection;
    iTable=Index%SampleMaskNumberPerSpatialDirection;
  };


  static bool InitFlag=false;
  static array_1d<bool> InitFlagTable;
  static array_1d<cSingleVariableDiscreteDistribution<double> > SampleMaskInjectionProbabilityTable;
  static array_1d<double> TotalSourceRate;


  if (InitFlag==false) {
    InitFlag=true;
    InitFlagTable.init(PIC::nTotalSpecies);
    SampleMaskInjectionProbabilityTable.init(PIC::nTotalSpecies);
    TotalSourceRate.init(PIC::nTotalSpecies);

    InitFlagTable=false;
    TotalSourceRate=0.0;
  }


  if (InitFlagTable(spec)==false) {
    //evaluate the source rate of the eneregetic particles and set up the random unjection face number generators
    double *SourceRateTable=new double [SampleMaskNumberPerSpatialDirection*SampleMaskNumberPerSpatialDirection*6];  //[iTable][jTable][iface] -> iTable+SampleMaskNumberPerSpatialDirection*jTable+SampleMaskNumberPerSpatialDirection*SampleMaskNumberPerSpatialDirection*iface

    InitFlagTable(spec)=true;

    for (iface=0;iface<6;iface++) for (iTable=0;iTable<SampleMaskNumberPerSpatialDirection;iTable++) for (jTable=0;jTable<SampleMaskNumberPerSpatialDirection;jTable++) {
      index=GetGlobalSampleMaskIndex(iTable,jTable,iface);

      SourceRateTable[index]=GetTotalSourceRate(spec,iface,iTable,jTable,true);
      TotalSourceRate(spec)+=SourceRateTable[index];
    }

    SampleMaskInjectionProbabilityTable(spec).InitArray(SourceRateTable,SampleMaskNumberPerSpatialDirection*SampleMaskNumberPerSpatialDirection*6,SampleMaskNumberPerSpatialDirection*SampleMaskNumberPerSpatialDirection*60);

    delete [] SourceRateTable;
  }


  double ModelParticlesInjectionRate=TotalSourceRate(spec)/PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
  double TimeCounter=0,LocalTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
  double x[3],v[3],WeightCorrectionFactor;
  cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* startNode=NULL;

  if (_SIMULATION_TIME_STEP_MODE_!=_SPECIES_DEPENDENT_GLOBAL_TIME_STEP_) exit(__LINE__,__FILE__,"Error: the time step mode is not correct");
  if (_SIMULATION_PARTICLE_WEIGHT_MODE_!=_SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_) exit(__LINE__,__FILE__,"Error: the particle weight mode is not correct");

  //inject particles
  if (PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]>0.0) while ((TimeCounter+=-log(rnd())/ModelParticlesInjectionRate)<LocalTimeStep) {
    index=SampleMaskInjectionProbabilityTable(spec).DistributeVariable();
    ProcessGlobalSampleMaskIndex(iTable,jTable,iface,index);


    if (GenerateParticleProperty(x,v,WeightCorrectionFactor,startNode,spec,iface,iTable,jTable)==true) {
      //a new particle should be generated
      int iCell,jCell,kCell;
      long int newParticle;

      PIC::Mesh::mesh->FindCellIndex(x,iCell,jCell,kCell,startNode);

      newParticle=PIC::ParticleBuffer::GetNewParticle(startNode->block->FirstCellParticleTable[iCell+_BLOCK_CELLS_X_*(jCell+_BLOCK_CELLS_Y_*kCell)]);
      PIC::ParticleBuffer::byte *newParticleData=PIC::ParticleBuffer::GetParticleDataPointer(newParticle);

      //apply condition of tracking the particle
      #if _PIC_PARTICLE_TRACKER_MODE_ == _PIC_MODE_ON_
      PIC::ParticleTracker::InitParticleID(newParticleData);
      PIC::ParticleTracker::ApplyTrajectoryTrackingCondition(x,v,spec,newParticleData,(void*)startNode);
      #endif

      //generate particles' velocity
      PIC::ParticleBuffer::SetX(x,newParticleData);
      PIC::ParticleBuffer::SetV(v,newParticleData);
      PIC::ParticleBuffer::SetI(spec,newParticleData);

      if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_==_INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,newParticleData);
      }

//      //inject the particle into the system
//      Earth::ParticleMover(newParticle,LocalTimeStep-TimeCounter,startNode);
    }
  }
}

void Earth::CutoffRigidity::DomainBoundaryParticleProperty::InjectParticlesDomainBoundary() {
  for (int spec=0;spec<PIC::nTotalSpecies;spec++) InjectParticlesDomainBoundary(spec);
}



