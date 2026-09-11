//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf
//$Id$
//modeling of collision between the model particles

#include "pic.h"

//sampling offset of the collision frequentcy
int PIC::MolecularCollisions::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset=-1;

//the limit of the collisions per a single particles
double PIC::MolecularCollisions::ParticleCollisionModel::CollisionLimitingThrehold=20.0;

//a user-defined function to accept a cell for modeling collisions
PIC::MolecularCollisions::ParticleCollisionModel::fDoSimulateCellCollisions PIC::MolecularCollisions::ParticleCollisionModel::DoSimulateCellCollisions=NULL;



//request the sampling data
int PIC::MolecularCollisions::ParticleCollisionModel::RequestSamplingData(int offset) {
  int SamplingLength=0;

  if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
    CollsionFrequentcySampling::SamplingBufferOffset = 
        offset + SamplingLength;
    SamplingLength += 
        sizeof(double) * (PIC::nTotalSpecies * PIC::nTotalSpecies);
  }

  return SamplingLength;
}

void PIC::MolecularCollisions::ParticleCollisionModel::PrintVariableList(FILE* fout,int DataSetNumber) {
  int s;

  for (s=0;s<PIC::nTotalSpecies;s++) fprintf(fout,", \"Coll Freq(%s) [m^{-3} s^{-1}]\"",PIC::MolecularData::GetChemSymbol(s));
}

void PIC::MolecularCollisions::ParticleCollisionModel::Interpolate(PIC::Mesh::cDataCenterNode** InterpolationList,double *InterpolationCoeficients,int nInterpolationCoeficients,PIC::Mesh::cDataCenterNode *CenterNode)  {
  double CollisionFrequentcy[PIC::nTotalSpecies*PIC::nTotalSpecies];
  int i,s;
  double *SamplingBuffer;

  for (i=0;i<PIC::nTotalSpecies*PIC::nTotalSpecies;i++) CollisionFrequentcy[i]=0.0;

  for (i=0;i<nInterpolationCoeficients;i++) {
    SamplingBuffer=(double*)(InterpolationList[i]->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+CollsionFrequentcySampling::SamplingBufferOffset);

    for (s=0;s<PIC::nTotalSpecies*PIC::nTotalSpecies;s++) CollisionFrequentcy[s]+=SamplingBuffer[s]*InterpolationCoeficients[i];
  }

  SamplingBuffer=(double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+CollsionFrequentcySampling::SamplingBufferOffset);
  for (s=0;s<PIC::nTotalSpecies*PIC::nTotalSpecies;s++) SamplingBuffer[s]=CollisionFrequentcy[s]/((PIC::LastSampleLength!=0) ? PIC::LastSampleLength : 1);
}

void PIC::MolecularCollisions::ParticleCollisionModel::PrintData(FILE* fout,int DataSetNumber,CMPI_channel *pipe,int CenterNodeThread,PIC::Mesh::cDataCenterNode *CenterNode) {
  double t;
  double *SamplingBuffer=(double*)(CenterNode->GetAssociatedDataBufferPointer()+PIC::Mesh::completedCellSampleDataPointerOffset+CollsionFrequentcySampling::SamplingBufferOffset);
  int s;

  bool gather_output_data=false;

  if (pipe==NULL) gather_output_data=true;
  else if (pipe->ThisThread==CenterNodeThread) gather_output_data=true;


  for (s=0;s<PIC::nTotalSpecies;s++) {
    if (gather_output_data==true) { //(pipe->ThisThread==CenterNodeThread) {
      t= *(SamplingBuffer+CollsionFrequentcySampling::Offset(DataSetNumber,s));
    }

    if ((PIC::ThisThread==0)||(pipe==NULL)) { //if (pipe->ThisThread==0) {
      if ((CenterNodeThread!=0)&&(pipe!=NULL)) pipe->recv(t,CenterNodeThread);

      fprintf(fout,"%e ",t);
    }
    else pipe->send(t);
  }
}

void PIC::MolecularCollisions::ParticleCollisionModel::Init() {
  if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
    //request sampling buffer and particle fields
    PIC::IndividualModelSampling::RequestSamplingData.push_back(RequestSamplingData);

    //print out of the otuput file
    PIC::Mesh::AddVaraibleListFunction(PrintVariableList);
    PIC::Mesh::PrintDataCenterNode.push_back(PrintData);
    PIC::Mesh::InterpolateCenterNode.push_back(Interpolate);
  }
}


//Non-time Counter collision procedure
//Modeling collision in a cell
/*!
 * Simplified version of the Non-time Counter (NTC) collision model implementation
 * This version assumes:
 * 1. All species have the same time step
 * 2. All species have the same particle weight
 * 3. No individual particle weight corrections (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ is off)
 */
void PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc_simplified(
    int i, int j, int k, 
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
    
  const int SigmaCrMax_nTest = 100;
  const double SigmaCrMax_SafetyMargin = 1.3;

  // Structure to store particle data
  struct cParticleDataList {
    double *v;  // Direct pointer to velocity state vector
    PIC::ParticleBuffer::byte *ParticleData;
  };

  // Get block and check validity
  PIC::Mesh::cDataBlockAMR *block = node->block;
  if (block == NULL) return;

  // Get cell and check validity
  PIC::Mesh::cDataCenterNode *cell = block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
  if (cell == NULL) return;

  // Get first particle in cell and cell measure
  long int FirstCellParticle = block->FirstCellParticleTable[i + _BLOCK_CELLS_X_*(j + _BLOCK_CELLS_Y_*k)];
  double cellMeasure = cell->Measure;

  // Get sampling data buffer if sampling mode is on
  char *SamplingData = NULL;
  if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
    SamplingData = cell->GetAssociatedDataBufferPointer() + PIC::Mesh::collectingCellSampleDataPointerOffset;
  }

  if (FirstCellParticle == -1) return;

  // Count particles per species
  int nParticleNumber[PIC::nTotalSpecies] = {0};
  for (long int ptr = FirstCellParticle; ptr != -1; ptr = PIC::ParticleBuffer::GetNext(ptr)) {
    nParticleNumber[PIC::ParticleBuffer::GetI(ptr)]++;
  }

  // Find maximum number of particles among all species
  int ParticleDataListLength = 0;
  for (int s = 0; s < PIC::nTotalSpecies; s++) {
    if (ParticleDataListLength < nParticleNumber[s]) {
      ParticleDataListLength = nParticleNumber[s];
    }
  }

  // Allocate particle lists
  cParticleDataList *s0ParticleDataList = new cParticleDataList[ParticleDataListLength];
  cParticleDataList *s1ParticleDataList = new cParticleDataList[ParticleDataListLength];

  // Loop through first species s0
  for (int s0 = 0; s0 < PIC::nTotalSpecies; s0++) {
    if (nParticleNumber[s0] == 0 || PIC::MolecularData::GetSpecieType(s0) != _PIC_SPECIE_TYPE__GAS_) {
      continue;
    }

    double m0 = PIC::MolecularData::GetMass(s0);
    double LocalTimeStep = block->GetLocalTimeStep(s0);  // Same for all species
    double LocalParticleWeight = block->GetLocalParticleWeight(s0);  // Same for all species

    // Populate s0 particle list
    cParticleDataList *s0List = s0ParticleDataList;
    int cnt = 0;
    for (long int ptr = FirstCellParticle; ptr != -1; ptr = PIC::ParticleBuffer::GetNext(ptr)) {
      if (PIC::ParticleBuffer::GetI(ptr) == (unsigned)s0) {
        PIC::ParticleBuffer::byte *ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
        s0List[cnt].ParticleData = ParticleData;
        s0List[cnt].v = PIC::ParticleBuffer::GetV(ParticleData);
        cnt++;
      }
    }

    // Loop through second species s1
    for (int s1 = s0; s1 < PIC::nTotalSpecies; s1++) {
      if (nParticleNumber[s1] == 0 || PIC::MolecularData::GetSpecieType(s1) != _PIC_SPECIE_TYPE__GAS_) {
        continue;
      }

      // Check if collisions should be simulated for this pair
      if (DoSimulateCellCollisions != NULL) {
        if (!DoSimulateCellCollisions(cell, s0, s1)) continue;
      }

      double m1 = PIC::MolecularData::GetMass(s1);

      // Populate s1 particle list (if different from s0)
      cParticleDataList *s1List = (s0 == s1) ? s0List : s1ParticleDataList;
      if (s0 != s1) {
        cnt = 0;
        for (long int ptr = FirstCellParticle; ptr != -1; ptr = PIC::ParticleBuffer::GetNext(ptr)) {
          if (PIC::ParticleBuffer::GetI(ptr) == (unsigned)s1) {
            PIC::ParticleBuffer::byte *ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
            s1List[cnt].ParticleData = ParticleData;
            s1List[cnt].v = PIC::ParticleBuffer::GetV(ParticleData);
            cnt++;
          }
        }
      }

      // Calculate maximum collision cross section
      double SigmaCrMax = 0.0;
      for (int ntest = 0; ntest < SigmaCrMax_nTest; ntest++) {
        int s0ptr = (int)(rnd()*nParticleNumber[s0]);
        int s1ptr = (int)(rnd()*nParticleNumber[s1]);
        
        double cr = Vector3D::Distance(s1List[s1ptr].v,s0List[s0ptr].v); 
        double SigmaCr = cr * PIC::MolecularData::MolecularModels::GetTotalCrossSection(
            s0, s0List[s0ptr].v, s1, s1List[s1ptr].v);
        
        if (SigmaCr > SigmaCrMax) SigmaCrMax = SigmaCr;
      }
      
      SigmaCrMax *= SigmaCrMax_SafetyMargin;

      // Calculate number of collision pairs
      double ancoll;
      if (s0 == s1) {
        ancoll = 0.5 * nParticleNumber[s0] * (nParticleNumber[s0] /*- 1*/) * 
                 LocalParticleWeight * SigmaCrMax * LocalTimeStep / cellMeasure;
      } else {
        ancoll = nParticleNumber[s0] * nParticleNumber[s1] * 
                 LocalParticleWeight * SigmaCrMax * LocalTimeStep / cellMeasure;
      }

      // Apply collision limiting if needed
      double CollisionLimitingFactor = 1.0;
      if (nParticleNumber[s0] * nParticleNumber[s1] < ancoll) {
        CollisionLimitingFactor = ancoll / (nParticleNumber[s0] * nParticleNumber[s1]);
        ancoll /= CollisionLimitingFactor;
      }

      // Determine actual number of collisions
      long int ncoll = (long int)ancoll;
      if (rnd() < (ancoll - ncoll)) ncoll++;

      // Simulate collisions
      while (ncoll-- > 0) {
        // Select random particle pairs
        int s0ptr = (int)(rnd() * nParticleNumber[s0]);
        int s1ptr;
        do {
          s1ptr = (int)(rnd() * nParticleNumber[s1]);
        } while (s0 == s1 && s0ptr == s1ptr);

        double *v0 = s0List[s0ptr].v;
        double *v1 = s1List[s1ptr].v;

        // Calculate relative velocity
        double cr = 0.0;
        double vrel[3], vcm[3];

        for (int idim = 0; idim < 3; idim++) {
          vrel[idim] = v1[idim] - v0[idim];
          vcm[idim] = (m1*v1[idim] + m0*v0[idim])/(m1 + m0);
          cr += vrel[idim] * vrel[idim];
        }
        cr = sqrt(cr);

        // Check if collision occurs
        double SigmaCr = cr * PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0, v0, s1, v1);
        if (rnd() * SigmaCrMax >= SigmaCr) continue;

        //model the internal energy exchange
        if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_ON_) {
          double crInit = cr;
	  bool UpdateFlag[2]={true,true};

          PIC::IDF::RedistributeEnergy(s0List[s0ptr].ParticleData,s1List[s1ptr].ParticleData, cr, UpdateFlag, cell);

          for (int idim = 0; idim < 3; idim++) {
            vrel[idim] = (cr > 1.0E-10) ? vrel[idim] * cr / crInit : 0.0;
          }
        }


        // Perform collision - directly modifies v0 and v1 which point to particle state
        PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision(v0,m0,v1,m1,vcm,vrel);

        // Sample collision frequency if sampling is enabled
        if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
          // Sample for first species
          int CollFreqOffset_s0 = CollsionFrequentcySampling::SamplingBufferOffset + 
                                 sizeof(double) * CollsionFrequentcySampling::Offset(s0, s1);
          *((double*)(SamplingData + CollFreqOffset_s0)) += LocalParticleWeight / 
                                                           LocalTimeStep / cellMeasure * 
                                                           CollisionLimitingFactor;

          // Sample for second species (if different from first)
            int CollFreqOffset_s1 = CollsionFrequentcySampling::SamplingBufferOffset + 
                                   sizeof(double) * CollsionFrequentcySampling::Offset(s1, s0);
            *((double*)(SamplingData + CollFreqOffset_s1)) += LocalParticleWeight / 
                                                             LocalTimeStep / cellMeasure * 
                                                             CollisionLimitingFactor;
        }
      }
    }
  }

  // Clean up
  delete [] s1ParticleDataList;
  delete [] s0ParticleDataList;
}

void PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc(int i,int j,int k, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  int SigmaCrMax_nTest=100;
  double SigmaCrMax_SafetyMargin=1.05;

  struct cParticleDataList {
    double vel[3];
    bool ValueChangedFlag;
    PIC::ParticleBuffer::byte *ParticleData;
  };

  int s,s0,s1;

  int nParticleNumber[PIC::nTotalSpecies],nMaxSpecParticleNumber,cnt;
  long int FirstCellParticle,ptr;
  double cellMeasure;
  int thread;
  PIC::Mesh::cDataBlockAMR *block;

  //particle lists
  int ParticleDataListLength=100;
  cParticleDataList *s0ParticleDataList; //=new cParticleDataList[ParticleDataListLength];
  cParticleDataList *s1ParticleDataList; //=new cParticleDataList[ParticleDataListLength];  //s0ParticleDataList,s1ParticleDataList are used to store the actual data
  cParticleDataList *s0List=NULL,*s1List=NULL; //s0List and s1List are used to access s0ParticleDataList and s1ParticleDataList

  //sampling data
  PIC::Mesh::cDataCenterNode *cell;
  char *SamplingData=NULL;

  block=node->block;
  if (block==NULL) return;

  FirstCellParticle=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
  cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
  if (cell==NULL) return;

  //simulate particle's collisions
  cellMeasure=cell->Measure;

  if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
    SamplingData=cell->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset;
  }

  if (FirstCellParticle!=-1) {
    //simulate collision in the cell
    //1. coalcualte the number of the model particles in the cell and re-allocate the particle lists if needed
    for (s=0;s<PIC::nTotalSpecies;s++) nParticleNumber[s]=0;
    for (ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) nParticleNumber[PIC::ParticleBuffer::GetI(ptr)]++;
    for (nMaxSpecParticleNumber=0,s=0;s<PIC::nTotalSpecies;s++) if (nMaxSpecParticleNumber<nParticleNumber[s]) nMaxSpecParticleNumber=nParticleNumber[s];


    ParticleDataListLength=nMaxSpecParticleNumber;
    s0ParticleDataList=new cParticleDataList[ParticleDataListLength];
    s1ParticleDataList=new cParticleDataList[ParticleDataListLength];


    //loop through the first species
    double m0,m1,am;
    double LocalParticleWeight_s0,LocalParticleWeight_s1;
    double LocalTimeStep_s0,LocalTimeStep_s1;
    double minParticleWeightCorrection_s0,minParticleWeightCorrection_s1,sumWeightCorrection_s0,sumWeightCorrection_s1;
    PIC::ParticleBuffer::byte *ParticleData;

    for (s0=0;s0<PIC::nTotalSpecies;s0++) if ((PIC::MolecularData::GetSpecieType(s0)==_PIC_SPECIE_TYPE__GAS_)&&(nParticleNumber[s0]!=0)) {
      m0=PIC::MolecularData::GetMass(s0);
      LocalParticleWeight_s0=block->GetLocalParticleWeight(s0);
      LocalTimeStep_s0=block->GetLocalTimeStep(s0);
      minParticleWeightCorrection_s0=1,sumWeightCorrection_s0=0.0;

      //populate the particle list
      s0List=s0ParticleDataList;

      for (cnt=0,ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) if (PIC::ParticleBuffer::GetI(ptr)==(unsigned)s0) {
        ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

        PIC::ParticleBuffer::GetV(s0ParticleDataList[cnt].vel,ParticleData);
        s0ParticleDataList[cnt].ParticleData=ParticleData;
        s0ParticleDataList[cnt].ValueChangedFlag=false;

        if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
          double wc = PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

          if (cnt == 0) {
            minParticleWeightCorrection_s0 = wc;
          } else if (wc < minParticleWeightCorrection_s0) {
            minParticleWeightCorrection_s0 = wc;
          }

          sumWeightCorrection_s0 += wc;
        } else {
          sumWeightCorrection_s0 += 1.0;
        }

        cnt++;
      }

      //loop through the second speices
      for (s1=s0;s1<PIC::nTotalSpecies;s1++) if ((PIC::MolecularData::GetSpecieType(s1)==_PIC_SPECIE_TYPE__GAS_)&&(nParticleNumber[s1]!=0)) {
        //call a user-defined funstion to verify whether collision should be modeled in the cell
        if (DoSimulateCellCollisions!=NULL) {
          if (DoSimulateCellCollisions(cell,s0,s1)==false) continue;
        }

        m1=PIC::MolecularData::GetMass(s1);
        am=m0+m1;
        LocalParticleWeight_s1=block->GetLocalParticleWeight(s1);
        LocalTimeStep_s1=block->GetLocalTimeStep(s1);
        sumWeightCorrection_s1=0.0;

        //populate the list
        if (s0==s1) s1List=s0List,minParticleWeightCorrection_s1=minParticleWeightCorrection_s0,sumWeightCorrection_s1=sumWeightCorrection_s0;
        else {
          s1List=s1ParticleDataList;

          for (cnt=0,ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) if (PIC::ParticleBuffer::GetI(ptr)==(unsigned)s1) {
            ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

            PIC::ParticleBuffer::GetV(s1ParticleDataList[cnt].vel,ParticleData);
            s1ParticleDataList[cnt].ParticleData=ParticleData;
            s1ParticleDataList[cnt].ValueChangedFlag=false;

            if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
              double wc = PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

              if (cnt == 0) {
                minParticleWeightCorrection_s1 = wc;
              } else if (wc < minParticleWeightCorrection_s1) {
                minParticleWeightCorrection_s1 = wc;
              }

              sumWeightCorrection_s1 += wc;
            } else {
              sumWeightCorrection_s1 += 1.0;
            }

            cnt++;
          }
        }

        //simulate collsions between the pair of species
        double SigmaCrMax=0.0,SigmaCr,ancoll;
        long int ncoll;
        double v0[3],v1[3],cr;

        //1.Evaluate the maximum value of SigmaCr
        for (int ntest=0;ntest<SigmaCrMax_nTest;ntest++) {
          memcpy(v0,s0List[(int)(rnd()*nParticleNumber[s0])].vel,3*sizeof(double));
          memcpy(v1,s1List[(int)(rnd()*nParticleNumber[s1])].vel,3*sizeof(double));
          cr=sqrt(pow(v1[0]-v0[0],2)+pow(v1[1]-v0[1],2)+pow(v1[2]-v0[2],2));

          SigmaCr=cr*PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0,v0,s1,v1);

          if (SigmaCr>SigmaCrMax) SigmaCrMax=SigmaCr;
        }

        SigmaCrMax*=SigmaCrMax_SafetyMargin;

        //2.Evaluate the prospective number of collisions
        double maxLocalTimeStep,minLocalParticleWeight;

        maxLocalTimeStep=max(LocalTimeStep_s0,LocalTimeStep_s1);
        minLocalParticleWeight=min(LocalParticleWeight_s0*minParticleWeightCorrection_s0,LocalParticleWeight_s1*minParticleWeightCorrection_s1);

        if (s0==s1) {
          ancoll=0.5*sumWeightCorrection_s0/minParticleWeightCorrection_s0*
            (sumWeightCorrection_s0/minParticleWeightCorrection_s0 /*-1.0*/)*
            LocalParticleWeight_s0*minParticleWeightCorrection_s0*SigmaCrMax*LocalTimeStep_s0/cellMeasure;
        }
        else {
          ancoll=(sumWeightCorrection_s0*LocalParticleWeight_s0)*(sumWeightCorrection_s1*LocalParticleWeight_s1)*
            SigmaCrMax*maxLocalTimeStep/minLocalParticleWeight/cellMeasure;
        }

        //3. Collision Limiting
        double CollisionLimitingFactor=1.0;

        // Computation of collision limiting factor 
        if (nParticleNumber[s0]*nParticleNumber[s1]<ancoll) {
          CollisionLimitingFactor=ancoll/nParticleNumber[s0]*nParticleNumber[s1];
          ancoll/=CollisionLimitingFactor;
        }

        ncoll=(long int)ancoll;
        ancoll-=ncoll;
        if (rnd()<ancoll) ncoll++;

        //4. simulate collisions
        int s0ptr,s1ptr,idim;
        double vrel[3],vcm[3];

        while (ncoll-->0) {
          s0ptr=(int)((int)(rnd()*nParticleNumber[s0]));

          if (s0!=s1) s1ptr=(int)((int)(rnd()*nParticleNumber[s1]));
          else {
            do {
              s1ptr=(int)((int)(rnd()*nParticleNumber[s1]));
            }
            while (s0ptr==s1ptr);
          }

          memcpy(v0,s0List[s0ptr].vel,3*sizeof(double));
          memcpy(v1,s1List[s1ptr].vel,3*sizeof(double));

          for (cr=0.0,idim=0;idim<3;idim++) {
            vrel[idim]=v1[idim]-v0[idim];
            cr+=vrel[idim]*vrel[idim];
	    vcm[idim] = (m1*v1[idim] + m0*v0[idim])/(m1 + m0);
          }

          cr=sqrt(cr);
          SigmaCr=cr*PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0,v0,s1,v1);

          if (rnd()*SigmaCrMax>=SigmaCr) continue;

          //determine weather the properties of particles were updated
          double pUpdate_s0,pUpdate_s1;
          bool UpdateFlag[2];

          pUpdate_s0=minLocalParticleWeight/LocalParticleWeight_s0 * LocalTimeStep_s0/maxLocalTimeStep;
          pUpdate_s1=minLocalParticleWeight/LocalParticleWeight_s1 * LocalTimeStep_s1/maxLocalTimeStep;

          if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
            pUpdate_s0 /= PIC::ParticleBuffer::GetIndividualStatWeightCorrection(
              s0List[s0ptr].ParticleData);
            pUpdate_s1 /= PIC::ParticleBuffer::GetIndividualStatWeightCorrection(
              s1List[s1ptr].ParticleData);
          } // _INDIVIDUAL_PARTICLE_WEIGHT_MODE_

          UpdateFlag[0]=(rnd()<pUpdate_s0) ? true :false;
          UpdateFlag[1]=(rnd()<pUpdate_s1) ? true :false;

          //model the internal energy exchange
          if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_ON_) {
            double crInit=cr;

            PIC::IDF::RedistributeEnergy(s0List[s0ptr].ParticleData,s1List[s1ptr].ParticleData,cr,UpdateFlag,cell);
            for (int idim=0;idim<3;idim++) {
              vrel[idim] = (cr > 1.0E-10) ? vrel[idim]*cr/crInit : 0.0;
            }
          }

          //the collision is considered to be true
          PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision(v0,m0,v1,m1,vcm,vrel);

          //update the velocities in the lists
          if (UpdateFlag[0]==true) {
            s0List[s0ptr].ValueChangedFlag=true;
            memcpy(s0List[s0ptr].vel,v0,3*sizeof(double));

            if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
              int CollFreqOffset=CollsionFrequentcySampling::SamplingBufferOffset+sizeof(double)*CollsionFrequentcySampling::Offset(s0,s1);

              *((double*)(SamplingData+CollFreqOffset))+=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(s0List[s0ptr].ParticleData)*LocalParticleWeight_s0/LocalTimeStep_s0/cellMeasure*CollisionLimitingFactor; // calculate collision frequency taking into account the collision limiting factor
            }
          }

          if (UpdateFlag[1]==true) {
            s1List[s1ptr].ValueChangedFlag=true;
            memcpy(s1List[s1ptr].vel,v1,3*sizeof(double));

            if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
              int CollFreqOffset=CollsionFrequentcySampling::SamplingBufferOffset+sizeof(double)*CollsionFrequentcySampling::Offset(s1,s0);

              *((double*)(SamplingData+CollFreqOffset))+=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(s1List[s1ptr].ParticleData)*LocalParticleWeight_s1/LocalTimeStep_s1/cellMeasure*CollisionLimitingFactor; // calculate collision frequency taking into account the collision limiting factor
            }
          }
        }

        //update the velocities of the species 's1'
        if (s0!=s1) {
          for (cnt=0;cnt<nParticleNumber[s1];cnt++) if (s1List[cnt].ValueChangedFlag==true) {
            PIC::ParticleBuffer::SetV(s1List[cnt].vel,s1List[cnt].ParticleData);
          }
        }
      }

      //update velocities of species 's0'
      for (cnt=0;cnt<nParticleNumber[s0];cnt++) if (s0List[cnt].ValueChangedFlag==true) {
        PIC::ParticleBuffer::SetV(s0List[cnt].vel,s0List[cnt].ParticleData);
      }
    }

    delete [] s1ParticleDataList;
    delete [] s0ParticleDataList;
  }
}

//NTC: Manager
void PIC::MolecularCollisions::ParticleCollisionModel::ntc() {
  CollisionModelManager(ModelCellCollisions_ntc);
}

void PIC::MolecularCollisions::ParticleCollisionModel::CollisionModelManager(void (*fCellCollision)(int, int, int, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*)) {
  int thread;

  //global offsets
  int LoadBalancingMeasureOffset=PIC::Mesh::cDataBlockAMR_static_data::LoadBalancingMeasureOffset;

  //simulate particle's collisions
  #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
  if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
    //reset the balancing counters
    for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) {
      for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) {
        cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=DomainBlockDecomposition::BlockTable[nLocalNode];

        if (node->block!=NULL) {
          *(thread+(double*)(node->block->GetAssociatedDataBufferPointer()+
              LoadBalancingMeasureOffset))=0.0;
        }
      }
    }
  } //_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_

  #pragma omp parallel for schedule(dynamic,1) 
  #endif  //_COMPILATION_MODE_
  for (int CellCounter=0;CellCounter<DomainBlockDecomposition::nLocalBlocks*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;CellCounter++) {
    int nLocalNode,ii=CellCounter,i,j,k;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node;

    nLocalNode=ii/(_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=nLocalNode*_BLOCK_CELLS_Z_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

    k=ii/(_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_);
    ii-=k*_BLOCK_CELLS_Y_*_BLOCK_CELLS_X_;

    j=ii/_BLOCK_CELLS_X_;
    ii-=j*_BLOCK_CELLS_X_;

    i=ii;

    double StartTime=MPI_Wtime();

    node=DomainBlockDecomposition::BlockTable[nLocalNode];
    auto block=node->block;

    if (block==NULL) continue;  

    fCellCollision(i,j,k,node); 

    #if _COMPILATION_MODE_ == _COMPILATION_MODE__HYBRID_
    thread=omp_get_thread_num(); 
    #else
    thread=0;
    #endif

    
    if (_PIC_DYNAMIC_LOAD_BALANCING_MODE_ == _PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_) {
      switch (_COMPILATION_MODE_) {
      case _COMPILATION_MODE__MPI_:
        node->ParallelLoadMeasure+=MPI_Wtime()-StartTime;
        break;
      case _COMPILATION_MODE__HYBRID_:
      {
        int iThread_OpenMP=0;

        #if _COMPILATION_MODE_==_COMPILATION_MODE__HYBRID_
        iThread_OpenMP=omp_get_thread_num();
        #endif

        *(iThread_OpenMP+(double*)(block->GetAssociatedDataBufferPointer()+LoadBalancingMeasureOffset))+=MPI_Wtime()-StartTime;
      }
      break;
      default:
        exit(__LINE__,__FILE__,"The option is unknown");
      }
    }
  }

  if ((_COMPILATION_MODE_==_COMPILATION_MODE__HYBRID_)&&(_PIC_DYNAMIC_LOAD_BALANCING_MODE_==_PIC_DYNAMIC_LOAD_BALANCING_EXECUTION_TIME_)) {
    //sum-up the balancing measure
    for (int nLocalNode=0;nLocalNode<DomainBlockDecomposition::nLocalBlocks;nLocalNode++) for (int thread=0;thread<PIC::nTotalThreadsOpenMP;thread++) {
      auto node=DomainBlockDecomposition::BlockTable[nLocalNode];
      node->ParallelLoadMeasure+=*(thread+(double*)(node->block->GetAssociatedDataBufferPointer()+LoadBalancingMeasureOffset));
    }
  }
}


//Majorant Frequency collision procedure
//Modeling collision in a cell
void PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_simplified(int i, int j, int k,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) { 
  const int SigmaCrMax_nTest = 100;
  const double SigmaCrMax_SafetyMargin = 1.05;

  // Structure to store particle data
  struct cParticleDataList {
    double *v;  // Direct pointer to velocity state vector
    PIC::ParticleBuffer::byte *ParticleData;
  };

  // Get block and check validity
  PIC::Mesh::cDataBlockAMR *block = node->block;
  if (block == NULL) return;

  // Get cell and check validity
  PIC::Mesh::cDataCenterNode *cell = block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
  if (cell == NULL) return;

  double cellMeasure=cell->Measure;

  // Get first particle in cell and cell measure
  long int FirstCellParticle = block->FirstCellParticleTable[i + _BLOCK_CELLS_X_*(j + _BLOCK_CELLS_Y_*k)];
  // Get sampling data buffer if sampling mode is on
  char *SamplingData = NULL;
  if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
    SamplingData = cell->GetAssociatedDataBufferPointer() + PIC::Mesh::collectingCellSampleDataPointerOffset;
  }

  // If no particles in cell, return
  if (FirstCellParticle == -1) return;

  // Count particles per species
  int nParticleNumber[PIC::nTotalSpecies] = {0};
  for (long int ptr = FirstCellParticle; ptr != -1; ptr = PIC::ParticleBuffer::GetNext(ptr)) {
    nParticleNumber[PIC::ParticleBuffer::GetI(ptr)]++;
  }

  // Find maximum number of particles among all species
  int ParticleDataListLength = 0;
  for (int s = 0; s < PIC::nTotalSpecies; s++) {
    if (ParticleDataListLength < nParticleNumber[s]) {
      ParticleDataListLength = nParticleNumber[s];
    }
  }

  // Allocate particle lists
  cParticleDataList *s0ParticleDataList = new cParticleDataList[ParticleDataListLength];
  cParticleDataList *s1ParticleDataList = new cParticleDataList[ParticleDataListLength];

  // Loop through first species s0
  for (int s0 = 0; s0 < PIC::nTotalSpecies; s0++) {
    if (nParticleNumber[s0] == 0 || PIC::MolecularData::GetSpecieType(s0) != _PIC_SPECIE_TYPE__GAS_) {
      continue;
    }

    double m0 = PIC::MolecularData::GetMass(s0);
    double LocalTimeStep = block->GetLocalTimeStep(s0);  // Same for all species
    double LocalParticleWeight = block->GetLocalParticleWeight(s0);  // Same for all species
    
    // Populate s0 particle list
    cParticleDataList *s0List = s0ParticleDataList;
    int cnt = 0;
    for (long int ptr = FirstCellParticle; ptr != -1; ptr = PIC::ParticleBuffer::GetNext(ptr)) {
      if (PIC::ParticleBuffer::GetI(ptr) == (unsigned)s0) {
        PIC::ParticleBuffer::byte *ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
        s0List[cnt].ParticleData = ParticleData;
        s0List[cnt].v = PIC::ParticleBuffer::GetV(ParticleData);
        cnt++;
      }
    }

    // Loop through second species s1
    for (int s1 = s0; s1 < PIC::nTotalSpecies; s1++) {
      if (nParticleNumber[s1] == 0 || PIC::MolecularData::GetSpecieType(s1) != _PIC_SPECIE_TYPE__GAS_) {
        continue;
      }

      // Check if collisions should be simulated for this pair
      if (DoSimulateCellCollisions != NULL) {
        if (!DoSimulateCellCollisions(cell, s0, s1)) continue;
      }

      double m1 = PIC::MolecularData::GetMass(s1);
      
      // Populate s1 particle list (if different from s0)
      cParticleDataList *s1List = (s0 == s1) ? s0List : s1ParticleDataList;
      if (s0 != s1) {
        cnt = 0;
        for (long int ptr = FirstCellParticle; ptr != -1; ptr = PIC::ParticleBuffer::GetNext(ptr)) {
          if (PIC::ParticleBuffer::GetI(ptr) == (unsigned)s1) {
            PIC::ParticleBuffer::byte *ParticleData = PIC::ParticleBuffer::GetParticleDataPointer(ptr);
            s1List[cnt].ParticleData = ParticleData;
            s1List[cnt].v = PIC::ParticleBuffer::GetV(ParticleData);
            cnt++;
          }
        }
      }

      // Calculate maximum collision cross section
      double SigmaCrMax = 0.0;
      double SigmaCrMean = 0.0;
      
      for (int ntest = 0; ntest < SigmaCrMax_nTest; ntest++) {
        int s0ptr = (int)(rnd()*nParticleNumber[s0]);
        int s1ptr = (int)(rnd()*nParticleNumber[s1]);
        
        double cr = Vector3D::Distance(s1List[s1ptr].v,s0List[s0ptr].v);
        double SigmaCr = cr * PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0, s0List[s0ptr].v, s1, s1List[s1ptr].v); 
        
        SigmaCrMean += SigmaCr;
        if (SigmaCr > SigmaCrMax) SigmaCrMax = SigmaCr;
      }
      
      SigmaCrMax *= SigmaCrMax_SafetyMargin;
      SigmaCrMean *= SigmaCrMax_SafetyMargin / SigmaCrMax_nTest;

      // Calculate majorant collision frequency
      double MajorantFrequency;
      if (s0 == s1) {
        MajorantFrequency = 0.5 * nParticleNumber[s0] * (nParticleNumber[s0] /*-1*/) * 
                           LocalParticleWeight * SigmaCrMax / cellMeasure;
      } else {
        MajorantFrequency = nParticleNumber[s0] * nParticleNumber[s1] * 
                           LocalParticleWeight * SigmaCrMax / cellMeasure;
      }

      // Apply collision limiting if needed
      double CollisionLimitingFactor = 1.0;
      if (LocalTimeStep * MajorantFrequency > CollisionLimitingThrehold * 
          max(nParticleNumber[s0], nParticleNumber[s1])) {
        CollisionLimitingFactor = LocalTimeStep * MajorantFrequency / 
                                (CollisionLimitingThrehold * max(nParticleNumber[s0], nParticleNumber[s1]));
        MajorantFrequency /= CollisionLimitingFactor;
      }

      // Simulate collisions using Majorant Frequency scheme
      double TimeCounter = 0.0;

      while ((TimeCounter += -log(rnd())/MajorantFrequency) < LocalTimeStep) {
        // Get particle pointers
        int s0ptr = (int)(rnd() * nParticleNumber[s0]);
        int s1ptr;
        do {
          s1ptr = (int)(rnd() * nParticleNumber[s1]);
        } while (s0 == s1 && s0ptr == s1ptr);

        double *v0 = s0List[s0ptr].v;
        double *v1 = s1List[s1ptr].v;

        // Calculate relative velocity
        double cr = 0.0;
        double vrel[3],vcm[3];

        for (int idim = 0; idim < 3; idim++) {
          vrel[idim] = v1[idim] - v0[idim];
          cr += vrel[idim] * vrel[idim];
	  vcm[idim] = (m1*v1[idim] + m0*v0[idim])/(m1 + m0);
        }
        cr = sqrt(cr);

        // Check if collision occurs
        double SigmaCr = cr * PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0, v0, s1, v1);
        if (rnd() * SigmaCrMax >= SigmaCr) continue;

	//model the internal energy exchange
        if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_ON_) {
          double crInit = cr;
          bool UpdateFlag[2]={true,true};

          PIC::IDF::RedistributeEnergy(s0List[s0ptr].ParticleData,s1List[s1ptr].ParticleData, cr, UpdateFlag, cell);

          for (int idim = 0; idim < 3; idim++) {
            vrel[idim] = (cr > 1.0E-10) ? vrel[idim] * cr / crInit : 0.0;
          }
        }

        // Perform collision - directly modifies v0 and v1 which point to particle state
        PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision(v0,m0,v1,m1,vcm,vrel);

        // Sample collision frequency if sampling is enabled
        if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
          // Sample for first species
          int CollFreqOffset_s0 = CollsionFrequentcySampling::SamplingBufferOffset + 
                                 sizeof(double) * CollsionFrequentcySampling::Offset(s0, s1);
          *((double*)(SamplingData + CollFreqOffset_s0)) += LocalParticleWeight / 
                                                           LocalTimeStep / cellMeasure * 
                                                           CollisionLimitingFactor;

          // Sample for second species (if different from first)
          int CollFreqOffset_s1 = CollsionFrequentcySampling::SamplingBufferOffset + 
                                   sizeof(double) * CollsionFrequentcySampling::Offset(s1, s0);
          *((double*)(SamplingData + CollFreqOffset_s1)) += LocalParticleWeight / 
                                                             LocalTimeStep / cellMeasure * 
                                                             CollisionLimitingFactor;
        }
      }

      // No need to update particle buffer - velocities are modified directly
    }
  }

  // Clean up
  delete [] s1ParticleDataList;
  delete [] s0ParticleDataList;
}


void PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf(int i,int j,int k, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  int SigmaCrMax_nTest=100;
  double SigmaCrMax_SafetyMargin=1.05;

  struct cParticleDataList {
    double vel[3];
    bool ValueChangedFlag;
    PIC::ParticleBuffer::byte *ParticleData;
  };


  int s,s0,s1;

  int nParticleNumber[PIC::nTotalSpecies],nMaxSpecParticleNumber,cnt;
  //  long int FirstCellParticleTable[_BLOCK_CELLS_X_*_BLOCK_CELLS_Y_*_BLOCK_CELLS_Z_],
  long int FirstCellParticle,ptr;
  double cellMeasure;
  PIC::Mesh::cDataBlockAMR *block;

  //particle lists
  int ParticleDataListLength=0;
  cParticleDataList *s0ParticleDataList; //=new cParticleDataList[ParticleDataListLength];
  cParticleDataList *s1ParticleDataList; //=new cParticleDataList[ParticleDataListLength];  //s0ParticleDataList,s1ParticleDataList are used to store the actual data
  cParticleDataList *s0List=NULL,*s1List=NULL; //s0List and s1List are used to access s0ParticleDataList and s1ParticleDataList

  //sampling data
  PIC::Mesh::cDataCenterNode *cell;
  char *SamplingData;

  //global offsets
  int LoadBalancingMeasureOffset=PIC::Mesh::cDataBlockAMR_static_data::LoadBalancingMeasureOffset;

  //simulate particle's collisions
  block=node->block;
  if (block==NULL) return;

  FirstCellParticle=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];
  cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));

  if (cell==NULL) return;
  cellMeasure=cell->Measure;

  if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
    SamplingData=cell->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset;
  }

  if (FirstCellParticle!=-1) {
    //simulate collision in the cell
    //1. coalcualte the number of the model particles in the cell and re-allocate the particle lists if needed
    for (s=0;s<PIC::nTotalSpecies;s++) nParticleNumber[s]=0;
    for (ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) nParticleNumber[PIC::ParticleBuffer::GetI(ptr)]++;
    for (s=0;s<PIC::nTotalSpecies;s++) if (ParticleDataListLength<nParticleNumber[s]) ParticleDataListLength=nParticleNumber[s];

    s0ParticleDataList=new cParticleDataList[ParticleDataListLength];
    s1ParticleDataList=new cParticleDataList[ParticleDataListLength];

    //loop through the first species
    double m0,m1,am;
    double LocalParticleWeight_s0,LocalParticleWeight_s1;
    double LocalTimeStep_s0,LocalTimeStep_s1;
    double minParticleWeightCorrection_s0,minParticleWeightCorrection_s1,sumWeightCorrection_s0,sumWeightCorrection_s1;
    PIC::ParticleBuffer::byte *ParticleData;

    for (s0=0;s0<PIC::nTotalSpecies;s0++) if ((PIC::MolecularData::GetSpecieType(s0)==_PIC_SPECIE_TYPE__GAS_)&&(nParticleNumber[s0]!=0)) {

    m0=PIC::MolecularData::GetMass(s0);
    LocalParticleWeight_s0=block->GetLocalParticleWeight(s0);
    LocalTimeStep_s0=block->GetLocalTimeStep(s0);
    minParticleWeightCorrection_s0=1,sumWeightCorrection_s0=0.0;

    //populate the particle list
    s0List=s0ParticleDataList;

    for (cnt=0,ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) if (PIC::ParticleBuffer::GetI(ptr)==(unsigned)s0) {
      ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

      PIC::ParticleBuffer::GetV(s0ParticleDataList[cnt].vel,ParticleData);
      s0ParticleDataList[cnt].ParticleData=ParticleData;
      s0ParticleDataList[cnt].ValueChangedFlag=false;

      if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
        double wc = PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

        if (cnt == 0) {
          minParticleWeightCorrection_s0 = wc;
        } else if (wc < minParticleWeightCorrection_s0) {
          minParticleWeightCorrection_s0 = wc;
        }

        sumWeightCorrection_s0 += wc;
      } else {
        sumWeightCorrection_s0 += 1.0;
      }

      cnt++;
    }

    //loop through the second speices
    for (s1=s0;s1<PIC::nTotalSpecies;s1++) if ((PIC::MolecularData::GetSpecieType(s1)==_PIC_SPECIE_TYPE__GAS_)&&(nParticleNumber[s1]!=0)) {
      //call a user-defined funstion to verify whether collision should be modeled in the cell

      if (DoSimulateCellCollisions!=NULL) {
        if (DoSimulateCellCollisions(cell,s0,s1)==false) continue;
      }

      m1=PIC::MolecularData::GetMass(s1);
      am=m0+m1;
      LocalParticleWeight_s1=block->GetLocalParticleWeight(s1);
      LocalTimeStep_s1=block->GetLocalTimeStep(s1);
      sumWeightCorrection_s1=0.0;

      //populate the list
      if (s0==s1) {
        s1List=s0List,minParticleWeightCorrection_s1=minParticleWeightCorrection_s0,sumWeightCorrection_s1=sumWeightCorrection_s0;
      }
      else {
        s1List=s1ParticleDataList;

        for (cnt=0,ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) if (PIC::ParticleBuffer::GetI(ptr)==(unsigned)s1) {
          ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

          PIC::ParticleBuffer::GetV(s1ParticleDataList[cnt].vel,ParticleData);
          s1ParticleDataList[cnt].ParticleData=ParticleData;
          s1ParticleDataList[cnt].ValueChangedFlag=false;

          if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
            double wc = PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

            if (cnt == 0) {
              minParticleWeightCorrection_s1 = wc;
            }
            else if (wc < minParticleWeightCorrection_s1) {
              minParticleWeightCorrection_s1 = wc;
            }

            sumWeightCorrection_s1 += wc;
          }
          else {
            sumWeightCorrection_s1 += 1.0;
          }

          cnt++;
        }
      }

      //simulate collsions between the pair of species
      double SigmaCrMax=0.0,SigmaCr,TimeCounter,SigmaCrMean=0.0;
      double v0[3],v1[3],cr;

      //1.Evaluate the maximum value of SigmaCr
      for (int ntest=0;ntest<SigmaCrMax_nTest;ntest++) {
        memcpy(v0,s0List[(int)(rnd()*nParticleNumber[s0])].vel,3*sizeof(double));
        memcpy(v1,s1List[(int)(rnd()*nParticleNumber[s1])].vel,3*sizeof(double));
        cr=sqrt(pow(v1[0]-v0[0],2)+pow(v1[1]-v0[1],2)+pow(v1[2]-v0[2],2));

        SigmaCr=cr*PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0,v0,s1,v1);

        if (SigmaCr>SigmaCrMax) SigmaCrMax=SigmaCr;
          SigmaCrMean+=SigmaCr;
        }

        SigmaCrMax*=SigmaCrMax_SafetyMargin;
        SigmaCrMean*=SigmaCrMax_SafetyMargin/SigmaCrMax_nTest;

        //2.Evaluate the prospective number of collisions
        double maxLocalTimeStep,minLocalParticleWeight;
        double MajorantFrequency;

        maxLocalTimeStep=max(LocalTimeStep_s0,LocalTimeStep_s1);
        minLocalParticleWeight=min(LocalParticleWeight_s0*minParticleWeightCorrection_s0,LocalParticleWeight_s1*minParticleWeightCorrection_s1);

        if (s0==s1) {
          MajorantFrequency=0.5*sumWeightCorrection_s0/minParticleWeightCorrection_s0*
           (sumWeightCorrection_s0/minParticleWeightCorrection_s0 /*-1.0*/)*
            LocalParticleWeight_s0*minParticleWeightCorrection_s0*SigmaCrMax/cellMeasure;
        }
        else {
          MajorantFrequency=(sumWeightCorrection_s0*LocalParticleWeight_s0)*(sumWeightCorrection_s1*LocalParticleWeight_s1)*
            SigmaCrMax/minLocalParticleWeight/cellMeasure;
        }

        if (MajorantFrequency*maxLocalTimeStep<1.0E-5) continue;

        //3. Collision Limiting
        double CollisionLimitingFactor=1.0;

        // Computation of collision limiting factor
        if (CollisionLimitingThrehold*max(nParticleNumber[s0],nParticleNumber[s1])<MajorantFrequency/SigmaCrMax*SigmaCrMean*maxLocalTimeStep) {
          CollisionLimitingFactor=(MajorantFrequency/SigmaCrMax*SigmaCrMean*maxLocalTimeStep)/(CollisionLimitingThrehold*max(nParticleNumber[s0],nParticleNumber[s1]));
          MajorantFrequency/=CollisionLimitingFactor;
        }


        //4. simulate collisions
        int s0ptr,s1ptr,idim;
        double vrel[3],vcm[3];

        //set the initial value of the counter
        TimeCounter=0.0;

        while ((TimeCounter+=-log(rnd())/MajorantFrequency)<maxLocalTimeStep) {
          s0ptr=(int)((int)(rnd()*nParticleNumber[s0]));

          if (s0!=s1) s1ptr=(int)((int)(rnd()*nParticleNumber[s1]));
          else {
            do {
              s1ptr=(int)((int)(rnd()*nParticleNumber[s1]));
            }
            while (s0ptr==s1ptr);
          }

          memcpy(v0,s0List[s0ptr].vel,3*sizeof(double));
          memcpy(v1,s1List[s1ptr].vel,3*sizeof(double));

          for (cr=0.0,idim=0;idim<3;idim++) {
            vrel[idim]=v1[idim]-v0[idim];
            cr+=vrel[idim]*vrel[idim];
	    vcm[idim] = (m1*v1[idim] + m0*v0[idim])/(m1 + m0);
          }

          cr=sqrt(cr);
          SigmaCr=cr*PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0,v0,s1,v1);

          if (rnd()*SigmaCrMax>=SigmaCr) continue;

          //determine weather the properties of particles were updated
          double pUpdate_s0,pUpdate_s1;
          bool UpdateFlag[2];

          pUpdate_s0=minLocalParticleWeight/LocalParticleWeight_s0 * LocalTimeStep_s0/maxLocalTimeStep;
          pUpdate_s1=minLocalParticleWeight/LocalParticleWeight_s1 * LocalTimeStep_s1/maxLocalTimeStep;

          if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
            pUpdate_s0 /= PIC::ParticleBuffer::GetIndividualStatWeightCorrection(s0List[s0ptr].ParticleData);
            pUpdate_s1 /= PIC::ParticleBuffer::GetIndividualStatWeightCorrection(s1List[s1ptr].ParticleData);
          }

          UpdateFlag[0]=(rnd()<pUpdate_s0) ? true :false;
          UpdateFlag[1]=(rnd()<pUpdate_s1) ? true :false;

          //model the internal energy exchange
          if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_ON_) {
            double crInit = cr;

            PIC::IDF::RedistributeEnergy(s0List[s0ptr].ParticleData,s1List[s1ptr].ParticleData,cr,UpdateFlag,cell);

            for (int idim = 0; idim < 3; idim++) {
              vrel[idim] = (cr > 1.0E-10) ? vrel[idim] * cr / crInit : 0.0;
            }
          }

          //the collision is considered to be true
          PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision(v0,m0,v1,m1,vcm,vrel);

          //update the velocities in the lists
          if (UpdateFlag[0]==true) {
            s0List[s0ptr].ValueChangedFlag=true;
            memcpy(s0List[s0ptr].vel,v0,3*sizeof(double));

            if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
              int CollFreqOffset=CollsionFrequentcySampling::SamplingBufferOffset+sizeof(double)*CollsionFrequentcySampling::Offset(s0,s1);

              *((double*)(SamplingData+CollFreqOffset))+=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(s0List[s0ptr].ParticleData)*LocalParticleWeight_s0/LocalTimeStep_s0/cellMeasure*CollisionLimitingFactor; // calculate collision frequency taking into account the collision limiting factor
            }
         }

         if (UpdateFlag[1]==true) {
            s1List[s1ptr].ValueChangedFlag=true;
            memcpy(s1List[s1ptr].vel,v1,3*sizeof(double));

            if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
              int CollFreqOffset = CollsionFrequentcySampling::SamplingBufferOffset 
                 + sizeof(double)*CollsionFrequentcySampling::Offset(s1, s0);
 
 
              *((double*)(SamplingData+CollFreqOffset)) 
                += PIC::ParticleBuffer::GetIndividualStatWeightCorrection(s1List[s1ptr].ParticleData)
                * LocalParticleWeight_s1 / LocalTimeStep_s1 / cellMeasure 
                * CollisionLimitingFactor; // calculate collision frequency taking into account the collision limiting factor
            }
          }
        }

        //update the velocities of the species 's1'
        if (s0!=s1) {
          for (cnt=0;cnt<nParticleNumber[s1];cnt++) if (s1List[cnt].ValueChangedFlag==true) {
            PIC::ParticleBuffer::SetV(s1List[cnt].vel,s1List[cnt].ParticleData);
          }
        }
      }

      //update velocities of species 's0'
      for (cnt=0;cnt<nParticleNumber[s0];cnt++) if (s0List[cnt].ValueChangedFlag==true) {
        PIC::ParticleBuffer::SetV(s0List[cnt].vel,s0List[cnt].ParticleData);
      }
    }

    delete [] s0ParticleDataList;
    delete [] s1ParticleDataList;
  } //end simulation of collisons in a cell
}

void PIC::MolecularCollisions::ParticleCollisionModel::mf() {
  CollisionModelManager(ModelCellCollisions_mf);
}




//Majorant Frequency collision procedure
void PIC::MolecularCollisions::ParticleCollisionModel::mf_Yinsi() {
  CollisionModelManager(ModelCellCollisions_mf_Yinsi);
}


void PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_Yinsi(int i,int j,int k, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node) {
  int SigmaCrMax_nTest=100;
  double SigmaCrMax_SafetyMargin=1.05;

  PIC::MolecularCollisions::ParticleCollisionModel::CollisionLimitingThrehold=40.0;

  struct cParticleDataList {
    double vel[3];
    bool ValueChangedFlag;
    PIC::ParticleBuffer::byte *ParticleData;
  };


  int s,s0,s1;
  int nParticleNumber[PIC::nTotalSpecies],nMaxSpecParticleNumber,cnt;
  long int FirstCellParticle,ptr;
  double cellMeasure;
  int thread;

  //particle lists
  int ParticleDataListLength=0;
  cParticleDataList *s0ParticleDataList=NULL; //=new cParticleDataList[ParticleDataListLength];
  cParticleDataList *s1ParticleDataList=NULL; //=new cParticleDataList[ParticleDataListLength];  //s0ParticleDataList,s1ParticleDataList are used to store the actual data
  cParticleDataList *s0List=NULL,*s1List=NULL; //s0List and s1List are used to access s0ParticleDataList and s1ParticleDataList

  //sampling data
  PIC::Mesh::cDataCenterNode *cell;
  char *SamplingData;
  PIC::Mesh::cDataBlockAMR *block;

  block=node->block;
  if (block==NULL) return;

  cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
  if (cell==NULL) return;

  FirstCellParticle=block->FirstCellParticleTable[i+_BLOCK_CELLS_X_*(j+_BLOCK_CELLS_Y_*k)];

  cell=block->GetCenterNode(_getCenterNodeLocalNumber(i,j,k));
  cellMeasure=cell->Measure;

  if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
    SamplingData=cell->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset;
  }

  if (FirstCellParticle!=-1) {
    //simulate collision in the cell
    //1. coalcualte the number of the model particles in the cell and re-allocate the particle lists if needed
    for (s=0;s<PIC::nTotalSpecies;s++) nParticleNumber[s]=0;
    for (ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) nParticleNumber[PIC::ParticleBuffer::GetI(ptr)]++;
    for (ParticleDataListLength=0,s=0;s<PIC::nTotalSpecies;s++) if (ParticleDataListLength<nParticleNumber[s]) ParticleDataListLength=nParticleNumber[s];

    s0ParticleDataList=new cParticleDataList[ParticleDataListLength];
    s1ParticleDataList=new cParticleDataList[ParticleDataListLength];

    //loop through the first species
    double m0,m1,am;
    double LocalParticleWeight_s0,LocalParticleWeight_s1;
    double LocalTimeStep_s0,LocalTimeStep_s1;
    double minParticleWeightCorrection_s0,minParticleWeightCorrection_s1,sumWeightCorrection_s0,sumWeightCorrection_s1;
    PIC::ParticleBuffer::byte *ParticleData;

    for (s0=0;s0<PIC::nTotalSpecies;s0++) if ((PIC::MolecularData::GetSpecieType(s0)==_PIC_SPECIE_TYPE__GAS_)&&(nParticleNumber[s0]!=0)) {

    m0=PIC::MolecularData::GetMass(s0);
    LocalParticleWeight_s0=block->GetLocalParticleWeight(s0);
    LocalTimeStep_s0=block->GetLocalTimeStep(s0);
    minParticleWeightCorrection_s0=1,sumWeightCorrection_s0=0.0;

    //populate the particle list
    s0List=s0ParticleDataList;

    for (cnt=0,ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) if (PIC::ParticleBuffer::GetI(ptr)==(unsigned)s0) {
      ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

      PIC::ParticleBuffer::GetV(s0ParticleDataList[cnt].vel,ParticleData);
      s0ParticleDataList[cnt].ParticleData=ParticleData;
      s0ParticleDataList[cnt].ValueChangedFlag=false;

      if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
        double wc = PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

        if (cnt == 0) {
          minParticleWeightCorrection_s0 = wc;
        } else if (wc < minParticleWeightCorrection_s0) {
          minParticleWeightCorrection_s0 = wc;
        }

        sumWeightCorrection_s0 += wc;
      } else {
        sumWeightCorrection_s0 += 1.0;
      }

      cnt++;
    }

    //loop through the second speices
    for (s1=s0;s1<PIC::nTotalSpecies;s1++) if ((PIC::MolecularData::GetSpecieType(s1)==_PIC_SPECIE_TYPE__GAS_)&&(nParticleNumber[s1]!=0)) {
      //call a user-defined function to verify whether collision should be modeled in the cell

      if (DoSimulateCellCollisions!=NULL) {
        if (DoSimulateCellCollisions(cell,s0,s1)==false) continue;
      }

      m1=PIC::MolecularData::GetMass(s1);
      am=m0+m1;
      LocalParticleWeight_s1=block->GetLocalParticleWeight(s1);
      LocalTimeStep_s1=block->GetLocalTimeStep(s1);
      sumWeightCorrection_s1=0.0;

      //populate the list
      if (s0==s1) s1List=s0List,minParticleWeightCorrection_s1=minParticleWeightCorrection_s0,sumWeightCorrection_s1=sumWeightCorrection_s0;
      else {
        s1List=s1ParticleDataList;

        for (cnt=0,ptr=FirstCellParticle;ptr!=-1;ptr=PIC::ParticleBuffer::GetNext(ptr)) if (PIC::ParticleBuffer::GetI(ptr)==(unsigned)s1) {
          ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

          PIC::ParticleBuffer::GetV(s1ParticleDataList[cnt].vel,ParticleData);
          s1ParticleDataList[cnt].ParticleData=ParticleData;
          s1ParticleDataList[cnt].ValueChangedFlag=false;

          if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
            double wc=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(ParticleData);

            if (cnt==0) {
              minParticleWeightCorrection_s1=wc;
            } else if (wc<minParticleWeightCorrection_s1) {
              minParticleWeightCorrection_s1=wc;
            }

            sumWeightCorrection_s1+=wc;
          } else {
            sumWeightCorrection_s1+=1.0;
          }

          cnt++;
        }
      }

      //simulate collsions between the pair of species
      double SigmaCrMax=0.0,SigmaCr,TimeCounter,SigmaCrMean=0.0;
      double v0[3],v1[3],cr;

      //1.Evaluate the maximum value of SigmaCr
      for (int ntest=0;ntest<SigmaCrMax_nTest;ntest++) {
        memcpy(v0,s0List[(int)(rnd()*nParticleNumber[s0])].vel,3*sizeof(double));
        memcpy(v1,s1List[(int)(rnd()*nParticleNumber[s1])].vel,3*sizeof(double));
        cr=sqrt(pow(v1[0]-v0[0],2)+pow(v1[1]-v0[1],2)+pow(v1[2]-v0[2],2));

        SigmaCr=cr*PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0,v0,s1,v1);

        if (SigmaCr>SigmaCrMax) SigmaCrMax=SigmaCr;
          SigmaCrMean+=SigmaCr;
        }

        SigmaCrMax*=SigmaCrMax_SafetyMargin;
        SigmaCrMean*=SigmaCrMax_SafetyMargin/SigmaCrMax_nTest;

        //2.Evaluate the prospective number of collisions
        double maxLocalTimeStep,minLocalParticleWeight[2];
        double MajorantFrequency[2];

        maxLocalTimeStep=max(LocalTimeStep_s0,LocalTimeStep_s1);
        minLocalParticleWeight[0]=LocalParticleWeight_s0*minParticleWeightCorrection_s0;
        minLocalParticleWeight[1]=LocalParticleWeight_s1*minParticleWeightCorrection_s1;

        if (s0==s1) {
          MajorantFrequency[0]=0.5*(nParticleNumber[s0] /*-1*/)*
             (sumWeightCorrection_s0*LocalParticleWeight_s0)*SigmaCrMax/cellMeasure;
          //N_a* (sum weight)/cellVol*vel
          // 0.5* n0* (n0-1)* w0 *sigma*relV/ cellV
          MajorantFrequency[1]=MajorantFrequency[0];
        }
        else {
          MajorantFrequency[0]=nParticleNumber[s0]*(sumWeightCorrection_s1*LocalParticleWeight_s1)*SigmaCrMax/cellMeasure;
          // n0*n1*w1*sig*rel/cellV
          // no*(sum weight)*rel/CellV
          // n0=sum(weightCorrection0)/minWeightCorr0  
          //one particle undergoes one collision,change v once equivalent to
          //two particles with half weight undergoes two collisions, each changes v once
          //two particles with 0.1 and 0.9 weight correction, each should undergo v change once

          MajorantFrequency[1]=nParticleNumber[s1]*(sumWeightCorrection_s0*LocalParticleWeight_s0)*SigmaCrMax/cellMeasure;
          // n1*(n0*w0*sig*rel/cellV)
          //(n0*w0*sig*rel/cellV) =1/s=same as analytical
          // n1=sum(weightCorrection1)/minWeightCorr1
        }

        //3. Collision Limiting
        double CollisionLimitingFactor[2]={1.0,1.0};

        // Computation of collision limiting factor
        if (maxLocalTimeStep*MajorantFrequency[0]>CollisionLimitingThrehold*nParticleNumber[s0]){
          //printf("maxLocalTimeStep:%e,MajorantFrequency:%e,prod:%e, s0:%d, s1:%d\n",maxLocalTimeStep,MajorantFrequency,maxLocalTimeStep*MajorantFrequency,s0,s1);
          CollisionLimitingFactor[0] = maxLocalTimeStep*MajorantFrequency[0]/CollisionLimitingThrehold/nParticleNumber[s0];
          MajorantFrequency[0] /=CollisionLimitingFactor[0]; 
        }

        if (maxLocalTimeStep*MajorantFrequency[1]>CollisionLimitingThrehold*nParticleNumber[s1]){
          //printf("maxLocalTimeStep:%e,MajorantFrequency:%e,prod:%e, s0:%d, s1:%d\n",maxLocalTimeStep,MajorantFrequency,maxLocalTimeStep*MajorantFrequency,s0,s1);
          CollisionLimitingFactor[1] = maxLocalTimeStep*MajorantFrequency[1]/CollisionLimitingThrehold/nParticleNumber[s1];
          MajorantFrequency[1] /=CollisionLimitingFactor[1]; 
        }

        //4. simulate collisions
        int s0ptr,s1ptr,idim;
        double vrel[3],vcm[3];

        //set the initial value of the counter
        TimeCounter=0.0;
        double MajorFreq=max(MajorantFrequency[0],MajorantFrequency[1]);
 
        while ((TimeCounter+=-log(rnd())/MajorFreq)<maxLocalTimeStep) {
          s0ptr=(int)((int)(rnd()*nParticleNumber[s0]));

          if (s0!=s1) s1ptr=(int)((int)(rnd()*nParticleNumber[s1]));
          else {
            do {
              s1ptr=(int)((int)(rnd()*nParticleNumber[s1]));
            }
            while (s0ptr==s1ptr);
          }

          memcpy(v0,s0List[s0ptr].vel,3*sizeof(double));
          memcpy(v1,s1List[s1ptr].vel,3*sizeof(double));

          for (cr=0.0,idim=0;idim<3;idim++) {
            vrel[idim]=v1[idim]-v0[idim];
            cr+=vrel[idim]*vrel[idim];
	    vcm[idim] = (m1*v1[idim] + m0*v0[idim])/(m1 + m0);
          }

          cr=sqrt(cr);
          SigmaCr=cr*PIC::MolecularData::MolecularModels::GetTotalCrossSection(s0,v0,s1,v1);

          if (rnd()*SigmaCrMax>=SigmaCr) continue;

          //determine weather the properties of particles were updated
          double pUpdate_s0,pUpdate_s1;
          bool UpdateFlag[2];

          pUpdate_s0=MajorantFrequency[0]/MajorFreq * LocalTimeStep_s0/maxLocalTimeStep;
          pUpdate_s1=MajorantFrequency[1]/MajorFreq * LocalTimeStep_s1/maxLocalTimeStep;

          if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
            exit(__LINE__,__FILE__,"Error: not implemented");
            //pUpdate_s0/=meanWeightCorr[0]/minParticleWeightCorrection_s0;//min weight correction increases the majorant frequency
            //pUpdate_s1/=meanWeightCorr[1]/minParticleWeightCorrection_s1;
          }
              
          UpdateFlag[0]=(rnd()<pUpdate_s0) ? true :false;
          UpdateFlag[1]=(rnd()<pUpdate_s1) ? true :false;

          //model the internal energy exchange
          if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_ == _PIC_MODE_ON_) {
            double crInit = cr;

            PIC::IDF::RedistributeEnergy(s0List[s0ptr].ParticleData,s1List[s1ptr].ParticleData, cr, UpdateFlag, cell);
          
            for (int idim = 0; idim < 3; idim++) {
              vrel[idim] = (cr > 1.0E-10) ? vrel[idim] * cr / crInit : 0.0;
            }
          }

          //the collision is considered to be true
          PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision(v0,m0,v1,m1,vcm,vrel);

          //update the velocities in the lists
          if (UpdateFlag[0]==true) {
            s0List[s0ptr].ValueChangedFlag=true;
            memcpy(s0List[s0ptr].vel,v0,3*sizeof(double));

            if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
              int CollFreqOffset=CollsionFrequentcySampling::SamplingBufferOffset+sizeof(double)*CollsionFrequentcySampling::Offset(s0,s1);

              *((double*)(SamplingData+CollFreqOffset))+=PIC::ParticleBuffer::GetIndividualStatWeightCorrection(s0List[s0ptr].ParticleData)*LocalParticleWeight_s0/LocalTimeStep_s0/cellMeasure*CollisionLimitingFactor[0]; // calculate collision frequency taking into account the collision limiting factor
            }
          }

          if (UpdateFlag[1]==true) {
            s1List[s1ptr].ValueChangedFlag=true;
            memcpy(s1List[s1ptr].vel,v1,3*sizeof(double));

            if (_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__ == _PIC_MODE_ON_) {
              int CollFreqOffset = 
                CollsionFrequentcySampling::SamplingBufferOffset + 
                sizeof(double) * CollsionFrequentcySampling::Offset(s1, s0);

              *((double*)(SamplingData + CollFreqOffset)) += 
                 PIC::ParticleBuffer::GetIndividualStatWeightCorrection(s1List[s1ptr].ParticleData) * 
                 LocalParticleWeight_s1 / LocalTimeStep_s1 / cellMeasure * 
                 CollisionLimitingFactor[1]; // calculate collision frequency taking into account the collision limiting factor
            } //_PIC__PARTICLE_COLLISION_MODEL__SAMPLE_COLLISION_FREQUENTCY_MODE__
          }
        }

        //update the velocities of the species 's1'
        if (s0!=s1) {
          for (cnt=0;cnt<nParticleNumber[s1];cnt++) if (s1List[cnt].ValueChangedFlag==true) {
            PIC::ParticleBuffer::SetV(s1List[cnt].vel,s1List[cnt].ParticleData);
          }
        }
      }//for (s1=s0;s1<PIC::nTotalSpecies;s1++) 

      //update velocities of species 's0'
      for (cnt=0;cnt<nParticleNumber[s0];cnt++) if (s0List[cnt].ValueChangedFlag==true) {
        PIC::ParticleBuffer::SetV(s0List[cnt].vel,s0List[cnt].ParticleData);
      }
  
    }
  
    delete [] s1ParticleDataList;
    delete [] s0ParticleDataList; 
  } //end simulation of collisons in a cell
}
 

