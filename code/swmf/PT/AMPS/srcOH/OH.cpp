//$Id: OH.cpp,v 1.32 2020/09/11 20:21:12 adamtm Exp $

#include "OH.h"
#include "pic.h"

#include "get_charge_exchange_wrapper_c.h"

// user defined global time step
double OH::UserGlobalTimeStep = 3.154e7;

//  injection boundary condition
double OH::InjectionVelocity[3] = {0.0, 0.0, 0.0};
double OH::InjectionNDensity    = 0.18E6;
double OH::InjectionTemperature = 6519;

//produce secondary ENAa in interaction woth solar wind
bool OH::ProduceENAflag=false;

// computational domain size
double OH::DomainXMin[3] = {-2.25E14,-2.25E14,-2.25E14};
double OH::DomainXMax[3] = { 2.25E14, 2.25E14, 2.25E14};
double OH::DomainDXMin   = 1.8E13;
double OH::DomainDXMax   = 1.8E13;

// Declaring origin offset variable
long int OH::OffsetOriginTag = -1;

// OUTPUT ---------------------------------------------------------------------
int OH::Output::TotalDataLength = 0; 
int OH::Output::ohSourceDensityOffset =-1; 
int OH::Output::ohSourceMomentumOffset=-1;
int OH::Output::ohSourceEnergyOffset  =-1;

//timers
PIC::Debugger::cTimer OH::ReactionProcessorTimer(_PIC_TIMER_MODE_HRES_);

void OH::Output::PrintVariableList(FILE* fout,int DataSetNumber) {
  fprintf(fout,",\"ohSourceDensity\",\"ohSourceMomentumX\",\"ohSourceMomentumY\",\"ohSourceMomentumZ\",\"ohSourceEnergy\"");

  //if DataSetNumber is 0, and  OH::Sampling::OriginLocation::nSampledOriginLocations!=-1 -> output density of particles produced in each source region
  if (OH::DoPrintSpec[DataSetNumber]) {
    for (int i=0;i<OH::Sampling::OriginLocation::nSampledOriginLocations;i++){
      fprintf(fout,", \"ENA Density (Source Region ID=%i)\"",i);
      fprintf(fout,", \"ENA Vx (Source Region ID=%i)\"",i);
      fprintf(fout,", \"ENA Vy (Source Region ID=%i)\"",i);
      fprintf(fout,", \"ENA Vz (Source Region ID=%i)\"",i);
      fprintf(fout,", \"ENA Temperature (Source Region ID=%i)\"",i);
    }

    fprintf(fout,", \"ENA Density (Total Solution)\"");
    fprintf(fout,", \"ENA Vx (Total Solution)\"");
    fprintf(fout,", \"ENA Vy (Total Solution)\"");
    fprintf(fout,", \"ENA Vz (Total Solution)\"");
    fprintf(fout,", \"ENA Temperature (Total Solution)\"");
  }
}

void OH::Output::Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode){

  double S1=0.0, S2[3]={0.0,0.0,0.0}, S3=0.0, TotalParticleWeight=0.0;
  int i,idim,j,k;
  char *offset;

  for (i=0;i<nInterpolationCoeficients;i++) {
    offset = InterpolationList[i]->GetAssociatedDataBufferPointer() + PIC::Mesh::completedCellSampleDataPointerOffset;

    S1+=(*((double*)(offset+OH::Output::ohSourceDensityOffset)))*InterpolationCoeficients[i];

    for(idim=0 ; idim<3; idim++) S2[idim]+=(*(idim+(double*)(offset+OH::Output::ohSourceMomentumOffset)))*InterpolationCoeficients[i];

    S3+=(*((double*)(offset+OH::Output::ohSourceEnergyOffset)))*InterpolationCoeficients[i];
  }

  offset = CenterNode->GetAssociatedDataBufferPointer() + PIC::Mesh::completedCellSampleDataPointerOffset;

  memcpy(offset+OH::Output::ohSourceDensityOffset, &S1,  sizeof(double));
  memcpy(offset+OH::Output::ohSourceMomentumOffset,&S2,3*sizeof(double));
  memcpy(offset+OH::Output::ohSourceEnergyOffset,  &S3,  sizeof(double));

  //evaluate density and velocity of ENAs produced in each source region
  for (int physpec=0;physpec<OH::nPhysSpec;physpec++){
    for (int iSource=0;iSource<OH::Sampling::OriginLocation::nSampledOriginLocations+1;iSource++) {
      double t=0.0,  Sv[3]={0.0,0.0,0.0}, Sv2[3]={0.0,0.0,0.0};

      for (i=0;i<nInterpolationCoeficients;i++) {
        offset=InterpolationList[i]->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset;

        t+=(*(iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[physpec]+(double*)(offset+OH::Sampling::OriginLocation::OffsetDensitySample)))*InterpolationCoeficients[i]/InterpolationList[i]->Measure;

        // grab particle weith from offset+ofsettfor particle weight
        TotalParticleWeight=(*(iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[physpec]+(double*)(offset+OH::Sampling::OriginLocation::OffsetDensitySample)));

        // have to divide by particle weight to interpolate velocity, if no particles in cell it will be zero
        if (TotalParticleWeight > 0.0) {
          for (j=0; j<3; j++){
            Sv[j]+=(*(j+3*iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[physpec]+(double*)(offset+OH::Sampling::OriginLocation::OffsetVelocitySample)))*InterpolationCoeficients[i]/TotalParticleWeight;
          }
        }

        // treating V2 the same as the velocity components
        if (TotalParticleWeight > 0.0) {
          for (int h=0; h<3; h++){
            Sv2[h]+=(*(h+3*iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[physpec]+(double*)(offset+OH::Sampling::OriginLocation::OffsetV2Sample)))*InterpolationCoeficients[i]/TotalParticleWeight;
          }
        }
      }

      offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset;

      *(iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[physpec]+(double*)(offset+OH::Sampling::OriginLocation::OffsetDensitySample))=t;
      for (k=0; k<3; k++) *(k+3*iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[physpec]+(double*)(offset+OH::Sampling::OriginLocation::OffsetVelocitySample))=Sv[k];
      for (int l=0; l<3; l++) *(l+3*iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[physpec]+(double*)(offset+OH::Sampling::OriginLocation::OffsetV2Sample))=Sv2[l];
    }
  }

}

void OH::Output::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode){
  double t,v[3]={0.0,0.0,0.0},v2[3]={0.0,0.0,0.0};

  bool gather_output_data=false;

  if (pipe==NULL) gather_output_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_output_data=true;

  //SourceDensity
  if (gather_output_data==true) { // (pipe->ThisThread==CenterNodeThread) {
    t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Output::ohSourceDensityOffset));
  }

  if ((PIC::ThisThread==0)||(pipe==NULL)) { //(pipe->ThisThread==0) {
    if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

    fprintf(fout,"%e ",t);
  }
  else pipe->send(t);

  //SourceMomentum
  for(int idim=0; idim < 3; idim++){
    if (gather_output_data==true) { //(pipe->ThisThread==CenterNodeThread) {
      t= *(idim+(double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Output::ohSourceMomentumOffset));
    }

    if ((PIC::ThisThread==0)||(pipe==NULL))  {
      if ((CenterNodeThread!=0)&&(pipe!=NULL))  pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",t);
    }
    else pipe->send(t);
  }

  //SourceEnergy
  if (gather_output_data==true) { //(pipe->ThisThread==CenterNodeThread) {
    t= *((double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Output::ohSourceEnergyOffset));
  }

  if ((PIC::ThisThread==0)||(pipe==NULL))  {
    if ((CenterNodeThread!=0)&&(pipe!=NULL))  pipe->recv(t,CenterNodeThread);

    fprintf(fout,"%e ",t);
  }
  else pipe->send(t);

  // ENA origin sampling output
  if (DoPrintSpec[DataSetNumber]) for (int iSource=0;iSource<OH::Sampling::OriginLocation::nSampledOriginLocations+1;iSource++) {
    //density of the ENAs produced in specific origin regions
    if (gather_output_data==true) { // (pipe->ThisThread==CenterNodeThread) {
      t= *(iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[DataSetNumber]+(double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Sampling::OriginLocation::OffsetDensitySample));
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",t/PIC::LastSampleLength);
    }
    else pipe->send(t);

    // velocity of the ENAs produced in specific origin regions
    for (int idim=0; idim<3; idim++){
      if (gather_output_data==true) { //(pipe->ThisThread==CenterNodeThread) {
        t= *(idim+3*iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[DataSetNumber]+(double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Sampling::OriginLocation::OffsetVelocitySample));
      }

      if ((PIC::ThisThread==0)||(pipe==NULL)) {
        if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

        fprintf(fout,"%e ",t);
      }
      else pipe->send(t);
    }

    // Temp of the ENAs produced in specific origin regions calculated using the average sqruared velocity and the square of the average velocity components
    if (gather_output_data==true) { // (pipe->ThisThread==CenterNodeThread) {
      for (int idim=0; idim<3; idim++){
        v[idim]= *(idim+3*iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[DataSetNumber]+(double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Sampling::OriginLocation::OffsetVelocitySample));
        v2[idim]= *(idim+3*iSource+(OH::Sampling::OriginLocation::nSampledOriginLocations+1)*OH::PhysSpec[DataSetNumber]+(double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+OH::Sampling::OriginLocation::OffsetV2Sample));

        t+=v2[idim]-v[idim]*v[idim];

        // cant have negative temperature which can happen if only 1 particle is in a cell
        if (t < 0.0) t=1.0E-15;
      }
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",PIC::MolecularData::GetMass(DataSetNumber)*t/(3.0*Kbol));
    }
    else pipe->send(t);
  }

}

int OH::Output::RequestDataBuffer(int offset){
  OH::Output::ohSourceDensityOffset=offset;
  OH::Output::TotalDataLength=PIC::CPLR::SWMF::nCommunicatedIonFluids;
  offset+=sizeof(double)*PIC::CPLR::SWMF::nCommunicatedIonFluids;

  OH::Output::ohSourceMomentumOffset=offset;
  OH::Output::TotalDataLength+=3*PIC::CPLR::SWMF::nCommunicatedIonFluids;
  offset+=3*sizeof(double)*PIC::CPLR::SWMF::nCommunicatedIonFluids;

  OH::Output::ohSourceEnergyOffset=offset;
  OH::Output::TotalDataLength+=PIC::CPLR::SWMF::nCommunicatedIonFluids;
  offset+=sizeof(double)*PIC::CPLR::SWMF::nCommunicatedIonFluids;

  return OH::Output::TotalDataLength*sizeof(double);
}


void OH::Output::Init() {
  //request sampling buffer and particle fields
  PIC::IndividualModelSampling::RequestSamplingData.push_back(OH::Output::RequestDataBuffer);

  //print out of the otuput file
  PIC::Mesh::PrintVariableListCenterNode.push_back(OH::Output::PrintVariableList);
  PIC::Mesh::PrintDataCenterNode.push_back(OH::Output::PrintData);
  PIC::Mesh::InterpolateCenterNode.push_back(OH::Output::Interpolate);
}


// Loss -----------------------------------------------------------------------

double OH::Loss::GetFrequencyTable(double *FrequencyTable,double *x, int spec, long int ptr,bool &PhotolyticReactionAllowedFlag,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node){

  double PlasmaNumberDensity, PlasmaPressure, PlasmaTemperature;
  double PlasmaBulkVelocity[3];

  double lifetime,TotalFrequency=0.0;

  //in case of running in a stand-along mode
#ifdef _OH_STAND_ALONG_MODE_
#if _OH_STAND_ALONG_MODE_ == _PIC_MODE_ON_
  PhotolyticReactionAllowedFlag=false;
  return lifetime;
#endif
#endif

  // change this to false to turn off charge-exchange
  PhotolyticReactionAllowedFlag=true;

  // this model has hydrogen and three charge-exchange species only
  if (spec!=_H_SPEC_ && spec != _H_ENA_V1_SPEC_ && spec != _H_ENA_V2_SPEC_ && spec != _H_ENA_V3_SPEC_) {
    PhotolyticReactionAllowedFlag=false;
    return -1.0;
  }

  // velocity of a particle
  double v[3];
  PIC::ParticleBuffer::GetV(v,ptr);  

  //loop through all background ion specices
  PIC::CPLR::InitInterpolationStencil(x,node);

  for (int iFluid=0;iFluid<PIC::CPLR::SWMF::nCommunicatedIonFluids;iFluid++) {
    PlasmaNumberDensity = PIC::CPLR::GetBackgroundPlasmaNumberDensity(iFluid);

    if (PlasmaNumberDensity>0.0) {
      PlasmaPressure      = PIC::CPLR::GetBackgroundPlasmaPressure(iFluid);
      PlasmaTemperature   = PlasmaPressure / (2*Kbol * PlasmaNumberDensity);
      PIC::CPLR::GetBackgroundPlasmaVelocity(iFluid,PlasmaBulkVelocity);


      switch (spec) {
      case _H_SPEC_:
        lifetime=ChargeExchange::LifeTime(_H_SPEC_, v, PlasmaBulkVelocity, PlasmaTemperature, PlasmaNumberDensity);
        break;
      case _H_ENA_V1_SPEC_:
        lifetime=ChargeExchange::LifeTime(_H_ENA_V1_SPEC_, v, PlasmaBulkVelocity, PlasmaTemperature, PlasmaNumberDensity);
        break;
      case _H_ENA_V2_SPEC_:
        lifetime=ChargeExchange::LifeTime(_H_ENA_V2_SPEC_, v, PlasmaBulkVelocity, PlasmaTemperature, PlasmaNumberDensity);
        break;
      case _H_ENA_V3_SPEC_:
        lifetime=ChargeExchange::LifeTime(_H_ENA_V3_SPEC_, v, PlasmaBulkVelocity, PlasmaTemperature, PlasmaNumberDensity);
        break;
      default:
        exit(__LINE__,__FILE__,"Error: unknown species");
      }

      FrequencyTable[iFluid]=1.0/lifetime;
      TotalFrequency+=FrequencyTable[iFluid];
    }
    else FrequencyTable[iFluid]=0.0;
  }

  return TotalFrequency;
}


double OH::Loss::LifeTime(double *x, int spec, long int ptr,bool &PhotolyticReactionAllowedFlag,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  double FrequencyTable[PIC::CPLR::SWMF::nCommunicatedIonFluids];
  double lifetime=0.0;

  GetFrequencyTable(FrequencyTable,x,spec,ptr,PhotolyticReactionAllowedFlag,node);

  for (int iFluid=0;iFluid<PIC::CPLR::SWMF::nCommunicatedIonFluids;iFluid++) if (FrequencyTable[iFluid]>0.0) lifetime+=1.0/FrequencyTable[iFluid];

  return (lifetime>0.0) ? lifetime : std::numeric_limits<double>::infinity();
}

//----------------------------------------------------------------------
void OH::Loss::ReactionProcessor_Lookup_Table(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){
  int spec;
  PIC::ParticleBuffer::byte *ParticleData;
  double xParent[3],vParent[3],ParentLifeTime,ParentTimeStep;
  bool ReactionOccurredFlag;

  ReactionProcessorTimer.Start();

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  spec=PIC::ParticleBuffer::GetI(ParticleData);
  PIC::ParticleBuffer::GetX(xParent,ParticleData);
  PIC::ParticleBuffer::GetV(vParent,ParticleData);


  switch (_SIMULATION_TIME_STEP_MODE_) {
  case _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_:
    ParentTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
    break;
  default:
    ParentTimeStep=0.0;
    exit(__LINE__,__FILE__,"Error: the time step node is not defined");
  }

  double PlasmaBulkVelocity[3],PlasmaPressure,PlasmaTemperature,PlasmaNumberDensity;
  PIC::CPLR::InitInterpolationStencil(xParent,node);
  PIC::CPLR::GetBackgroundPlasmaVelocity(0,PlasmaBulkVelocity);
  PlasmaNumberDensity = PIC::CPLR::GetBackgroundPlasmaNumberDensity(0);

  if (PlasmaNumberDensity<=0.0) {
    //the solar wind density is not positive -> update the particle lists and return
    PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
    PIC::ParticleBuffer::SetPrev(-1,ptr);

    if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
    FirstParticleCell=ptr;
    return;
  }


  PlasmaPressure      = PIC::CPLR::GetBackgroundPlasmaPressure(0);
  PlasmaTemperature   = PlasmaPressure / (2*Kbol * PlasmaNumberDensity);

  double ParentParticleWeight,WeightLoss;
  ParentParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec]*PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

  PIC::Mesh::cDataCenterNode *CenterNode;
  char *offset;

  CenterNode=PIC::Mesh::Search::FindCell(xParent,node); ///node->block->GetCenterNode(nd);
  offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset;



  //Update the Sources from the table
  double PlasNumDen,v2_th_Plas,NeuNumDen;
  double neu_v2_th=0;
  double SourceIon_V[5],SourceNeu_V[5];
  double c = (PlasmaNumberDensity>0.0) ? ParentParticleWeight/(PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]*PlasmaNumberDensity)/CenterNode->Measure : 0.0;
  //double c2 = PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]*CenterNode->Measure/PlasmaNumberDensity;
  PlasNumDen = PlasmaNumberDensity;
  v2_th_Plas = 2.0*Kbol*PlasmaTemperature / _MASS_(_H_);
  NeuNumDen =ParentParticleWeight/CenterNode->Measure;

  //Calling the subroutine and separating the terms from the table
  OH_get_charge_exchange_wrapper(&PlasNumDen,&v2_th_Plas,PlasmaBulkVelocity,&NeuNumDen,&neu_v2_th,vParent,SourceIon_V,SourceNeu_V);

  double rate,Force[3],EnergySource,EnergyLoss,ForceSource[3],ForceLoss[3];

  rate=SourceIon_V[0];

  for (int dim=0;dim<3;dim++){
    ForceSource[dim]=SourceNeu_V[dim+1];
    ForceLoss[dim]=SourceIon_V[dim+1];
  }

  EnergySource=SourceNeu_V[4];
  EnergyLoss=SourceIon_V[4];

  for (int idim=0; idim<3; idim++) {
    *(3*0+idim + (double*)(offset+OH::Output::ohSourceMomentumOffset))+=(ForceSource[idim]-ForceLoss[idim])/PlasmaNumberDensity;
  }

  *(0+(double*)(offset+OH::Output::ohSourceEnergyOffset))+=(EnergySource-EnergyLoss)/PlasmaNumberDensity;


  //Accounting for how much is loss each time step
  WeightLoss=CenterNode->Measure*rate*ParentTimeStep;
  Exosphere::ChemicalModel::TotalLossRate[spec]+=WeightLoss/ParentTimeStep;

  if (ParentParticleWeight-WeightLoss<0.01*ParentParticleWeight) {

    //delete the particle if more is lost than initial weight:
     PIC::ParticleBuffer::DeleteParticle(ptr);
  }
  else {
    //update the weight correction factor of the particle
    ParentParticleWeight-=WeightLoss;
    PIC::ParticleBuffer::SetIndividualStatWeightCorrection(ParentParticleWeight/PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec],ptr);

    //add the particle to the list of the particles existing in the system after reaction
    PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
    PIC::ParticleBuffer::SetPrev(-1,ptr);

    if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
    FirstParticleCell=ptr;
  }

  //Generate ENA if needed
  //Init the ENA geberation tables
  static bool init_flag=false;
  static double MaxProductTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_SPEC_];
  static double MinProductStatWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_SPEC_];

  if (ProduceENAflag==true) {
  if (init_flag==false) {
    //init stuff needed for generating ENAa
    init_flag=true;

    //determine MinWeightOverTimeStep
    if (_H_ENA_V3_SPEC_>=0) {
      MinProductStatWeight=min(MinProductStatWeight,PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V3_SPEC_]);
      MaxProductTimeStep=max(MaxProductTimeStep,PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V3_SPEC_]);
    }

    if (_H_ENA_V2_SPEC_>=0) {
      MinProductStatWeight=min(MinProductStatWeight,PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V2_SPEC_]);
      MaxProductTimeStep=max(MaxProductTimeStep,PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V2_SPEC_]);
    }

    if (_H_ENA_V1_SPEC_>=0) {
      MinProductStatWeight=min(MinProductStatWeight,PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V1_SPEC_]);
      MaxProductTimeStep=max(MaxProductTimeStep,PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V1_SPEC_]);
    }
  }

  double anpart=WeightLoss/PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]*MaxProductTimeStep/MinProductStatWeight;
  int npart=floor(anpart);


  if (rnd()<anpart-npart) npart++;

  while (npart-->0) {
    ///generate velocity of the new particle
    double vp[3],speed;
    bool CreateNewParticle=false;
    double nNewparticles_d;
    int NewParticleSpec;


    OH::sampleVp(vp,vParent,PlasmaBulkVelocity,PlasmaTemperature,spec);
    speed=Vector3D::Length(vp);

    //determine species of the new particle
    if (_H_ENA_V3_SPEC_ >= 0 && speed >= 500.0E3) {
      if (rnd()<(nNewparticles_d=MinProductStatWeight/MaxProductTimeStep*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V3_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V3_SPEC_])) {
        CreateNewParticle=true;
        NewParticleSpec=_H_ENA_V3_SPEC_;
      }
    }
    else if (_H_ENA_V2_SPEC_ >=0 && speed>=150.0E3 && speed<500.0E3) {
      if (rnd()<(nNewparticles_d=MinProductStatWeight/MaxProductTimeStep*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V2_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V2_SPEC_])) {
        CreateNewParticle=true;
        NewParticleSpec=_H_ENA_V2_SPEC_;
      }
    }
    else if (_H_ENA_V1_SPEC_ >=0 && speed>=50.0E3 && speed<150.0E3) {
      if (rnd()<(nNewparticles_d=MinProductStatWeight/MaxProductTimeStep*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V1_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V1_SPEC_])) {
        CreateNewParticle=true;
        NewParticleSpec=_H_ENA_V1_SPEC_;
      }
    }
    else {
      if (rnd()<(nNewparticles_d=MinProductStatWeight/MaxProductTimeStep*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_SPEC_])) {
        CreateNewParticle=true;
        NewParticleSpec=_H_SPEC_;
      }
    }


    if (CreateNewParticle==true) {
      long int new_ptr;
      int nNewparticles=nNewparticles_d;
      PIC::ParticleBuffer::byte *NewParticleData;

      if (nNewparticles_d<1.0) {
        nNewparticles=1;
      }
      else {
        nNewparticles=nNewparticles_d;

        nNewparticles_d-=nNewparticles;
        if (rnd()<nNewparticles_d) nNewparticles++;
      }

      Exosphere::ChemicalModel::TotalSourceRate[NewParticleSpec]+=nNewparticles*PIC::ParticleWeightTimeStep::GlobalParticleWeight[NewParticleSpec]/PIC::ParticleWeightTimeStep::GlobalTimeStep[NewParticleSpec];

      for (int np=0;np<nNewparticles;np++) {
        new_ptr=PIC::ParticleBuffer::GetNewParticle();
        NewParticleData=PIC::ParticleBuffer::GetParticleDataPointer(new_ptr);

        PIC::ParticleBuffer::CloneParticle(NewParticleData,ParticleData);
        PIC::ParticleBuffer::SetV(vp,NewParticleData);
        PIC::ParticleBuffer::SetI(NewParticleSpec,NewParticleData);
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,NewParticleData);

        // tagging the particle to the right population that it was created
        OH::SetOriginTag(OH::GetEnaOrigin(PlasmaNumberDensity,PlasmaPressure,PlasmaBulkVelocity), NewParticleData);

        //add the particle to the list of the particles existing in the system after reaction
        PIC::ParticleBuffer::SetNext(FirstParticleCell,NewParticleData);
        PIC::ParticleBuffer::SetPrev(-1,NewParticleData);

        if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(new_ptr,FirstParticleCell);
        FirstParticleCell=new_ptr;
      }
    }
    }
  }

  ReactionProcessorTimer.UpdateTimer();
}


//----------------------------------------------------------------------
void OH::Loss::ReactionProcessor(long int ptr,long int& FirstParticleCell,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node){
  //as a result of the reaction only velocity of a particle is changed
  //----------------------------------------------------------------------

  //the life time of the original particle
  int spec;
  PIC::ParticleBuffer::byte *ParticleData;
  double xParent[3],vParent[3],ParentLifeTime,ParentTimeStep;
  bool ReactionOccurredFlag;

  ReactionProcessorTimer.Start();

  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  spec=PIC::ParticleBuffer::GetI(ParticleData);
  PIC::ParticleBuffer::GetX(xParent,ParticleData);
  PIC::ParticleBuffer::GetV(vParent,ParticleData);

  ParentLifeTime=LifeTime(xParent,spec,ptr,ReactionOccurredFlag,node);

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
  ParentTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
#else
  ParentTimeStep=0.0;
  exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif
  double WeightLoss,tWeightQuantumLoss;

  double InitWeightQuantum=0.001;
  double WeightQuantum=InitWeightQuantum;
  double EventLimiter=10;


  WeightLoss=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData)*(1.0-exp(-ParentTimeStep/ParentLifeTime));
  tWeightQuantumLoss=(int)(WeightLoss/WeightQuantum);  

  if (tWeightQuantumLoss>EventLimiter) {
    WeightQuantum*=tWeightQuantumLoss/EventLimiter;
    tWeightQuantumLoss=EventLimiter;
  }



  int nWeightQuantumLoss=(int)tWeightQuantumLoss;

  tWeightQuantumLoss-=nWeightQuantumLoss; 
  if (rnd()<tWeightQuantumLoss) nWeightQuantumLoss++; 

  PIC::CPLR::InitInterpolationStencil(xParent,node);

  double vp[3];
    double PlasmaBulkVelocity[3];
    double PlasmaNumberDensity, PlasmaPressure, PlasmaTemperature;


auto SimulateReaction = [&] (double ParticleWeightCorrection) { 
    //inject the products of the reaction
    double ParentTimeStep,ParentParticleWeight;

    // new particle comes from solar wind and has random velocity from proton Maxwellian distribution
//    double PlasmaBulkVelocity[3];
//    double PlasmaNumberDensity, PlasmaPressure, PlasmaTemperature;


//    PIC::CPLR::InitInterpolationStencil(xParent,node);

    //determine with shich ions fluid that partice will interact with
    int ifluid_interact=0;
    int ifluid_contribute=0;

    //determine the ion fluid for charge exchange
    if (PIC::CPLR::SWMF::nCommunicatedIonFluids>1) {
      double FrequencyTable[PIC::CPLR::SWMF::nCommunicatedIonFluids],TotalFrequency,cnt=0.0,cnt_max;
      bool PhotolyticReactionAllowedFlag;

      TotalFrequency=GetFrequencyTable(FrequencyTable,xParent,spec,ptr,PhotolyticReactionAllowedFlag,node);

      cnt_max=rnd()*TotalFrequency;

      for (ifluid_interact=0;ifluid_interact<PIC::CPLR::SWMF::nCommunicatedIonFluids;ifluid_interact++) {
        cnt+=FrequencyTable[ifluid_interact];
        if (cnt>=cnt_max) break;
      }

      if (ifluid_interact==PIC::CPLR::SWMF::nCommunicatedIonFluids) ifluid_interact=PIC::CPLR::SWMF::nCommunicatedIonFluids-1;
    }

    //determine the ion fluid where the new ion would contributed 
    //the ion fluid is determined such that abs(v-u_f)/vth_f is minimized
    if (PIC::CPLR::SWMF::nCommunicatedIonFluids>1) { 
      double tt,t,tmin=-1.0;
        
      for (int ifluid=0;ifluid<PIC::CPLR::SWMF::nCommunicatedIonFluids;ifluid++) {
        PIC::CPLR::GetBackgroundPlasmaVelocity(ifluid,PlasmaBulkVelocity);
        PlasmaNumberDensity = PIC::CPLR::GetBackgroundPlasmaNumberDensity(ifluid);
        PlasmaPressure      = PIC::CPLR::GetBackgroundPlasmaPressure(ifluid);
        PlasmaTemperature   = PlasmaPressure / (2*Kbol * PlasmaNumberDensity);

        for (int idim=0;idim<3;idim++) {
          tt=PlasmaBulkVelocity[idim]-vParent[idim];
          t+=tt*tt;
        }

        t/=PlasmaTemperature;
        if ((tmin<0.0)||(tmin>t)) tmin=t,ifluid_contribute=ifluid; 
      }  
    }



#if  _SIMULATION_PARTICLE_WEIGHT_MODE_ == _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_
    ParentParticleWeight=ParticleWeightCorrection*PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
#else
    ParentParticleWeight=0.0;
    exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
#endif

#if _SIMULATION_TIME_STEP_MODE_ == _SPECIES_DEPENDENT_GLOBAL_TIME_STEP_
    ParentTimeStep=PIC::ParticleWeightTimeStep::GlobalTimeStep[spec];
#else
    ParentTimeStep=0.0;
    exit(__LINE__,__FILE__,"Error: the time step node is not defined");
#endif

    //account for the parent particle correction factor
    ParentParticleWeight*=WeightQuantum;

    // calculating the random velocity of the proton from the maxwellian velocity of the local plasma
    PIC::CPLR::GetBackgroundPlasmaVelocity(ifluid_interact,PlasmaBulkVelocity);
    PlasmaNumberDensity = PIC::CPLR::GetBackgroundPlasmaNumberDensity(ifluid_interact);
    PlasmaPressure      = PIC::CPLR::GetBackgroundPlasmaPressure(ifluid_interact);
    PlasmaTemperature   = PlasmaPressure / (2*Kbol * PlasmaNumberDensity);

    if ((isfinite(PlasmaNumberDensity)==false)||(isfinite(PlasmaPressure)==false)||(isfinite(PlasmaTemperature)==false)) exit(__LINE__,__FILE__);

    OH::sampleVp(vp,vParent,PlasmaBulkVelocity,PlasmaTemperature,spec);

    // charge exchange process transfers momentum and energy to plasma
    PIC::Mesh::cDataCenterNode *CenterNode;
    char *offset;

    CenterNode=PIC::Mesh::Search::FindCell(xParent,node); ///node->block->GetCenterNode(nd);
    offset=CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset;

    double vh2 = 0.0, vp2 = 0.0;

//    double c = ParentParticleWeight/PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]/CenterNode->Measure;
    double c = (PlasmaNumberDensity>0.0) ? ParentParticleWeight/(PIC::ParticleWeightTimeStep::GlobalTimeStep[spec]*PlasmaNumberDensity)/CenterNode->Measure : 0.0;

    *(ifluid_interact+(double*)(offset+OH::Output::ohSourceDensityOffset))-=c*_MASS_(_H_);
    *(ifluid_contribute+(double*)(offset+OH::Output::ohSourceDensityOffset))+=c*_MASS_(_H_);

    for (int idim=0; idim<3; idim++) {
      *(3*ifluid_interact+idim + (double*)(offset+OH::Output::ohSourceMomentumOffset))-=c*_MASS_(_H_)*vp[idim];
      *(3*ifluid_contribute+idim + (double*)(offset+OH::Output::ohSourceMomentumOffset))+=c*_MASS_(_H_)*vParent[idim];

      vh2+=vParent[idim]*vParent[idim];
      vp2+=vp[idim]*vp[idim];
    }

    if ((isfinite(c)==false)||(isfinite(vh2)==false)||(isfinite(vp2)==false)) exit(__LINE__,__FILE__);

    *(ifluid_interact+(double*)(offset+OH::Output::ohSourceEnergyOffset))-=c*0.5*_MASS_(_H_)*vp2;
    *(ifluid_contribute+(double*)(offset+OH::Output::ohSourceEnergyOffset))+=c*0.5*_MASS_(_H_)*vh2;



    if ((isfinite(vp[0])==false)||(isfinite(vp[1])==false)||(isfinite(vp[2])==false)) exit(__LINE__,__FILE__,"Error: out of range");  


    //consider a possibility ofr clearing a new particle 
    bool CreateNewParticle=false;
    int NewParticleSpec;
    double nNewparticles_d;

    // adding neutral to correct species depending on its velocity
    if (_H_ENA_V3_SPEC_ >= 0 && sqrt(vp2) >= 500.0E3) {
      if (rnd()<(nNewparticles_d=ParentParticleWeight/ParentTimeStep*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V3_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V3_SPEC_])) {
        CreateNewParticle=true;
        NewParticleSpec=_H_ENA_V3_SPEC_;  
      }
    }
    else if (_H_ENA_V2_SPEC_ >=0 && sqrt(vp2)>=150.0E3 && sqrt(vp2)<500.0E3) {
      if (rnd()<(nNewparticles_d=ParentParticleWeight/ParentTimeStep*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V2_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V2_SPEC_])) {
        CreateNewParticle=true;
        NewParticleSpec=_H_ENA_V2_SPEC_;
      }
    }
    else if (_H_ENA_V1_SPEC_ >=0 && sqrt(vp2)>=50.0E3 && sqrt(vp2)<150.0E3) {
      if (rnd()<(nNewparticles_d=ParentParticleWeight/ParentTimeStep*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_ENA_V1_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_ENA_V1_SPEC_])) {
        CreateNewParticle=true;
        NewParticleSpec=_H_ENA_V1_SPEC_;
      }
    }
    else {
      if (rnd()<(nNewparticles_d=ParentParticleWeight/ParentTimeStep*PIC::ParticleWeightTimeStep::GlobalTimeStep[_H_SPEC_]/PIC::ParticleWeightTimeStep::GlobalParticleWeight[_H_SPEC_])) {
        CreateNewParticle=true;
        NewParticleSpec=_H_SPEC_;
      }
    }

    if (CreateNewParticle==true) {
      long int new_ptr;
      int nNewparticles=nNewparticles_d;
      PIC::ParticleBuffer::byte *NewParticleData;

      if (nNewparticles_d<1.0) {
        nNewparticles=1;
      }
      else {
        nNewparticles=nNewparticles_d; 

        nNewparticles_d-=nNewparticles;
        if (rnd()<nNewparticles_d) nNewparticles++; 
      }

      Exosphere::ChemicalModel::TotalSourceRate[NewParticleSpec]+=nNewparticles*PIC::ParticleWeightTimeStep::GlobalParticleWeight[NewParticleSpec]/PIC::ParticleWeightTimeStep::GlobalTimeStep[NewParticleSpec];

      for (int np=0;np<nNewparticles;np++) { 
        new_ptr=PIC::ParticleBuffer::GetNewParticle();
        NewParticleData=PIC::ParticleBuffer::GetParticleDataPointer(new_ptr);

        PIC::ParticleBuffer::CloneParticle(NewParticleData,ParticleData);
        PIC::ParticleBuffer::SetV(vp,NewParticleData); 
        PIC::ParticleBuffer::SetI(NewParticleSpec,NewParticleData);
        PIC::ParticleBuffer::SetIndividualStatWeightCorrection(1.0,NewParticleData);
  
        // tagging the particle to the right population that it was created
        OH::SetOriginTag(OH::GetEnaOrigin(PlasmaNumberDensity,PlasmaPressure,PlasmaBulkVelocity), NewParticleData);

        //add the particle to the list of the particles existing in the system after reaction
        PIC::ParticleBuffer::SetNext(FirstParticleCell,NewParticleData);
        PIC::ParticleBuffer::SetPrev(-1,NewParticleData);

        if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(new_ptr,FirstParticleCell);
        FirstParticleCell=new_ptr;
      }
    }
  };


  for (int i=0;i<nWeightQuantumLoss;i++) SimulateReaction(PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData));

  //determine whether the original particle need to be deleted
  double new_weight_correction=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData)*exp(-ParentTimeStep/ParentLifeTime);
  double ParentParticleWeight;

  switch (_SIMULATION_PARTICLE_WEIGHT_MODE_) {
  case _SPECIES_DEPENDENT_GLOBAL_PARTICLE_WEIGHT_: 
    ParentParticleWeight=PIC::ParticleWeightTimeStep::GlobalParticleWeight[spec];
    break;
  default:
    exit(__LINE__,__FILE__,"Error: the weight mode is node defined");
  }

  Exosphere::ChemicalModel::TotalLossRate[spec]+=ParentParticleWeight*(PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData)-new_weight_correction)/ParentTimeStep;

  if (new_weight_correction<InitWeightQuantum) {
    PIC::ParticleBuffer::DeleteParticle(ptr);
  }
  else {
    PIC::ParticleBuffer::SetIndividualStatWeightCorrection(new_weight_correction,ParticleData);

    //add the particle to the list of the particles existing in the system after reaction
    PIC::ParticleBuffer::SetNext(FirstParticleCell,ptr);
    PIC::ParticleBuffer::SetPrev(-1,ptr);

    if (FirstParticleCell!=-1) PIC::ParticleBuffer::SetPrev(ptr,FirstParticleCell);
    FirstParticleCell=ptr;
  }

  ReactionProcessorTimer.UpdateTimer();
}


void OH::FinalizeSimulation() {
  //print timing 
  ReactionProcessorTimer.PrintMeanMPI("$PREFIX: time used by OH::Loss::ReactionProcessor()");

  PIC::Debugger::Timer.PrintSampledDataMPI();
}

void OH::Init_BeforeParser(){
  OH::InitPhysicalSpecies();
  Exosphere::Init_BeforeParser();
  PIC::ParticleBuffer::RequestDataStorage(OH::OffsetOriginTag, sizeof(int));
  OH::Output::Init();

  //set the coupling procedure
  PIC::CPLR::SWMF::SendCenterPointData.push_back(Coupling::Send);

  //request sampling data
  PIC::IndividualModelSampling::RequestSamplingData.push_back(Sampling::OriginLocation::RequestSamplingData);

}

void OH::Init_AfterParser(){
  if (OH::Sampling::LymanAlpha::LymanAlphaSampleDirectionTableLength!=0) {
    OH::Sampling::LymanAlpha::Init();
    PIC::Sampling::ExternalSamplingLocalVariables::RegisterSamplingRoutine(OH::Sampling::LymanAlpha::Sampling,OH::Sampling::LymanAlpha::OutputSampledData);
  }
}


// User defined functions -----------------------------------------------------
int OH::user_set_face_boundary(long int ptr,double* xInit,double* vInit,int nIntersectionFace,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>  *startNode) {
  //setting User defined function to process particles leaving domain at certain faces
  //useful for 1D runs if just want flows in one direction

  int res;

  // removing particles if hit faces perpandiulat to x axis
  if (nIntersectionFace == 0 || nIntersectionFace == 1) res=_PARTICLE_DELETED_ON_THE_FACE_; 

  // keep and reflect particles if hit face perpandiculat to y or z axes
  // y axis reflection
  if (nIntersectionFace == 2 || nIntersectionFace == 3) {
    vInit[1]=-vInit[1];
    res=_PARTICLE_REJECTED_ON_THE_FACE_; // particles are not deleted but remain in domain
  }

  // z axis reflection
  if (nIntersectionFace == 4 || nIntersectionFace == 5) {
    vInit[2]=-vInit[2];
    res=_PARTICLE_REJECTED_ON_THE_FACE_;
  }

  return res;
}

//-----------------------------------------------------------------------------
//substitutes for Exosphere functions
char Exosphere::ObjectName[_MAX_STRING_LENGTH_PIC_],Exosphere::IAU_FRAME[_MAX_STRING_LENGTH_PIC_],Exosphere::SO_FRAME[_MAX_STRING_LENGTH_PIC_];

//calcualte the true anomaly angle
double Exosphere::OrbitalMotion::GetTAA(SpiceDouble et) {
  return 0.0;
}

int Exosphere::ColumnIntegral::GetVariableList(char *vlist) {
  int spec,nVariables=0;

  //column density
  for (spec=0;spec<PIC::nTotalSpecies;spec++) {
    if (vlist!=NULL) sprintf(vlist,"%s,  \"Column Integral(%s)\"",vlist,PIC::MolecularData::GetChemSymbol(spec));
    nVariables+=1;
  }

  return nVariables;
}

void Exosphere::ColumnIntegral::CoulumnDensityIntegrant(double *res,int resLength,double* x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
  int i,j,k,nd,cnt=0,spec;
  double NumberDensity;


  nd=PIC::Mesh::mesh->FindCellIndex(x,i,j,k,node);
  for (i=0;i<resLength;i++) res[i]=0.0;

  for (spec=0;spec<PIC::nTotalSpecies;spec++) {
    //get the local density number
    NumberDensity=node->block->GetCenterNode(nd)->GetNumberDensity(spec);
    res[cnt++]=NumberDensity;
    //    res[cnt++]=NumberDensity*node->block->GetCenterNode(nd)->GetMeanParticleSpeed(spec);
  }


  if (cnt!=resLength) exit(__LINE__,__FILE__,"Error: the length of the vector is not coinsistent with the number of integrated variables");
}

void Exosphere::ColumnIntegral::ProcessColumnIntegrationVector(double *res,int resLength) {
  //do nothing
}

double Exosphere::SurfaceInteraction::StickingProbability(int spec,double& ReemissionParticleFraction,double Temp) {
  ReemissionParticleFraction=0.0;

  return 1.0;
}


double Exosphere::GetSurfaceTemperature(double cosSubsolarAngle,double *x_LOCAL_SO_OBJECT) {


  return 100.0;
}

void OH::InitializeParticleWithEnaOriginTag(long int ptr, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *startNode, int iInjectionMode){


  PIC::ParticleBuffer::byte *ParticleData;
  double x[3];

  // find the particle position
  ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);
  PIC::ParticleBuffer::GetX(x,ParticleData);

  // get the backgroudn paramters
  double PlasmaBulkVelocity[3];
  double PlasmaNumberDensity, PlasmaPressure;
  PIC::CPLR::InitInterpolationStencil(x, startNode);
  PIC::CPLR::GetBackgroundPlasmaVelocity(PlasmaBulkVelocity);
  PlasmaNumberDensity = PIC::CPLR::GetBackgroundPlasmaNumberDensity();
  PlasmaPressure      = PIC::CPLR::GetBackgroundPlasmaPressure();

  // assign the origin tag to the particle
  OH::SetOriginTag(OH::GetEnaOrigin(PlasmaNumberDensity,PlasmaPressure,PlasmaBulkVelocity), ParticleData);

}


int OH::GetEnaOrigin(double PlasmaNumberDensity, double PlasmaPressure, double *PlasmaBulkVelocity){
  // determines which population the ENA should be added to based on where in the heliosphere it was created
  // 0 = SSSW
  // 1 = inner heliosheath
  // 2 = disturbed ISM / Outer heliosheath
  // 3 = Prisine ISM

  double mach=0.0;
  double PlasmaBulkSpeed=0.0,PlasmaBulkSpeed2=0.0;
  double PlasmaTemperature=0.0;

  PlasmaTemperature   = PlasmaPressure / (2*Kbol * PlasmaNumberDensity);

  for(int idim=0; idim<3; idim++) PlasmaBulkSpeed2 += PlasmaBulkVelocity[idim]*PlasmaBulkVelocity[idim];
  PlasmaBulkSpeed = sqrt(PlasmaBulkSpeed2);

  mach=PlasmaBulkSpeed*sqrt(3.0*PlasmaNumberDensity*_MASS_(_H_)/(5.0*PlasmaPressure));

  // adding neutral to correct population where it underwent charge exchange
  if (mach >= 1.0 && PlasmaBulkSpeed < 100.0E3) {
    // particle is in the pristine ISM - population 4
    return 3;
  }
  else if (mach < 1.0 && PlasmaTemperature < 3.28E5 && PlasmaBulkSpeed < 100.0E3) {
    // particle is in the disturbed ISM / Outer heliosheath - population 3
    return 2;
  }
  else if (mach < 1.0 && PlasmaTemperature >= 3.28E5) {
    // particle is in the HS / inner heliosheath - population 2
    return 1;
  }
  else {
    // particle is in the super sonic SW - population 1
    return 0;
  }
}

// the distribution to select the proton particle is taken from eq 2.13 of Malama 1991                                   
double OH::VpDistribution(double *vp, double *vh, double *up, double vth)
{
  double erel = 0.5*1.674E-27*((vh[0]-vp[0])*(vh[0]-vp[0])+(vh[1]-vp[1])*(vh[1]-vp[1])+(vh[2]-vp[2])*(vh[2]-vp[2]))*6.2415E15; // in keV

  if (erel==0.0) return 0.0;

  double sigma = (4.15-0.531*log(erel))*(4.15-0.531*log(erel))*pow(1-exp(-67.3/erel),4.5)*1E-20; // cross section in m^2

  return sqrt((vh[0]-vp[0])*(vh[0]-vp[0])+(vh[1]-vp[1])*(vh[1]-vp[1])+(vh[2]-vp[2])*(vh[2]-vp[2]))*sigma*exp(-((vp[0]-up[0])*(vp[0]-up[0])+(vp[1]-up[1])*(vp[1]-up[1])+(vp[2]-up[2])*(vp[2]-up[2]))/(vth*vth)); // velocity in m/s
}

// sampling the 3D distribution function from Malama 1991 using the Accept-Reject Method
#if _AVX_INSTRUCTIONS_USAGE_MODE_ == _AVX_INSTRUCTIONS_USAGE_MODE__256_
void OH::sampleVp(double *vp, double *vh, double *up, double tp, int spec) {
  float vth = sqrt(2.0*Kbol*tp/PIC::MolecularData::GetMass(spec));

  //f=|urel|*sigma*expt(-v^2)
  //parameter of the exponent
  const float limit=1.6;
  const float limit2=limit*limit;

  const static __m128 width_v=_mm_setr_ps(2.0*limit,2.0*limit,2.0*limit,0.0);
  const static __m128 shift_v=_mm_setr_ps(limit,limit,limit,0.0);

  __m128 vp_v,rnd_v,v_v,v2_v;
  float c1,pp,ppmax,sigma,erel,v2,urel2,urel;

  __m128 vh_v=_mm_setr_ps((vh[0]-up[0])/vth,(vh[1]-up[1])/vth,(vh[2]-up[2])/vth,0.0);

  float vth2=vth*vth;
  static const float c=0.5*1.674E-27*6.2415E15;

  //evaluate ppmax
  v2=1.0/4;
  urel=1.0/2.0+sqrt(vh_v[0]*vh_v[0]+vh_v[1]*vh_v[1]+vh_v[2]*vh_v[2]);
  urel2=urel*urel;
  erel=c*urel2*vth2;
  c1=4.15f-0.531f*logf(erel);
  sigma=c1*c1*powf(1.0f-expf(-67.3f/erel),4.5f); //*1E-20; // cross section in m^2

  ppmax=urel*sigma*exp(-v2);

  do {
    do {
      rnd_v=_mm_setr_ps(rnd(),rnd(),rnd(),0.0);
      vp_v=_mm_fmsub_ps(rnd_v,width_v,shift_v);

      //parameter of the exponent
      v2_v=_mm_dp_ps(vp_v,vp_v,0xff);
    }
    while (v2_v[0]>limit2);

    v2=v2_v[0];

    //relative speed
    v_v=_mm_sub_ps(vh_v,vp_v);
    v2_v=_mm_dp_ps(v_v,v_v,0xff);
    _mm_store_ss(&urel2,v2_v);

    urel=sqrtf(urel2);

    erel=c*urel2*vth2;
    c1=4.15f-0.531f*logf(erel);
    sigma=c1*c1*powf(1.0f-expf(-67.3f/erel),4.5f); //*1E-20; // cross section in m^2

    pp=urel*sigma*expf(-v2);
  }
  while (rnd()>pp/ppmax);

  alignas(64) float vhf[3];

  _mm_store_ps(vhf,vh_v);

  vp[0]=vhf[0]*vth+up[0];
  vp[1]=vhf[1]*vth+up[1];
  vp[2]=vhf[2]*vth+up[2];
}
#else
void OH::sampleVp(double *vp, double *vh, double *up, double tp, int spec)
{
  int accepted = 0;
  double vth = sqrt(2.0*Kbol*tp/PIC::MolecularData::GetMass(spec));
  
  // M (holds normalization costants for both distributions), g(vp) is the maxwellian distribution
  double sup[3] = {up[0]-3.*vth,up[1]-3.*vth,up[2]-3.*vth};
  double M=OH::VpDistribution(sup,vh,up,vth)/exp(-((sup[0]-up[0])*(sup[0]-up[0])+(sup[1]-up[1])*(sup[1]-up[1])+(sup[2]-up[2])*(sup[2]-up[2]))/(vth*vth));

  while (accepted < 1) {
    // generating vp as sampled from g(vp)
    PIC::Distribution::MaxwellianVelocityDistribution(vp,up,tp,spec);

    if (rnd() < OH::VpDistribution(vp,vh,up,vth)/(M*exp(-((vp[0]-up[0])*(vp[0]-up[0])+(vp[1]-up[1])*(vp[1]-up[1])+(vp[2]-up[2])*(vp[2]-up[2]))/(vth*vth))))
    {
      accepted = 1;
    }
  }
}
#endif
//=====================================================================================================
//sampling of the ENAs density individually for each origin region
int OH::Sampling::OriginLocation::nSampledOriginLocations=-1;
int OH::Sampling::OriginLocation::OffsetDensitySample=-1;
int OH::Sampling::OriginLocation::OffsetVelocitySample=-1;
int OH::Sampling::OriginLocation::OffsetV2Sample=-1;

int OH::Sampling::OriginLocation::RequestSamplingData(int offset) {
  int res=0;

  if (nSampledOriginLocations!=-1) {
    OffsetDensitySample=offset;
    OffsetVelocitySample=offset+(nSampledOriginLocations+1)*sizeof(double);
    OffsetV2Sample=offset+4.0*(nSampledOriginLocations+1)*sizeof(double);
    res=(3+1+3)*(nSampledOriginLocations+1)*sizeof(double);
  }

  return res;
}

void OH::Sampling::OriginLocation::SampleParticleData(char *ParticleData,double LocalParticleWeight,char  *SamplingBuffer,int spec) {
  int OriginID;
  double *v;
  double v2[3]={0.0,0.0,0.0};

  if (nSampledOriginLocations!=-1) {
    OriginID=OH::GetOriginTag((PIC::ParticleBuffer::byte*)ParticleData);
    v=PIC::ParticleBuffer::GetV((PIC::ParticleBuffer::byte*)ParticleData);
    for (int idim=0; idim<3; idim++) v2[idim]=v[idim]*v[idim];

    if (OriginID>=nSampledOriginLocations) {
      char msg[500];

      sprintf(msg,"$PREFIX:Error: OriginID is out of range. Update the input file with the corrected value of the number of the source regions");
      sprintf(msg,"%s\n$PREFIX:Error: OriginID=%i",msg,OriginID);
      sprintf(msg,"%s\n$PREFIX:Error: nSampledOriginLocations=%i\n",msg,nSampledOriginLocations);

      exit(__LINE__,__FILE__,msg);
    }

    // sampling based on origin location
    *(OriginID+(nSampledOriginLocations+1)*OH::PhysSpec[spec]+(double*)(SamplingBuffer+OffsetDensitySample))+=LocalParticleWeight;
    for (int i=0; i<3; i++) *(i+3*OriginID+(nSampledOriginLocations+1)*OH::PhysSpec[spec]+(double*)(SamplingBuffer+OffsetVelocitySample))+=v[i]*LocalParticleWeight;
    for (int j=0; j<3; j++) *(j+3*OriginID+(nSampledOriginLocations+1)*OH::PhysSpec[spec]+(double*)(SamplingBuffer+OffsetV2Sample))+=v2[j]*LocalParticleWeight;

    // sampling based on total solution
    *(nSampledOriginLocations+(nSampledOriginLocations+1)*OH::PhysSpec[spec]+(double*)(SamplingBuffer+OffsetDensitySample))+=LocalParticleWeight;
    for (int i=0; i<3; i++) *(i+3*nSampledOriginLocations+(nSampledOriginLocations+1)*OH::PhysSpec[spec]+(double*)(SamplingBuffer+OffsetVelocitySample))+=v[i]*LocalParticleWeight;
    for (int j=0; j<3; j++) *(j+3*nSampledOriginLocations+(nSampledOriginLocations+1)*OH::PhysSpec[spec]+(double*)(SamplingBuffer+OffsetV2Sample))+=v2[j]*LocalParticleWeight;
  }
}


int OH::nPhysSpec=-1;
int OH::PhysSpec[PIC::nTotalSpecies];
bool OH::DoPrintSpec[PIC::nTotalSpecies];

void OH::InitPhysicalSpecies(){
  double m0,m1,q0,q1;

  // holds the number of the index of each additional unique physical species
  OH::nPhysSpec=0;

  // resetting/initializing the array holding the physical species index 
  for (int spec=0; spec<PIC::nTotalSpecies; spec++){
    OH::PhysSpec[spec]=-1;
    OH::DoPrintSpec[spec]=false;
  }

  // loop that goes through all species in species list and compares them to the elements in the array to determine if they hold the same mass and charge to make them the same physical species
  //allows us to tag all hydrogen ENA species as 1 "physical" hydrogen species to loop over when sampling output
  for (int s0=0; s0<PIC::nTotalSpecies; s0++){
    m0=PIC::MolecularData::GetMass(s0);
    q0=PIC::MolecularData::GetElectricCharge(s0);

    //compares element to all previous elements to determine if same physical species
    for (int s1=0; s1<s0; s1++){
      m1=PIC::MolecularData::GetMass(s1);
      q1=PIC::MolecularData::GetElectricCharge(s1);

      if (m0 == m1 && q0 == q1){
        OH::PhysSpec[s0]=OH::PhysSpec[s1];
        break;
      }
    }
    // if there was no match to previous species then it is a new species
    if (OH::PhysSpec[s0]<0) {
      //setting index of new physical species to hold value of nPhysSpec
      OH::PhysSpec[s0]=OH::nPhysSpec;

      //setting the value of the array to print region sampling data of that physical species in this species data file
      OH::DoPrintSpec[s0]=true;

      // updating counter of number of species
      OH::nPhysSpec++;
    }

    // printing species list and the corresponding physical species tag
    if (PIC::ThisThread==0){
      cout << "Species Index: " << s0 << " -> Physical Species Index: " << OH::PhysSpec[s0] << " DoPrintSpec: " << DoPrintSpec[s0] << endl;
    }
  }
}
