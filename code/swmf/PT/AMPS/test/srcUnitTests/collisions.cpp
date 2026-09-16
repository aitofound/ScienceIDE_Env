#include <gtest/gtest.h>
#include "pic.h"

#include "ParticleTestBase.h"
#include "sampling.h"

void collisions_test_for_linker() {}

#if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_OFF_
namespace AMPS_COLLISON_TEST  {
  double energySumInit=0.0,energySumAfter=0.0,mvSumInit[3]={0.0,0.0,0.0},mvSumAfter[3]={0.0,0.0,0.0};
  cSampledValues vSumAfter[3],mSpeedSumAfter[2],mSpeedSumInit[2];
  cSampledValues CollsionFrequentcyTheory[2][2];
  cSampledValues CollsionFrequentcy[2][2];
  cSampledValues MeanRotEnergyInit[2],MeanRotEnergyAfter[2],MeanRotEnergyTheory[2];
  cSampledValues MeanVibEnergyInit[2],MeanVibEnergyAfter[2],MeanVibEnergyTheory[2];
  cSampledValues MeanKineticEnergyInit[2],MeanKineticEnergyAfter[2];


  void ResetAll() {
    // Reset single variables
    energySumInit=0.0;
    energySumAfter=0.0;

    // Reset arrays of size 3
    for (int i = 0; i < 3; i++) {
        mvSumInit[i]=0.0;
        mvSumAfter[i]=0.0;
        vSumAfter[i].Reset();
    }

    // Reset arrays of size 2
    for (int i = 0; i < 2; i++) {
        mSpeedSumAfter[i].Reset();
        mSpeedSumInit[i].Reset();
        MeanRotEnergyInit[i].Reset();
        MeanRotEnergyAfter[i].Reset();
        MeanRotEnergyTheory[i].Reset();

        MeanVibEnergyInit[i].Reset();
        MeanVibEnergyAfter[i].Reset();
        MeanVibEnergyTheory[i].Reset();

	MeanKineticEnergyInit[i].Reset();
        MeanKineticEnergyAfter[i].Reset();
    }

    // Reset 2x2 arrays
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            CollsionFrequentcyTheory[i][j].Reset();
            CollsionFrequentcy[i][j].Reset();
        }
    }
}


  void  VelocityAfterCollision(double &RelativeErrorMomentum,double &RelativeErrorEnergy,double &RelativeErrorVel,int s0,int s1,void (*fVelocityAfterCollision)(double*,double,double*,double,double*,double*)) {
    double v_s0[3],v_s1[3],m_s0,m_s1,am,vrel[3],vbulk[3]={0.0,0.0,0.0},energy_sum_init,energy_sum_after,mvsum_init[3],mvsum_after[3],vcm[3];
    double vrel_init,vrel_after;
    int nTest,i;

    const int nTotalTests=100000;
    const double Temp=300.0;

    m_s0=PIC::MolecularData::GetMass(s0);
    m_s1=PIC::MolecularData::GetMass(s1);

    RelativeErrorMomentum=0.0;
    RelativeErrorEnergy=0.0;
    RelativeErrorVel=0.0;

    for (nTest=0;nTest<nTotalTests;nTest++) {
      //generate particle velocity and calculate relative speed
      PIC::Distribution::MaxwellianVelocityDistribution(v_s0,vbulk,Temp,s0);
      PIC::Distribution::MaxwellianVelocityDistribution(v_s1,vbulk,Temp,s1);

      //calculating initial parameters
      energy_sum_init=0.0;

      for (i=0;i<3;i++) {
        double t;
        
        t=m_s0*v_s0[i];
        energy_sum_init+=0.5*t*v_s0[i];
        mvsum_init[i]=t;

        t=m_s1*v_s1[i];
        energy_sum_init+=0.5*t*v_s1[i];
        mvsum_init[i]+=t; 

        vrel[i]=v_s0[i]-v_s1[i];
	      vcm[i]=(m_s0*v_s0[i]+m_s1*v_s1[i])/(m_s0+m_s1);
      }

      vrel_init=Vector3D::Length(vrel);

      //redistribute the particle velocity
      fVelocityAfterCollision(v_s0,m_s0,v_s1,m_s1,vcm,vrel);

      //Verify the result 
      energy_sum_after=0.0;
      for (i=0;i<3;i++) {
        double t;
        
        t=m_s0*v_s0[i];
        energy_sum_after+=0.5*t*v_s0[i];
        mvsum_after[i]=t;

        t=m_s1*v_s1[i];
        energy_sum_after+=0.5*t*v_s1[i];
        mvsum_after[i]+=t; 

        vrel[i]=v_s0[i]-v_s1[i];
      }   

      vrel_after=Vector3D::Length(vrel);
      RelativeErrorVel+=fabs(vrel_init-vrel_after)/(vrel_init+vrel_after);

      RelativeErrorEnergy+=fabs(energy_sum_init-energy_sum_after)/(energy_sum_init+energy_sum_after);  
      RelativeErrorMomentum+=Vector3D::Distance(mvsum_init,mvsum_after)/(Vector3D::Length(mvsum_init)+Vector3D::Length(mvsum_after));    
    }

  
  }

  void HeatBath(int s0,int s1,double Tkin, double Trot,double Tvib,bool UseCommonStatWeight,bool UseCommonTimeStep,void (*fCellCollision)(int, int, int, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*),int nIterations) {
    namespace MD=PIC::MolecularData;
    namespace MC=PIC::MolecularCollisions;
    namespace PB=PIC::ParticleBuffer; 

    int i,j,icell=0,jcell=0,kcell=0;
    long int ptr;
    PIC::Mesh::cDataCenterNode *cell;
    PIC::Mesh::cDataBlockAMR *block;
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];    

    int CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+    
      sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(0,0);

    //1. Identify the cell
    block=node->block;
    if (block==NULL) exit(__LINE__,__FILE__,"Block is NULL");

    cell=block->GetCenterNode(_getCenterNodeLocalNumber(icell,jcell,kcell));
    if (cell==NULL) exit(__LINE__,__FILE__,"Cell is NULL");

    char *SamplingData=cell->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset;
  
    //2. Populate the particle lists
    int nTotalInjectedParticles=30;
    double v[3],vbulk[3]={0.0,0.0,0.0},mvsum[3]={0.0,0.0,0.0},msum=0.0;

    if (s0==s1) {
      CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
        sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(s0,s0);

//      *((double*)(SamplingData+CollFreqOffset))=0.0;
    }
    else {
      int map[4][2]={{s0,s0},{s0,s1},{s1,s0},{s1,s1}};

      for (i=0;i<4;i++) { 
        CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
          sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(map[i][0],map[i][1]);

 //         *((double*)(SamplingData+CollFreqOffset))=0.0;
      }
    }

    for (int ispec=0;ispec<2;ispec++) {
      int s=(ispec==0) ? s0 : s1;
      double m=MD::GetMass(s); 

      for (int i=0;i<nTotalInjectedParticles;i++) {
        ptr=PB::GetNewParticle(block->FirstCellParticleTable[icell+_BLOCK_CELLS_X_*(jcell+_BLOCK_CELLS_Y_*kcell)]);
        PIC::Distribution::MaxwellianVelocityDistribution(v,vbulk,Tkin,s);

        PB::SetV(v,ptr);
        PB::SetI(s,ptr);

        if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
          PB::SetIndividualStatWeightCorrection(1.0,ptr);
        }

        if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_==_PIC_MODE_ON_) {
          PIC::ParticleBuffer::byte *ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr); 

          switch (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL_) {
          case _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL__LB_:
            PIC::IDF::LB::InitRotTemp(Trot,ParticleData); 
            PIC::IDF::LB::InitVibTemp(Tvib,ParticleData); 
          break;

          default:
            exit(__LINE__,__FILE__,"Error: not implemented");
          }
        }

        for (int j=0;j<3;j++) mvsum[j]+=m*v[j];
        msum+=m;
      }
    }

    //3. Calculate the initial momentum and energy 
    ptr=block->FirstCellParticleTable[icell+_BLOCK_CELLS_X_*(jcell+_BLOCK_CELLS_Y_*kcell)];

    while (ptr!=-1) {
      double *v=PB::GetV(ptr);
      int s=PB::GetI(ptr);
      double m=MD::GetMass(s);
      int ispec=(s==s0) ? 0 : 1;


      for (int j=0;j<3;j++) {
        energySumInit+=0.5*m*v[j]*v[j];
        mvSumInit[j]+=m*v[j];
      }

      MeanKineticEnergyInit[ispec]+=0.5*m*Vector3D::DotProduct(v,v);

      if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_==_PIC_MODE_ON_) {
        PIC::ParticleBuffer::byte *ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

        switch (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL_) {
        case _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL__LB_:
          MeanVibEnergyInit[ispec]+=PIC::IDF::LB::GetVibE(-1,ParticleData);
          MeanRotEnergyInit[ispec]+=PIC::IDF::LB::GetRotE(ParticleData);

          energySumInit+=PIC::IDF::LB::GetVibE(-1,ParticleData);
          energySumInit+=PIC::IDF::LB::GetRotE(ParticleData);
          break;
        default:
          exit(__LINE__,__FILE__,"Error: not implemented"); 
        }	  
      }

      ptr=PB::GetNext(ptr);
    }


    //4. Simulate collisions: deermine the particle weight such that for dt=1, there will be 1 collision per particle
    double dt[2]={1.0,1.0},Measure=1.0;

    //set the weight, time step, and measure
    double InitWeight[2],InitMeasure,InitTimeStep[2];
    double m_s0,m_s1,m,vv[3]={0.0,0.0,0.0};
    double MeanRelativeVelocity,sigma,eanRelativeVelocity,StatWeight[2],n[2];
    int ispec,s;

    m_s0=MD::GetMass(s0);
    m_s1=MD::GetMass(s1); 
    m=m_s0*m_s1/(m_s0+m_s1);
    MeanRelativeVelocity=sqrt(8.0*Kbol*Tkin/(Pi*m));

    InitMeasure=cell->Measure;
    cell->Measure=Measure;

    for (ispec=0;ispec<2;ispec++) {
      s=(ispec==0) ? s0 : s1;

      sigma=MD::MolecularModels::GetTotalCrossSection(s,vv,s,vv);
      m=MD::GetMass(s);

      MeanRelativeVelocity=sqrt(16.0*Kbol*Tkin/(Pi*m));
      StatWeight[ispec]=Measure/(2.0*nTotalInjectedParticles*sigma*MeanRelativeVelocity*dt[ispec]);

      if (s0!=s1) {
        InitTimeStep[ispec]=block->GetLocalTimeStep(s);
        InitWeight[ispec]=block->GetLocalParticleWeight(s);
      }
      else {
        InitTimeStep[ispec]=(ispec==0) ? block->GetLocalTimeStep(s) : InitTimeStep[0];
        InitWeight[ispec]=(ispec==0) ? block->GetLocalParticleWeight(s) : InitWeight[0];
      }

      if (ispec==1) {
        if (UseCommonTimeStep==true) {
          dt[1]=dt[0];
	      } else if (dt[0]==dt[1]) {
          dt[1]=0.33*dt[0];
	    }

	    if (UseCommonStatWeight==true) {
        StatWeight[1]=StatWeight[0];
	    }
	    else if (StatWeight[0]==StatWeight[1]) {
        StatWeight[1]=0.33*StatWeight[0];
	    }
    }

    block->SetLocalTimeStep(dt[ispec],s);
    block->SetLocalParticleWeight(StatWeight[ispec],s);

    n[ispec]=StatWeight[ispec]*nTotalInjectedParticles/Measure; 
  }

    if (s0==s1) {
      MeanRelativeVelocity=sqrt(16.0*Kbol*Tkin/(Pi*m_s0));
      sigma=MD::MolecularModels::GetTotalCrossSection(s0,vv,s0,vv);

      CollsionFrequentcyTheory[0][0]=pow(n[0]+n[1],2)*sigma*MeanRelativeVelocity;     
      CollsionFrequentcyTheory[1][1]=CollsionFrequentcyTheory[0][0];
    }
    else {
      for (i=0;i<2;i++) for (j=0;j<2;j++) {
        m=m_s0*m_s1/(m_s0+m_s1);
        MeanRelativeVelocity=sqrt(8.0*Kbol*Tkin/(Pi*m));
        sigma=MD::MolecularModels::GetTotalCrossSection(s0,vv,s1,vv); 
        CollsionFrequentcyTheory[i][j]=n[0]*n[1]*sigma*MeanRelativeVelocity;
      }   
    }


     if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_==_PIC_MODE_ON_) {
        for (ispec=0;ispec<2;ispec++) {
          int n,s=(ispec==0) ? s0 : s1;
          double RotDF,EtaVib,c,e=0.0; 

          switch (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL_) {
          case _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL__LB_:
            RotDF=PIC::IDF::nTotalRotationalModes[s];
            MeanRotEnergyTheory[ispec]=0.5*RotDF*Kbol*Trot;

            for (n=0;n<PIC::IDF::nTotalVibtationalModes[s];n++) {
              double c,ThetaVib,EtaVib;
	    
              ThetaVib=PIC::IDF::CharacteristicVibrationalTemperature[n+s*PIC::IDF::nSpeciesMaxVibrationalModes];
              c=ThetaVib/Tvib;
              EtaVib=2.0*c/(exp(c)-1.0);
              e+=0.5*EtaVib*Kbol*Tvib;
            }

            MeanVibEnergyTheory[ispec]=e;

            break;
          default:
            exit(__LINE__,__FILE__,"Error: not implemented");
          }
        }
      }


    for (int i=0;i<nIterations;i++) {
      fCellCollision(icell,jcell,kcell,node);
    }

    //5. Get the collision frequency
    CollsionFrequentcy[0][0]=(*SamplingData)/nIterations;

    if (s0==s1) {
      CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
        sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(s0,s0);

      //CollsionFrequentcy[0][0]=(*((double*)(SamplingData+CollFreqOffset)))/nIterations;
    }
    else {
      int map[4][2]={{s0,s0},{s0,s1},{s1,s0},{s1,s1}};

      for (i=0;i<4;i++) {
        CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
          sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(map[i][0],map[i][1]);

        //CollsionFrequentcy[map[i][0]][map[i][1]]=(*((double*)(SamplingData+CollFreqOffset)))/nIterations; 
      }
    }



    //6. Verify the results
    ptr=block->FirstCellParticleTable[icell+_BLOCK_CELLS_X_*(jcell+_BLOCK_CELLS_Y_*kcell)];

    while (ptr!=-1) {
      double *v=PB::GetV(ptr);
      int s=PB::GetI(ptr);
      double m=MD::GetMass(s);
      int ispec=(s==s0) ? 0 : 1;

      for (int j=0;j<3;j++) {
        mvSumAfter[j]+=m*v[j];
	      vSumAfter[j]+=v[j];
        energySumAfter+=0.5*m*v[j]*v[j];
      }

      MeanKineticEnergyAfter[ispec]+=0.5*m*Vector3D::DotProduct(v,v);

      if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_==_PIC_MODE_ON_) {
        PIC::ParticleBuffer::byte *ParticleData=PIC::ParticleBuffer::GetParticleDataPointer(ptr);

        switch (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL_) {
        case _PIC_INTERNAL_DEGREES_OF_FREEDOM_MODEL__LB_:
          MeanVibEnergyAfter[ispec]+=PIC::IDF::LB::GetVibE(-1,ParticleData);
          MeanRotEnergyAfter[ispec]+=PIC::IDF::LB::GetRotE(ParticleData);

          energySumAfter+=PIC::IDF::LB::GetVibE(-1,ParticleData);
          energySumAfter+=PIC::IDF::LB::GetRotE(ParticleData);
          break;
        default:
          exit(__LINE__,__FILE__,"Error: not implemented");
        }
      }

      long int ptr_next=PB::GetNext(ptr);
      PB::DeleteParticle(ptr);
      ptr=ptr_next;
    }

    //7. Restore the initial state
    for (ispec=0;ispec<2;ispec++) {
      s=(ispec==0) ? s0 : s1;

      block->SetLocalTimeStep(InitTimeStep[ispec],s); 
      block->SetLocalParticleWeight(InitWeight[ispec],s);
    }


    block->FirstCellParticleTable[icell+_BLOCK_CELLS_X_*(jcell+_BLOCK_CELLS_Y_*kcell)]=-1;
  }
}

struct ParticleCollisionTestCase {
    void (*fCellCollision)(int, int, int, cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>*); // Function pointer
    int s0,s1;
    string name;
};


// Derived class for particle collision tests
class ParticleCollisionTest :
    public ParticleTestBase<>,
    public ::testing::TestWithParam<ParticleCollisionTestCase> {
protected:
    void SetUp() override {
        ParticleTestBase<>::SetUp();
    }

    void TearDown() override {
        ParticleTestBase<>::TearDown();
    }
};


//Test NTC collision model: tested conservations of momentum and energy, and colliaiio frequency
TEST_P(ParticleCollisionTest, MyHandlesInputs) {
  namespace MC=PIC::MolecularCollisions;
  using namespace AMPS_COLLISON_TEST;
  int s0,s1;

  // Print the function name
  ParticleCollisionTestCase test_case=GetParam();
  s0=test_case.s0;
  s1=test_case.s1;

  std::cout << "\033[1m" << "Testing function: " << test_case.name << " (s0=" << s0 << ", s1=" << s1 <<") \033[0m" << std::endl;

  AMPS_COLLISON_TEST::ResetAll();

  //reset collision freq counters
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node=PIC::Mesh::mesh->ParallelNodesDistributionList[PIC::Mesh::mesh->ThisThread];

    int CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
      sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(0,0);

    auto block=node->block;
    if (block==NULL) exit(__LINE__,__FILE__,"Block is NULL");

    auto cell=block->GetCenterNode(_getCenterNodeLocalNumber(0,0,0));
    if (cell==NULL) exit(__LINE__,__FILE__,"Cell is NULL");

    char *SamplingData=cell->GetAssociatedDataBufferPointer()+PIC::Mesh::collectingCellSampleDataPointerOffset;
 

  if (s0==s1) {
     CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
        sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(s0,s0);

      *((double*)(SamplingData+CollFreqOffset))=0.0;
    }
    else {
      int map[4][2]={{s0,s0},{s0,s1},{s1,s0},{s1,s1}};

      for (int i=0;i<4;i++) {
        CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
          sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(map[i][0],map[i][1]);

          *((double*)(SamplingData+CollFreqOffset))=0.0;
      }
    }
    
  int nTotalCollisionCycles=120;
  int nIteractoinInCycle=4000;

  for (int itest=0;itest<nTotalCollisionCycles;itest++) {
    HeatBath(s0,s1,300,300,300,true,true,test_case.fCellCollision,nIteractoinInCycle);
  }

  //calcualte collision freq
  if (s0==s1) {
    CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
      sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(s0,s0);

    CollsionFrequentcy[0][0]=(*((double*)(SamplingData+CollFreqOffset)))/(nTotalCollisionCycles*nIteractoinInCycle);
  }
  else {
    int map[4][2]={{s0,s0},{s0,s1},{s1,s0},{s1,s1}};

    for (int i=0;i<4;i++) {
      CollFreqOffset=MC::ParticleCollisionModel::CollsionFrequentcySampling::SamplingBufferOffset+
        sizeof(double)*MC::ParticleCollisionModel::CollsionFrequentcySampling::Offset(map[i][0],map[i][1]);

      CollsionFrequentcy[map[i][0]][map[i][1]]=(*((double*)(SamplingData+CollFreqOffset)))/(nTotalCollisionCycles*nIteractoinInCycle); 
    }
  }

  //evaluate temparature
  double TkinInit[2],Tkin[2],Trot[2]={0.0,0.0};
  cRelativeDiff RelativeDiff;

  if (s0==s1) {
    EXPECT_LT(RelativeDiff(energySumInit,energySumAfter),1.0E-10); 
    EXPECT_LT(RelativeDiff(CollsionFrequentcy[0][0],CollsionFrequentcyTheory[0][0]),3.0E-2) << endl;

    for (int jj=0;jj<3;jj++) if (mvSumAfter[jj]+mvSumInit[jj]>0.0) {
      EXPECT_LT(RelativeDiff(mvSumAfter[jj],mvSumInit[jj]),1.0E-10) << " jj=" << jj << endl;
    }

    TkinInit[0]=2.0*MeanKineticEnergyInit[0]/(3.0*Kbol);
    Tkin[0]=2.0*MeanKineticEnergyAfter[0]/(3.0*Kbol);

    if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_==_PIC_MODE_ON_) {
      int RotDF=PIC::IDF::nTotalRotationalModes[s0];

      if (RotDF!=0) {
        Trot[0]=2.0/(RotDF*Kbol)*MeanRotEnergyAfter[0];
        EXPECT_LT(RelativeDiff(Tkin[0],Trot[0]),3.0E-2);
      }

      if (MeanVibEnergyTheory[0]+MeanVibEnergyInit[0]!=0.0) 
        EXPECT_LT(RelativeDiff(MeanVibEnergyTheory[0],MeanVibEnergyInit[0]),1.0E-5);

      if (MeanRotEnergyTheory[0]+MeanRotEnergyInit[0]!=0.0)
        EXPECT_LT(RelativeDiff(MeanRotEnergyTheory[0],MeanRotEnergyInit[0]),1.0E-5); 

      if (MeanVibEnergyAfter[0]+MeanVibEnergyInit[0]!=0.0)
        EXPECT_LT(RelativeDiff(MeanVibEnergyAfter[0],MeanVibEnergyInit[0]),1.0E-5);

      if (MeanRotEnergyAfter[0]+MeanRotEnergyInit[0]!=0.0)
        EXPECT_LT(RelativeDiff(MeanRotEnergyAfter[0],MeanRotEnergyInit[0]),1.0E-5);
    }
  }
  else {
    EXPECT_LT(RelativeDiff(energySumInit,energySumAfter),1.0E-10) << test_case.name << ", s0=" << s0 << ", s1=" << s1 << endl;

    for (int i=0;i<2;i++) for (int j=0;j<2;j++) {
      EXPECT_LT(RelativeDiff(CollsionFrequentcy[i][j],CollsionFrequentcyTheory[i][j]),3.0E-2) << " i=" << i << ", j=" << j << endl;
    }

    for (int jj=0;jj<3;jj++) if (mvSumAfter[jj]+mvSumInit[jj]>0.0) {
      EXPECT_LT(RelativeDiff(mvSumAfter[jj],mvSumInit[jj]),1.0E-10) << " jj=" << jj << endl;
    }

    TkinInit[0]=2.0*MeanKineticEnergyInit[0]/(3.0*Kbol);
    TkinInit[1]=2.0*MeanKineticEnergyInit[1]/(3.0*Kbol);

    EXPECT_LT(fabs(TkinInit[0]-TkinInit[1])/(TkinInit[0]+TkinInit[1]),3.0E-2);

    Tkin[0]=2.0*MeanKineticEnergyAfter[0]/(3.0*Kbol);
    Tkin[1]=2.0*MeanKineticEnergyAfter[1]/(3.0*Kbol);

    EXPECT_LT(fabs(Tkin[0]-Tkin[1])/(Tkin[0]+Tkin[1]),3.0E-2);

    if (_PIC_INTERNAL_DEGREES_OF_FREEDOM_MODE_==_PIC_MODE_ON_) {
      int RotDF;

      RotDF=PIC::IDF::nTotalRotationalModes[s0];
      if (RotDF!=0) {
        Trot[0]=2.0/(RotDF*Kbol)*MeanRotEnergyAfter[0];
        EXPECT_LT(RelativeDiff(Tkin[0],Trot[0]),1.0E-2);
      }

      RotDF=PIC::IDF::nTotalRotationalModes[s1];
      if (RotDF!=0) {
        Trot[1]=2.0/(RotDF*Kbol)*MeanRotEnergyAfter[1];
        EXPECT_LT(RelativeDiff(Tkin[1],Trot[1]),1.0E-2);
      }
	    
      for (int i=0;i<2;i++) {
        if (MeanVibEnergyAfter[i]+MeanVibEnergyInit[i]!=0.0)
          EXPECT_LT(RelativeDiff(MeanVibEnergyAfter[i],MeanVibEnergyInit[i]),1.0E-5) << " i=" << i << endl;

        if (MeanRotEnergyAfter[i]+MeanRotEnergyInit[i]!=0.0)
          EXPECT_LT(RelativeDiff(MeanRotEnergyAfter[i],MeanRotEnergyInit[i]),1.0E-5) << " i=" << i << endl;

        if (MeanVibEnergyTheory[i]+MeanVibEnergyInit[i]!=0.0)
          EXPECT_LT(RelativeDiff(MeanVibEnergyTheory[i],MeanVibEnergyInit[i]),1.0E-5) << " i=" << i << endl;

        if (MeanRotEnergyTheory[i]+MeanRotEnergyInit[i]!=0.0)
          EXPECT_LT(RelativeDiff(MeanRotEnergyTheory[i],MeanRotEnergyInit[i]),1.0E-5) << " i=" << i << endl;
      }
    } 
  }

//  for (int jj=0;jj<3;jj++) EXPECT_LT(fabs(vSumAfter[jj]),1.0E-10) << ", jj=" << jj <<  endl;
}



INSTANTIATE_TEST_SUITE_P(
    ParticleCollisionTest,             // Test suite name
    ParticleCollisionTest,             // Test fixture name
    ::testing::Values(                 // Test cases
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_simplified,0,0,"mf_simplified"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_simplified,0,1,"mf_simplified"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_simplified,1,0,"mf_simplified"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_simplified,1,1,"mf_simplified"},

        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc_simplified,0,0,"ntc_simplified"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc_simplified,0,1,"ntc_simplified"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc_simplified,1,0,"ntc_simplified"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc_simplified,1,1,"ntc_simplified"},

        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_Yinsi,0,0,"mf_Yinsi"}, 
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_Yinsi,0,1,"mf_Yinsi"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_Yinsi,1,0,"mf_Yinsi"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf_Yinsi,1,1,"mf_Yinsi"},

        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf,0,0,"mf"}, 
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf,0,1,"mf"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf,1,0,"mf"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_mf,1,1,"mf"},

        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc,0,0,"ntc"}, 
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc,0,1,"ntc"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc,1,0,"ntc"},
        ParticleCollisionTestCase{PIC::MolecularCollisions::ParticleCollisionModel::ModelCellCollisions_ntc,1,1,"ntc"}
      )
);

//particle velocity redistribution test 
struct VelocityAfterCollisionTestCase {
  void (*fVelocityAfterCollision)(double*,double,double*,double,double*,double*); // Function pointer
  int s0,s1;
  string name;
};

// Derived class for velocity after collision tests
class VelocityAfterCollisionTest :
    public ParticleTestBase<>,
    public ::testing::TestWithParam<VelocityAfterCollisionTestCase> {
protected:
    void SetUp() override {
        ParticleTestBase<>::SetUp();
    }

    void TearDown() override {
        ParticleTestBase<>::TearDown();
    }
};


TEST_P(VelocityAfterCollisionTest, MyHandlesInputs) {
  using namespace AMPS_COLLISON_TEST;
  double RelativeErrorEnergy,RelativeErrorMomentum,RelativeErrorVel;
  int s0,s1;

  // Print the function name
  VelocityAfterCollisionTestCase test_case=GetParam();
  s0=test_case.s0;
  s1=test_case.s1;

  std::cout << "\033[1m" << "Testing function: " << test_case.name << " (s0=" << s0 << ", s1=" << s1 <<") \033[0m" << std::endl;

  VelocityAfterCollision(RelativeErrorMomentum,RelativeErrorEnergy,RelativeErrorVel,s0,s1,test_case.fVelocityAfterCollision);
  EXPECT_LT(RelativeErrorMomentum,1.0E-10);
  EXPECT_LT(RelativeErrorEnergy,1.0E-10);
  EXPECT_LT(RelativeErrorVel,1.0E-10);
}

INSTANTIATE_TEST_SUITE_P(
    VelocityAfterCollisionTest,        // Test suite name
    VelocityAfterCollisionTest,        // Test fixture name
    ::testing::Values(                 // Test cases
        VelocityAfterCollisionTestCase{PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision,0,0,"VelocityScattering::HS"}, 
        VelocityAfterCollisionTestCase{PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision,0,1,"VelocityScattering::HS"},
        VelocityAfterCollisionTestCase{PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision,1,0,"VelocityScattering::HS"},
        VelocityAfterCollisionTestCase{PIC::MolecularCollisions::VelocityScattering::HS::VelocityAfterCollision,1,1,"VelocityScattering::HS"}
      )
);

#endif
  
