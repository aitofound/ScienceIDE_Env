#include <gtest/gtest.h>
#include "pic.h"

#include "ParticleTestBase.h"
#include "sampling.h"

void distribution_test_for_linker() {}

// Derived class for particle distribution tests
class ParticleDistributionTest :
    public ParticleTestBase<10000>,
    public ::testing::Test {
protected:
    void SetUp() override {
        ParticleTestBase<10000>::SetUp();
    }

    void TearDown() override {
        ParticleTestBase<10000>::TearDown();
    }
};



TEST_F(ParticleDistributionTest, MexwellianDistributionTest) {
  double v[3],vbulk[3]={0.0};
  int spec,iTest;
  cSampledValues speed_sum,vel_sum[3],energy_sum;
  cRelativeDiff RelativeDiff;
  double m=PIC::MolecularData::GetMass(0);

  const int nTests=100000;
  const double Temp=300.0;

  for (iTest=0;iTest<nTests;iTest++) {
    PIC::Distribution::MaxwellianVelocityDistribution(v,vbulk,Temp,0);

    speed_sum+=Vector3D::Length(v);
    energy_sum+=0.5*m*Vector3D::DotProduct(v,v);  

    for (int i=0;i<3;i++) vel_sum[i]+=v[i];
  } 

  //verify the results 

  for (int i=0;i<3;i++) {
    EXPECT_LT(fabs(vel_sum[i].GetVal())/speed_sum.GetVal(),1.0E-2); 
  }

  double TempTheory,SpeedTheory;

  SpeedTheory=sqrt(8.0*Kbol*Temp/(Pi*m));
  TempTheory=2.0*energy_sum/(3.0*Kbol);

  EXPECT_LT(RelativeDiff(speed_sum,SpeedTheory),1.0E-2);
  EXPECT_LT(RelativeDiff(TempTheory,Temp),1.0E-2);
}


