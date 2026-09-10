#include <gtest/gtest.h>
#include "pic.h"

#include "ParticleTestBase.h"

void split_merge_test_for_linker() {}

#if _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_
namespace AMPS_SPLIT_MERGE_TEST {

  void GenerateSingleWeight(int s, double Temp,int nInjectedParticles,long int& FirstParticles) {
    namespace PB=PIC::ParticleBuffer;
    double vbulk[3]={0.0,0.0,0.0},v[3];
    long int p;

    for (int i=0;i<nInjectedParticles;i++) {
      PIC::Distribution::MaxwellianVelocityDistribution(v,vbulk,Temp,s); 

      if (_PIC_FIELD_LINE_MODE_==_PIC_MODE_ON_) {
        v[1]=sqrt(v[1]*v[1]+v[2]*v[2]);
        v[2]=0.0;
      }

      p=PB::GetNewParticle(FirstParticles);
      PB::SetI(s,p);
      PB::SetV(v,p);

      if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
        PB::SetIndividualStatWeightCorrection(1.0,p);
      }
      else {
        exit(__LINE__,__FILE__,"Error: splitting/merging is available only when  _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_");
      }	
    }
  }

  void GenerateVariableWeight(int s, double Temp,int nInjectedParticles,long int& FirstParticles) {
    namespace PB=PIC::ParticleBuffer;
    namespace MD=PIC::MolecularData;
    double beta,w,v[3],m,thermal_speed;
    long int p;

    m=MD::GetMass(s);
    thermal_speed=sqrt(3.0*Kbol*Temp/m);
    beta=m/(2.0*Kbol*Temp);

    for (int i=0;i<nInjectedParticles;i++) {
      for (int idim=0;idim<3;idim++) v[idim]=(-2.0+rnd()*4.0)*thermal_speed;
      w=exp(-beta*Vector3D::DotProduct(v,v));         

      if (_PIC_FIELD_LINE_MODE_==_PIC_MODE_ON_) {
        v[1]=sqrt(v[1]*v[1]+v[2]*v[2]);
        v[2]=0.0;
      }

      p=PB::GetNewParticle(FirstParticles);
      PB::SetI(s,p);
      PB::SetV(v,p);

      if (_INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_) {
        PB::SetIndividualStatWeightCorrection(w,p);
      }
      else {
        exit(__LINE__,__FILE__,"Error: splitting/merging is available only when  _INDIVIDUAL_PARTICLE_WEIGHT_MODE_ == _INDIVIDUAL_PARTICLE_WEIGHT_ON_");
      }
    }
  }

  void GetMomentumAndEnergy(int s,double& Weight,double* Momentum,double& Energy,long int FirstParticle) {
    namespace MD=PIC::MolecularData;
    namespace PB=PIC::ParticleBuffer;
    double m,v[3],w;
    int i;

    m=MD::GetMass(s);
    Energy=0.0,Weight=0.0;

    for (i=0;i<3;i++) Momentum[i]=0.0;
      
    while (FirstParticle!=-1) {
      if (PB::GetI(FirstParticle)==s) {
        PB::GetV(v,FirstParticle);
        w=PB::GetIndividualStatWeightCorrection(FirstParticle);

        Weight+=w;
        Energy+=0.5*m*w*Vector3D::DotProduct(v,v);
        for (i=0;i<3;i++) Momentum[i]+=m*w*v[i];
      }

      FirstParticle=PB::GetNext(FirstParticle);
    }
  }  
}       


//particle merge test
struct ParticleMergeTestCase {
  int s0,s1; 
  int nInjectParticles; 
  void (*fGenerateParticlePopulation)(int s,double,int,long int&); // Function pointer
  string name;
};




// Derived class for particle merge tests
class ParticleMergeTest :
    public ParticleTestBase<10000>,
    public ::testing::TestWithParam<ParticleMergeTestCase> { 
protected:
    void SetUp() override {
        ParticleTestBase<10000>::SetUp();
    }

    void TearDown() override {
        ParticleTestBase<10000>::TearDown();
    }
};

class ParticleSplitTest :
    public ParticleTestBase<10000>,
    public ::testing::TestWithParam<ParticleMergeTestCase> {
protected:
    void SetUp() override {
        ParticleTestBase<10000>::SetUp();
    }

    void TearDown() override {
        ParticleTestBase<10000>::TearDown();
    }
};


//Test particle merging  
TEST_P(ParticleMergeTest, MyHandlesInputs) {
  namespace PB=PIC::ParticleBuffer;
  using namespace AMPS_SPLIT_MERGE_TEST;

  long int FirstParticle=-1;
  ParticleMergeTestCase test_case=GetParam();
  double p_init,p;
  int s0,s1;

  s0=test_case.s0;
  s1=test_case.s1;

  std::cout << "\033[1m" << "Testing function: " << test_case.name << " (s0=" << s0 << ", s1=" << s1 <<") \033[0m" << std::endl;

  auto GetParticleNumber =  [&] (int s) {
    long int ptr=FirstParticle; 
    int res=0;

    while (ptr!=-1) {
      if (PB::GetI(ptr)==s) res++;   
      ptr=PB::GetNext(ptr);
    }

    return res;
  }; 

  //init particles 
  double Temp=400.0; 
  test_case.fGenerateParticlePopulation(s0,Temp,test_case.nInjectParticles,FirstParticle);
  test_case.fGenerateParticlePopulation(s1,Temp,test_case.nInjectParticles,FirstParticle);

  //get initial momentum and energy   
  double Energy_s0,Energy_s1,Momentum_s0[3],Momentum_s1[3];
  double Energy_init_s0,Energy_init_s1,Momentum_init_s0[3],Momentum_init_s1[3];
  double w_init_s0,w_init_s1,w_s0,w_s1;
  int npart,npart_init_s0,npart_init_s1,n_requested_particles;

  //reduce the number of particles (s0) 
  if (s0==s1) { 
    GetMomentumAndEnergy(s0,w_init_s0,Momentum_init_s0,Energy_init_s0,FirstParticle);
    npart_init_s0=GetParticleNumber(s0);
    n_requested_particles=0.3*test_case.nInjectParticles;
    PIC::ParticleSplitting::MergeParticleList(s0,FirstParticle,n_requested_particles);

    npart=GetParticleNumber(s0); 
    EXPECT_NE(npart,npart_init_s0);
    EXPECT_EQ(npart,n_requested_particles);

    GetMomentumAndEnergy(s0,w_s0,Momentum_s0,Energy_s0,FirstParticle);
    p_init=Vector3D::Length(Momentum_init_s0);
    p=Vector3D::Length(Momentum_s0);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s0-Energy_s0)/(Energy_init_s0+Energy_s0),1.0E-10);
    EXPECT_LT(fabs(w_init_s0-w_s0)/(w_init_s0+w_s0),1.0E-10);
  }
  else {
    GetMomentumAndEnergy(s0,w_init_s0,Momentum_init_s0,Energy_init_s0,FirstParticle);
    GetMomentumAndEnergy(s1,w_init_s1,Momentum_init_s1,Energy_init_s1,FirstParticle);

     //reduce the number of particles (s0)
    npart_init_s0=GetParticleNumber(s0);
    npart_init_s1=GetParticleNumber(s1);
    n_requested_particles=0.3*test_case.nInjectParticles;
    PIC::ParticleSplitting::MergeParticleList(s0,FirstParticle,n_requested_particles);

    npart=GetParticleNumber(s0);
    EXPECT_EQ(npart,n_requested_particles); 

    npart=GetParticleNumber(s1);
    EXPECT_EQ(npart_init_s1,npart);

    GetMomentumAndEnergy(s0,w_s0,Momentum_s0,Energy_s0,FirstParticle);
    GetMomentumAndEnergy(s1,w_s1,Momentum_s1,Energy_s1,FirstParticle);

    p_init=Vector3D::Length(Momentum_init_s0);
    p=Vector3D::Length(Momentum_s0);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s0-Energy_s0)/(Energy_init_s0+Energy_s0),1.0E-10);
    EXPECT_LT(fabs(w_init_s0-w_s0)/(w_init_s0+w_s0),1.0E-10);

    p_init=Vector3D::Length(Momentum_init_s1);
    p=Vector3D::Length(Momentum_s1);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s1-Energy_s1)/(Energy_init_s1+Energy_s1),1.0E-10);
    EXPECT_LT(fabs(w_init_s1-w_s1)/(w_init_s1+w_s1),1.0E-10);

    //reduce the number of particles (s1)
    npart_init_s0=GetParticleNumber(s0);
    npart_init_s1=GetParticleNumber(s1);
    n_requested_particles=0.3*test_case.nInjectParticles;
    PIC::ParticleSplitting::MergeParticleList(s1,FirstParticle,n_requested_particles);

    npart=GetParticleNumber(s0);
    EXPECT_EQ(npart_init_s0,npart);

    npart=GetParticleNumber(s1);
    EXPECT_EQ(npart,n_requested_particles);

    GetMomentumAndEnergy(s0,w_s0,Momentum_s0,Energy_s0,FirstParticle);
    GetMomentumAndEnergy(s1,w_s1,Momentum_s1,Energy_s1,FirstParticle);

    p_init=Vector3D::Length(Momentum_init_s0);
    p=Vector3D::Length(Momentum_s0);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s0-Energy_s0)/(Energy_init_s0+Energy_s0),1.0E-10);
    EXPECT_LT(fabs(w_init_s0-w_s0)/(w_init_s0+w_s0),1.0E-10);

    p_init=Vector3D::Length(Momentum_init_s1);
    p=Vector3D::Length(Momentum_s1);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s1-Energy_s1)/(Energy_init_s1+Energy_s1),1.0E-10);
    EXPECT_LT(fabs(w_init_s1-w_s1)/(w_init_s1+w_s1),1.0E-10);
  }
}

#if _PIC_FIELD_LINE_MODE_==_PIC_MODE_OFF_
INSTANTIATE_TEST_SUITE_P(
    ParticleMergeTest,             // Test suite name
    ParticleMergeTest,             // Test fixture name
    ::testing::Values(                 // Test cases
      ParticleMergeTestCase{0,0,1000,AMPS_SPLIT_MERGE_TEST::GenerateSingleWeight,"Single particle weight test"},   	      
      ParticleMergeTestCase{0,1,1000,AMPS_SPLIT_MERGE_TEST::GenerateSingleWeight,"Single particle weight test"},
      ParticleMergeTestCase{0,0,1000,AMPS_SPLIT_MERGE_TEST::GenerateVariableWeight,"Variable particle weight test"},
      ParticleMergeTestCase{0,1,1000,AMPS_SPLIT_MERGE_TEST::GenerateVariableWeight,"Variable particle weight test"}
      )
    );
#else 
INSTANTIATE_TEST_SUITE_P(
    ParticleMergeTest,             // Test suite name
    ParticleMergeTest,             // Test fixture name
    ::testing::Values(                 // Test cases
      ParticleMergeTestCase{0,0,1000,AMPS_SPLIT_MERGE_TEST::GenerateSingleWeight,"Single particle weight test"},
      ParticleMergeTestCase{0,1,1000,AMPS_SPLIT_MERGE_TEST::GenerateSingleWeight,"Single particle weight test"},
      ParticleMergeTestCase{0,0,1000,AMPS_SPLIT_MERGE_TEST::GenerateVariableWeight,"Variable particle weight test"},
      ParticleMergeTestCase{0,1,1000,AMPS_SPLIT_MERGE_TEST::GenerateVariableWeight,"Variable particle weight test"}
      )
    );


#endif


//========================================================================
// Particle Split tests
//Test particle merging  
TEST_P(ParticleSplitTest, MyHandlesInputs) {
  namespace PB=PIC::ParticleBuffer;
  using namespace AMPS_SPLIT_MERGE_TEST;

  long int FirstParticle=-1;
  ParticleMergeTestCase test_case=GetParam();
  double p_init,p;
  int s0,s1;

  s0=test_case.s0;
  s1=test_case.s1;

  std::cout << "\033[1m" << "Testing function: " << test_case.name << " (s0=" << s0 << ", s1=" << s1 <<") \033[0m" << std::endl;

  auto GetParticleNumber =  [&] (int s) {
    long int ptr=FirstParticle; 
    int res=0;

    while (ptr!=-1) {
      if (PB::GetI(ptr)==s) res++;   
      ptr=PB::GetNext(ptr);
    }

    return res;
  }; 

  //init particles 
  double Temp=400.0; 
  test_case.fGenerateParticlePopulation(s0,Temp,test_case.nInjectParticles,FirstParticle);
  test_case.fGenerateParticlePopulation(s1,Temp,test_case.nInjectParticles,FirstParticle);

  //get initial momentum and energy   
  double Energy_s0,Energy_s1,Momentum_s0[3],Momentum_s1[3];
  double Energy_init_s0,Energy_init_s1,Momentum_init_s0[3],Momentum_init_s1[3];
  double w_init_s0,w_s0,w_init_s1,w_s1;
  int npart,npart_init_s0,npart_init_s1,n_requested_particles;

  //increase the number of particles (s0) 
  if (s0==s1) { 
    GetMomentumAndEnergy(s0,w_init_s0,Momentum_init_s0,Energy_init_s0,FirstParticle);
    npart_init_s0=GetParticleNumber(s0);
    n_requested_particles=3*test_case.nInjectParticles;
    PIC::ParticleSplitting::SplitParticleList(s0,FirstParticle,n_requested_particles);

    npart=GetParticleNumber(s0); 
    EXPECT_NE(npart,npart_init_s0);
    EXPECT_EQ(npart,n_requested_particles);

    GetMomentumAndEnergy(s0,w_s0,Momentum_s0,Energy_s0,FirstParticle);
    p_init=Vector3D::Length(Momentum_init_s0);
    p=Vector3D::Length(Momentum_s0);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s0-Energy_s0)/(Energy_init_s0+Energy_s0),1.0E-10);
    EXPECT_LT(fabs(w_init_s0-w_s0)/(w_init_s0+w_s0),1.0E-10);
  }
  else {
    GetMomentumAndEnergy(s0,w_init_s0,Momentum_init_s0,Energy_init_s0,FirstParticle);
    GetMomentumAndEnergy(s1,w_init_s1,Momentum_init_s1,Energy_init_s1,FirstParticle);

     //increase the number of particles (s0)
    npart_init_s0=GetParticleNumber(s0);
    npart_init_s1=GetParticleNumber(s1);
    n_requested_particles=3*test_case.nInjectParticles;
    PIC::ParticleSplitting::SplitParticleList(s0,FirstParticle,n_requested_particles);

    npart=GetParticleNumber(s0);
    EXPECT_EQ(npart,n_requested_particles); 

    npart=GetParticleNumber(s1);
    EXPECT_EQ(npart_init_s1,npart);

    GetMomentumAndEnergy(s0,w_s0,Momentum_s0,Energy_s0,FirstParticle);
    GetMomentumAndEnergy(s1,w_s1,Momentum_s1,Energy_s1,FirstParticle);

    p_init=Vector3D::Length(Momentum_init_s0);
    p=Vector3D::Length(Momentum_s0);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s0-Energy_s0)/(Energy_init_s0+Energy_s0),1.0E-10);
    EXPECT_LT(fabs(w_init_s0-w_s0)/(w_init_s0+w_s0),1.0E-10);

    p_init=Vector3D::Length(Momentum_init_s1);
    p=Vector3D::Length(Momentum_s1);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s1-Energy_s1)/(Energy_init_s1+Energy_s1),1.0E-10);
    EXPECT_LT(fabs(w_init_s1-w_s1)/(w_init_s1+w_s1),1.0E-10);

    //increase the number of particles (s1)
    npart_init_s0=GetParticleNumber(s0);
    npart_init_s1=GetParticleNumber(s1);
    n_requested_particles=3*test_case.nInjectParticles;
    PIC::ParticleSplitting::SplitParticleList(s1,FirstParticle,n_requested_particles);

    npart=GetParticleNumber(s0);
    EXPECT_EQ(npart_init_s0,npart);

    npart=GetParticleNumber(s1);
    EXPECT_EQ(npart,n_requested_particles);

    GetMomentumAndEnergy(s0,w_s0,Momentum_s0,Energy_s0,FirstParticle);
    GetMomentumAndEnergy(s1,w_s1,Momentum_s1,Energy_s1,FirstParticle);

    p_init=Vector3D::Length(Momentum_init_s0);
    p=Vector3D::Length(Momentum_s0);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s0-Energy_s0)/(Energy_init_s0+Energy_s0),1.0E-10);
    EXPECT_LT(fabs(w_init_s0-w_s0)/(w_init_s0+w_s0),1.0E-10);

    p_init=Vector3D::Length(Momentum_init_s1);
    p=Vector3D::Length(Momentum_s1);
    EXPECT_LT(fabs(p_init-p)/(p_init+p),1.0E-10);
    EXPECT_LT(fabs(Energy_init_s1-Energy_s1)/(Energy_init_s1+Energy_s1),1.0E-10);
    EXPECT_LT(fabs(w_init_s1-w_s1)/(w_init_s1+w_s1),1.0E-10);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ParticleSplitTest,             // Test suite name
    ParticleSplitTest,             // Test fixture name
    ::testing::Values(                 // Test cases
      ParticleMergeTestCase{0,0,20,AMPS_SPLIT_MERGE_TEST::GenerateSingleWeight,"Field lines: Single particle weight test"},
      ParticleMergeTestCase{0,1,20,AMPS_SPLIT_MERGE_TEST::GenerateSingleWeight,"Field lines: Single particle weight test"},
      ParticleMergeTestCase{0,0,20,AMPS_SPLIT_MERGE_TEST::GenerateVariableWeight,"Field lines: Variable particle weight test"},
      ParticleMergeTestCase{0,1,20,AMPS_SPLIT_MERGE_TEST::GenerateVariableWeight,"Field lines: Variable particle weight test"}
      )
    );
#endif



